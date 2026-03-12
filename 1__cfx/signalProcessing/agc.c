/**
 * @file agc.c
 */

#include <agc.h>
#include <audioMixer.h>
#include <main.h>

// 감쇠 영역 Gain 10dB 직선의 y 절편 값 (Q-format 형식)
int chess_storage(XMEM) m_attenuation_region_ybias_db_Q8_16[10] = {
    -1661530,  // -23.4197
    -2008762,  // -28.1882
    -2355995,  // -32.9567
    -2703227,  // -37.7252
    -3050459,  // -42.4938
    -3397692,  // -47.2623
    -3744924,  // -52.0308
    -4092157,  // -56.7993
    -4439389,  // -61.5678
    -4786621,  // -66.3363
};

// 증폭 영역 Gain 10dB 직선의 y 절편 값 (Q-format 형식)
int chess_storage(XMEM) m_amplify_region_ybias_db_Q8_16[10] = {
    0,       // 10*log10(1)=0
    108138,  // 10*log10(1.4622)=1.6501
    216277,  // 10*log10(2.138)=3.3001
    324416,  // 10*log10(3.1262)=4.9502
    432555,  // 10*log10(4.5712)=6.6003
    540694,  // 10*log10(6.684)=8.2503
    648833,  // 10*log10(9.7733)=9.9004
    756971,  // 10*log10(14.2905)=11.5505
    865110,  // 10*log10(20.8956)=13.2005
    973249,  // 10*log10(30.5535)=14.8506
};

// noise 영역 Gain 10dB 직선의 y 절편 값 (Q-format 형식)
int chess_storage(XMEM) m_noise_region_ybias_db_Q12_12[10] = {
    481268,  // 117.4972
    488027,  // 119.1473
    494785,  // 120.7973
    501544,  // 122.4474
    508303,  // 124.0975
    515061,  // 125.7475
    521820,  // 127.3976
    528579,  // 129.0477
    535337,  // 130.6977
    542096,  // 132.3478
};

// Rotation Point 입력 10dB 값 (Q-format 형식)
int chess_storage(XMEM) m_rotation_point_db_Q8_16 = -6209352;  // -94.7472 <=== 읍압레벨 : 75dB SPL

// Noise Gate 입력 10dB 값 (Q-format 형식)
int chess_storage(XMEM) m_noise_gate_db_Q8_16 = -7700296;  // -117.4972 <=== 읍압레벨 : 29.5dB SPL

// 감쇠 영역 gain 10dB 직선 기울기 값 (Q-format 형식)
int chess_storage(XMEM) m_attenuation_region_slope_Q8_16[10] = {
    47999,  // 0.73241
    43193,  // 0.65908
    38387,  // 0.58574
    33581,  // 0.51241
    28774,  // 0.43907
    23968,  // 0.36573
    19162,  // 0.2924
    14356,  // 0.21906
    9550,   // 0.14572
    4744,   // 0.072388
};

int _XMEM m_noise_region_slope_Q8_16   = 131072;  // 2.0    // 잡음 영역 gain 10dB 직선 기울기
int _XMEM m_amplify_region_slope_Q8_16 = 65536;   // 1.0    // 증폭 영역 gain 10dB 직선 기울기

int _XMEM m_attack_coeff_Q8_16            = 62259;  // 0.95    // attack 계수
int _XMEM m_release_coeff_Q8_16           = 655;    // 0.01    // Release 계수
int _XMEM m_one_minus_attack_coeff_Q8_16  = 3276;   // 0.05    // (1-attack 계수
int _XMEM m_one_minus_release_coeff_Q8_16 = 64880;  // 0.99    // (1-Release) 계수

int _XMEM m_mathlib_gain_db_Q8_16      = 1972830;  // 10 * LOG10(1024) = 30.103 dB    // math library를 사용하기 위한 normalize 계수 10 dB
int _XMEM m_mathlib_gain_linear_Q12_12 = 4194304;  // 1024.0                          // math library를 사용하기 위한 normalize gain (linear 표현)

int _XMEM m_prev_agc_10db_gain_Q8_16 = 0;  // gain 1 (linear) ==> 0 (10dB gain)
int _XMEM m_audio_volume             = 0;

int _XMEM m_agc_output_buffer[df_inputADC_DataBuffLength] = {
    0,
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void audio_agc(void)
{
    int input_audio_mix_max_value;
    int linear_agc_gain;

    input_audio_mix_max_value = ((int _XMEM *) HEAR_ADDR_AUDIO_MIX_ABS_MAX_VALUE)[0];

#if 0
    if (Addr_SharedMem->CM3_tempValue1 == 0)
    {
        Addr_SharedMem->maxAudioInput = input_audio_mix_max_value;
    }
#else
    Addr_SharedMem->maxAudioInput = input_audio_mix_max_value;
#endif

    /* 절대값 값을 구한 오디오 입력 값 16개 중 최대 값을 로그 계산한 후에 이 로그 값이 AGC 입력 범위 중 어디에 속하는지 검사한다.
     * (AGC 입력 범위: Noise gate, Rotation point, AGC max input) */

    /* 오디오 입력 값 중 최대값 기준으로 적용해야할 리니어 게인을 구함 */
    linear_agc_gain = calculate_agc_gain(input_audio_mix_max_value);

    /* 리니어 게인 적용 */
    apply_agc_gain(linear_agc_gain);
}

// AGC 게인을 Attack/Release Time을 적용하여 변화시킨다.
int audio_agc_attack_release(int current_agc_10db_gain_Q8_16)
{
    long long_acc;

    // prev gain <= target gain : release
    // attackRelease_gain = (releaseCoeff * target_agc_gain) + ((1 - releaseCoeff) * prev_attackRelase_gain)
    if (m_prev_agc_10db_gain_Q8_16 <= current_agc_10db_gain_Q8_16)
    {
        long_acc = ((long) m_release_coeff_Q8_16) * current_agc_10db_gain_Q8_16;                        // Q16.32
        long_acc = long_acc + (((long) m_one_minus_release_coeff_Q8_16) * m_prev_agc_10db_gain_Q8_16);  // Q16.32
        long_acc = long_acc >> 16;                                                                      // Q16.16
        // m_prev_agc_10db_gain_Q8_16 = (int) (long_acc >> 16);                                                              // Q8.16
    }
    // prev gain > target gain : attack
    // attackRelease_gain = (attackCoeff * target_agc_gain) + ((1 - attackCoeff) * prev_attackRelase_gain)
    else
    {
        long_acc = ((long) m_attack_coeff_Q8_16) * current_agc_10db_gain_Q8_16;                        // Q16.32
        long_acc = long_acc + (((long) m_one_minus_attack_coeff_Q8_16) * m_prev_agc_10db_gain_Q8_16);  // Q16.32
        long_acc = long_acc >> 16;                                                                     // Q16.16
        // m_prev_agc_10db_gain_Q8_16 = (int) (long_acc >> 16);                                                             // Q8.16
    }

    if (long_acc > INT24_MAX)
    {
        long_acc = INT24_MAX;
    }
    else if (long_acc < INT24_MIN)
    {
        long_acc = INT24_MIN;
    }

    m_prev_agc_10db_gain_Q8_16 = (int) long_acc;

    return m_prev_agc_10db_gain_Q8_16;
}

int calculate_agc_gain(int agc_input)
{
    m8p16_t mathlib_m8p16;

    long normalized_mathlib_agc_gain_linear_Q1_23;
    long normalized_agc_input;
    long long_acc;

    int agc_input_db_Q8_16;
    int agc_input_db_Q12_12;
    int agc_gain_db_Q8_16;
    int normalized_mathlib_agc_gain_db_Q8_16;
    int agc_gain_linear_Q12_12;
    int attack_release_agc_gain_db_Q8_16;

    /* 입력 값은 항상 1보다 커야 함 */
    if (agc_input < 1)
    {
        agc_input = 1;
    }

    /* cfx_pwr_to_dB() : 입력 m1p47, 출력 m8p16
     *                   단, 23 이상부터 정상적인 계산 가능
     *                   m8의 경우 -128 보다 작은 값 출력 불가능
     *                   Lshift 5를 적용하여 항상 32 이상의 값이 입력되도록 구현 */
    normalized_agc_input = agc_input << NORMALIZE_SHIFT_BIT_NUM_FOR_10DB_LIBRARY;

    /* 계산 : 10 * LOG10(normalized_agc_input)
     * cfx_pwr_to_dB는 24비트 결과를 출력하기 때문에,
     * to_int 적용 후 >> 연산을 수행해도 음수/양수 변환 에러는 발생하지 않는다. */
    agc_input_db_Q8_16  = to_int(cfx_pwr_to_dB(cfx_as_frac48(normalized_agc_input)).v);
    agc_input_db_Q12_12 = agc_input_db_Q8_16 >> 4;

    // sensitivity Level에 따라 AGC 파라미터들의 값이 달라지므로, 먼저 sensitivity level을 읽어들인다.
    // addr_audioVolume = 0,1,2,3,4,5,6,7..

    if (agc_input_db_Q8_16 < m_noise_gate_db_Q8_16)
    {
        // Noise region

        // noise region slope                        : Q8.16
        // agc input db                              : Q8.16
        // noise region slope * agc input db         : Q16.32
        // (noise region slope * agc input db) >> 20 : Q16.12
        // noise region ybias db                     : Q12.12
        // agc gain db                               : Q8.16

        long_acc = (((long) m_noise_region_slope_Q8_16) * agc_input_db_Q8_16) >> 20;  // Q8.16 * Q8.16 = Q16.32 => Q16.32 >> 20 = Q16.12 // 부호 유지
        long_acc = long_acc + m_noise_region_ybias_db_Q12_12[m_audio_volume];         // Q16.12 + Q12.12 = Q16.12
        long_acc = long_acc - agc_input_db_Q12_12;                                    // Q16.12 - Q12.12 = Q16.12
        long_acc = long_acc << 4;                                                     // Q16.12 << 4 = Q16.16
        // agc_gain_db_Q8_16 = (int) (long_acc << 4);                                             // Q8.16

#if 0
        /**
         * CM3의 Main 함수에서 디버깅 하기 위해 삽입한 코드이다.
         * 어느 영역의 AGC 계수를 사용할 것인지 디버깅하는 코드이다. */
        if (Addr_SharedMem->CM3_tempValue1 == 0)
        {
            Addr_SharedMem->CM3_tempValue2 = 1;
        }
#endif
    }
    else if (m_rotation_point_db_Q8_16 < agc_input_db_Q8_16)
    {
        // Attenuation region

        // attenuation region slope : Q8.16
        // agc input db             : Q8.16
        // noise region ybias db    : Q8.16
        // agc gain db              : Q8.16

        // Q8.16 * Q8.16 = Q16.32 => Q16.32 >> 16 = Q16.16 // 부호 유지
        long_acc = (((long) m_attenuation_region_slope_Q8_16[m_audio_volume]) * agc_input_db_Q8_16) >> 16;
        long_acc = long_acc + m_attenuation_region_ybias_db_Q8_16[m_audio_volume];  // Q16.16 + Q8.16 = Q16.16
        long_acc = long_acc - agc_input_db_Q8_16;                                   // Q16.16 + Q8.16 = Q16.16
                                                                                    // agc_gain_db_Q8_16 = (int) long_acc;

#if 0
        /**
         * CM3의 Main 함수에서 디버깅 하기 위해 삽입한 코드이다.
         * 어느 영역의 AGC 계수를 사용할 것인지 디버깅하는 코드이다. */
        if (Addr_SharedMem->CM3_tempValue1 == 0)
        {
            Addr_SharedMem->CM3_tempValue2 = 2;
        }
#endif
    }
    else
    {
        // Amplify region

        // amplify region ybias db    : Q8.16
        // agc gain db                : Q8.16

        long_acc = agc_input_db_Q8_16 + m_amplify_region_ybias_db_Q8_16[m_audio_volume];  // Q8.16 + Q8.16 = Q8.16
        long_acc = long_acc - agc_input_db_Q8_16;                                         // Q8.16 - Q8.16 = Q8.16
                                                                                          // agc_gain_db_Q8_16 = (int) long_acc;

#if 0
        /**
         * CM3의 Main 함수에서 디버깅 하기 위해 삽입한 코드이다.
         * 어느 영역의 AGC 계수를 사용할 것인지 디버깅하는 코드이다. */
        if (Addr_SharedMem->CM3_tempValue1 == 0)
        {
            Addr_SharedMem->CM3_tempValue2 = 3;
        }
#endif
    }

    if (long_acc > INT24_MAX)
    {
        long_acc = INT24_MAX;
    }
    else if (long_acc < INT24_MIN)
    {
        long_acc = INT24_MIN;
    }

    agc_gain_db_Q8_16 = (int) long_acc;

#if 1  // attack, release
    attack_release_agc_gain_db_Q8_16     = audio_agc_attack_release(agc_gain_db_Q8_16);
    normalized_mathlib_agc_gain_db_Q8_16 = attack_release_agc_gain_db_Q8_16 - m_mathlib_gain_db_Q8_16;
#else
    normalized_mathlib_agc_gain_db_Q8_16 = agc_gain_db_Q8_16 - m_mathlib_gain_db_Q8_16;
#endif

    mathlib_m8p16.v                          = (cfx_as_frac24(normalized_mathlib_agc_gain_db_Q8_16)).v;
    normalized_mathlib_agc_gain_linear_Q1_23 = cfx_as_long(cfx_dB_to_pwr(mathlib_m8p16)) >> 24;

    // cfx_dB_to_pwr 결과는 frac48_t (Q1.47)이며, 이를 >> 24 적용하면 frac24_t (Q1.23)과 동일하다고 보면 된다.

    long_acc = normalized_mathlib_agc_gain_linear_Q1_23 * m_mathlib_gain_linear_Q12_12;  // Q13.35
    long_acc = long_acc >> 23;                                                           // Q13.35 => Q12.12
    // agc_gain_linear_Q12_12 = (int) (long_acc >> 23);  // Q12.12

    if (long_acc > INT24_MAX)
    {
        long_acc = INT24_MAX;
    }
    else if (long_acc < INT24_MIN)
    {
        long_acc = INT24_MIN;
    }

    agc_gain_linear_Q12_12 = (int) long_acc;

    return agc_gain_linear_Q12_12;
}

void apply_agc_gain(int agc_gain_linear)
{
    long       gained_value;
    int _XMEM *p_mix        = (int _XMEM *) HEAR_ADDR_AUDIO_MIX;
    int _XMEM *p_agc_output = (int _XMEM *) &m_agc_output_buffer[0];

    // agc gain linear : Q12.12
    // mix audio data  : Q24.0
    // gained value    : Q36.12
    // agc output      : Q24.0

    for (register int i = 0; i < df_inputADC_DataBuffLength; i++)
        chess_loop_range(df_inputADC_DataBuffLength, df_inputADC_DataBuffLength)
        {
            gained_value = ((long) agc_gain_linear) * p_mix[i];  // Q12.12 * Q24.0 = Q36.12
            gained_value = gained_value >> 12;                   // Q36.12 >> 12 = Q36.0

            if (gained_value > INT24_MAX)
            {
            	gained_value = INT24_MAX;
            }
            else if (gained_value < INT24_MIN)
            {
            	gained_value = INT24_MIN;
            }

            p_agc_output[i] = (int) gained_value;
            //p_agc_output[i] = (int) (gained_value >> 12);
        }
}
