/**
 * @file nonlinearMapping.c
 */

#include <nonlinearMapping.h>

#include <OTE_1_5_gen_UART.h>

// p 값
int chess_storage(XMEM) g_coeff_p_Q1_23 = 1258291;  // p = 0.15의 Q-format 표현, QI1F23

/* normalizeFactor^(-p) 값
 * x^p 계산을 위해 사용되는 cfx_pwr_to_dB_asm() 함수는 frac48 입력, m8p16 출력 형식을 갖는다.
 * cfx_pwr_to_dB_asm() 뿐만 아니라 cfx_dB_to_pwr_asm() 함수도 마찬가지이고,
 * 각 함수 입출력에 대한 자료형을 맞춰야 한다.
 * 그래서 normalizeFactor^(-p) 식에서 normalizeFactor도 frac48로 계산한다.
 * 즉, (normalizeFactor / 2^47)^(-p) 를 계산해야 한다.
 * normalizeFactor        = 32
 * normalizeFactor_frac48 = 0.0000000000002273736754432320
 * normalizeFactor_frac48^(-p) = 78.7932
 * 결과는 78.7932로 출력되며, 이를 Q8.16으로 표현하면 5163793으로 나타낼 수 있다. */

/* normalizeFactor_frac48^(-p) = 78.7932...의 Q8.16 표현 */
int chess_storage(XMEM) g_normalize_factor_frac48_power_minus_p_Q8_16 = 5163793;

int chess_storage(XMEM) g_max_output_volume_ratio_Q2_22[4] = {
    2936012,  // 볼륨 단계 1에 해당하는 볼륨의 최대값과 비교한 비율값(0.7)의 QI2F22 표현
    3355443,  // 볼륨 단계 2에 해당하는 볼륨의 최대값과 비교한 비율값(0.8)의 QI2F22 표현
    3774873,  // 볼륨 단계 3에 해당하는 볼륨의 최대값과 비교한 비율값(0.9)의 QI2F22 표현
    4194304   // 볼륨 단계 4에 해당하는 볼륨의 최대값과 비교한 비율값(1.0)의 QI2F22 표현
};

#if 1  // NOTE : 로그매핑에 볼륨 적용할 때 아래 계수를 사용하도록 수정해야함
/* 오디오 볼륨(증폭영역 게인 선형표현), (Q-format 형식)  */
int chess_storage(XMEM) g_logarithmMapping_audioVolume_linearGain_Q9_15[10] = {
    32768,   // 1
    46955,   // 1.4622
    68657,   // 2.138
    100391,  // 3.1262
    146792,  // 4.5712
    214639,  // 6.684
    313846,  // 9.7733
    458905,  // 14.2905
    671011,  // 20.8956
    981153,  // 30.5535
};

// 로그매핑 때 A, B 계수 만들 때 사용되는 xMin, xMax에 볼륨에 해당하는 선형 게인을 곱한 뒤 >> 15해서 자리를 맞춰주면 된다.
#endif

int chess_storage(XMEM) g_logaritmMapping_coeff_A_Q16_8[df_MaxNumOfElectrode] = {0};
int chess_storage(XMEM) g_logaritmMapping_coeff_B_Q16_8[df_MaxNumOfElectrode] = {0};

int chess_storage(XMEM) g_max_limit_stimulus_amplitude_C_level[df_MaxNumOfElectrode] = {0};
int chess_storage(XMEM) g_pcm_amplitude_level[df_MaxNumOfElectrode]                  = {0};  // 최초값은 0이어야 한다.

void logarithmMapping(void)
{
    int temp_x_power_p[df_MaxNumOfElectrode];

    /* 각 전극 별로 x_power_p (QI8F16) 값을 미리 계산하여 배열에 저장함.
     * Pre-calculate and store x_power_p for all electrodes to reduce redundant operations. */
    for (register int i = 0; i < df_MaxNumOfElectrode; i++)
        chess_loop_range(df_MaxNumOfElectrode, df_MaxNumOfElectrode) {
            temp_x_power_p[i] = calculate_x_power_p(g_freq_rep_values_scaled[i]);
    }

    int mute_mode = Addr_SharedMem->is_enabled_mute_stimulation_under_t_level;
    int t_offset  = Addr_SharedMem->mute_stimulation_t_level_offset;

    /* Loop Unswitching & Branchless Math 적용
     * 루프 내부의 조건 분기(if-else)를 제거하기 위해 묵음 처리 모드(mute_mode)에 따라 루프를 분리했음.
     * 3항 연산자(Ternary operator)를 사용하여 분기 없는(branchless) 수학 연산을 수행함.
     *
     * a * x_power p 계산: Q8.16 * Q16.8 -> Q24.24 -> Q24.8 -> Q16.8
     * a * x_power_p + b 계산: Q16.8 + Q16.8 -> Q16.8
     * y는 소수점 영역을 제거하고 정수형 Q16.0 형식을 갖다. */
    if (mute_mode == 1) {
        // 묵음 처리 활성화 상태 (Mute stimulation enabled)
        for (register int i = 0; i < df_MaxNumOfElectrode; i++)
            chess_loop_range(df_MaxNumOfElectrode, df_MaxNumOfElectrode) {
                int coeff_a = g_logaritmMapping_coeff_A_Q16_8[i];   // QI16F8
                int coeff_b = g_logaritmMapping_coeff_B_Q16_8[i];   // QI16F8

                /* 오버플로우 방지 (Overflow clamp)
                 * a * x_power p 계산은
                 * Q8.16 * Q16.8    -> Q24.24
                 * Q24.24 >> 16     -> Q24.8
                 * (int) Q24.8      -> Q16.8 형식을 갖는다. */
                long long_acc = ((long)coeff_a) * temp_x_power_p[i];
                long_acc >>= 16;
                long_acc = (long_acc > INT24_MAX) ? INT24_MAX : ((long_acc < INT24_MIN) ? INT24_MIN : long_acc);

                /* long_acc = (int) ((((long) coeff_a) * x_power_p) >> 16);
                 * a * x_power_p + b 계산은
                 * Q16.8 + Q16.8    -> Q16.8 형식을 갖는다.
                 * y는 소수점 영역을 제거하고 정수형 Q16.0 형식을 갖는다. */
                int y = ((int)long_acc + coeff_b) >> 8;   // Q16.0 부호 영향 없음

                int c_level = g_max_limit_stimulus_amplitude_C_level[i];
                int t_level = g_mapping_stimulus_amplitude_T_level[i];

                // T 레벨 + offset 미만이면 0, 최대 출력은 C 레벨로 제한
                // Set to 0 if below T-level + offset, clamp max output to C-level
                y = (y < (t_level + t_offset)) ? 0 : y;
                y = (y > c_level) ? c_level : y;

                g_pcm_amplitude_level[i]                        = y;  // 자극 출력 PCM에서 사용
                Addr_SharedMem->currentOutputStimulLevel_255[i] = y;  // 이퀄라이져 기능을 위하여 CM3와 공유
        }
    }
    else if (mute_mode == 2) {
        // 묵음 처리 비활성화 상태 (Mute stimulation disabled)
        for (register int i = 0; i < df_MaxNumOfElectrode; i++)
            chess_loop_range(df_MaxNumOfElectrode, df_MaxNumOfElectrode) {
                int coeff_a = g_logaritmMapping_coeff_A_Q16_8[i];
                int coeff_b = g_logaritmMapping_coeff_B_Q16_8[i];

                long long_acc = ((long)coeff_a) * temp_x_power_p[i];
                long_acc >>= 16;

                long_acc = (long_acc > INT24_MAX) ? INT24_MAX : ((long_acc < INT24_MIN) ? INT24_MIN : long_acc);

                int y = ((int)long_acc + coeff_b) >> 8;

                int c_level = g_max_limit_stimulus_amplitude_C_level[i];
                int t_level = g_mapping_stimulus_amplitude_T_level[i];

                // T 레벨과 C 레벨 사이로 값 제한
                // Clamp amplitude strictly between T-level and C-level
                y = (y < t_level) ? t_level : y;
                y = (y > c_level) ? c_level : y;

                g_pcm_amplitude_level[i]                        = y;  // 자극 출력 PCM에서 사용
                Addr_SharedMem->currentOutputStimulLevel_255[i] = y;  // 이퀄라이져 기능을 위하여 CM3와 공유
        }
    }
    else {
        // 묵음 처리에 대해서 정의 되지 않은 알 수 없는 상태에서는 자극 출력하지 않음
        for (register int i = 0; i < df_MaxNumOfElectrode; i++)
            chess_loop_range(df_MaxNumOfElectrode, df_MaxNumOfElectrode) {
                int coeff_a = g_logaritmMapping_coeff_A_Q16_8[i];
                int coeff_b = g_logaritmMapping_coeff_B_Q16_8[i];

                long long_acc = ((long)coeff_a) * temp_x_power_p[i];
                long_acc >>= 16;

                long_acc = (long_acc > INT24_MAX) ? INT24_MAX : ((long_acc < INT24_MIN) ? INT24_MIN : long_acc);

                int y = ((int)long_acc + coeff_b) >> 8;

                int c_level = g_max_limit_stimulus_amplitude_C_level[i];

                // 자극 출력하지 않음 (0 처리), 단 C 레벨 한계치는 유지
                // Output disabled (set to 0), constrained by C-level limit
                y = (y > c_level) ? c_level : 0;

                g_pcm_amplitude_level[i]                        = y;  // 자극 출력 PCM에서 사용
                Addr_SharedMem->currentOutputStimulLevel_255[i] = y;  // 이퀄라이져 기능을 위하여 CM3와 공유
        }
    }

    /* 원본 코드의 Addr_SharedMem_indicatorStimulOutput_OnOff 는 다음을 가리킴.
     * Addr_SharedMem->calculatedStimulationIndcator_byCM3.indicatorStimulOutput_OnOff_Coltroled_byCM3
     * 자극 알림은 CM3의 main.c에서 알림이 필요할 때 세팅하고 동작시키도록 구조화 되어 있다.
     * 자극 알림 기능이 활성화 되면, CM3에서 카운팅을 통해
     * On, off 각각을 약 80 msec 만큼 3회 반복 한 뒤 자극 알림 기능이 해제 된다. */

    /* 자극 알림 기능이 켜졌을 때 */
    if (Addr_SharedMem->calculatedStimulationIndcator_byCM3.indicatorStimulOutput_OnOff_Coltroled_byCM3 == 1)
    {
        /* 원본 코드의 Addr_SharedMem_UserSetting_MapNumber 는 다음을 가리킴.
         * Addr_SharedMem->userSettingValue.mapNum */

        /* 매핑 라이브 실행 중에 알림 채널을 변경했을 경우 */
        if (Addr_SharedMem->userSettingValue.mapNum < 0)
        {
            /* Addr_SharedMem->userSettingValue.mapNum이 0보다 작은 경우는 매핑의 라이브 실행 중인 경우이므로,
             * 이 구문을 수행한다는 것은 매핑을 진행하며 자극 알림을 설정하는 경우라고 예상할 수 있다. */

            /* 원본 코드의 Addr_SharedMem_stimulationIndicatorChannelNum 는 다음을 가리킴.
             * Addr_SharedMem->currentMapData.stimulationIndicatorChannelNum
             * 정상적인 PcmLiveStimulatino 모드라면,
             * addr_MapProgramData_indicatorStimulCannel_index 는 맵이 변경 될 때마다
             * system_control.c의 Addr_SharedMem->mapChangeFlag.cm3Command_mapChange 이벤트 처리 구문에서 업데이트 된다.
             * 하지만, 매핑 라이브 실행 중에 알림 채널을 설정하는 경우
             * 매핑 앱을 통해 Addr_SharedMem->currentMapData.stimulationIndicatorChannelNum 의 값을 실시간으로 변경하고,
             * CFX가 이 값을 사용하도록 구현되었다. */
            addr_MapProgramData_indicatorStimulCannel_index = Addr_SharedMem->currentMapData.stimulationIndicatorChannelNum;
        }

        /* 원본 코드의 Addr_SharedMem_indcatorStimulationLevel_255 는 다음을 가리킴.
         * Addr_SharedMem->calculatedStimulationIndcator_byCM3.indicatorStimulLevel_255 */
        g_pcm_amplitude_level[addr_MapProgramData_indicatorStimulCannel_index - 1] =
            Addr_SharedMem->calculatedStimulationIndcator_byCM3.indicatorStimulLevel_255;
    }
}

void read_C_T_Level_fromCM3(void)
{
    // 채널별 C-T 값 업데이트
    for (int i = 0; i < df_MaxNumOfElectrode; i++)
    {
        g_mapping_stimulus_amplitude_T_level[i] = Addr_SharedMem->calculatedStimulPara_byCM3.T_level_255[i];
        g_mapping_stimulus_amplitude_C_level[i] = Addr_SharedMem->calculatedStimulPara_byCM3.C_level_255[i];
    }
}

// 변경된 볼륨값에 따라 최대 자극 레벨 값을 다시 계산한다.
void calculate_newStimulation_maxLevel(int stimulationVolume)
{
    int  C;
    int  T;
    int  new_C;
    int  volumeRatio_Q2_22;
    int  dynamicRange;
    long long_acc;

    volumeRatio_Q2_22 = g_max_output_volume_ratio_Q2_22[stimulationVolume - 1];

    for (register int i = 0; i < df_MaxNumOfElectrode; i++)
        chess_loop_range(df_MaxNumOfElectrode, df_MaxNumOfElectrode)
        {
            C = g_mapping_stimulus_amplitude_C_level[i];
            T = g_mapping_stimulus_amplitude_T_level[i];

            /* T           : Y min (Q24.0)
             * C           : Y max (Q24.0)
             * volumeRatio : 0.7, 0.8, 0.9, 1.0 (Q2.22) */

            /* Q24.0 * Q2.22 => Q26.22
             * Q26.22 >> 22  => Q26.0
             * (int) Q26.0   => Q24.0 */

            long_acc = ((long) (C - T)) * ((long) volumeRatio_Q2_22);
            long_acc = long_acc >> 22;

            if (long_acc > INT24_MAX)
            {
                long_acc = INT24_MAX;
            }
            else if (long_acc < INT24_MIN)
            {
                long_acc = INT24_MIN;
            }

            dynamicRange = (int) long_acc;
            // dynamicRange = (int) (((C - T) * ((long) volumeRatio_Q2_22)) >> 22);

            new_C = T + dynamicRange;

            if (C < new_C)
            {
                new_C = C;
            }
            else if (new_C < T)
            {
                new_C = T;
            }

            g_max_limit_stimulus_amplitude_C_level[i] = new_C;  // 새로운 자극 최대값 저장
        }
}

/*
 * @brief Logarithm function에서 사용되는 곡선의 곡률 및 절편 계수를 계산한다.
 */
void calculate_logaritmMapping_coeff(void)
{
    int  xMin_power_p_Q8_16;
    int  xMax_power_p_Q8_16;
    int  xpDiff_Q8_16;
    int  yDiff_Q11_13;
    int  coeff_A_Q16_8;
    int  coeff_B_Q16_8;
    long long_acc;

    for (register int i = 0; i < df_MaxNumOfElectrode; i++)
        chess_loop_range(df_MaxNumOfElectrode, df_MaxNumOfElectrode)
        {
            /* A = (stimulMax - stimulMin) / (xMax^p - xMin^p) */

            /* (xMax^p - xMin^p) 과정
             * calculate_x_power_p() 출력 결과는 Q8.16 */
            xMax_power_p_Q8_16 = calculate_x_power_p(addr_MapProgramData_xMaxLevel[i]);
            xMin_power_p_Q8_16 = calculate_x_power_p(addr_MapProgramData_xMinLevel[i]);
            xpDiff_Q8_16       = xMax_power_p_Q8_16 - xMin_power_p_Q8_16;

            /* yMax - yMin => yDiff (Q11.13)
             * Q11.13 형식으로 맞추는 이유는,
             * 과거 자극 amplitude가 최대 1024까지 설정했던 이력으로 인한 레거시 코드이다. */
            yDiff_Q11_13 = (g_max_limit_stimulus_amplitude_C_level[i] - g_mapping_stimulus_amplitude_T_level[i]) << 13;

            /* NOTE : 아래 DIVIDE 라이브러리 관련 Q형식 자리 맞추는 방식은 정우님이 하신 내용을 그대로 사용했음.
             *      : 잘 이해되지 않아서 수정하지 않았음. */

            // QI24F0을 QI24F0로 나눈값으로 QI24F24로 나온다.
            // QI8F13을 Q11F13으로 나누었다면 동일하게  QI24F24으로 나올 것이므로
            // Q11F13을  QI8F16으로 나눈 결과값에 F3비트 적게 QI24F21이 최종적으로 출력될 것이다.

            /* 자세히 예시를 들어서 정리
             * cfx_div_frac24_frac24_m24p24 함수의 인자 앞이 분자, 뒤에가 분모이다.
             * 이 함수는 ((분자 << 24) / 분모)를 계산한다.
             * 가령 아래의 경우 yDiff_Q11_13 << 24 즉, Q11.37 / Q8.16 을 수행한다.
             * 그런데 to_fix를 함으로써 실제로 각 분자, 분모는 Q1.23으로 계산되는 효과를 갖는데,
             * cfx_div_frac24_frac24_m24p24 함수의 결과가 Q24.24로 출력된다.
             * 실제 계산에 사용된 분자 Q11.13은 분모 Q8.16보다 F 영역이 .3만큼 작다.
             * 그래서 cfx_div_frac24_frac24_m24p24 함수의 결과가 Q24.21로 출력된다.
             * 실험적으로 검증되었다. 전체 48비트에서 하위 24비트에 Q3.21이 저장되고, 상위 24비트에 남은 Q21이 저장된다.
             * 그래서 결과에 >> 13을 수행하면 Q24.8이 된다. 이를 (int) 형변환 하면 Q16.8이 된다. */

            /* 엑셀 값을 기준으로 예를 들어본다.
             * 255 / 1 = 255 가 계산되어야 한다.
             * 255 => Q11.13 형식으로 하면 => 2,088,960
             * 1   => Q8.16  형식으로 하면 => 65,536
             * cfx_div_frac24_frac24_m24p24 함수는 분자를 << 24 해서 계산한다고 했으니,
             * 255 => Q11.37 형식으로 F가 24비트 증가하면 => 35,046,933,135,360
             * 35,046,933,135,360 / 65,536 = 534,773,760 (Q24.21, 분자가 분모 보다 F가 3비트 작기 때문에 Q24.21로 출력)
             * 534,773,760 >> 21 = 255 (Q24.0)
             * 만약 Q24.8 형식으로 계산 결과를 얻고 싶다면, 534,773,760 >> 13 = 65280 (Q24.8)
             * 이후 Q24.8을 (int) 형 변환하면 Q16.8로 정리 */

            long_acc = to_long(cfx_div_frac24_frac24_m24p24(to_fix(yDiff_Q11_13), to_fix(xpDiff_Q8_16))) >> 13;

            if (long_acc > INT24_MAX)
            {
                long_acc = INT24_MAX;
            }
            else if (long_acc < INT24_MIN)
            {
                long_acc = INT24_MIN;
            }

            coeff_A_Q16_8 = (int) long_acc;  // QI16F8
            // coeff_A_Q16_8 = to_long(cfx_div_frac24_frac24_m24p24(to_fix(yDiff_Q11_13), to_fix(xpDiff_Q8_16))) >> 13;  // QI16F8

            long_acc = (((long) coeff_A_Q16_8) * ((long) xMax_power_p_Q8_16)) >> 16;
            long_acc = ((long) (g_max_limit_stimulus_amplitude_C_level[i] << 8)) - long_acc;

            if (long_acc > INT24_MAX)
            {
                long_acc = INT24_MAX;
            }
            else if (long_acc < INT24_MIN)
            {
                long_acc = INT24_MIN;
            }

            coeff_B_Q16_8 = (int) long_acc;  // Q16.8
            // coeff_B_Q16_8 = (g_max_limit_stimulus_amplitude_C_level[i] << 8) - ((coeff_A_Q16_8 * ((long) xMax_power_p_Q8_16)) >> 16);

            g_logaritmMapping_coeff_A_Q16_8[i] = coeff_A_Q16_8;  // QI16F8
            g_logaritmMapping_coeff_B_Q16_8[i] = coeff_B_Q16_8;  // QI16F8
        }
}

void calculate_logaritmMapping_coeff_with_audioVolume(void)
{
    int  audioVolume_gained_xMinLevel;
    int  xMin_power_p_Q8_16;
    int  xMax_power_p_Q8_16;
    int  xpDiff_Q8_16;
    int  yDiff_Q11_13;
    int  coeff_A_Q16_8;
    int  coeff_B_Q16_8;
    long long_acc;

    for (register int i = 0; i < df_MaxNumOfElectrode; i++)
        chess_loop_range(df_MaxNumOfElectrode, df_MaxNumOfElectrode)
        {
            /* 우선 오디오 볼륨에 해당하는 게인을 xMin에 적용 */
            long_acc = (long) g_logarithmMapping_audioVolume_linearGain_Q9_15[m_audio_volume];
            long_acc = long_acc * ((long) addr_MapProgramData_xMinLevel[i]);
            long_acc = long_acc >> 15;

            if (long_acc > INT24_MAX)
            {
                long_acc = INT24_MAX;
            }
            else if (long_acc < INT24_MIN)
            {
                long_acc = INT24_MIN;
            }

            audioVolume_gained_xMinLevel = (int) long_acc;

            /* A = (stimulMax - stimulMin) / (xMax^p - xMin^p) */

            /* (xMax^p - xMin^p) 과정
             * calculate_x_power_p() 출력 결과는 Q8.16 */
            xMax_power_p_Q8_16 = calculate_x_power_p(addr_MapProgramData_xMaxLevel[i]);
            // xMin_power_p_Q8_16 = calculate_x_power_p(addr_MapProgramData_xMinLevel[i]);
            xMin_power_p_Q8_16 = calculate_x_power_p(audioVolume_gained_xMinLevel);
            xpDiff_Q8_16       = xMax_power_p_Q8_16 - xMin_power_p_Q8_16;

            /* yMax - yMin => yDiff (Q11.13)
             * Q11.13 형식으로 맞추는 이유는,
             * 과거 자극 amplitude가 최대 1024까지 설정했던 이력으로 인한 레거시 코드이다. */
            yDiff_Q11_13 = (g_max_limit_stimulus_amplitude_C_level[i] - g_mapping_stimulus_amplitude_T_level[i]) << 13;

            /* NOTE : 아래 DIVIDE 라이브러리 관련 Q형식 자리 맞추는 방식은 정우님이 하신 내용을 그대로 사용했음.
             *      : 잘 이해되지 않아서 수정하지 않았음. */

            // QI24F0을 QI24F0로 나눈값으로 QI24F24로 나온다.
            // QI8F13을 Q11F13으로 나누었다면 동일하게  QI24F24으로 나올 것이므로
            // Q11F13을  QI8F16으로 나눈 결과값에 F3비트 적게 QI24F21이 최종적으로 출력될 것이다.

            /* 자세히 예시를 들어서 정리
             * cfx_div_frac24_frac24_m24p24 함수의 인자 앞이 분자, 뒤에가 분모이다.
             * 이 함수는 ((분자 << 24) / 분모)를 계산한다.
             * 가령 아래의 경우 yDiff_Q11_13 << 24 즉, Q11.37 / Q8.16 을 수행한다.
             * 그런데 to_fix를 함으로써 실제로 각 분자, 분모는 Q1.23으로 계산되는 효과를 갖는데,
             * cfx_div_frac24_frac24_m24p24 함수의 결과가 Q24.24로 출력된다.
             * 실제 계산에 사용된 분자 Q11.13은 분모 Q8.16보다 F 영역이 .3만큼 작다.
             * 그래서 cfx_div_frac24_frac24_m24p24 함수의 결과가 Q24.21로 출력된다.
             * 실험적으로 검증되었다. 전체 48비트에서 하위 24비트에 Q3.21이 저장되고, 상위 24비트에 남은 Q21이 저장된다.
             * 그래서 결과에 >> 13을 수행하면 Q24.8이 된다. 이를 (int) 형변환 하면 Q16.8이 된다. */

            /* 엑셀 값을 기준으로 예를 들어본다.
             * 255 / 1 = 255 가 계산되어야 한다.
             * 255 => Q11.13 형식으로 하면 => 2,088,960
             * 1   => Q8.16  형식으로 하면 => 65,536
             * cfx_div_frac24_frac24_m24p24 함수는 분자를 << 24 해서 계산한다고 했으니,
             * 255 => Q11.37 형식으로 F가 24비트 증가하면 => 35,046,933,135,360
             * 35,046,933,135,360 / 65,536 = 534,773,760 (Q24.21, 분자가 분모 보다 F가 3비트 작기 때문에 Q24.21로 출력)
             * 534,773,760 >> 21 = 255 (Q24.0)
             * 만약 Q24.8 형식으로 계산 결과를 얻고 싶다면, 534,773,760 >> 13 = 65280 (Q24.8)
             * 이후 Q24.8을 (int) 형 변환하면 Q16.8로 정리 */

            long_acc = to_long(cfx_div_frac24_frac24_m24p24(to_fix(yDiff_Q11_13), to_fix(xpDiff_Q8_16))) >> 13;

            if (long_acc > INT24_MAX)
            {
                long_acc = INT24_MAX;
            }
            else if (long_acc < INT24_MIN)
            {
                long_acc = INT24_MIN;
            }

            coeff_A_Q16_8 = (int) long_acc;  // QI16F8
            // coeff_A_Q16_8 = to_long(cfx_div_frac24_frac24_m24p24(to_fix(yDiff_Q11_13), to_fix(xpDiff_Q8_16))) >> 13;  // QI16F8

            long_acc = (((long) coeff_A_Q16_8) * ((long) xMax_power_p_Q8_16)) >> 16;
            long_acc = ((long) (g_max_limit_stimulus_amplitude_C_level[i] << 8)) - long_acc;

            if (long_acc > INT24_MAX)
            {
                long_acc = INT24_MAX;
            }
            else if (long_acc < INT24_MIN)
            {
                long_acc = INT24_MIN;
            }

            coeff_B_Q16_8 = (int) long_acc;  // Q16.8
            // coeff_B_Q16_8 = (g_max_limit_stimulus_amplitude_C_level[i] << 8) - ((coeff_A_Q16_8 * ((long) xMax_power_p_Q8_16)) >> 16);

            g_logaritmMapping_coeff_A_Q16_8[i] = coeff_A_Q16_8;  // QI16F8
            g_logaritmMapping_coeff_B_Q16_8[i] = coeff_B_Q16_8;  // QI16F8

#if 1
            /**
             * 로그 매핑과 관련된 각종 계수를 디버깅하는 코드 */
            if (FS_MEM_UART->flag[0] == 1)
            {
                FS_MEM_UART->buffer[0 + i]   = addr_MapProgramData_xMinLevel[i];           // xMin
                FS_MEM_UART->buffer[32 + i]  = audioVolume_gained_xMinLevel;               // audioVolume gained xMin
                FS_MEM_UART->buffer[64 + i]  = addr_MapProgramData_xMaxLevel[i];           // xMax
                FS_MEM_UART->buffer[96 + i]  = g_logaritmMapping_coeff_A_Q16_8[i];         // coefficient A
                FS_MEM_UART->buffer[128 + i] = g_logaritmMapping_coeff_B_Q16_8[i];         // coefficient B
                FS_MEM_UART->buffer[160 + i] = g_max_limit_stimulus_amplitude_C_level[i];  // C level
                FS_MEM_UART->buffer[192 + i] = g_mapping_stimulus_amplitude_T_level[i];    // T level
            }
#endif
        }

        /**
         * [  0]에서 [ 31] : xMin
         * [ 32]에서 [ 63] : 오디오 볼륨 게인 적용된 xMin
         */

#if 0  // [400]에서 [499]까지를 사용해 로그 계산 Y가 T에서 C 사이로 출력되는지 확인하는 코드
    {
        // 로그 매핑 기능 변수
        int x_power_p;
        int coeff_a;
        int coeff_b;
        int a_x_power_p;
        int a_x_power_p_b;
        int y;
        int adder;

        adder = (addr_MapProgramData_xMaxLevel[0] - audioVolume_gained_xMinLevel) / 98;

        // 로그 매핑 기능
        coeff_a   = g_logaritmMapping_coeff_A_Q16_8[0];                 // QI16F8
        coeff_b   = g_logaritmMapping_coeff_B_Q16_8[0];                 // QI16F8
        x_power_p = calculate_x_power_p(audioVolume_gained_xMinLevel);  // QI8F16

        /* a * x_power p 계산은
         * Q8.16 * Q16.8    -> Q24.24
         * Q24.24 >> 16     -> Q24.8
         * (int) Q24.8      -> Q16.8 형식을 갖는다. */
        long_acc = ((long) coeff_a) * x_power_p;
        long_acc = long_acc >> 16;

        if (long_acc > INT24_MAX)
        {
            long_acc = INT24_MAX;
        }
        else if (long_acc < INT24_MIN)
        {
            long_acc = INT24_MIN;
        }

        a_x_power_p = (int) long_acc;
        // a_x_power_p = (int) ((((long) coeff_a) * x_power_p) >> 16);

        /* a * x_power_p + b 계산은
         * Q16.8 + Q16.8    -> Q16.8 형식을 갖는다. */
        a_x_power_p_b = a_x_power_p + coeff_b;  // Q16.8

        /* y는 소수점 영역을 제거하고 정수형 Q16.0 형식을 갖는다. */
        y = a_x_power_p_b >> 8;  // Q16.0 // 부호 영향 없음

        FS_MEM_UART->buffer[400] = y;

        for (register int i = 1; i < 99; i++)
            chess_loop_range(98, 98)
            {
                audioVolume_gained_xMinLevel = audioVolume_gained_xMinLevel + adder;

                x_power_p = calculate_x_power_p(audioVolume_gained_xMinLevel);  // QI8F16

                /* a * x_power p 계산은
                 * Q8.16 * Q16.8    -> Q24.24
                 * Q24.24 >> 16     -> Q24.8
                 * (int) Q24.8      -> Q16.8 형식을 갖는다. */
                long_acc = ((long) coeff_a) * x_power_p;
                long_acc = long_acc >> 16;

                if (long_acc > INT24_MAX)
                {
                    long_acc = INT24_MAX;
                }
                else if (long_acc < INT24_MIN)
                {
                    long_acc = INT24_MIN;
                }

                a_x_power_p = (int) long_acc;
                // a_x_power_p = (int) ((((long) coeff_a) * x_power_p) >> 16);

                /* a * x_power_p + b 계산은
                 * Q16.8 + Q16.8    -> Q16.8 형식을 갖는다. */
                a_x_power_p_b = a_x_power_p + coeff_b;  // Q16.8

                /* y는 소수점 영역을 제거하고 정수형 Q16.0 형식을 갖는다. */
                y = a_x_power_p_b >> 8;  // Q16.0 // 부호 영향 없음

                FS_MEM_UART->buffer[400 + i] = y;
            }

        audioVolume_gained_xMinLevel = addr_MapProgramData_xMaxLevel[0];

        x_power_p = calculate_x_power_p(audioVolume_gained_xMinLevel);  // QI8F16

        /* a * x_power p 계산은
         * Q8.16 * Q16.8    -> Q24.24
         * Q24.24 >> 16     -> Q24.8
         * (int) Q24.8      -> Q16.8 형식을 갖는다. */
        long_acc = ((long) coeff_a) * x_power_p;
        long_acc = long_acc >> 16;

        if (long_acc > INT24_MAX)
        {
            long_acc = INT24_MAX;
        }
        else if (long_acc < INT24_MIN)
        {
            long_acc = INT24_MIN;
        }

        a_x_power_p = (int) long_acc;
        // a_x_power_p = (int) ((((long) coeff_a) * x_power_p) >> 16);

        /* a * x_power_p + b 계산은
         * Q16.8 + Q16.8    -> Q16.8 형식을 갖는다. */
        a_x_power_p_b = a_x_power_p + coeff_b;  // Q16.8

        /* y는 소수점 영역을 제거하고 정수형 Q16.0 형식을 갖는다. */
        y = a_x_power_p_b >> 8;  // Q16.0 // 부호 영향 없음

        FS_MEM_UART->buffer[499] = y;
    }
#endif

#if 1
    /**
     * 로그 매핑과 관련된 각종 계수를 디버깅하는 코드 */
    FS_MEM_UART->flag[0] = 2;
#endif
}

int calculate_x_power_p(int x)
{
    int  normalized_x;
    int  normalized_x_db_m8p16;
    int  normalized_p_x_db_Q9_16;
    int  normalized_x_power_p_Q1_23;
    int  x_power_p_Q8_16;
    long long_acc;

    if (x < 1)
    {
        return 0;
    }

    /* cfx_pwr_to_dB_asm() : frac48 입력,  m8p16 출력
     * cfx_dB_to_pwr_asm() :  m8p16 입력, frac48 출력 */

    /* 자료형 정보는 다음과 같다.
     * typedef lfix frac48
     * typedef fix  m8p16 */

    normalized_x = x << NORMALIZE_SHIFT_BIT_NUM_FOR_10DB_LIBRARY;

    /* 10 * LOG10(x * normalizeFactor)
     * cfx_pwr_to_dB_asm() : frac48 입력, m8p16 출력 */
    normalized_x_db_m8p16 = to_int(cfx_pwr_to_dB_asm(to_lfix(normalized_x)));

    /* y = p * 10 * LOG10(x * NormalizeFactor)
     * m8p16 형식에 Q1.23 형식을 곱하여 Q9.39 형식이 된다.
     * 오른쪽 23 쉬프트를 하여 Q9.16 형식으로 변경한다. */
    long_acc = ((long) g_coeff_p_Q1_23) * ((long) normalized_x_db_m8p16);
    long_acc = long_acc >> 23;

    if (long_acc > INT24_MAX)
    {
        long_acc = INT24_MAX;
    }
    else if (long_acc < INT24_MIN)
    {
        long_acc = INT24_MIN;
    }

    normalized_p_x_db_Q9_16 = (int) long_acc;
    // normalized_p_x_db_Q9_16 = (g_coeff_p_Q1_23 * ((long) normalized_x_db_m8p16)) >> 23;

    /* 10^(y / 10)
     * cfx_dB_to_pwr_asm() : m8p16 입력, frac48 출력 */
    normalized_x_power_p_Q1_23 = to_long(cfx_dB_to_pwr_asm(to_fix(normalized_p_x_db_Q9_16))) >> 24; // >> 24하면 부호가 유지된 채로 frac48_t가 frac24_t로 변환

    /* 10^(y / 10) * NormalizeFactor^(-p)
     * normalizeFactor도 frac48로 계산한다.
     * 즉, (normalizeFactor / 2^47)^(-p) 를 계산해야 한다.
     * 결과는 78.79324245로 출력되며, 이를 Q8.16으로 표현하면 5163793 값이 된다. */
    long_acc = ((long) normalized_x_power_p_Q1_23) * g_normalize_factor_frac48_power_minus_p_Q8_16;
    long_acc = long_acc >> 23;

    if (long_acc > INT24_MAX)
    {
        long_acc = INT24_MAX;
    }
    else if (long_acc < INT24_MIN)
    {
    	long_acc = INT24_MIN;
    }

    x_power_p_Q8_16 = (int)long_acc;
    //x_power_p_Q8_16 = (int)((((long) normalized_x_power_p_Q1_23) * g_normalize_factor_frac48_power_minus_p_Q8_16) >> 23);

    /* x^p */
    return x_power_p_Q8_16;
}
