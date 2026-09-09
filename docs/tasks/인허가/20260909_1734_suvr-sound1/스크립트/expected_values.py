# -*- coding: utf-8 -*-
"""시험 벡터와 예상 출력값 산출 - 대상 함수의 연산을 정수 의미까지 그대로 재현한다.

여기서 나온 값이 SUVC 의 통과 기준이 되고, 은수님이 실기에서 얻은 값과 대조한다.
**재현할 수 없는 것은 값을 내지 않는다.** DSP 어셈블리 라이브러리
(cfx_pwr_to_dB_asm, cfx_dB_to_pwr_asm, Win_DFT_R_Forward, vMag)에 의존하는
함수는 «라이브러리 출력에 의존» 으로 표기하고 은수님 실측에 맡긴다.
"""
import io, os, sys, json

# ------------------------------------------------------------------ 소스 상수
# 근거 위치를 함께 적는다. 값을 지어내지 않는다.
CONST = {
    'FPGA_oneChannelDataTokenTime': (41, 'src/2__cm3/99_includeBoard/FPGA_ver2_1_0.h:146'),
    'FPGA_electrodAndStimulLevelTokenTime': (10, 'src/2__cm3/99_includeBoard/FPGA_ver2_1_0.h:147'),
    'FPGA_interphaseGapTokenTime': (4, 'src/2__cm3/99_includeBoard/FPGA_ver2_1_0.h:148'),
    'df_MaxNumTransferableChannel': (24, 'src/1__cfx/signalProcessing/definitionsForAlgorithm.h:137'),
    'm_attack_coeff_Q8_16': (62259, 'src/1__cfx/signalProcessing/agc.c:74'),
    'm_release_coeff_Q8_16': (655, 'src/1__cfx/signalProcessing/agc.c:75'),
    'm_one_minus_attack_coeff_Q8_16': (3276, 'src/1__cfx/signalProcessing/agc.c:76'),
    'm_one_minus_release_coeff_Q8_16': (64880, 'src/1__cfx/signalProcessing/agc.c:77'),
    'df_MaxNumOfElectrode': (32, 'src/1__cfx/signalProcessing/definitionsForAlgorithm.h:143'),
    'df_MaxNum_NofM': (16, 'src/1__cfx/signalProcessing/definitionsForAlgorithm.h:146'),
}
INT24_MAX = 8388607
INT24_MIN = -8388608


def c_int(v, bits=32):
    """C 의 고정폭 정수 감싸기를 흉내 낸다."""
    m = 1 << bits
    v &= m - 1
    return v - m if v >= (m >> 1) else v


def sat24(v):
    return INT24_MAX if v > INT24_MAX else (INT24_MIN if v < INT24_MIN else v)


def asr(v, n):
    """C 의 부호 있는 우측 시프트 (산술 시프트)."""
    return v >> n


# ------------------------------------------------------------------ 재현 함수

def data_clearBit(Data, bitPosition, bitLength):
    """commonDataProcessing.c:18 를 그대로 옮긴다."""
    bitSet = 0x01
    n = bitLength
    while n != 1:
        bitSet = c_int((bitSet << 1) | 0x01)
        n -= 1
    bitClearMasker = c_int(~c_int(bitSet << bitPosition))
    return c_int(Data & bitClearMasker)


def calculationNumFramePerOneChannle(pulsewidth):
    """stimulationParaCal.c:32 를 그대로 옮긴다."""
    one = CONST['FPGA_oneChannelDataTokenTime'][0]
    gap = CONST['FPGA_interphaseGapTokenTime'][0]
    lvl = CONST['FPGA_electrodAndStimulLevelTokenTime'][0]
    duration = pulsewidth
    duration += duration
    duration += gap
    duration += lvl
    tokenTime = one
    numFramePerChannel = 1
    while True:
        if duration < tokenTime:
            break
        numFramePerChannel += 1
        tokenTime += one
    return numFramePerChannel


def calculationTransferableChanneNum(numFramePerChannel, usableElectrodNum):
    """stimulationParaCal.c:60. 활성 경로는 나눗셈 한 줄이며 둘째 인자를 쓰지 않는다."""
    return CONST['df_MaxNumTransferableChannel'][0] // numFramePerChannel


def audio_agc_attack_release(current_q8_16, prev_q8_16):
    """agc.c:119. 직전 이득이 상태로 남으므로 함께 넘겨 받는다."""
    if prev_q8_16 <= current_q8_16:
        a = CONST['m_release_coeff_Q8_16'][0]
        b = CONST['m_one_minus_release_coeff_Q8_16'][0]
        branch = u'릴리즈'
    else:
        a = CONST['m_attack_coeff_Q8_16'][0]
        b = CONST['m_one_minus_attack_coeff_Q8_16'][0]
        branch = u'어택'
    acc = a * current_q8_16 + b * prev_q8_16
    acc = asr(acc, 16)
    acc = sat24(acc)
    return acc, branch


def nofm_peak_pick(amplitude):
    """stimulationStrategy.c:140 을 그대로 옮긴다.

    점수 = (진폭 << 5) | (31 - 색인). 같은 진폭이면 색인이 작은 쪽(저주파)이
    큰 점수를 가지므로 살아남는다.
    """
    NE = CONST['df_MaxNumOfElectrode'][0]
    NM = CONST['df_MaxNum_NofM'][0]
    packed = [(amplitude[i] << 5) | (31 - i) for i in range(NE)]
    top = packed[:NM]
    min_packed = top[0]
    min_idx = 0
    for i in range(1, NM):
        if top[i] < min_packed:
            min_packed, min_idx = top[i], i
    for i in range(NM, NE):
        cur = packed[i]
        if cur > min_packed:
            top[min_idx] = cur
            min_packed, min_idx = top[0], 0
            for t in range(1, NM):
                if top[t] < min_packed:
                    min_packed, min_idx = top[t], t
    mask = 0
    for i in range(NM):
        mask |= 1 << (31 - (top[i] & 0x1F))
    out = [b for b in range(NE) if mask & (1 << b)]
    return out


def nofm_interleaving(adjacent):
    """stimulationStrategy.c:262 를 그대로 옮긴다. 절반씩 나눠 교차 배치한다."""
    NM = CONST['df_MaxNum_NofM'][0]
    half = NM // 2
    out = [-1] * NM
    for i in range(half):
        out[i * 2] = adjacent[i]
        out[i * 2 + 1] = adjacent[i + half]
    if NM % 2 != 0:
        out[NM - 1] = adjacent[NM - 1]
    return out


# ------------------------------------------------------------------ 벡터 정의

def build_vectors():
    V = []

    # --- CM3-112 비트 구간 소거
    cases = []
    for data, pos, length in ((0xFFFF, 0, 4), (0xFFFF, 4, 4), (0x00FF, 2, 3),
                              (0x1234, 8, 8), (0xAAAA, 1, 1)):
        cases.append({'in': u'Data=0x%04X, bitPosition=%d, bitLength=%d' % (data, pos, length),
                      'out': u'0x%04X' % (data_clearBit(data, pos, length) & 0xFFFFFFFF)})
    V.append({'su': 'CM3-112', 'fn': 'data_clearBit', 'kind': u'재현',
              'note': u'순수 비트 연산이라 전부 재현된다.', 'cases': cases})

    # --- CM3-108 채널당 프레임 수
    cases = [{'in': u'pulsewidth=%d' % pw,
              'out': u'%d' % calculationNumFramePerOneChannle(pw)}
             for pw in (10, 20, 25, 30, 40, 50, 60)]
    V.append({'su': 'CM3-108', 'fn': 'calculationNumFramePerOneChannle', 'kind': u'재현',
              'note': u'duration = 2*pulsewidth + 4 + 10 이 41 의 몇 배인지로 정해진다.',
              'cases': cases})

    # --- CM3-109 전달 가능 채널 수
    cases = [{'in': u'numFramePerChannel=%d, usableElectrodNum=%d' % (n, e),
              'out': u'%d' % calculationTransferableChanneNum(n, e)}
             for n, e in ((1, 16), (2, 16), (3, 16), (4, 22), (5, 22), (8, 22))]
    V.append({'su': 'CM3-109', 'fn': 'calculationTransferableChanneNum', 'kind': u'재현',
              'note': u'활성 경로는 24 / numFramePerChannel 뿐이며 둘째 인자를 쓰지 않는다. '
                      u'둘째 인자를 바꿔도 결과가 같아야 한다.',
              'cases': cases})

    # --- CM3-111 출력단 보정 값
    V.append({'su': 'CM3-111', 'fn': 'readDAC_offsetValue', 'kind': u'상수',
              'note': u'상수 DAC_offset_uA 를 그대로 반환한다. 실기에서 그 값이 나오는지만 확인한다.',
              'cases': [{'in': u'없음', 'out': u'DAC_offset_uA 정의값과 같아야 한다'}]})

    # --- CFX-071 이득 시정수 적용
    cases = []
    for prev, cur in ((0, 65536), (65536, 0), (65536, 131072), (131072, 65536), (0, 0)):
        out, br = audio_agc_attack_release(cur, prev)
        cases.append({'in': u'직전 이득=%d, 입력 이득=%d' % (prev, cur),
                      'out': u'%d (%s 경로)' % (out, br)})
    V.append({'su': 'CFX-071', 'fn': 'audio_agc_attack_release', 'kind': u'재현',
              'note': u'직전 이득이 내부 상태로 남는다. 시험 전에 직전 이득을 지정한 값으로 두어야 한다. '
                      u'직전 <= 입력이면 릴리즈(계수 655), 아니면 어택(계수 62259) 경로다.',
              'cases': cases})

    # --- CFX-087 NofM 피크 선택
    NE = CONST['df_MaxNumOfElectrode'][0]
    cases = []
    # 벡터 가 : 색인이 커질수록 진폭이 커진다 -> 상위 16 개는 뒤쪽 색인
    amp_a = [i * 10 for i in range(NE)]
    cases.append({'in': u'진폭[i] = i*10 (0, 10, ... 310)',
                  'out': u'%s' % nofm_peak_pick(amp_a)})
    # 벡터 나 : 전부 같은 진폭 -> 저주파 보호 규칙으로 앞쪽 16 개
    amp_b = [100] * NE
    cases.append({'in': u'진폭 전부 100 (동점)',
                  'out': u'%s  (같은 크기면 저주파가 살아남는다)' % nofm_peak_pick(amp_b)})
    # 벡터 다 : 짝수 색인만 큰 값
    amp_c = [(200 if i % 2 == 0 else 5) for i in range(NE)]
    cases.append({'in': u'짝수 색인 200, 홀수 색인 5',
                  'out': u'%s' % nofm_peak_pick(amp_c)})
    # 벡터 라 : 한 곳만 최대
    amp_d = [0] * NE
    amp_d[20] = 1000
    cases.append({'in': u'색인 20 만 1000, 나머지 0',
                  'out': u'%s' % nofm_peak_pick(amp_d)})
    V.append({'su': 'CFX-087', 'fn': 'stimulationStrategy_NofM_Peak_Pick', 'kind': u'재현',
              'note': u'점수 = (진폭 << 5) | (31 - 색인). 같은 진폭이면 색인이 작은 쪽이 큰 점수를 '
                      u'가지므로 저주파가 보호된다. 결과는 색인 오름차순으로 나온다.',
              'cases': cases})

    # --- CFX-088 NofM 인터리빙
    cases = []
    adj_a = list(range(16))
    cases.append({'in': u'입력 %s' % adj_a, 'out': u'%s' % nofm_interleaving(adj_a)})
    adj_b = nofm_peak_pick(amp_c)
    cases.append({'in': u'앞 시험 «짝수 색인 200» 의 선택 결과 %s' % adj_b,
                  'out': u'%s' % nofm_interleaving(adj_b)})
    V.append({'su': 'CFX-088', 'fn': 'stimulationStrategy_NofM_Interleaving', 'kind': u'재현',
              'note': u'16 개를 앞 8 개와 뒤 8 개로 나눠 한 칸씩 번갈아 배치한다. '
                      u'인접 전극이 잇달아 자극되지 않게 하는 처리다.',
              'cases': cases})

    # --- CFX-086 NofM 버퍼 초기화
    V.append({'su': 'CFX-086', 'fn': 'stimulationStrategy_NofM_Init_Buffers', 'kind': u'재현',
              'note': u'선택 버퍼와 교차 버퍼 16 칸, 임시 버퍼 32 칸을 모두 -1 로 채운다.',
              'cases': [{'in': u'버퍼에 임의 값을 채워 둔 뒤 호출',
                         'out': u'g_NofM_adjacent_BandIndex[0..15] = -1, '
                                u'g_NofM_notAdjacent_BandIndex[0..15] = -1, '
                                u'addr_stimulationTempBuff[0..31] = -1'}]})

    # --- 재현 불가 목록
    LIB = [
        ('CFX-081', 'calculate_x_power_p',
         u'cfx_pwr_to_dB_asm 과 cfx_dB_to_pwr_asm 어셈블리 라이브러리를 거친다.'),
        ('CFX-073', 'update_FFT_inputData', u'HEAR 함수체인 출력 버퍼에 의존한다.'),
        ('CFX-074', 'find_freq_rep_value', u'주파수 변환 체인(vMag) 출력에 의존한다.'),
        ('CFX-069', 'audio_agc', u'전처리 체인이 만든 최대값 버퍼에 의존한다.'),
        ('CFX-070', 'calculate_agc_gain', u'입력이 전처리 체인 출력이며 dB 변환 라이브러리를 거친다.'),
        ('CFX-072', 'apply_agc_gain', u'믹서 출력 버퍼 전체에 적용된다.'),
        ('CFX-078', 'calculate_logaritmMapping_coeff', u'dB 변환 라이브러리를 거친다.'),
        ('CFX-079', 'calculate_logaritmMapping_coeff_with_audioVolume', u'같은 라이브러리를 거친다.'),
        ('CM3-110', 'setting_stimulationRange', u'출력단 보정 설정과 맵 데이터에 의존한다.'),
    ]
    for su, fn, why in LIB:
        V.append({'su': su, 'fn': fn, 'kind': u'실측',
                  'note': why + u' 기댓값을 지어내지 않는다. 은수님 실측값을 기록한다.',
                  'cases': [{'in': u'시험 벡터 주입', 'out': u'(실측 기록)'}]})

    return V


def main():
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
    V = build_vectors()
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, 'expected_values.json')
    io.open(out, 'w', encoding='utf-8').write(json.dumps(V, ensure_ascii=False, indent=1))

    n_rep = len([v for v in V if v['kind'] == u'재현'])
    n_meas = len([v for v in V if v['kind'] == u'실측'])
    print(u'시험 벡터 %d 유닛 (재현 %d / 상수 %d / 실측 %d)'
          % (len(V), n_rep, len(V) - n_rep - n_meas, n_meas))
    print()
    for v in V:
        if v['kind'] == u'실측':
            continue
        print(u'=== %s %s [%s] ===' % (v['su'], v['fn'], v['kind']))
        print(u'  %s' % v['note'])
        for c in v['cases']:
            print(u'    %-52s -> %s' % (c['in'], c['out']))
        print()
    print(u'--- 실측에 맡기는 유닛 %d개 ---' % n_meas)
    for v in V:
        if v['kind'] == u'실측':
            print(u'  %s %-46s %s' % (v['su'], v['fn'], v['note'][:52]))
    print()
    print(u'저장: %s' % out)


if __name__ == '__main__':
    main()
