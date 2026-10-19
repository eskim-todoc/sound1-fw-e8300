"""Independent arithmetic checks of documented expectations, not target test results."""
from pathlib import Path
import json, math
import numpy as np

T=Path(__file__).resolve().parent
QA=T/'qa/unit-reconciliation'
checks=[]
def check(name,actual,expected):
    assert actual==expected,(name,actual,expected)
    checks.append({'case':name,'actual':actual,'expected':expected})
def sat24(v):return min(8388607,max(-8388608,v))
check('single mix',320>>5,10)
check('dual mix',(320>>5)+(-64>>5),8)
check('gain mix',(((320>>5)*65536)+((320>>5)*65536))>>16,20)
check('beam boundary',[(64*i>>5)>>1 for i in range(1,16)]+[(2048>>5)>>1],list(range(1,16))+[32])
check('AGC release',(655*65536+64880*0)>>16,655)
check('AGC attack',(62259*0+3276*65536)>>16,3276)
check('AGC apply',[sat24(x*8192>>12) for x in [-10,0,10,8388607,-8388608]],[-20,0,20,8388607,-8388608])
check('C volume',[50+((150-50)*v>>22) for v in [2936012,3355443,3774873,4194304]],[119,129,139,150])
A=((150-50)<<24)//(131072-65536)
B=(150<<8)-(A*131072>>16)
check('mapping coefficients',[A,B],[25600,-12800])
check('frame widths',[math.floor((2*p+4+10)/41)+1 for p in [13,14]],[1,2])
check('DAC offset',(100*14435)>>15,44)
check('DAC amplitude',((300-100)*20871)>>15,127)
for label,amps,want in [('tie',[100]*32,list(range(16))),('increasing',list(range(32)),list(range(16,32)))]:
    selected=sorted(sorted(range(32),key=lambda i:(amps[i]<<5)|(31-i),reverse=True)[:16])
    check('NofM '+label,selected,want)
order=[n for pair in zip(range(8),range(8,16)) for n in pair]
check('interleave',order,[0,8,1,9,2,10,3,11,4,12,5,13,6,14,7,15])
for prev,want in [(15,order),(0,[15]+order[:-1])]:
    result=order[-1:]+order[:-1] if abs(prev-order[-1])>abs(prev-order[0]) else order
    check('NofM start '+str(prev),result,want)
for b in [32,64]:
    x=np.rint(65536*np.sin(2*np.pi*b*np.arange(512)/512))
    # Both conventional periodic and symmetric windows have this unique peak bin.
    for window in [np.hanning(512),0.5-0.5*np.cos(2*np.pi*np.arange(512)/512)]:
        check('FFT peak '+str(b),int(np.argmax(np.abs(np.fft.rfft(x*window))[:256])),b)
result={'scope':'문서 기대값의 독립 산술 검산. CFX/HEAR/CM3 실기 실행 결과가 아님.','cases':checks}
(QA/'vector-checks.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
print('Independent arithmetic cases:',len(checks))
