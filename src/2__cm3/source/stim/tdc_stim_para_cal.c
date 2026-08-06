#include <hw.h>
#include <tdc_isd_pcm.h>
#include <stdbool.h>

#include <board.h>
#include <internalStimulationChip.h>
#include <tdc_isd.h>
#include <tdc_stim_definitions.h>

#include <tdc_isd_init_fpga.h>
#include <tdc_isd.h>
#include <tdc_shm.h>
#include <tdc_stim_para_cal.h>
#include <tdc_sys_error.h>

static ST_STIUL_DAC_REGISTER_VALUE stimulationDAC_para;

// const ST_STIUL_DAC_REGISTER_VALUE *tdc_stim_read_dac_register_value(void)
ST_STIUL_DAC_REGISTER_VALUE *tdc_stim_read_dac_register_value(void)
{
    return &(stimulationDAC_para);
}

static int DAC_offset_uA;

const int tdc_stim_read_dac_offset_value(void)
{
    return DAC_offset_uA;
}

int tdc_stim_calc_frame_per_channel(int pulsewidth)
{
    int numFramePerChannel, duration, tokenTime;

    // 소요시간 계산 - 측정 시작 시 펄스폭
    //  펄스폭 시간 (펄스 위상 x 2)
    duration = pulsewidth;
    duration += duration;
    // 펄스 위상 반전 간격
    duration += FPGA_interphaseGapTokenTime;  // FPGA_interphaseGapTokenTime : 4
    // 자극파라미터 전송 시간
    duration += FPGA_electrodAndStimulLevelTokenTime;  // FPGA_electrodAndStimulLevelTokenTime : 10

    tokenTime          = FPGA_oneChannelDataTokenTime;  // FPGA_oneChannelDataTokenTime : 41
    numFramePerChannel = 1;
    while (true)
    {
        if (duration < tokenTime)
        {
            break;
        }
        numFramePerChannel++;
        tokenTime += FPGA_oneChannelDataTokenTime;  // FPGA_oneChannelDataTokenTime : 41
    }

    return numFramePerChannel;
}

int tdc_stim_calc_transferable_channel_num(int numFramePerChannel, int usableElectrodNum)
{
    int TransferabelChannelNumPerOneMilSec;

    // 24나누기 채널당 프레임 수...

    /* 프레임 수를 1 씩 늘려가며 반복 계산하던 구버전 채널수 산출은 제거했다(#if 0 사장).
     * 현재는 아래처럼 나눗셈 1회로 구한다. */

    TransferabelChannelNumPerOneMilSec = df_MaxNumTransferableChannel / numFramePerChannel;

    return TransferabelChannelNumPerOneMilSec;
}

// 1. 동적 영역 > 1536_uA      ,,,,  경우     slope 8uA
//
//      1) slope 8uA를 사용한다.
//      2) DAC offset 해상도는 2uA를 사용한다.
//      3) 최소 T_uA 값을 offset 값으로 사용한다.
//      4) 각 채널별 C-T값을 0~256사이에서 계산한다.
//
//
//
//
//
// 2. 1536_uA > 동적 영역 > 1024_uA      ,,,,  경우     slope 6uA
//
//      1) slope 6uA를 사용한다.
//      2) DAC offset 해상도는 2uA를 사용한다.
//      3) 최소 T_uA 값을 offset 값으로 사용한다.
//      4) 각 채널별 C-T값을 0~256사이에서 계산한다.
//
//
//
//
//
// 3. 1024_uA > 동적 영역   > 512_uA    ,,,,  경우     slope 4uA
//
//
//      3.1. 최대 자극 전류 값 > 1536   ,,,,  경우
//              1) slope 4uA를 사용한다.
//              2) DAC offset 해상도는 4uA를 사용한다.
//              3) 최소 T_uA 값을 offset 값으로 사용한다.
//              4) 해당 범위에서  C-T 값을 채널별로 계산한다.
//
//      3.2. 1536 > 최대 자극 전류    ,,,,  경우
//                3.2.1 최소 T_uA 값이 512보다 클 경우
//                      1) slope 2uA를 사용한다.
//                      2) DAC offset 해상도는 2uA를 사용한다.
//                      3) offset 값을 512uA으로 설정한다.
//                      4) 해당 범위에서  C-T 값을 채널별로 계산한다.
//                3.2.1 최소 T_uA 값이 512보다 작은 경우
//                      1) slope 4uA를 사용한다.
//                      2) DAC offset 해상도는 2uA를 사용한다.
//                      3) 최소 T_uA 값을 offset 값으로 사용한다.
//                      4) 해당 범위에서  C-T 값을 채널별로 계산한다.
//
//
//
//
//
// 4. 512_uA > 동적 영역       ,,,,  경우      slope 2uA
//
//
//      4.1. 최대 자극 전류 값 > 1536   ,,,,  경우
//              1) slope 4uA를 사용한다.
//              2) DAC offset 해상도는 4uA를 사용한다.
//              3) 최소 T_uA 값을 offset 값으로 사용한다.
//              4) 해당 범위에서  C-T 값을 채널별로 계산한다.
//
//      4.2. 1536 > 최대 자극 전류  > 1024  ,,,,  경우
//              1) slope 2uA를 사용한다.
//              2) DAC offset 해상도는 4uA를 사용한다.
//              3) 최소 T_uA 값을 offset 값으로 사용한다.
//              4) 해당 범위에서  C-T 값을 채널별로 계산한다.
//
//      4.3. 1024 > 최대 자극 전류   ,,,,  경우
//                  4.3.1 최소 T_uA 값이 512보다 클 경우
//                      1) slope 2uA를 사용한다.
//                      2) DAC offset 해상도는 2uA를 사용한다.
//                      3) offset 값을 512uA으로 설정한다.
//                      4) 해당 범위에서  C-T 값을 채널별로 계산한다.
//                  4.3.1 최소 T_uA 값이 512보다 작은 경우
//                      1) slope 2uA를 사용한다.
//                      2) DAC offset 해상도는 2uA를 사용한다.
//                      3) 최소 T_uA 값을 offset 값으로 사용한다.
//                      4) 해당 범위에서  C-T 값을 채널별로 계산한다.

bool tdc_stim_set_range(int pulseWidth, int *T_level_uA, int *C_level_uA, int NumCh, int *T_level_255, int *C_level_255)
{
    int i;
    int maxStimul_uA = 0, minStimul_uA = stimulDAC_D_only_Saturation_uA + offsetDAC_B_Saturation_uA, dynamicRange_uA = 0;
    int DAC_offsetLevel;

    int  tempINT;
    int  deliveryCharge_pico;
    bool MaxDeliveryChargeOver = false;
    bool configurationError    = false;

    // 자극 출력의 동적 영역 계산

    // 최대, 최소 자극 레벨 계산
    for (i = 0; i < NumCh; i++)
    {
        if (C_level_uA[i] > maxStimul_uA)
        {
            maxStimul_uA = C_level_uA[i];
        }

        if (T_level_uA[i] > (-1))
        {
            if (T_level_uA[i] < minStimul_uA)
            {
                minStimul_uA = T_level_uA[i];
            }
        }
    }

    dynamicRange_uA = maxStimul_uA - minStimul_uA;

/* [전하량 초과 검사 - 현재 꺼져 있음. 켜기 전에 아래를 읽을 것]
 *
 * 이 #ifdef 가 보는 Df_MaxDeliveryChargeLimitation 은 정의된 적이 없다.
 * processorDirective.h 에 있는 것은 접미사가 붙은 Df_MaxDeliveryChargeLimitationKKK 이며,
 * git 이력상 접미사 없는 이름은 한 번도 존재한 적이 없다. 즉 오타가 아니라 의도적 비활성이다.
 * (KKK 는 교차 프로젝트 복제 동결 심볼이라 임의 rename 불가)
 *
 * 수식 자체는 정합하다: uA x us = pC, 임계 df_MaxDeliveryCharge_pC = 75000 (75 nC).
 *
 * 다만 지금 이 매크로를 그냥 정의해서 켜면 오히려 더 위험하다.
 * 초과가 잡히면 아래 if(!MaxDeliveryChargeOver) 를 건너뛰어 DAC 파라미터
 * (C_level_255[] / T_level_255[] / stimulationDAC_para) 가 미설정으로 남는데,
 * 그 else 절이 configurationError 를 false 로 두고 함수는 true(성공)를 반환한다.
 * 호출자는 계산이 성공했다고 믿고 미설정 파라미터로 진행하게 된다.
 * -> 켜려면 else 절의 configurationError 처리를 먼저 바로잡아야 한다.
 *
 * 실질 방어선은 자극이 실제로 나가는 매핑 경로에 살아 있다(초과 시 앱에 에러 전송 +
 * 매핑 커맨드 리셋): isd_map_impedance.c · isd_map_ecap.c · isd_map_specific_stim.c
 *
 * 어떤 방식이 맞을지 미정이라 판단 근거를 남긴 채 #if 구조를 존치한다(2026-07-27 은수님 결정).
 * 상세: docs/tasks/cm3/20260723_cm3-full-refactor-2nd/분석-데이터/06_전하량-안전검사-분석.md */
#ifdef Df_MaxDeliveryChargeLimitation

    deliveryCharge_pico = maxStimul_uA * pulseWidth;
    if (deliveryCharge_pico > df_MaxDeliveryCharge_pC)
    {
        MaxDeliveryChargeOver = true;
    }

#else

    MaxDeliveryChargeOver = false;
#endif

    if (!MaxDeliveryChargeOver)
    {
        if (dynamicRange_uA <= stimulDAC_A_only_Saturation_uA)  // 동적 영역  비교  Stimul DAC A
        {
            if ((stimulDAC_A_only_Saturation_uA + offsetDAC_A_Saturation_uA) >= maxStimul_uA)  // Stimul DAC A + Offset DAC A
            {
                stimulationDAC_para.DAC_Slope_register       = Stimulation_DAC_A;
                stimulationDAC_para.DAC_offsetSlope_register = Offset_DAC_A;
            }
            else
            {
                if ((stimulDAC_A_only_Saturation_uA + offsetDAC_B_Saturation_uA) >= maxStimul_uA)  // Stimul DAC A + Offset DAC B
                {
                    stimulationDAC_para.DAC_Slope_register       = Stimulation_DAC_A;
                    stimulationDAC_para.DAC_offsetSlope_register = Offset_DAC_B;
                }
                else
                {
                    if ((stimulDAC_B_only_Saturation_uA + offsetDAC_A_Saturation_uA) >= maxStimul_uA)  // Stimul DAC B + Offset DAC A
                    {
                        stimulationDAC_para.DAC_Slope_register       = Stimulation_DAC_B;
                        stimulationDAC_para.DAC_offsetSlope_register = Offset_DAC_A;
                    }
                    else
                    {
                        if ((stimulDAC_B_only_Saturation_uA + offsetDAC_B_Saturation_uA) >= maxStimul_uA)  // Stimul DAC B + Offset DAC B
                        {
                            stimulationDAC_para.DAC_Slope_register       = Stimulation_DAC_B;
                            stimulationDAC_para.DAC_offsetSlope_register = Offset_DAC_B;
                        }
                        else
                        {
                            if ((stimulDAC_C_only_Saturation_uA + offsetDAC_A_Saturation_uA) >= maxStimul_uA)  // Stimul DAC C + Offset DAC A
                            {
                                stimulationDAC_para.DAC_Slope_register       = Stimulation_DAC_C;
                                stimulationDAC_para.DAC_offsetSlope_register = Offset_DAC_A;
                            }
                            else
                            {
                                if ((stimulDAC_C_only_Saturation_uA + offsetDAC_B_Saturation_uA) >= maxStimul_uA)  // Stimul DAC C + Offset DAC B
                                {
                                    stimulationDAC_para.DAC_Slope_register       = Stimulation_DAC_C;
                                    stimulationDAC_para.DAC_offsetSlope_register = Offset_DAC_B;
                                }
                                else
                                {
                                    if ((stimulDAC_D_only_Saturation_uA + offsetDAC_A_Saturation_uA) >= maxStimul_uA)  // Stimul DAC D + Offset DAC A
                                    {
                                        stimulationDAC_para.DAC_Slope_register       = Stimulation_DAC_D;
                                        stimulationDAC_para.DAC_offsetSlope_register = Offset_DAC_A;
                                    }
                                    else
                                    {
                                        if ((stimulDAC_D_only_Saturation_uA + offsetDAC_B_Saturation_uA) >= maxStimul_uA)  // Stimul DAC D + Offset DAC B
                                        {
                                            stimulationDAC_para.DAC_Slope_register       = Stimulation_DAC_D;
                                            stimulationDAC_para.DAC_offsetSlope_register = Offset_DAC_B;
                                        }
                                        else
                                        {
                                            // 에러
                                            configurationError = true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        else
        {
            if (dynamicRange_uA <= stimulDAC_B_only_Saturation_uA)  // 동적 영역  비교  Stimul DAC B
            {
                if ((stimulDAC_B_only_Saturation_uA + offsetDAC_A_Saturation_uA) >= maxStimul_uA)  // Stimul DAC B + Offset DAC A
                {
                    stimulationDAC_para.DAC_Slope_register       = Stimulation_DAC_B;
                    stimulationDAC_para.DAC_offsetSlope_register = Offset_DAC_A;
                }
                else
                {
                    if ((stimulDAC_B_only_Saturation_uA + offsetDAC_B_Saturation_uA) >= maxStimul_uA)  // Stimul DAC B + Offset DAC B
                    {
                        stimulationDAC_para.DAC_Slope_register       = Stimulation_DAC_B;
                        stimulationDAC_para.DAC_offsetSlope_register = Offset_DAC_B;
                    }
                    else
                    {
                        if ((stimulDAC_C_only_Saturation_uA + offsetDAC_A_Saturation_uA) >= maxStimul_uA)  // Stimul DAC C + Offset DAC A
                        {
                            stimulationDAC_para.DAC_Slope_register       = Stimulation_DAC_C;
                            stimulationDAC_para.DAC_offsetSlope_register = Offset_DAC_A;
                        }
                        else
                        {
                            if ((stimulDAC_C_only_Saturation_uA + offsetDAC_B_Saturation_uA) >= maxStimul_uA)  // Stimul DAC C + Offset DAC B
                            {
                                stimulationDAC_para.DAC_Slope_register       = Stimulation_DAC_C;
                                stimulationDAC_para.DAC_offsetSlope_register = Offset_DAC_B;
                            }
                            else
                            {
                                if ((stimulDAC_D_only_Saturation_uA + offsetDAC_A_Saturation_uA) >= maxStimul_uA)  // Stimul DAC D + Offset DAC A
                                {
                                    stimulationDAC_para.DAC_Slope_register       = Stimulation_DAC_D;
                                    stimulationDAC_para.DAC_offsetSlope_register = Offset_DAC_A;
                                }
                                else
                                {
                                    if ((stimulDAC_D_only_Saturation_uA + offsetDAC_B_Saturation_uA) >= maxStimul_uA)  // Stimul DAC D + Offset DAC B
                                    {
                                        stimulationDAC_para.DAC_Slope_register       = Stimulation_DAC_D;
                                        stimulationDAC_para.DAC_offsetSlope_register = Offset_DAC_B;
                                    }
                                    else
                                    {
                                        // 에러
                                        configurationError = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                if (dynamicRange_uA < stimulDAC_C_only_Saturation_uA)  // 동적 영역  비교  Stimul DAC C
                {
                    if ((stimulDAC_C_only_Saturation_uA + offsetDAC_A_Saturation_uA) >= maxStimul_uA)  // Stimul DAC C + Offset DAC A
                    {
                        stimulationDAC_para.DAC_Slope_register       = Stimulation_DAC_C;
                        stimulationDAC_para.DAC_offsetSlope_register = Offset_DAC_A;
                    }
                    else
                    {
                        if ((stimulDAC_C_only_Saturation_uA + offsetDAC_B_Saturation_uA) >= maxStimul_uA)  // Stimul DAC C + Offset DAC B
                        {
                            stimulationDAC_para.DAC_Slope_register       = Stimulation_DAC_C;
                            stimulationDAC_para.DAC_offsetSlope_register = Offset_DAC_B;
                        }
                        else
                        {
                            if ((stimulDAC_D_only_Saturation_uA + offsetDAC_A_Saturation_uA) >= maxStimul_uA)  // Stimul DAC D + Offset DAC A
                            {
                                stimulationDAC_para.DAC_Slope_register       = Stimulation_DAC_D;
                                stimulationDAC_para.DAC_offsetSlope_register = Offset_DAC_A;
                            }
                            else
                            {
                                if ((stimulDAC_D_only_Saturation_uA + offsetDAC_B_Saturation_uA) >= maxStimul_uA)  // Stimul DAC D + Offset DAC B
                                {
                                    stimulationDAC_para.DAC_Slope_register       = Stimulation_DAC_D;
                                    stimulationDAC_para.DAC_offsetSlope_register = Offset_DAC_B;
                                }
                                else
                                {
                                    // 에러
                                    configurationError = true;
                                }
                            }
                        }
                    }
                }
                else
                {
                    if (dynamicRange_uA < stimulDAC_D_only_Saturation_uA)  // 동적 영역  비교  Stimul DAC D
                    {
                        if ((stimulDAC_D_only_Saturation_uA + offsetDAC_A_Saturation_uA) >= maxStimul_uA)  // Stimul DAC D + Offset DAC A
                        {
                            stimulationDAC_para.DAC_Slope_register       = Stimulation_DAC_D;
                            stimulationDAC_para.DAC_offsetSlope_register = Offset_DAC_A;
                        }
                        else
                        {
                            if ((stimulDAC_D_only_Saturation_uA + offsetDAC_B_Saturation_uA) >= maxStimul_uA)  // Stimul DAC D + Offset DAC B
                            {
                                stimulationDAC_para.DAC_Slope_register       = Stimulation_DAC_D;
                                stimulationDAC_para.DAC_offsetSlope_register = Offset_DAC_B;
                            }
                            else
                            {
                                // 에러
                                configurationError = true;
                            }
                        }
                    }
                    else
                    {
                        // 에러
                        configurationError = true;
                    }
                }
            }
        }

        if (!configurationError)
        {
            switch (stimulationDAC_para.DAC_offsetSlope_register)
            {
                case Offset_DAC_A:
                {
                    if (minStimul_uA >= offsetDAC_A_Saturation_uA)
                    {
                        DAC_offset_uA = offsetDAC_A_Saturation_uA;
                    }
                    else
                    {
                        DAC_offset_uA = minStimul_uA;
                    }

                    tempINT         = DAC_offset_uA * reciprocal_dividing_QI1F15_Offset_DAC_A;
                    DAC_offsetLevel = tempINT >> 15;
                }
                break;

                case Offset_DAC_B:
                {
                    if (minStimul_uA >= offsetDAC_B_Saturation_uA)
                    {
                        DAC_offset_uA = offsetDAC_B_Saturation_uA;
                    }
                    else
                    {
                        DAC_offset_uA = minStimul_uA;
                    }

                    tempINT         = DAC_offset_uA * reciprocal_dividing_QI1F15_Offset_DAC_B;
                    DAC_offsetLevel = tempINT >> 15;
                }
                break;
            }

            stimulationDAC_para.DAC_offsetLevel_register = DAC_offsetLevel;

            /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

            // C-T uA단위를  C-T 255 레벨로 변환.
            switch (stimulationDAC_para.DAC_Slope_register)
            {
                case Stimulation_DAC_A:
                {
                    for (i = 0; i < df_MaxNumOfElectrode; i++)
                    {
                        if (i < NumCh)
                        {
                            tempINT        = (C_level_uA[i] - DAC_offset_uA) * reciprocal_dividing_QI1F15_Stimulation_DAC_A;  //
                            C_level_255[i] = tempINT >> 15;                                                                   //

                            tempINT        = (T_level_uA[i] - DAC_offset_uA) * reciprocal_dividing_QI1F15_Stimulation_DAC_A;  //
                            T_level_255[i] = tempINT >> 15;                                                                   //
                        }
                        else
                        {
                            C_level_255[i] = 0;
                            T_level_255[i] = 0;
                        }
                    }
                }
                break;

                case Stimulation_DAC_B:
                {
                    for (i = 0; i < df_MaxNumOfElectrode; i++)
                    {
                        if (i < NumCh)
                        {
                            tempINT        = (C_level_uA[i] - DAC_offset_uA) * reciprocal_dividing_QI1F15_Stimulation_DAC_B;  //
                            C_level_255[i] = tempINT >> 15;                                                                   //

                            tempINT        = (T_level_uA[i] - DAC_offset_uA) * reciprocal_dividing_QI1F15_Stimulation_DAC_B;  //
                            T_level_255[i] = tempINT >> 15;                                                                   //
                        }
                        else
                        {
                            C_level_255[i] = 0;
                            T_level_255[i] = 0;
                        }
                    }
                }
                break;

                case Stimulation_DAC_C:
                {
                    for (i = 0; i < df_MaxNumOfElectrode; i++)
                    {
                        if (i < NumCh)
                        {
                            tempINT        = (C_level_uA[i] - DAC_offset_uA) * reciprocal_dividing_QI1F15_Stimulation_DAC_C;  //
                            C_level_255[i] = tempINT >> 15;                                                                   //

                            tempINT        = (T_level_uA[i] - DAC_offset_uA) * reciprocal_dividing_QI1F15_Stimulation_DAC_C;  //
                            T_level_255[i] = tempINT >> 15;                                                                   //
                        }
                        else
                        {
                            C_level_255[i] = 0;
                            T_level_255[i] = 0;
                        }
                    }
                }
                break;

                case Stimulation_DAC_D:
                {
                    for (i = 0; i < df_MaxNumOfElectrode; i++)
                    {
                        if (i < NumCh)
                        {
                            tempINT        = (C_level_uA[i] - DAC_offset_uA) * reciprocal_dividing_QI1F15_Stimulation_DAC_D;  //
                            C_level_255[i] = tempINT >> 15;                                                                   //

                            tempINT        = (T_level_uA[i] - DAC_offset_uA) * reciprocal_dividing_QI1F15_Stimulation_DAC_D;  //
                            T_level_255[i] = tempINT >> 15;                                                                   //
                        }
                        else
                        {
                            C_level_255[i] = 0;
                            T_level_255[i] = 0;
                        }
                    }
                }
                break;
            }
        }
    }
    else
    {
        configurationError = false;
        tdc_sys_error_update(en__dataProcessing_ERROR, en__MaxChargeOver_mapdata, __LINE__);
    }

    if (configurationError)
    {
        return false;
    }
    else
    {
        return true;
    }
}

bool tdc_stim_calc_para_and_cfx_share(void)
{
    int  T_level_255[df_MaxNumOfElectrode], C_level_255[df_MaxNumOfElectrode];
    int  frameNumPerOneChannel, transferableChannelNum;
    int  i;
    bool configurationDone = false;

    ST__CFX_CM3_SharedMemory_mapData                    *p_mapdata;
    ST__CFX_CM3_SharedMemory_calculatedStimulPara_byCM3 *p_calculatedStimulPara_byCM3;
    p_mapdata = tdc_shm_get_pointer_current_map_data();

    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);

    frameNumPerOneChannel  = tdc_stim_calc_frame_per_channel(p_mapdata->stimulationPulsePhaseWidth);
    transferableChannelNum = tdc_stim_calc_transferable_channel_num(frameNumPerOneChannel, p_mapdata->numFrequencyBand);

    /* IMPORTANT : NofM 사용 시 프레임 수 고정. NofM 가능 여부를 판별하는 기능이 필요함 */
#if 1
    if (p_mapdata->stimulationStrategy == df_stimulationStrategy_nOFm)
    {
        if (p_mapdata->numFrequencyBand < 16)
        {
            TDC_PRINTF_E("[PARAM] [NofM] NOT AVAILABLE! 'BAND NUM < 16' !! \r\n");
        }
        else
        {
            frameNumPerOneChannel  = 3;  // 채널 당 3 프레임
            transferableChannelNum = 8;  // 1 밀리초에 최대 8 채널 자극
            TDC_PRINTF_W("[PARAM] [NofM] FRAME NUM PER 1 CH  : 3 \r\n");
            TDC_PRINTF_W("[PARAM] [NofM] TRANSFERABLE CH NUM : 8 \r\n");
        }
    }
#endif

    configurationDone = tdc_stim_set_range(
        p_mapdata->stimulationPulsePhaseWidth, p_mapdata->T_level_uA, p_mapdata->C_level_uA, p_mapdata->numFrequencyBand, T_level_255, C_level_255);

    // CFX와 공유
    p_calculatedStimulPara_byCM3 = tdc_shm_get_pointer_calculated_stimul_para_by_cm3();

    p_calculatedStimulPara_byCM3->frameNumPerChannel   = frameNumPerOneChannel;
    p_calculatedStimulPara_byCM3->transferableFrameNum = transferableChannelNum;

    TDC_PRINTF_D("[PARAM] PULSE PHASE WIDTH            : %d \r\n", p_mapdata->stimulationPulsePhaseWidth);
    TDC_PRINTF_D("[PARAM] FRAME NUMBER PER ONE CHANNEL : %d \r\n", frameNumPerOneChannel);
    TDC_PRINTF_D("[PARAM] TRANSFERABLE CHANNEL NUMBER  : %d \r\n", transferableChannelNum);

    cfx_cm3_sharedMemoryAll.CM3_tempValue1 = 0;
    cfx_cm3_sharedMemoryAll.CM3_tempValue2 = 0;

    for (i = 0; i < df_MaxNumOfElectrode; i++)
    {
        p_calculatedStimulPara_byCM3->C_level_255[i] = C_level_255[i];
        p_calculatedStimulPara_byCM3->T_level_255[i] = T_level_255[i];
    }

    return configurationDone;
}
