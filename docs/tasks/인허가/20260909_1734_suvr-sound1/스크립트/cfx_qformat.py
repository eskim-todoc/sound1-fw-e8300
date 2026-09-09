# -*- coding: utf-8 -*-
"""CFX Q 포맷 연산 재현.

«기댓값을 미리 낼 수 없다» 는 판단이 틀렸다. SDK 헤더에 dB 변환이 명세되어 있고
Q 포맷 연산은 정수 산술이므로 그대로 재현된다.

  SDK: include/cfx/sk5_cfx_dsp_data_conversions.h
    cfx_pwr_to_dB(pwr) = 10*log10(abs(pwr))     PWR_TO_DB_FACTOR = [10*log10(2)] * 2^20
    cfx_dB_to_pwr(dB)  = 10^(dB/10)             DB_TO_PWR_FACTOR = 1/[10*log10(2)] * 2^23

  SDK: include/cfx/sk5_cfx_dsp_types.h
    frac48_t : 48 비트 분수, 범위 [-1, 1 - 2^-47]
    m8p16_t  : 24 비트 = 정수 8 + 소수 16.  값 / 65536 = 실제 dB

하드웨어 log2 / exp2 유닛의 마지막 자리 오차만 남는다. 1.5세대 SUVR 도 이 계통에는
«고정소수점 연산 고려» 를 통과 기준에 달았다. 같은 기준으로 허용 오차를 함께 낸다.
"""
import math

INT24_MAX = 8388607
INT24_MIN = -8388608

# ------------------------------------------------------------------ 소스 상수
# agc.c 의 실제 값이다. 근거 위치를 함께 적는다.
ATTEN_YBIAS_Q8_16 = [-1661530, -2008762, -2355995, -2703227, -3050459,
                     -3397692, -3744924, -4092157, -4439389, -4786621]   # agc.c:8
AMP_YBIAS_Q8_16 = [0, 108138, 216277, 324416, 432555,
                   540694, 648833, 756971, 865110, 973249]               # agc.c:22
NOISE_YBIAS_Q12_12 = [481268, 488027, 494785, 501544, 508303,
                      515061, 521820, 528579, 535337, 542096]            # agc.c:38
ATTEN_SLOPE_Q8_16 = [47999, 43193, 38387, 33581, 28774,
                     23968, 19162, 14356, 9550, 4744]                    # agc.c:58
ROTATION_POINT_DB_Q8_16 = -6209352      # agc.c:52   -94.7472 dB
NOISE_GATE_DB_Q8_16 = -7700296          # agc.c:55  -117.4972 dB
NOISE_SLOPE_Q8_16 = 131072              # agc.c:70   2.0
MATHLIB_GAIN_DB_Q8_16 = 1972830         # agc.c:81   30.103 dB
MATHLIB_GAIN_LINEAR_Q12_12 = 4194304    # agc.c:82   1024.0
ATTACK_Q8_16 = 62259                    # agc.c:74   0.95
RELEASE_Q8_16 = 655                     # agc.c:75   0.01
ONE_MINUS_ATTACK_Q8_16 = 3276           # agc.c:76   0.05
ONE_MINUS_RELEASE_Q8_16 = 64880         # agc.c:77   0.99

NORMALIZE_SHIFT = 5                     # definitionsForAlgorithm.h:93
COEFF_P_Q1_23 = 1258291                 # nonlinearMapping.c:10   p = 0.15
NORM_FACTOR_POW_MINUS_P_Q8_16 = 5163793  # nonlinearMapping.c:24


def sat24(v):
    return INT24_MAX if v > INT24_MAX else (INT24_MIN if v < INT24_MIN else v)


# ------------------------------------------------------------------ dB 변환
# frac48 은 [-1, 1) 분수다. to_lfix(int) 는 24 비트 정수를 그 분수로 본다.
# 즉 frac48 실수값 = int_val / 2^23.

def pwr_to_dB_m8p16(raw, scale_bits=47):
    """cfx_pwr_to_dB. 입력은 frac48 로 재해석된 정수, 출력은 m8p16.

    cfx_as_frac48(long) 은 «비트 재해석» 이다. SDK 주석이 못박는다.
        frac48_t c = 1.0;  cfx_as_long(c) == 0x7FFFFFFFFFFF
    즉 frac48 실수값 = raw / 2^47 이다.  (frac24 는 2^23)

    실수 의미: 10 * log10(raw / 2^scale_bits)
    """
    if raw <= 0:
        return None                      # log 정의역 밖. 코드가 미리 걸러야 한다
    x = float(raw) / float(1 << scale_bits)
    db = 10.0 * math.log10(x)
    return int(round(db * 65536))


def dB_to_pwr_q1_23(m8p16_val):
    """cfx_dB_to_pwr 뒤에 이어지는 >> 24 까지 포함. 출력은 Q1.23.

    실수 의미: 10^(dB/10) 을 frac48 로 얻은 뒤 24 비트 내려 Q1.23 으로 만든다.
    """
    db = float(m8p16_val) / 65536.0
    v = math.pow(10.0, db / 10.0)
    if v >= 1.0:
        v = 1.0 - 1.0 / (1 << 23)        # frac48 은 1 미만이다
    return int(v * (1 << 23))


# ------------------------------------------------------------------ 유닛 재현

def calculate_x_power_p(x, coeff_p_q1_23=COEFF_P_Q1_23,
                        norm_q8_16=NORM_FACTOR_POW_MINUS_P_Q8_16):
    """nonlinearMapping.c:606 을 그대로 옮긴다.

    dB 도메인에서 x^p 를 구한다.
      normalized_x = x << 5
      dB = 10*log10(normalized_x / 2^23)
      p*dB = (p_Q1_23 * dB_m8p16) >> 23
      x^p = 10^(p*dB/10) 을 Q1.23 으로
      결과 = (그 값 * norm_Q8_16) >> 23   -> Q8.16
    """
    if x < 1:
        return 0
    normalized_x = x << NORMALIZE_SHIFT
    db = pwr_to_dB_m8p16(normalized_x)
    acc = (coeff_p_q1_23 * db) >> 23
    acc = sat24(acc)
    p_db = acc
    pw = dB_to_pwr_q1_23(p_db)
    acc = (pw * norm_q8_16) >> 23
    return sat24(acc)


def audio_agc_attack_release(current_q8_16, prev_q8_16):
    """agc.c:119."""
    if prev_q8_16 <= current_q8_16:
        a, b, br = RELEASE_Q8_16, ONE_MINUS_RELEASE_Q8_16, u'릴리즈'
    else:
        a, b, br = ATTACK_Q8_16, ONE_MINUS_ATTACK_Q8_16, u'어택'
    return sat24((a * current_q8_16 + b * prev_q8_16) >> 16), br


def calculate_agc_gain(agc_input, audio_volume=0, prev_gain_q8_16=0):
    """agc.c:156 을 그대로 옮긴다. 3 구간 판정과 시정수 적용을 포함한다.

    반환: (선형 이득 Q12.12, 구간 이름, 입력 dB m8p16)
    """
    if agc_input < 1:
        agc_input = 1
    normalized = agc_input << NORMALIZE_SHIFT
    in_db = pwr_to_dB_m8p16(normalized)
    in_db_q12_12 = in_db >> 4

    if in_db < NOISE_GATE_DB_Q8_16:
        region = u'잡음'
        acc = (NOISE_SLOPE_Q8_16 * in_db) >> 20
        acc = acc + NOISE_YBIAS_Q12_12[audio_volume]
        acc = acc - in_db_q12_12
        acc = acc << 4
    elif ROTATION_POINT_DB_Q8_16 < in_db:
        region = u'감쇠'
        acc = (ATTEN_SLOPE_Q8_16[audio_volume] * in_db) >> 16
        acc = acc + ATTEN_YBIAS_Q8_16[audio_volume]
        acc = acc - in_db
    else:
        region = u'증폭'
        acc = in_db + AMP_YBIAS_Q8_16[audio_volume]
        acc = acc - in_db

    gain_db = sat24(acc)
    ar_db, _ = audio_agc_attack_release(gain_db, prev_gain_q8_16)
    norm_db = ar_db - MATHLIB_GAIN_DB_Q8_16
    lin_q1_23 = dB_to_pwr_q1_23(norm_db)
    acc = (lin_q1_23 * MATHLIB_GAIN_LINEAR_Q12_12) >> 23
    return sat24(acc), region, in_db


# ------------------------------------------------------------------ HEAR 후단
# 이 유닛들의 «입력» 은 HEAR 가 채운 버퍼다. 그 버퍼에 시험 벡터를 주입하면
# HEAR 내부를 재현하지 않아도 출력이 결정된다.
#
# HEAR Configurable Accelerator Reference 로 확인한 것:
#   vAbsolute  (p.279) 실수 벡터 각 원소의 절대값. BFP 없음
#   MaxElement (p.333) 실수 벡터의 최대값
#   vMag       (p.243) 복소 크기. **제곱근을 테이블 조회로 근사한다**
#                      출력 형식 24비트(2.22) 또는 48비트(2.46)
# 앞의 둘은 정확하고, vMag 만 근사라 참값과 오차가 있다.

DATA_BUF_LEN = 16                       # definitionsForAlgorithm.h:56
HALF_FFT_SIZE = 256                     # definitionsForAlgorithm.h:98  (512/2)
RIGHT_SHIFT_MAX_MAG = 4                 # definitionsForAlgorithm.h:111 (창 함수 DFT)


def apply_agc_gain(mix_buffer, agc_gain_linear_q12_12):
    """agc.c:307. (이득 * 표본) >> 12 뒤 24 비트 포화."""
    out = []
    for v in mix_buffer:
        g = (agc_gain_linear_q12_12 * v) >> 12
        out.append(sat24(g))
    return out


def update_fft_input(agc_output):
    """FrequencyAnalysis.c:32. AGC 출력 버퍼를 «역순» 으로 FIFO 에 밀어넣는다."""
    return list(reversed(agc_output))


def find_freq_rep_value(vmag, pass_bin_index, num_electrode=32):
    """FrequencyAnalysis.c:45.

    대역 색인이 0 이상인 자리만 보고 그 대역의 최대 크기를 남긴다.
    이어서 정해진 비트수만큼 내려 축소본을 만든다.
    반환: (대역 대표값, 축소본)
    """
    rep = [0] * num_electrode
    for i in range(min(len(vmag), len(pass_bin_index))):
        idx = pass_bin_index[i]
        if idx >= 0:
            if vmag[i] > rep[idx]:
                rep[idx] = vmag[i]
    scaled = [v >> RIGHT_SHIFT_MAX_MAG for v in rep]
    return rep, scaled


def hear_vabsolute(vec):
    """HEAR vAbsolute (p.279). 24 비트 포화를 함께 본다."""
    return [sat24(abs(v)) for v in vec]


def hear_max_element(vec):
    """HEAR MaxElement (p.333)."""
    return max(vec)


if __name__ == '__main__':
    import io, sys
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

    print(u'=== calculate_x_power_p (p = 0.15) ===')
    for x in (0, 1, 100, 1000, 10000, 100000):
        r = calculate_x_power_p(x)
        print(u'  x=%-8d -> %-10d  (Q8.16, 실수 %.4f)' % (x, r, r / 65536.0))

    print()
    print(u'=== calculate_agc_gain (볼륨 0, 직전 이득 0) ===')
    for v in (1, 100, 1000, 10000, 100000, 1000000, 8000000):
        g, reg, db = calculate_agc_gain(v)
        print(u'  입력=%-9d 입력dB=%-9.3f %s구간 -> 이득 %-9d (선형 %.4f)'
              % (v, db / 65536.0, reg, g, g / 4096.0))

    print()
    print(u'=== 구간 경계 확인 ===')
    print(u'  잡음 게이트 %.4f dB / 회전점 %.4f dB'
          % (NOISE_GATE_DB_Q8_16 / 65536.0, ROTATION_POINT_DB_Q8_16 / 65536.0))
