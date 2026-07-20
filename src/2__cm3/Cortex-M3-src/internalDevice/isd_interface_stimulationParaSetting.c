#include <stdbool.h>
#include <hw.h>

#include <cfx_cm3_sharedMemory.h>
#include <FPGA.h>
#include <driver_PCM.h>
#include <isd_interface_FPGA.h>
#include <internalStimulationChip.h>
#include <isd_interface.h>
#include <driver_i2c_for_ISD.h>
#include <definitionsForAlgorithm.h>
#include <isd_interface_init_FPGA.h>
#include <isd_interface.h>
#include <indicatorByStimul.h>
#include <stimulationParaCal.h>
#include <electrodeMapping.h>
#include <error.h>

static bool stimulationParameterSettingDone = false;

void clear_sitmulationParaSettingDone(void)
{
    stimulationParameterSettingDone = false;
}

void done_setting_StimulPara_variable(void)
{
    stimulationParameterSettingDone = true;
}

bool isStimulParaIsSettingDone(void)
{
    return stimulationParameterSettingDone;
}

bool settingStimulPara_monoPolarMode(bool isdControlStateChagedFlag)
{
    static const ST_STIUL_DAC_REGISTER_VALUE      *p_stimulDAC_setting;
    static const ST__CFX_CM3_SharedMemory_mapData *p_mapdata;

    static int flowControlCounter = 0;
    static int tempCounter        = 0;
    static int sent_stimulConfig  = 0;
    static int writenBacktelRegisterValue;

    int  i;
    int  bitReverse;
    int  w_FPGA_registerValue;
    int  r_FPGA_registerValue;
    int  w_isd_registerValue;
    int  r_isd_registerValue;
    int  fifoCounter;
    int  compare;
    int  pcm_index = 0;
    int  backtelBuff[64];
    int  nop = 0;
    bool isdSettingError;
    bool stimulationConfigError = false;
    bool FPGA_FIFO_empty;
    bool FPGA_error;

    tempCounter++;

    if (isdControlStateChagedFlag)
    {
        flowControlCounter = 0;
    }

    pcm_index = 0;

    switch (flowControlCounter)
    {
        // FPGA에 에러 발생 여부 체크 후, 펄스 폭 최소 설정, 백텔 레지스터 8비트 모드 설정
        // PCM 사용
        case 0:
        {
            check_FPGA_PCM_Error(&FPGA_error);

            if (FPGA_error)
            {
                // FPGA 에러 발생, FPGA 초기화
                change_isd_state(en__isdStatus_PowerIC_OK);
            }
            else
            {
                chang_PulseWidth_minimum(pcm_index++);

                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);

                writenBacktelRegisterValue = change_8BitBacktel_mode(pcm_index++);

                for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
                {
                    fillSepcificCommndBuffer(i, pcm_Mold_NopStandby);
                }

                changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
                changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
            }
        }
        break;

        // 마지막 PCM 사용 후 약 4ms 뒤
        // 펄스 폭 최소 설정 검증, 백텔 레지스터 8비트 모드 검증 후 백텔 FIFO 클리어
        case 4:
        {
            if (read_FPGA_PulseWidth(&r_FPGA_registerValue))
            {
                if (r_FPGA_registerValue == FPGA_pulsePhaseWidth_minimum)
                {
                    // 펄스 폭 설정이 정상적으로 완료되었기 때문에 펄스폭 값을 업데이트한다.
                    upadte_fpga_pulsePhaseWidth_written_Value(r_FPGA_registerValue);
                }
                else
                {
                    // 펄스폭 설정 실패, FPGA 초기화
                    errorCodeUpdate(en__FPGA_CONFIGUARATION_ERROR, en_PulseWidthDifferent, __LINE__);
                    change_isd_state(en__isdStatus_PowerIC_OK);

                    stimulationConfigError = true;
                }
            }

            if (read_FPGA_backtelConfig(&r_FPGA_registerValue))
            {
                if (writenBacktelRegisterValue == r_FPGA_registerValue)
                {
                    // PCM 으로 설정한 FPGA 백텔 레지스터가 원하는 값으로 쓰여졌는지확인 후 업데이트
                    upadte_fpga_backtelConfig_written_Value(writenBacktelRegisterValue);

                    if (write_FPGA_clear_FIFO())  // 백텔 FIFO 지우기
                    {
                        // FPGA 상태를 읽어 본다.
                        if (check_FPGA_FIFO_empty(&FPGA_FIFO_empty))  // 지워 졌는지 확인.
                        {
                            if (!FPGA_FIFO_empty)
                            {
                                errorCodeUpdate(en__FPGA_CONFIGUARATION_ERROR, en_FIFO_NotCleared, __LINE__);
                                change_isd_state(en__isdStatus_PowerIC_OK);  //

                                stimulationConfigError = true;
                            }
                        }
                    }
                }
                else
                {
                    // 백텔 레지스터 설정 오류
                    errorCodeUpdate(en__FPGA_CONFIGUARATION_ERROR, en_RegisterConfigError_byPCM, __LINE__);
                    change_isd_state(en__isdStatus_PowerIC_OK);

                    stimulationConfigError = true;
                }
            }
        }
        break;

        // 마지막 PCM 사용 후 약 5ms 뒤
        case 5:  // 자극 파라미터 설정 - 쓰기
        {
            p_stimulDAC_setting = readStimulDAC_RegisterValue();
            p_mapdata           = getPointerCurrentMapData();

            // ISD - DAC Offset "쓰기" ('h05: slope offset register)
            w_isd_registerValue = ISD_registerAddr_DAC_offsetValue;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
            w_isd_registerValue = w_isd_registerValue << 8;
            w_isd_registerValue = w_isd_registerValue | p_stimulDAC_setting->DAC_offsetLevel_register;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

            // ISD - 자극 파라미터 설정 "쓰기" ('h06: stimulation configuration register)
            w_isd_registerValue = ISD_registerAddr_StimulationConfig;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;

            // 7번 비트 STIM_REF_HW_CTRL_DISABLE (리셋 값: 0b1)
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | 1;

            // 6번 비트 SLOPE_OFFSET_RESOULUTION
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | p_stimulDAC_setting->DAC_offsetSlope_register;

            // 5~4번 비트 SLOPE_SELECT
            w_isd_registerValue = w_isd_registerValue << 2;
            w_isd_registerValue = w_isd_registerValue | p_stimulDAC_setting->DAC_Slope_register;

            // 3~2번 비트 MP_CONFIG (모노폴라 출력 모드에서 기준전극)
            w_isd_registerValue = w_isd_registerValue << 2;

            // 리셋값이 3이며, 바이폴라, 공통접지, 동시 자극의 경우는 접지를 끊는다.
            switch (p_mapdata->stimulationMode)
            {
                case en__monopolr_body:
                    w_isd_registerValue = w_isd_registerValue | en__monopolr_body;
                    break;

                case en__monopolr_rod:
                    w_isd_registerValue = w_isd_registerValue | en__monopolr_rod;
                    break;

                case en__monopolr_BothRodBody:
                    w_isd_registerValue = w_isd_registerValue | en__monopolr_BothRodBody;
                    break;

                default:
                    w_isd_registerValue = w_isd_registerValue | en__referenceNA;
                    break;
            }

            // 1~0번 STIM_MODE (자극 출력 모드: 모노폴라, 바이폴라, 공통접지, 동시모사)
            w_isd_registerValue = w_isd_registerValue << 2;

            switch (p_mapdata->stimulationMode)
            {
                case en__monopolr_body:
                case en__monopolr_rod:
                case en__monopolr_BothRodBody:
                    w_isd_registerValue = w_isd_registerValue | 0;  // monopolar
                    break;

                case en__bipolar:
                    w_isd_registerValue = w_isd_registerValue | 1;  // bipolar
                    break;

                case en__commonground:
                    w_isd_registerValue = w_isd_registerValue | 2;  // common ground
                    break;

                case en__semi_simultaneously:
                    w_isd_registerValue = w_isd_registerValue | 3;  // bipolar + monopolar
                    break;
            }

            // 만들어 놓은 패킷을 0xFF로 비트연산 해서 사용하는게 아니고, 나중에 비교하는데 사용한다.
            sent_stimulConfig   = w_isd_registerValue & 0xFF;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

            /*
              /=================================================================================/
              /      ** PCM protocol for ISD data packet **                                     /
              /=================================================================================/
              /                                                                                 /
              /      +-+ <============================= 3 bits : header                         /
              /      |-|                                                                        /
              /      |-|   +-----+ <=================== 7 bits : address                        /
              /      |-|   |-----|                                                              /
              /      |-|   |-----|   +------+ <======== 8 bits : data                           /
              /      |-|   |-----|   |------|                                                   /
              /      jih g fedcba9 8 76543210                                                   /
              /          |     |                                                                /
              /          |     + <===================== 1 bit  : r/w                            /
              /          |                                                                      /
              /          + <=========================== 1 bit  : packet mode                    /
              /                                                  (parameter/configuration)      /
              /                                                                                 /
              /=================================================================================/
              /                                                                                 /
              /       j: bit 19          i: bit 18          h: bit 18          g: bit 18        /
              /                                                                                 /
              /=================================================================================/
             */

#ifndef DisalbedBackTel
            // ISD - DAC Offset "읽기" ('h05: slope offset register)
            w_isd_registerValue = ISD_registerAddr_DAC_offsetValue;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_readRegister;
            w_isd_registerValue = w_isd_registerValue << 8;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

            fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
            fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
            fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);

            // ISD - 자극 파라미터 설정 "읽기" ('h06: stimulation configuration register)
            w_isd_registerValue = ISD_registerAddr_StimulationConfig;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_readRegister;
            w_isd_registerValue = w_isd_registerValue << 8;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

            for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
            {
                fillSepcificCommndBuffer(i, pcm_Mold_NopBacktel);
            }
#else
            for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
            {
                fillSepcificCommndBuffer(i, pcm_Mold_NopStandby);
            }
#endif

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        case 9:  // 자극 파라미터 설정 확인
        {
#ifndef DisalbedBackTel
            isdSettingError = false;

            if (read_FPGA_backtel_FIFO(backtelBuff, 2))
            {
                // DAC offset 값 확인
                if (backtelBuff[0] != p_stimulDAC_setting->DAC_offsetLevel_register)
                {
                    isdSettingError = true;
                }

                // 자극 설정값 확인
                if (backtelBuff[1] != sent_stimulConfig)
                {
                    isdSettingError = true;
                }
            }

            // 오프셋 값과 자극 파라미터 설정에 오류가 없으면 정상
            if (isdSettingError)
            {
                change_isd_state(en__isdStatus_FPGA_Ok);  // 내부기 전송 파워 설정 부터 다시.
                stimulationConfigError = true;
            }
#endif
        }
        break;

        // 마지막 PCM 사용 후 약 5ms 뒤, 자극용 파라미터 설정 시작(펄스 폭 설정)
        case 10:
        {
            // 펄스 폭을 맵 데이터에 해당하는 값으로 다시 설정
            chang_PulseWidth(pcm_index++, p_mapdata->stimulationPulsePhaseWidth);

            disable_Backtel(pcm_index++);

            for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
            {
                fillSepcificCommndBuffer(i, pcm_Mold_NopStandby);
            }

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        // 마지막 PCM 사용 후 약 4ms 뒤, 펄스 폭 설정 검증 및 백텔 FIFO 클리어
        case 14:
        {
            if (read_FPGA_PulseWidth(&r_FPGA_registerValue))
            {
                if (r_FPGA_registerValue == p_mapdata->stimulationPulsePhaseWidth)
                {
                    upadte_fpga_pulsePhaseWidth_written_Value(p_mapdata->stimulationPulsePhaseWidth);  //
                    // 자극 파라미터 설정 완료
                    if (write_FPGA_clear_FIFO())
                    {
                        TDC_PRINTF_V("[STIMULATION] DONE, STIMULATION PARAMETER SETTING \r\n");
                        done_setting_StimulPara_variable();
                    }
                    else
                    {
                        change_isd_state(en__isdStatus_PowerIC_OK);
                    }
                }
                else
                {
                    if (read_FPGA_systemError_Flag(&r_FPGA_registerValue))
                    {
                        change_isd_state(en__isdStatus_PowerIC_OK);
                    }

                    change_isd_state(en__isdStatus_PowerIC_OK);
                    stimulationConfigError = true;
                }
            }
        }
        break;

        default:
            break;
    }

    flowControlCounter++;

    return stimulationConfigError;
}

bool settingStimulPara_biPolarMode(bool isdControlStateChagedFlag)
{
    int i;
    int w_FPGA_registerValue;
    int bitReverse;
    int r_FPGA_registerValue;
    int w_isd_registerValue;
    int r_isd_registerValue;
    int fifoCounter;
    int compare;
    int pcm_index = 0;

    int                                            en__bipolarRefereceElectorodeIndex = 0;
    static const ST__CFX_CM3_SharedMemory_mapData *p_mapdata;
    static const ST_STIUL_DAC_REGISTER_VALUE      *p_stimulDAC_setting;

    static int flowControlCounter = 0;

    static int tempCounter            = 0;
    static int sent_stimulConfig      = 0;
    static int referencElectrod_index = 0;
    static int backtelBuff[df_MaxNumOfElectrode];
    static int bipolarReferenceElectrodeNum[df_MaxNumOfElectrode];

    int nop = 0;
    int en__bipolarRefer_EelectrodeNum;
    int stimulElectrodeNum;

    bool isdSettingError        = false;
    bool stimulationConfigError = false;
    bool FPGA_FIFO_empty;

    tempCounter++;
    int tempA, tempB, tempC, tempD;

    if (isdControlStateChagedFlag)
    {
        flowControlCounter     = 0;
        referencElectrod_index = 0;

        clearIsdControlStateChagedFlag();
    }

    pcm_index = 0;

    switch (flowControlCounter)
    {
        // 펄스 폭 최소 설정, 백텔 레지스터 8비트 모드 설정
        // PCM 사용
        case 0:
        {
            chang_PulseWidth_minimum(pcm_index++);  // 펄스 폭 0으로 설정
            upadte_fpga_pulsePhaseWidth_written_Value(FPGA_pulsePhaseWidth_minimum);

            change_8BitBacktel_mode(pcm_index++);  // 백텔 레지스터 8 비트 설정 (백텔 하드웨어 회로 ON 포함)

            fill_pcmBuff_lastSimulationOut(&pcm_index);  // 이전 자극 파라미터를 출력하기 위한 임의의 값 출력

            for (; pcm_index < df_MaxNumTransferableChannel;)
            {
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            }

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        // 마지막 PCM 사용 후 약 5ms 뒤
        // 바이폴라 기준 전극 버퍼 구성, 백텔 FIFO 클리어
        case 5:
        {
            p_stimulDAC_setting = readStimulDAC_RegisterValue();
            p_mapdata           = getPointerCurrentMapData();

            // 바이폴라 기준 전극 버퍼 구성
            for (i = 0; i < df_MaxNumOfElectrode; i++)
            {
                // 기본 값으로 바이폴라 레퍼런스 전극 번호를 31로 일괄 초기화한다.
                bipolarReferenceElectrodeNum[i] = unusedReferenceElectrode_DummyNum;
            }

            TDC_PRINTF_I("[PARA] TOTAL NUM FREQ BAND : %d \r\n", p_mapdata->numFrequencyBand);

            for (i = 0; i < p_mapdata->numFrequencyBand; i++)
            {
#if 0
                if (p_mapdata->usableStimulationElectrodIndex[i] != 99)
                {
                    stimulElectrodeNum = p_mapdata->usableStimulationElectrodIndex[i] - 1;
                    if (p_mapdata->usableReferenceElectrodIndex[i] != 99)
                        bipolarReferenceElectrodeNum[stimulElectrodeNum] = p_mapdata->usableReferenceElectrodIndex[i] - 1;
                }
#else
                bipolarReferenceElectrodeNum[electrodeMap[p_mapdata->usableStimulationElectrodIndex[i] - 1]] =  // 코드가 길어서 강제 줄 바꿈
                    electrodeMap[p_mapdata->usableReferenceElectrodIndex[i] - 1];

                TDC_PRINTF_I("[PARA] (1 BASE), BAND : %2d, STIM ELEC NUM : %2d (PCB : %2d), REF ELEC NUM : %2d (PCB : %2d) \r\n",  //
                          i + 1,
                          p_mapdata->usableStimulationElectrodIndex[i],
                          electrodeMap[p_mapdata->usableStimulationElectrodIndex[i] - 1] + 1,
                          p_mapdata->usableReferenceElectrodIndex[i],
                          electrodeMap[p_mapdata->usableReferenceElectrodIndex[i] - 1] + 1);
#endif
            }

            for (i = p_mapdata->numFrequencyBand; i < df_MaxNumOfElectrode; i++)
            {
                TDC_PRINTF_I("[PARA] (1 BASE), BAND : %2d, STIM ELEC NUM : %2d (PCB : XX), REF ELEC NUM : %2d (PCB : XX) \r\n",  //
                          i + 1,
                          p_mapdata->usableStimulationElectrodIndex[i],
                          p_mapdata->usableReferenceElectrodIndex[i]);
            }

            if (write_FPGA_clear_FIFO())  // 백텔 FIFO 클리어
            {
                if (check_FPGA_FIFO_empty(&FPGA_FIFO_empty))  // 지워 졌는지 확인.
                {
                    if (!FPGA_FIFO_empty)
                    {
                        change_isd_state(en__isdStatus_PowerIC_OK);  //
                        stimulationConfigError = true;
                    }
                }
                else
                {
                    change_isd_state(en__isdStatus_PowerIC_OK);  //
                    stimulationConfigError = true;
                }
            }
            else
            {
                change_isd_state(en__isdStatus_PowerIC_OK);  //
                stimulationConfigError = true;
            }
        }
        break;

        // 마지막 PCM 사용 후 약 6ms 뒤
        case 6:
        {
            // 내부기 칩 레지스터에서 Bipolar 기준전극 FIFO 지우기
            w_isd_registerValue = ISD_registerAddr_cipherDataStatus;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
            w_isd_registerValue = w_isd_registerValue << 8;

            w_isd_registerValue = w_isd_registerValue | 0x80;  // CHIP_ID_FIFO_RDDATA_INDEX에 아무값이나 쓰면 FIFO가 지워진다.
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

            for (; pcm_index < df_MaxNumTransferableChannel;)
            {
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            }

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        // 마지막 PCM 사용 후 약 2ms 뒤
        // 바이폴라, 0~23번 자극 채널에 대응하는 기준 전극 번호 설정
        case 8:
        {
            for (i = 0; i < df_MaxNumTransferableChannel; i++)  // 0~23번 자극채널에 대응하는 기준 전극 번호
            {
                w_isd_registerValue = ISD_registerAddr_en__bipolar_referenceElectroldIndex;
                w_isd_registerValue = w_isd_registerValue << 1;
                w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
                w_isd_registerValue = w_isd_registerValue << 8;

                // 바이폴러 레퍼런스 전극 번호는 비트 [4:0] 범위, 범위 초과한 값 입력시 FIFO 클리어 발생 → 이후 백텔 카운트 0 에러 발생할 수 있음
                w_isd_registerValue = w_isd_registerValue | (0x1F & bipolarReferenceElectrodeNum[i]);
                w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

                fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
            }

            for (; pcm_index < df_MaxNumTransferableChannel;)
            {
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            }

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        // 마지막 PCM 사용 후 약 2ms 뒤
        // 바이폴라, 24~32번 자극 채널에 대응하는 기준 전극 번호 설정
        case 10:
        {
            for (i = 24; i < 32; i++)  // 24~32번 자극채널에 대응하는 기준 전극 번호
            {
                w_isd_registerValue = ISD_registerAddr_en__bipolar_referenceElectroldIndex;
                w_isd_registerValue = w_isd_registerValue << 1;
                w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
                w_isd_registerValue = w_isd_registerValue << 8;

                // 바이폴러 레퍼런스 전극 번호는 비트 [4:0] 범위, 범위 초과한 값 입력시 FIFO 클리어 발생 → 이후 백텔 카운트 0 에러 발생할 수 있음
                w_isd_registerValue = w_isd_registerValue | (0x1F & bipolarReferenceElectrodeNum[i]);
                w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

                fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
            }

            for (; pcm_index < df_MaxNumTransferableChannel;)
            {
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            }

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

#ifndef DisalbedBackTel
        // 마지막 PCM 사용 후 약 4ms 뒤
        // PCM 출력 - 바이폴라 기준 전극 값 확인 - FIFO 값 확인 0~5번 전극
        case 14:
        {
            w_isd_registerValue = ISD_registerAddr_en__bipolar_referenceElectroldIndex;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_readRegister;
            w_isd_registerValue = w_isd_registerValue << 8;

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            for (i = 0; i < 6; i++)
            {
                fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
            }

            for (; pcm_index < df_MaxNumTransferableChannel;)
            {
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            }

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        // 마지막 PCM 사용 후 약 2ms 뒤
        // PCM 출력 - 바이폴라 기준 전극 값 확인 - FIFO 값 확인 6~11번 전극
        case 16:
        {
            w_isd_registerValue = ISD_registerAddr_en__bipolar_referenceElectroldIndex;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_readRegister;
            w_isd_registerValue = w_isd_registerValue << 8;

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            for (i = 0; i < 6; i++)
            {
                fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
            }

            for (; pcm_index < df_MaxNumTransferableChannel;)
            {
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            }

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        // 마지막 PCM 사용 후 약 2ms 뒤
        // PCM 출력 - 바이폴라 기준 전극 값 확인 - FIFO 값 확인 12~17번 전극
        case 18:
        {
            w_isd_registerValue = ISD_registerAddr_en__bipolar_referenceElectroldIndex;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_readRegister;
            w_isd_registerValue = w_isd_registerValue << 8;

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            for (i = 0; i < 6; i++)
            {
                fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
            }

            for (; pcm_index < df_MaxNumTransferableChannel;)
            {
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            }

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        // 마지막 PCM 사용 후 약 2ms 뒤
        // PCM 출력 - 바이폴라 기준 전극 값 확인 - FIFO 값 확인 18~23번 전극
        case 20:
        {
            w_isd_registerValue = ISD_registerAddr_en__bipolar_referenceElectroldIndex;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_readRegister;
            w_isd_registerValue = w_isd_registerValue << 8;

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            for (i = 0; i < 6; i++)
            {
                fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
            }

            for (; pcm_index < df_MaxNumTransferableChannel;)
            {
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            }

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        // 마지막 PCM 사용 후 약 2ms 뒤
        // PCM 출력 - 바이폴라 기준 전극 값 확인 - FIFO 값 확인 24~29번 전극
        case 22:
        {
            w_isd_registerValue = ISD_registerAddr_en__bipolar_referenceElectroldIndex;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_readRegister;
            w_isd_registerValue = w_isd_registerValue << 8;

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            for (i = 0; i < 6; i++)
            {
                fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
            }

            for (; pcm_index < df_MaxNumTransferableChannel;)
            {
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            }

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        // 마지막 PCM 사용 후 약 2ms 뒤
        // PCM 출력 - 바이폴라 기준 전극 값 확인 - FIFO 값 확인 30~31번 전극
        case 24:
        {
            w_isd_registerValue = ISD_registerAddr_en__bipolar_referenceElectroldIndex;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_readRegister;
            w_isd_registerValue = w_isd_registerValue << 8;

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            for (i = 0; i < 2; i++)
            {
                fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
            }

            for (; pcm_index < df_MaxNumTransferableChannel;)
            {
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            }

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        // 마지막 PCM 사용 후 약 5ms 뒤
        // I2C 읽기 - 바이폴라 기준 전극 값 확인 - FIFO 값 확인
        case 29:
        {
            for (i = 0; i < df_MaxNumOfElectrode; i++)
            {
                backtelBuff[i] = 0;
            }

            // nop=(cfx_i2c_read(df_I2C_ADDR_FPGA_FIFO_counter, &r_FPGA_registerValue, 1));
            if (read_FPGA_FIFO_counter(&r_FPGA_registerValue))
            {
                if (r_FPGA_registerValue != 0)
                {
                    TDC_PRINTF_I("[PARA] BIPOLAR REF READ BACKTEL COUNT : %d \r\n", r_FPGA_registerValue);

                    // if (cfx_i2c_read(df_I2C_ADDR_FPGA_BackTel, backtelBuff, df_MaxNumOfElectrode))
                    if (read_FPGA_backtel_FIFO(backtelBuff, r_FPGA_registerValue))
                    {
                        for (i = 0; i < df_MaxNumOfElectrode - 1; i++)
                        {
                            TDC_PRINTF_I("[PARA] CHIP, (0 BASE), INDEX MAP (PCB) [%2d] : (PCB) %d \r\n",  //
                                      i,
                                      ((0x1F) & (backtelBuff[i])));

                            // if (electrodeMap[bipolarReferenceElectrodeNum[i]] != ((0x1F) & (backtelBuff[i])))
                            if (bipolarReferenceElectrodeNum[i] != ((0x1F) & (backtelBuff[i])))
                            {
                                TDC_PRINTF_E("[PARA] PRE-SETTING BIPOLAR REF INDEX : %d, READ REF INDEX : %d \r\n",  //
                                          bipolarReferenceElectrodeNum[i],
                                          (0x1F) & (backtelBuff[i]));
                                change_isd_state(en__isdStatus_FPGA_Ok);  // 내부기 전송 파워 설정 부터 다시.
                                stimulationConfigError = true;
                            }
                        }

                        TDC_PRINTF_I("[PARA] CHIP, (0 BASE), INDEX MAP (PCB) [%2d] : (PCB) %d \r\n",  //
                                  df_MaxNumOfElectrode - 1,
                                  ((0x1F) & (backtelBuff[df_MaxNumOfElectrode - 1])));
                    }
                }
                else
                {
                    // 백텔이 안들어 왔다.
                    TDC_PRINTF_W("[PARA] BIPOLAR REF READ BACKTEL COUNT IS 0 \r\n");
                    change_isd_state(en__isdStatus_FPGA_Ok);  // 내부기 전송 파워 설정 부터 다시.
                    stimulationConfigError = true;
                }
            }
            else
            {
                TDC_PRINTF_E("[PARA] I2C FAILED TO READ FIFO COUNT FOR READING BIPOLAR REF \r\n");
                change_isd_state(en__isdStatus_PowerIC_OK);  // FPGA 리셋
                stimulationConfigError = true;
            }
        }
        break;
#endif

        // 마지막 PCM 사용 후 약 6ms 뒤
        case 30:  // 자극 파라미터 설정 - 쓰기
        {
            // 펄스 폭 0으로 설정
            chang_PulseWidth_minimum(pcm_index++);
            upadte_fpga_pulsePhaseWidth_written_Value(FPGA_pulsePhaseWidth_minimum);  // 실제로는 Fpag 값을 읽어보고 업데이트 해야 된다.
            // 8bit backter 설정
            change_8BitBacktel_mode(pcm_index++);

            // ISD  - DAC Offset 쓰기
            w_isd_registerValue = ISD_registerAddr_DAC_offsetValue;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
            w_isd_registerValue = w_isd_registerValue << 8;

            w_isd_registerValue = w_isd_registerValue | p_stimulDAC_setting->DAC_offsetLevel_register;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
            // for(i=0; i<mapData->frameNumPerChannel*2; i++)
            //   fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            // fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            // fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);

            // ISD  - 자극 파라미터 설정  쓰기
            w_isd_registerValue = ISD_registerAddr_StimulationConfig;

            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;

            // 1, 7번비트
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | 1;

            // offsetResolution 6번 비트
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | p_stimulDAC_setting->DAC_offsetSlope_register;

            // DAC slope    4,5번 비트
            w_isd_registerValue = w_isd_registerValue << 2;
            w_isd_registerValue = w_isd_registerValue | p_stimulDAC_setting->DAC_Slope_register;

            // 모노폴라 출력 모드에서 기준전극 모드 2,3번 비트
            w_isd_registerValue = w_isd_registerValue << 2;
            switch (p_mapdata->stimulationMode)
            {
                case en__monopolr_body:
                    w_isd_registerValue = w_isd_registerValue | en__monopolr_body;
                    break;

                case en__monopolr_rod:
                    w_isd_registerValue = w_isd_registerValue | en__monopolr_rod;
                    break;

                case en__monopolr_BothRodBody:
                    w_isd_registerValue = w_isd_registerValue | en__monopolr_BothRodBody;
                    break;

                default:
                    w_isd_registerValue = w_isd_registerValue | en__referenceNA;  // // 리셋값이 3이며, 바이폴라, 공통접지, 동시 자극의 경우는 접지를 끊는다.
                    break;
            }

            // 자극 출력 모드 0,1번 비트 (모노폴라, 바이폴라, 공통접지, 동시모사)
            w_isd_registerValue = w_isd_registerValue << 2;
            switch (p_mapdata->stimulationMode)
            {
                case en__monopolr_body:
                case en__monopolr_rod:
                case en__monopolr_BothRodBody:
                    w_isd_registerValue = w_isd_registerValue | 0;  // mono polar
                    break;

                case en__bipolar:
                    w_isd_registerValue = w_isd_registerValue | 1;  // en__bipolar
                    break;

                case en__commonground:
                    w_isd_registerValue = w_isd_registerValue | 2;  // common ground
                    break;

                case en__semi_simultaneously:
                    w_isd_registerValue = w_isd_registerValue | 3;
                    break;
            }

            sent_stimulConfig   = w_isd_registerValue & 0xFF;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

#ifndef DisalbedBackTel
            // ISD  - DAC Offset 읽기
            w_isd_registerValue = ISD_registerAddr_DAC_offsetValue;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_readRegister;
            w_isd_registerValue = w_isd_registerValue << 8;

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
            // for(i=0; i<mapData->frameNumPerChannel*2; i++)
            //   fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
            fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
            fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
            fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);

            // ISD  - 자극 파라미터 설정  읽기
            w_isd_registerValue = ISD_registerAddr_StimulationConfig;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_readRegister;
            w_isd_registerValue = w_isd_registerValue << 8;

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

            for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
            {
                fillSepcificCommndBuffer(i, pcm_Mold_NopBacktel);
            }
#else

            for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
            {
                fillSepcificCommndBuffer(i, pcm_Mold_NopStandby);
            }

#endif

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        // 마지막 PCM 사용 후 약 4ms 뒤
        case 34:  // 자극 파라미터 설정 확인
        {
#ifndef DisalbedBackTel
            isdSettingError = false;

            if (read_FPGA_FIFO_counter(&r_FPGA_registerValue))
            {
                if (r_FPGA_registerValue == 2)
                {
                    if (read_FPGA_backtel_FIFO(backtelBuff, 2))
                    {
                        // DAC offset 값 확인
                        if (backtelBuff[0] != p_stimulDAC_setting->DAC_offsetLevel_register)
                        {
                            isdSettingError = true;

                            TDC_PRINTF_W("[PARA] SETTING ERROR, SETTING OFFSET DAC LEVEL : %d, READ OFFSET DAC LEVEL : %d \r\n",  //
                                      p_stimulDAC_setting->DAC_offsetLevel_register,
                                      backtelBuff[0]);
                        }

                        // 자극 설정값 확인
                        if (backtelBuff[1] != sent_stimulConfig)
                        {
                            isdSettingError = true;  //

                            TDC_PRINTF_W("[PARA] SETTING ERROR, SETTING STIMUL CONFIG : %d, READ STIMUL CONFIG : %d \r\n",  //
                                      sent_stimulConfig,
                                      backtelBuff[1]);
                        }
                    }

                    // 오프셋 값과 자극 파라미터 설정에 오류가 없으면 정상
                    if (isdSettingError)
                    {
                        change_isd_state(en__isdStatus_FPGA_Ok);  // 내부기 전송 파워 설정 부터 다시.
                        stimulationConfigError = true;
                    }
                }
                else
                {
                    TDC_PRINTF_W("[PARA] OFFSET DAC LEVEL, STIM CONFIG READ BACKTEL COOUNT IS NOT 2, (COUNT: %d) \r\n", r_FPGA_registerValue);
                    change_isd_state(en__isdStatus_FPGA_Ok);  // 내부기 전송 파워 설정 부터 다시.
                    stimulationConfigError = true;
                }
            }
            else
            {
                TDC_PRINTF_W("[PARA] OFFSET DAC LEVEL, STIM CONFIG READ BACKTEL COOUNT IS 0 \r\n");
                change_isd_state(en__isdStatus_FPGA_Ok);  // 내부기 전송 파워 설정 부터 다시.
                stimulationConfigError = true;
            }
#endif
        }
        break;

        // 마지막 PCM 사용 후 약 5ms 뒤
        ////////////////////////////////////////////////////////////////////////
        case 35:  // 펄스 폭 설정
        {
            // 펄스 폭 .. 설정 PCM 출력으로  FPGA에 전달
            chang_PulseWidth(pcm_index++, p_mapdata->stimulationPulsePhaseWidth);

            for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
            {
                fillSepcificCommndBuffer(i, pcm_Mold_NopStandby);
            }

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        // 마지막 PCM 사용 후 약 4ms 뒤
        case 39:
        {
            if (read_FPGA_PulseWidth(&r_FPGA_registerValue))
            {
                if (r_FPGA_registerValue == p_mapdata->stimulationPulsePhaseWidth)
                {
                    upadte_fpga_pulsePhaseWidth_written_Value(p_mapdata->stimulationPulsePhaseWidth);  // 실제로는 Fpag 값을 읽어보고 업데이트 해야 된다.
                    // 자극 파라미터 설정 완료
                    // update_isd_Link_is_Connected();

                    if (write_FPGA_clear_FIFO())
                    {
                        TDC_PRINTF_V("[STIMULATION] DONE, STIMULATION PARAMETER SETTING \r\n");
                        done_setting_StimulPara_variable();
                    }
                    else
                    {
                        TDC_PRINTF_E("[PARA] I2C FAILED TO CLEAR FPGA FIFO \r\n");
                        change_isd_state(en__isdStatus_PowerIC_OK);
                    }
                }
                else
                {
                    TDC_PRINTF_E("[PARA] FAILED TO SET FPGA PULSE WIDTH \r\n");

                    if (read_FPGA_systemError_Flag(&r_FPGA_registerValue))
                    {
                        change_isd_state(en__isdStatus_PowerIC_OK);
                    }

                    change_isd_state(en__isdStatus_PowerIC_OK);
                    stimulationConfigError = true;
                }
            }
            else
            {
                TDC_PRINTF_E("[PARA] I2C FAILED TO READ FPGA PULSE WIDTH \r\n");
                change_isd_state(en__isdStatus_FPGA_Ok);  // 내부기 전송 파워 설정 부터 다시.
            }
        }
        break;

        default:
            break;
    }

    flowControlCounter++;

    // 라이브 모드 시작 후 설정 과정에서 에러가 발생하면 다시 처음부터 시작해야 하는데
    // 실제 코드는 마지막 flowControlCounter에서 이어서 진행하는 버그가 있음
    // 그래서 에러 발생 시 카운터를 직접 0으로 초기화 하도록 수정하였음 by 김은수 2026.03.05
    if (stimulationConfigError)
    {
        flowControlCounter = 0;
    }

    return stimulationConfigError;
}

bool stimulationSetting(bool startTrigger)
{
    ST__CFX_CM3_SharedMemory_mapData *p_mapdata;
    bool error;

    error     = false;
    p_mapdata = getPointerCurrentMapData();

    if (startTrigger)
    {
        TDC_PRINTF_V("[STIMULATION] TRIGGER, STIMULATION PARAMETER SETTING \r\n");
    }

    switch(p_mapdata->stimulationMode)
    {
        case en__monopolr_body:
        case en__monopolr_rod:
        case en__monopolr_BothRodBody:
        {
            // 내부기 칩 레지스터 0x05, 0x06을 설정하여
            // 오프셋 DAC 값과 오프셋 DAC의 기울기, 슬로프 DAC의 기울기, 자극 모드 및 레퍼런스 전극 설정을 진행한다.
            //TDC_PRINTF_I("[SETTING] START MONOPOLAR MODE SETTING \r\n");
            error = settingStimulPara_monoPolarMode(startTrigger);
        }
        break;

        case en__bipolar:
        {
            //TDC_PRINTF_I("[SETTING] START BIPOLAR MODE SETTING \r\n");
            error = settingStimulPara_biPolarMode(startTrigger);
        }
        break;

        case en__commonground:
        {
            //TDC_PRINTF_I("[SETTING] START CG MODE SETTING \r\n");
            error = settingStimulPara_monoPolarMode(startTrigger);
        }
        break;

        default:
            break;
    }

    return error;
}

