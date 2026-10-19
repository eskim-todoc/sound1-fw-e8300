"""AGC 고정소수점 시뮬레이터

src/1__cfx/signalProcessing/agc.c 의 calculate_agc_gain() / apply_agc_gain() 산술을
그대로 재현한다. AUDIO_INPUT_RSHIFT 를 바꿔 가며 동작을 비교하는 데 쓴다.

  python -c "import io; exec(io.open('agc_sim.py',encoding='utf-8').read()); report(5,'current'); report(0,'no shift')"

주의
  - attack/release 는 정상상태(목표값 도달)로 둔다. 과도 응답은 재현하지 않는다.
  - cfx_pwr_to_dB / cfx_dB_to_pwr 는 하드웨어 가속기라 반올림이 여기 근사와
    다를 수 있다. 1 LSB 수준의 결론은 그만큼 불확실하다.
  - 콘솔이 CP949 라 한글 출력이 깨져 보일 수 있다. 값은 정상이다.

상세: 08_왜 믹서에서 5비트를 내리는가.md
"""
import math
FS=0x7FFFFF; INT24_MAX=0x7FFFFF; INT24_MIN=-0x800000
FS_SPL=120.0
AMP=[0,108138,216277,324416,432555,540694,648833,756971,865110,973249]
ATT=[-1661530,-2008762,-2355995,-2703227,-3050459,-3397692,-3744924,-4092157,-4439389,-4786621]
SLP=[47999,43193,38387,33581,28774,23968,19162,14356,9550,4744]
NOI=[481268,488027,494785,501544,508303,515061,521820,528579,535337,542096]
NOISE_SLOPE=131072; NG=-7700296; ROT=-6209352
MATH_DB=1972830; MATH_LIN=4194304

def clamp24(x): return INT24_MAX if x>INT24_MAX else (INT24_MIN if x<INT24_MIN else x)

def pwr_to_dB_q816(v47):
    """cfx_pwr_to_dB: 10*log10(frac48) -> m8p16 (Q8.16, [-128,+128) 포화)"""
    if v47<=0: v47=1
    d=10*math.log10(v47/2**47)
    q=int(round(d*65536))
    return clamp24(max(q, -128*65536))     # m8p16 하한 -128

def agc_gain_q1212(mix_peak, vol, shift_in_agc=5):
    a = mix_peak if mix_peak>=1 else 1
    norm = a << shift_in_agc
    indb = pwr_to_dB_q816(norm)
    indb12 = indb >> 4
    if indb < NG:
        acc = ((NOISE_SLOPE*indb) >> 20) + NOI[vol] - indb12
        acc = acc << 4
    elif ROT < indb:
        acc = ((SLP[vol]*indb) >> 16) + ATT[vol] - indb
    else:
        acc = indb + AMP[vol] - indb
    acc = clamp24(acc)
    gdb = acc                                   # attack/release 정상상태 = 목표값
    norm_db = gdb - MATH_DB
    lin_q123 = int((10**(norm_db/65536/10))*2**23)   # cfx_dB_to_pwr >>24
    acc = (lin_q123*MATH_LIN) >> 23
    return clamp24(acc), gdb, indb

def run(rshift):
    """rshift: 믹서 >>N.  반환 = {vol: [(spl, out_peak)]}"""
    res={}
    for v in range(10):
        row=[]
        for i in range(200,1261):
            spl=i/10.0
            mic=int(FS*10**((spl-FS_SPL)/20))
            if mic>FS: mic=FS
            mix=mic>>rshift
            g,gdb,indb=agc_gain_q1212(mix,v)
            out=(g*mix)>>12
            out=clamp24(out)
            row.append((spl,out,g,gdb/65536.0,indb/65536.0))
        res[v]=row
    return res

def report(rshift,label):
    r=run(rshift)
    print('\n########## %s (믹서 >>%d) ##########'%(label,rshift))
    print('%-5s %10s %10s %10s %10s  %s'%('vol','out@60','out@75','out@100','out@120','역전(출력이 줄어드는 구간)'))
    for v in range(10):
        d=dict((round(s,1),o) for s,o,_,_,_ in r[v])
        rev=[]
        prev=None
        for s,o,_,_,_ in r[v]:
            if prev is not None and o<prev[1]:
                rev.append((prev[0],s,prev[1],o))
            prev=(s,o)
        if rev:
            # 연속 구간 병합
            lo=rev[0][0]; hi=rev[-1][1]; mx=max(x[2] for x in rev); mn=min(x[3] for x in rev)
            msg='**있음** %.1f~%.1f dBSPL, %d -> %d (%.1f dB 감소)'%(lo,hi,mx,mn,20*math.log10(max(mn,1)/mx))
        else:
            msg='없음'
        print('%-5d %10d %10d %10d %10d  %s'%(v,d[60.0],d[75.0],d[100.0],d[120.0],msg))
    return r
