# -*- coding: utf-8 -*-
"""시험 벡터와 예상 출력값 산출 - 대상 함수의 연산을 정수 의미까지 그대로 재현한다.

여기서 나온 값이 SUVC 와 SUVR 의 통과 기준이 되고, 은수님이 실기에서 얻은 값과 대조한다.

**문서 하나만 보고 시험할 수 있어야 한다.** 그래서 유닛마다 시험 준비, 가정 및 제한,
시험 절차, 입력과 예상 출력을 모두 갖춘다. 다른 문서를 찾아보게 하지 않는다.

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
    'pcm_Mold_Preamble': (0xFF555, 'src/1__cfx/internalDevice/driver_PCM.h:34'),
    'pcm_Mold_NopStandby': (0x25555, 'src/1__cfx/internalDevice/driver_PCM.h:35'),
    'pcm_Mold_NopBacktel': (0x35555, 'src/1__cfx/internalDevice/driver_PCM.h:36'),
}
INT24_MAX = 8388607
INT24_MIN = -8388608

# 공통 준비 문구 - 코어별로 심볼 이름이 다르다
PREP_CFX = (u'- 빌드 설정에 TDC_UNIT_TEST 를 정의한다.\n'
            u'- src/1__cfx/test/tdc_unit_test_cfx.c 를 빌드에 넣는다.\n'
            u'- 기존 소스는 수정하지 않는다. 매크로가 없으면 이 파일은 빈 번역 단위가 된다.\n'
            u'- 디버거에서 tdc_ut_run_all() 을 호출한다.\n'
            u'- tdc_ut_done 에 1 이 대입되는 지점에 중단점을 걸고 tdc_ut_result 를 조회한다.')
PREP_CM3 = (u'- 빌드 설정에 TDC_UNIT_TEST 를 정의한다.\n'
            u'- src/2__cm3/Cortex-M3-src/test/tdc_unit_test_cm3.c 를 빌드에 넣는다.\n'
            u'- 기존 소스는 수정하지 않는다. 매크로가 없으면 이 파일은 빈 번역 단위가 된다.\n'
            u'- 디버거에서 tdc_ut_cm3_run_all() 을 호출한다.\n'
            u'- tdc_ut_cm3_done 에 1 이 대입되는 지점에 중단점을 걸고 tdc_ut_cm3_result 를 조회한다.')
PREP_LIB = (u'- 대상 유닛이 호출되는 지점에 디버깅용 중단점을 적용한다.\n'
            u'- 입력 버퍼와 출력 버퍼를 함께 조회할 수 있도록 창을 준비한다.')

LIMIT_HARNESS = (u'- 결과 배열에서 값 -999 는 구분자다. 그 다음 값이 유닛 번호다.\n'
                 u'- 시험은 전역 상태를 바꾼다. 시험 후에는 장치를 재기동한다.\n'
                 u'- 시험 코드는 평시 빌드에서 제외되므로 정상 동작에 영향을 주지 않는다.')
LIMIT_LIB = (u'- 이 유닛의 출력은 DSP 라이브러리 또는 HEAR 함수체인 결과에 의존한다.\n'
             u'- 기댓값을 미리 낼 수 없으므로 실측값을 기록한다.\n'
             u'- 고정소수점 연산 차이를 고려하여 판정한다.')


def c_int(v, bits=32):
    m = 1 << bits
    v &= m - 1
    return v - m if v >= (m >> 1) else v


def sat24(v):
    return INT24_MAX if v > INT24_MAX else (INT24_MIN if v < INT24_MIN else v)


def asr(v, n):
    return v >> n


# ------------------------------------------------------------------ 재현 함수

def data_clearBit(Data, bitPosition, bitLength):
    """commonDataProcessing.c:18 를 그대로 옮긴다."""
    bitSet = 0x01
    n = bitLength
    while n != 1:
        bitSet = c_int((bitSet << 1) | 0x01)
        n -= 1
    return c_int(Data & c_int(~c_int(bitSet << bitPosition)))


def calculationNumFramePerOneChannle(pulsewidth):
    """stimulationParaCal.c:32 를 그대로 옮긴다."""
    one = CONST['FPGA_oneChannelDataTokenTime'][0]
    gap = CONST['FPGA_interphaseGapTokenTime'][0]
    lvl = CONST['FPGA_electrodAndStimulLevelTokenTime'][0]
    duration = pulsewidth + pulsewidth + gap + lvl
    tokenTime = one
    n = 1
    while duration >= tokenTime:
        n += 1
        tokenTime += one
    return n


def calculationTransferableChanneNum(numFramePerChannel, usableElectrodNum):
    """stimulationParaCal.c:60. 활성 경로는 나눗셈 한 줄이며 둘째 인자를 쓰지 않는다."""
    return CONST['df_MaxNumTransferableChannel'][0] // numFramePerChannel


def audio_agc_attack_release(current_q8_16, prev_q8_16):
    """agc.c:119. 직전 이득이 상태로 남으므로 함께 넘겨 받는다."""
    if prev_q8_16 <= current_q8_16:
        a, b, br = (CONST['m_release_coeff_Q8_16'][0],
                    CONST['m_one_minus_release_coeff_Q8_16'][0], u'릴리즈')
    else:
        a, b, br = (CONST['m_attack_coeff_Q8_16'][0],
                    CONST['m_one_minus_attack_coeff_Q8_16'][0], u'어택')
    return sat24(asr(a * current_q8_16 + b * prev_q8_16, 16)), br


def nofm_peak_pick(amplitude):
    """stimulationStrategy.c:140 을 그대로 옮긴다.

    점수 = (진폭 << 5) | (31 - 색인). 같은 진폭이면 색인이 작은 쪽(저주파)이
    큰 점수를 가지므로 살아남는다.
    """
    NE = CONST['df_MaxNumOfElectrode'][0]
    NM = CONST['df_MaxNum_NofM'][0]
    packed = [(amplitude[i] << 5) | (31 - i) for i in range(NE)]
    top = packed[:NM]
    min_packed, min_idx = top[0], 0
    for i in range(1, NM):
        if top[i] < min_packed:
            min_packed, min_idx = top[i], i
    for i in range(NM, NE):
        if packed[i] > min_packed:
            top[min_idx] = packed[i]
            min_packed, min_idx = top[0], 0
            for t in range(1, NM):
                if top[t] < min_packed:
                    min_packed, min_idx = top[t], t
    mask = 0
    for i in range(NM):
        mask |= 1 << (31 - (top[i] & 0x1F))
    return [b for b in range(NE) if mask & (1 << b)]


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


def seq(vals):
    return u' '.join(str(v) for v in vals)


# ------------------------------------------------------------------ 벡터 정의

def build_vectors():
    V = []
    NE = CONST['df_MaxNumOfElectrode'][0]

    # ---------------------------------------------------------- CM3-112
    cases = []
    for data, pos, length in ((0xFFFF, 0, 4), (0xFFFF, 4, 4), (0x00FF, 2, 3),
                              (0x1234, 8, 8), (0xAAAA, 1, 1)):
        cases.append({'in': u'Data=0x%04X, bitPosition=%d, bitLength=%d' % (data, pos, length),
                      'out': u'0x%04X' % (data_clearBit(data, pos, length) & 0xFFFF)})
    V.append({
        'su': 'CM3-112', 'fn': 'data_clearBit', 'kind': u'재현',
        'note': u'순수 비트 연산이라 입력에 대한 출력이 결정된다.',
        'prep': PREP_CM3,
        'limit': LIMIT_HARNESS,
        'proc': (u'- 하네스가 아래 5 가지 입력을 차례로 넣어 원 함수를 호출한다.\n'
                 u'- 구분자 -999 와 유닛 번호 112 다음에 결과 5 개가 이어진다.\n'
                 u'- 결과를 16 진수로 보아 아래 예상 출력과 대조한다.'),
        'cases': cases})

    # ---------------------------------------------------------- CM3-108
    cases = [{'in': u'pulsewidth=%d' % pw, 'out': u'%d' % calculationNumFramePerOneChannle(pw)}
             for pw in (10, 20, 25, 30, 40, 50, 60)]
    V.append({
        'su': 'CM3-108', 'fn': 'calculationNumFramePerOneChannle', 'kind': u'재현',
        'note': u'duration = 2 x pulsewidth + 4 + 10 이 41 의 몇 배인지로 정해진다. '
                u'상수는 FPGA_oneChannelDataTokenTime 41, FPGA_interphaseGapTokenTime 4, '
                u'FPGA_electrodAndStimulLevelTokenTime 10 이다.',
        'prep': PREP_CM3,
        'limit': LIMIT_HARNESS,
        'proc': (u'- 하네스가 아래 7 가지 펄스 폭을 차례로 넣어 원 함수를 호출한다.\n'
                 u'- 구분자 -999 와 유닛 번호 108 다음에 결과 7 개가 이어진다.'),
        'cases': cases})

    # ---------------------------------------------------------- CM3-109
    cases = [{'in': u'numFramePerChannel=%d, usableElectrodNum=%d' % (n, e),
              'out': u'%d' % calculationTransferableChanneNum(n, e)}
             for n, e in ((1, 16), (2, 16), (3, 16), (4, 22), (5, 22), (8, 22))]
    for n in (1, 2, 3, 4, 5, 8):
        cases.append({'in': u'numFramePerChannel=%d, usableElectrodNum=8 (대조)' % n,
                      'out': u'%d (앞의 같은 프레임 수 결과와 같아야 한다)'
                             % calculationTransferableChanneNum(n, 8)})
    V.append({
        'su': 'CM3-109', 'fn': 'calculationTransferableChanneNum', 'kind': u'재현',
        'note': u'활성 경로는 df_MaxNumTransferableChannel(24) / numFramePerChannel 한 줄뿐이고 '
                u'둘째 인자를 쓰지 않는다. 둘째 인자를 바꿔도 결과가 같아야 한다는 것까지 시험한다.',
        'prep': PREP_CM3,
        'limit': LIMIT_HARNESS + u'\n- 뒤 6 개는 둘째 인자만 8 로 바꾼 대조 시험이다.',
        'proc': (u'- 하네스가 앞 6 가지 조합을 넣고, 이어서 둘째 인자만 8 로 바꿔 6 회를 더 호출한다.\n'
                 u'- 구분자 -999 와 유닛 번호 109 다음에 결과 12 개가 이어진다.\n'
                 u'- 앞 6 개와 뒤 6 개가 완전히 같은지 확인한다. 다르면 둘째 인자를 쓰는 경로가 살아 있는 것이다.'),
        'cases': cases})

    # ---------------------------------------------------------- CM3-111
    V.append({
        'su': 'CM3-111', 'fn': 'readDAC_offsetValue', 'kind': u'상수',
        'note': u'상수 DAC_offset_uA 를 그대로 반환한다.',
        'prep': PREP_CM3,
        'limit': LIMIT_HARNESS,
        'proc': (u'- 하네스가 원 함수를 한 번 호출한다.\n'
                 u'- 구분자 -999 와 유닛 번호 111 다음에 결과 1 개가 이어진다.\n'
                 u'- 그 값이 소스의 DAC_offset_uA 정의값과 같은지 확인한다.'),
        'cases': [{'in': u'인자 없음',
                   'out': u'소스의 DAC_offset_uA 정의값과 같아야 한다'}]})

    # ---------------------------------------------------------- CFX-071
    cases = []
    for prev, cur in ((0, 65536), (65536, 0), (65536, 131072), (131072, 65536), (0, 0)):
        out, br = audio_agc_attack_release(cur, prev)
        cases.append({'in': u'직전 이득=%d, 입력 이득=%d' % (prev, cur),
                      'out': u'%d  (%s 경로)' % (out, br)})
    V.append({
        'su': 'CFX-071', 'fn': 'audio_agc_attack_release', 'kind': u'재현',
        'note': u'직전 이득이 내부 상태로 남는다. 직전 <= 입력 이면 릴리즈 경로(계수 655, '
                u'1-계수 64880), 아니면 어택 경로(계수 62259, 1-계수 3276)다. '
                u'결과는 (계수 x 입력 + (1-계수) x 직전) >> 16 이며 24 비트로 포화시킨다.',
        'prep': PREP_CFX + u'\n- 하네스가 시험마다 m_prev_agc_10db_gain_Q8_16 을 지정한 값으로 둔다.',
        'limit': LIMIT_HARNESS + u'\n- 직전 이득이 상태로 남으므로 시험 순서를 바꾸면 결과가 달라진다.',
        'proc': (u'- 하네스가 아래 5 가지 조합에 대해 직전 이득을 먼저 대입한 뒤 원 함수를 호출한다.\n'
                 u'- 구분자 -999 와 유닛 번호 71 다음에 결과 5 개가 이어진다.'),
        'cases': cases})

    # ---------------------------------------------------------- CFX-086
    V.append({
        'su': 'CFX-086', 'fn': 'stimulationStrategy_NofM_Init_Buffers', 'kind': u'재현',
        'note': u'선택 버퍼와 교차 버퍼 각 16 칸, 임시 버퍼 32 칸을 모두 -1 로 채운다.',
        'prep': PREP_CFX + u'\n- 하네스가 호출 전에 세 버퍼를 모두 777 로 채운다.',
        'limit': LIMIT_HARNESS,
        'proc': (u'- 하네스가 세 버퍼를 777 로 채운 뒤 원 함수를 호출한다.\n'
                 u'- 구분자 -999 와 유닛 번호 86 다음에 결과 64 개가 이어진다.\n'
                 u'- 순서는 선택 버퍼 16, 교차 버퍼 16, 임시 버퍼 32 다.'),
        'cases': [
            {'in': u'g_NofM_adjacent_BandIndex[0..15] 을 777 로 채운 뒤 호출',
             'out': u'16 개 모두 -1'},
            {'in': u'g_NofM_notAdjacent_BandIndex[0..15] 을 777 로 채운 뒤 호출',
             'out': u'16 개 모두 -1'},
            {'in': u'addr_stimulationTempBuff[0..31] 을 777 로 채운 뒤 호출',
             'out': u'32 개 모두 -1'},
        ]})

    # ---------------------------------------------------------- CFX-087
    amp_a = [i * 10 for i in range(NE)]
    amp_b = [100] * NE
    amp_c = [(200 if i % 2 == 0 else 5) for i in range(NE)]
    amp_d = [0] * NE
    amp_d[20] = 1000
    cases = [
        {'in': u'벡터 가 : 진폭[i] = i x 10  (0, 10, 20, ... 310)',
         'out': seq(nofm_peak_pick(amp_a))},
        {'in': u'벡터 나 : 진폭 32 개가 모두 100 (동점)',
         'out': seq(nofm_peak_pick(amp_b)) + u'   <- 같은 크기면 저주파가 살아남는다'},
        {'in': u'벡터 다 : 짝수 색인 200, 홀수 색인 5',
         'out': seq(nofm_peak_pick(amp_c))},
        {'in': u'벡터 라 : 색인 20 만 1000, 나머지 0',
         'out': seq(nofm_peak_pick(amp_d))},
    ]
    V.append({
        'su': 'CFX-087', 'fn': 'stimulationStrategy_NofM_Peak_Pick', 'kind': u'재현',
        'note': u'점수 = (진폭 << 5) | (31 - 색인). 같은 진폭이면 색인이 작은 쪽이 큰 점수를 '
                u'가지므로 저주파가 보호된다. 32 개 중 16 개를 골라 색인 오름차순으로 낸다. '
                u'벡터 나가 저주파 보호 규칙을 확인하는 시험이다.',
        'prep': PREP_CFX + u'\n- 하네스가 벡터마다 g_pcm_amplitude_level[0..31] 을 채운다.',
        'limit': LIMIT_HARNESS +
                 u'\n- 벡터 나에서 뒤쪽 색인이 나오면 점수 병합의 부호 규칙이 뒤집힌 것이다.',
        'proc': (u'- 하네스가 아래 네 벡터를 차례로 진폭 배열에 넣고 원 함수를 호출한다.\n'
                 u'- 구분자 -999 와 유닛 번호 87 다음에 결과 64 개(벡터당 16 개)가 이어진다.\n'
                 u'- g_NofM_adjacent_BandIndex[0..15] 를 벡터마다 조회한다.'),
        'cases': cases})

    # ---------------------------------------------------------- CFX-088
    adj_a = list(range(16))
    adj_b = nofm_peak_pick(amp_c)
    cases = [
        {'in': u'벡터 가 : 선택 결과를 0 1 2 3 ... 15 로 둔 뒤 호출',
         'out': seq(nofm_interleaving(adj_a))},
        {'in': u'벡터 나 : 앞 시험의 «짝수 색인 200» 선택 결과 (%s) 로 둔 뒤 호출' % seq(adj_b),
         'out': seq(nofm_interleaving(adj_b))},
    ]
    V.append({
        'su': 'CFX-088', 'fn': 'stimulationStrategy_NofM_Interleaving', 'kind': u'재현',
        'note': u'16 개를 앞 8 개와 뒤 8 개로 나눠 한 칸씩 번갈아 배치한다. '
                u'출력[i x 2] = 입력[i], 출력[i x 2 + 1] = 입력[i + 8] 이다. '
                u'인접 전극이 잇달아 자극되지 않게 하는 처리다.',
        'prep': PREP_CFX + u'\n- 하네스가 벡터마다 g_NofM_adjacent_BandIndex[0..15] 를 채운다.',
        'limit': LIMIT_HARNESS,
        'proc': (u'- 하네스가 아래 두 벡터를 차례로 선택 버퍼에 넣고 원 함수를 호출한다.\n'
                 u'- 구분자 -999 와 유닛 번호 88 다음에 결과 32 개(벡터당 16 개)가 이어진다.\n'
                 u'- g_NofM_notAdjacent_BandIndex[0..15] 를 벡터마다 조회한다.'),
        'cases': cases})

    # ---------------------------------------------------------- PCM 비트스트림
    # 24 칸을 상수 몰드로 채우는 단순 로직이라 전부 재현된다.
    NCH = CONST['df_MaxNumTransferableChannel'][0]
    STB = CONST['pcm_Mold_NopStandby'][0]
    BKT = CONST['pcm_Mold_NopBacktel'][0]
    PRE = CONST['pcm_Mold_Preamble'][0]

    PCM_PREP = (PREP_CFX + u'\n- PCM 기록 버퍼(HEAR_ADDR_FIFO_PCM_WRITING)를 조회할 수 있게 '
                           u'창을 준비한다.')
    PCM_LIMIT = (LIMIT_HARNESS +
                 u'\n- 버퍼 기록 방향이 감소 방향이다. 시작 주소에서 아래로 24 칸을 본다.')

    for su, fn, name, mold, extra in (
        ('CFX-095', 'fn_PcmBitStream_Mode_NopStandby', u'대기', STB, None),
        ('CFX-096', 'fn_PcmBitStream_Mode_NopBacktel', u'연결 검사용', BKT, None),
        ('CFX-097', 'fn_PcmBitStream_Mode_FillZero', u'영 채움', 0, None),
    ):
        V.append({
            'su': su, 'fn': fn, 'kind': u'재현',
            'note': u'PCM 기록 버퍼 시작 주소부터 감소 방향으로 %d 칸을 같은 값으로 채운다. '
                    u'채우는 값은 0x%05X 다.' % (NCH, mold),
            'prep': PCM_PREP,
            'limit': PCM_LIMIT,
            'proc': (u'- 버퍼를 미리 다른 값(예: 0x12345)으로 채워 둔다.\n'
                     u'- 원 함수를 한 번 호출한다.\n'
                     u'- 기록 버퍼 시작 주소부터 감소 방향으로 %d 칸을 조회한다.' % NCH),
            'cases': [{'in': u'버퍼를 0x12345 로 채운 뒤 호출',
                       'out': u'%d 칸 모두 0x%05X' % (NCH, mold)}]})

    V.append({
        'su': 'CFX-092', 'fn': 'fn_PcmBitStream_Mode_Preamble', 'kind': u'재현',
        'note': u'상태 변수 g_pcmPreambleState 에 따라 두 갈래로 동작하고 호출마다 상태가 바뀐다. '
                u'영 채움 단계에서는 %d 칸을 0 으로 채우고 상태를 토글로 옮긴다. '
                u'토글 단계에서는 %d 칸을 0xFFFFF 로 채우고 마지막 한 칸에 프리앰블 몰드 0x%05X 를 '
                u'넣은 뒤, PCM 모드를 대기로 바꾸고 상태를 영 채움으로 되돌린다.'
                % (NCH, NCH - 1, PRE),
        'prep': PCM_PREP + u'\n- 공유 메모리의 PCM 모드 항목도 함께 조회할 수 있게 한다.',
        'limit': PCM_LIMIT + u'\n- 호출마다 상태가 바뀌므로 두 번 연속 호출해 두 갈래를 모두 본다.',
        'proc': (u'- PCM 상태 초기화 유닛을 먼저 호출해 상태를 영 채움으로 둔다.\n'
                 u'- 원 함수를 한 번 호출하고 버퍼 %d 칸을 조회한다. (1 회차)\n'
                 u'- 원 함수를 한 번 더 호출하고 버퍼 %d 칸과 PCM 모드를 조회한다. (2 회차)'
                 % (NCH, NCH)),
        'cases': [
            {'in': u'1 회차 : 상태가 영 채움인 상태에서 호출',
             'out': u'%d 칸 모두 0. 상태가 토글로 바뀐다.' % NCH},
            {'in': u'2 회차 : 이어서 한 번 더 호출',
             'out': u'앞 %d 칸은 0xFFFFF, 마지막 1 칸은 0x%05X. '
                    u'PCM 모드가 대기로 바뀌고 상태가 영 채움으로 되돌아간다.' % (NCH - 1, PRE)},
        ]})

    # ---------------------------------------------------------- 실측 유닛
    LIB = [
        ('CFX-075', 'read_FFT_PassBin_index',
         u'파일 시스템 영역에 저장된 통과 대역 색인을 읽어 온다. 저장된 내용에 의존한다.', PREP_LIB,
         (u'- 맵의 대역 수와 저장 영역의 대역 수가 같은지 먼저 확인한다.\n'
          u'- 호출 후 g_pass_bin_index 배열을 조회해 저장 영역의 값과 같은지 기록한다.\n'
          u'- 대역 수가 다를 때는 배열이 바뀌지 않아야 한다. 그 경우도 기록한다.'), None),
        ('CFX-076', 'read_C_T_Level_fromCM3',
         u'CM3 가 공유 메모리에 올린 C 레벨과 T 레벨을 읽어 온다. CM3 계산 결과에 의존한다.',
         PREP_LIB,
         (u'- 공유 메모리의 C/T 레벨과 호출 후 CFX 쪽 배열을 함께 조회해 같은지 기록한다.'), None),
        ('CFX-077', 'calculate_newStimulation_maxLevel',
         u'자극 볼륨과 C/T 레벨로 최대 레벨을 구한다. 맵 데이터에 의존한다.', PREP_LIB,
         (u'- 자극 볼륨을 1, 5, 10 으로 바꿔 가며 호출하고 산출된 최대 레벨을 기록한다.'), None),
        ('CFX-082', 'prepare_pcmStimulationPacketHeader',
         u'맵의 자극 모드와 전극 배정에 따라 헤더가 정해진다. 맵 데이터에 의존한다.', PCM_PREP,
         (u'- 호출 후 준비된 패킷 헤더 값을 조회해 기록한다.\n'
          u'- 맵의 자극 모드를 바꿔 가며 헤더가 달라지는지 기록한다.'), None),
        ('CFX-083', 'stimulationStrategy',
         u'맵의 자극 기법 설정에 따라 CIS 또는 NofM 으로 갈린다.', PCM_PREP,
         (u'- 맵의 자극 기법을 CIS 로 두고 호출해 어느 하위 유닛이 불리는지 확인한다.\n'
          u'- NofM 으로 바꿔 다시 호출해 확인한다.\n'
          u'- 각각의 PCM 출력 버퍼를 조회해 기록한다.'), None),
        ('CFX-084', 'stimulationStrategy_CIS',
         u'대역 대표값과 매핑 결과에 의존한다. 앞단 라이브러리 출력이 입력이 된다.', PCM_PREP,
         (u'- 호출 후 PCM 출력 버퍼에서 자극 헤더, 전극 번호, 자극 레벨을 조회해 기록한다.\n'
          u'- 맵의 CIS 대역 순서와 실제 출력 순서가 맞는지 기록한다.'), None),
        ('CFX-085', 'stimulationStrategy_NofM',
         u'피크 선택과 인터리빙 결과를 받아 패킷을 만든다. 앞단 결과에 의존한다.', PCM_PREP,
         (u'- 호출 후 PCM 출력 버퍼를 조회해 교차 배치 순서대로 자극이 실렸는지 기록한다.'), None),
        ('CFX-089', 'stimulationStrategy_NofM_Packet',
         u'선택된 대역의 자극 레벨을 패킷으로 만든다. 매핑 결과에 의존한다.', PCM_PREP,
         (u'- 호출 후 만들어진 패킷 값을 조회해 전극 번호와 자극 레벨이 자리에 맞는지 기록한다.'), None),
        ('CFX-090', 'stimulationStrategy_NofM_PCM_Write',
         u'만들어진 패킷을 PCM 버퍼에 기록한다. 앞단 결과에 의존한다.', PCM_PREP,
         (u'- 호출 전후의 PCM 기록 버퍼를 조회해 기록 위치와 내용을 기록한다.'), None),
        ('CFX-093', 'fn_PcmBitStream_Mode_LiveStimulation',
         u'실시간 자극 데이터를 비트스트림으로 만든다. 자극 기법 결과에 의존한다.', PCM_PREP,
         (u'- 호출 후 PCM 기록 버퍼 24 칸을 조회해 자극용 헤더, 전극 번호, 자극 레벨을 기록한다.\n'
          u'- 채널당 프레임 수를 1 과 2 로 바꿔 가며 각각 기록한다.'), None),
        ('CFX-094', 'fn_PcmBitStream_Mode_SepcificCommand',
         u'공유 메모리의 특정 명령 버퍼 내용을 그대로 내보낸다. 버퍼 내용에 의존한다.', PCM_PREP,
         (u'- 특정 명령 버퍼에 알려진 값을 넣고 호출한다.\n'
          u'- PCM 기록 버퍼 24 칸이 그 값과 같은지 조회해 기록한다.'), None),
        ('CFX-101', 'frame_1_perChannel',
         u'채널당 1 프레임 구성으로 검사 프레임을 만든다. 검사 회로 설정에 의존한다.', PCM_PREP,
         (u'- 채널당 프레임 수를 1 로 두고 호출한다.\n'
          u'- PCM 기록 버퍼를 조회해 검사 프레임 구성과 자극 자리 배치를 기록한다.'), None),
        ('CFX-081', 'calculate_x_power_p',
         u'cfx_pwr_to_dB_asm 과 cfx_dB_to_pwr_asm 어셈블리 라이브러리를 거친다.',
         PREP_CFX,
         (u'- 하네스가 입력 0, 1, 100, 1000, 10000, 100000 을 차례로 넣어 호출한다.\n'
          u'- 구분자 -999 와 유닛 번호 81 다음에 «입력, 출력» 짝으로 12 개가 이어진다.\n'
          u'- 각 짝의 출력을 그대로 기록한다.'),
         [{'in': u'x = 0', 'out': u'0  (x < 1 이면 0 을 반환해야 한다. 이 항만 확인 가능)'},
          {'in': u'x = 1', 'out': u'(실측 기록)'},
          {'in': u'x = 100', 'out': u'(실측 기록)'},
          {'in': u'x = 1000', 'out': u'(실측 기록)'},
          {'in': u'x = 10000', 'out': u'(실측 기록)'},
          {'in': u'x = 100000', 'out': u'(실측 기록)'}]),
        ('CFX-069', 'audio_agc', u'전처리 함수체인이 만든 최대값 버퍼에 의존한다.', PREP_LIB,
         (u'- 신호를 입력한 상태에서 이 유닛의 진입과 복귀에 중단점을 적용한다.\n'
          u'- 입력 최대값 버퍼와 출력 이득을 함께 조회해 기록한다.'), None),
        ('CFX-070', 'calculate_agc_gain',
         u'입력이 전처리 체인 출력이며 dB 변환 라이브러리를 거친다.', PREP_LIB,
         (u'- 중단점에서 입력값과 반환 이득을 조회해 기록한다.\n'
          u'- 잡음 구간, 증폭 구간, 감쇠 구간 각각에서 한 번씩 기록한다.'), None),
        ('CFX-072', 'apply_agc_gain', u'믹서 출력 버퍼 전체에 적용된다.', PREP_LIB,
         (u'- 적용 전후의 믹서 출력 버퍼 16 표본을 조회해 기록한다.'), None),
        ('CFX-073', 'update_FFT_inputData', u'HEAR 함수체인 출력 버퍼에 의존한다.', PREP_LIB,
         (u'- 호출 전후의 주파수 분석 입력 버퍼를 조회해 자리 이동이 일어나는지 기록한다.'), None),
        ('CFX-074', 'find_freq_rep_value', u'주파수 변환 체인(vMag) 출력에 의존한다.', PREP_LIB,
         (u'- 변환 체인 출력과 산출된 대역 대표값을 함께 조회해 기록한다.'), None),
        ('CFX-078', 'calculate_logaritmMapping_coeff', u'dB 변환 라이브러리를 거친다.', PREP_LIB,
         (u'- 호출 후 로그 매핑 계수 A 와 B 를 조회해 기록한다.'), None),
        ('CFX-079', 'calculate_logaritmMapping_coeff_with_audioVolume',
         u'같은 dB 변환 라이브러리를 거친다.', PREP_LIB,
         (u'- 오디오 볼륨을 바꿔 가며 계수 A 와 B 를 조회해 기록한다.'), None),
        ('CM3-110', 'setting_stimulationRange',
         u'출력단 보정 설정과 맵 데이터에 의존한다.', PREP_LIB,
         (u'- 호출 후 255 단계로 환산된 T 레벨과 C 레벨 배열을 조회해 기록한다.'), None),
    ]
    for su, fn, why, prep, proc, cases in LIB:
        V.append({
            'su': su, 'fn': fn, 'kind': u'실측',
            'note': why + u' 기댓값을 미리 낼 수 없으므로 지어내지 않는다.',
            'prep': prep,
            'limit': LIMIT_LIB,
            'proc': proc,
            'cases': cases or [{'in': u'시험 벡터 주입', 'out': u'(실측 기록)'}]})

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
    print(u'케이스 합계 %d' % sum(len(v['cases']) for v in V))
    print()
    for v in V:
        if v['kind'] == u'실측':
            continue
        print(u'=== %s %s [%s] · 케이스 %d ===' % (v['su'], v['fn'], v['kind'], len(v['cases'])))
        for c in v['cases']:
            print(u'    %-58s -> %s' % (c['in'][:58], c['out']))
    print()
    print(u'--- 실측에 맡기는 유닛 %d개 ---' % n_meas)
    for v in V:
        if v['kind'] == u'실측':
            print(u'  %s %-46s 케이스 %d' % (v['su'], v['fn'][:46], len(v['cases'])))
    print()
    print(u'저장: %s' % out)


if __name__ == '__main__':
    main()
