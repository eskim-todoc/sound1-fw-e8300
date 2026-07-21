#include <hw.h>
#include <tdc_isd_pcm.h>
#include <stdbool.h>

#include "board.h"
#include "internalStimulationChip.h"
#include "tdc_isd.h"
#include "tdc_hal_i2c_cfx.h"
#include "tdc_stim_definitions.h"

#include "tdc_isd_init_fpga.h"
#include "tdc_isd.h"
#include "cfx_cm3_sharedMemory.h"
#include "tdc_stim_para_cal.h"
#include "tdc_sys_error.h"

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
    duration += FPGA_interphaseGapTokenTime; // FPGA_interphaseGapTokenTime : 4
    // 자극파라미터 전송 시간
    duration += FPGA_electrodAndStimulLevelTokenTime; // FPGA_electrodAndStimulLevelTokenTime : 10

    tokenTime          = FPGA_oneChannelDataTokenTime; // FPGA_oneChannelDataTokenTime : 41
    numFramePerChannel = 1;
    while (true)
    {
        if (duration < tokenTime)
        {
            break;
        }
        numFramePerChannel++;
        tokenTime += FPGA_oneChannelDataTokenTime; // FPGA_oneChannelDataTokenTime : 41
    }

    return numFramePerChannel;
}

int tdc_stim_calc_transferable_channel_num(int numFramePerChannel, int usableElectrodNum)
{
    int entireAllChanelFrameNum, TransferabelChannelNumPerOneMilSec, sumTransferabelChannelNum;

    int numFrame;

    // 24나누기 채널당 프레임 수...

#if 0

    entireAllChanelFrameNum=usableElectrodNum*numFramePerChannel; // 모든 채널을 전송하는 데 필요한 시간.

        if(entireAllChanelFrameNum<df_MaxNumTransferableChannel) // 모든 채널을 1msec동안 전송할 수 있다.
        {
            TransferabelChannelNumPerOneMilSec=usableElectrodNum;
        }
        else // 몯든 채널을 1msec 동안 전송이 불가능하다.
        {
            TransferabelChannelNumPerOneMilSec=1;
            sumTransferabelChannelNum=numFramePerChannel;
            while(true)
            {
                if(df_MaxNumTransferableChannel<=sumTransferabelChannelNum)
                {
                    numFrame=TransferabelChannelNumPerOneMilSec*numFramePerChannel;
                    if(numFrame>df_MaxNumTransferableChannel)
                        TransferabelChannelNumPerOneMilSec--;
                    break;


                }
                TransferabelChannelNumPerOneMilSec++;
                sumTransferabelChannelNum+=numFramePerChannel;
            }

        }

#else

    TransferabelChannelNumPerOneMilSec = df_MaxNumTransferableChannel / numFramePerChannel;

#endif

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

    EN_Offset_DAC_Slope      Offset_DAC_Slope;
    EN_Stimulation_DAC_Slope StimulationDAC_Slope;

    int  tempINT;
    int  reciprocal;
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

#ifdef Df_MaxDeliveryChargeLimitation

    deliveryCharge_pico = maxStimul_uA * pulseWidth;
    if (deliveryCharge_pico > df_MaxDeliveryCharge_pC)
        MaxDeliveryChargeOver = true;

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
    p_mapdata = getPointerCurrentMapData();

    changePcmOutputMode(PcmBitStream_Mode_NopStandby);

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

    configurationDone = tdc_stim_set_range(p_mapdata->stimulationPulsePhaseWidth, p_mapdata->T_level_uA, p_mapdata->C_level_uA, p_mapdata->numFrequencyBand, T_level_255, C_level_255);

    // CFX와 공유
    p_calculatedStimulPara_byCM3 = getPointerCalculatedStimulPara_byCM3();

    p_calculatedStimulPara_byCM3->frameNumPerChannel   = frameNumPerOneChannel;
    p_calculatedStimulPara_byCM3->transferableFrameNum = transferableChannelNum;

    TDC_PRINTF_D("[PARAM] PULSE PHASE WIDTH            : %d \r\n", p_mapdata->stimulationPulsePhaseWidth);
    TDC_PRINTF_D("[PARAM] FRAME NUMBER PER ONE CHANNEL : %d \r\n", frameNumPerOneChannel);
    TDC_PRINTF_D("[PARAM] TRANSFERABLE CHANNEL NUMBER  : %d \r\n", transferableChannelNum);

    cfx_cm3_sharedMemoryAll.CM3_tempValue1 = 0;
    cfx_cm3_sharedMemoryAll.CM3_tempValue2 = 0;

    for(i=0;i<df_MaxNumOfElectrode; i++)
    {
        p_calculatedStimulPara_byCM3->C_level_255[i]=C_level_255[i];
        p_calculatedStimulPara_byCM3->T_level_255[i]=T_level_255[i];
    }

    return configurationDone;
}
