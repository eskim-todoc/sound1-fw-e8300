#include <hw.h>
#include <isdExecution/dirver_PCM.h>
#include <stdbool.h>

#include "board.h"
#include "isd_interface_init_FPGA.h"
#include "internalStimulationChip.h"
#include "isd_interface.h"

#if 0
#include "driver_cfx_i2c.h"
#else
#include "dirver_i2c_for_ISD.h"
#endif

#include "definitionsForAlgorithm.h"

#include "isd_interface.h"
#include "cfx_cm3_sharedMemory.h"
#include "isd_interface_FPGA.h"

#include "error.h"

// #include "FPGA.h"
#if defined(Board_is_OTE_VER_1_2)
#include "driver_REN_ISL91128.h"
#elif defined(Board_is_TD_DEV_ver_1_4) || defined(Board_is_OTE_VER_1_4) || defined(Board_is_OTE_VER_1_5)
#include "driver_REN_ISL9122.h"
#elif defined(Board_is_OTE_VER_1_3)
#include "driver_REN_ISL98608.h"
#endif

#include "commonDataProcessing.h"
#include "LedOutput.h"

static int FPGA_version;

#define df_startStabilizationCounter 100

void init_txPowerIC(bool isdControlStateChagedFlag)
{
    static int flowControlCounter = 0;
    int        RF_TxPowerValue;
    int        tempValue;

    if (isdControlStateChagedFlag)
    {
        flowControlCounter = 0;
        clearIsdControlStateChagedFlag();
    }

    switch (flowControlCounter)
    {
        case 0:
            change_i2c_is_free();
            OnOff_3V_PMIC_CM3_to_CFX(0);
            break;

        case 3:
            OnOff_3V_PMIC_CM3_to_CFX(1);
            break;

        case 10:  // 전압 제어 범위 중 최소 값으로 시작.
        {
            change_i2c_is_busy();

            // Pwoer IC의 전원이 켜져 있어야 한다.
            if (!Reset_REN_ISL9122())
            {
                // update_CM3Status_toCFX(flowControlCounter);
                ci_printe("[LINK] POWER PMIC RESET FAILED \r\n");
                errorCodeUpdate(en__RF_PowerIC_ERROR, en__NON_RESETTABLE, __LINE__);
            }
        }
        break;

        case 11:  // 전압 제어 범위 중 최소 값으로 시작.
        {
        }
        break;

        case 12:  // 전압 제어 범위 중 최소 값으로 시작.
        {
            write_change_TxPowerLevel(ResetVoltageSetValue);
        }
        break;

        case 13:  // 전송 파워 증가 시퀀스
        {
            if (read_txPowerLevel(&RF_TxPowerValue))
            {
                // 증가
                tempValue = RF_TxPowerValue + VoltageControlStep;

                // 큰 step 으로 전압을 올릴 수 있을 때
                if (tempValue <= MaxVoltageControlValue)
                {
                    write_change_TxPowerLevel(tempValue);

                    flowControlCounter = 12;  // 함수 종료 전 flowControlCounter++; → 실제로 case 13을 반복
                }
                else  // 큰 step 으로 전압을 올리기 어려울 때
                {
                    tempValue = RF_TxPowerValue + 1;

                    if (tempValue <= MaxVoltageControlValue)
                    {
                        write_change_TxPowerLevel(tempValue);  // 함수 종료 전 flowControlCounter++; → 실제로 case 13을 반복
                        flowControlCounter = 12;
                    }
                    else
                    {
                        // 더이상 step을 증가 시킬 수 없음 종료
                        flowControlCounter = df_startStabilizationCounter;
                    }
                }
            }
            // I2C 통신 실패에 대한 에러는 read_txPowerLevel() 함수 내부에서
            // 자체적으로 change_isd_state(en__isdStatus_PowerIC_Reset); 를 수행하여 해결함
        }
        break;

        case df_startStabilizationCounter + 10:  // 전송 파워 증가 시퀀스
        {
            change_isd_state(en__isdStatus_PowerIC_OK);
            clearErrorFlag(en__RF_PowerIC_ERROR);

            ci_printd("[LINK] POWER PMIC INIT SUCCESS \r\n");
        }
        break;
    }

    flowControlCounter++;
}

void init_FPGA(bool isdControlStateChagedFlag)
{
    static int flowControlCounter = 0;
    int        r_FPGA_registerValue;
    int        comparing;

    if (isdControlStateChagedFlag)
    {
        flowControlCounter = 0;
        clearIsdControlStateChagedFlag();
    }

    switch (flowControlCounter)  //
    {
        case 0:  // PCM은 0을 출력한다.
        {
            reset_Fpga_variable();
            changePcmOutputMode(PcmBitStream_Mode_FillZero);
            change_i2c_is_free();
        }
        break;

        case 10:  //
        {
            change_i2c_is_busy();
        }
        break;

        case 11:  // FPGA를 리셋한다.
        {
            if (write_FPGA_reset())
            {
                if (read_FPGA_version(&FPGA_version))
                {
                    ci_printv("[FPGA] VERSION : %u.%u \r\n", ((FPGA_version >> 4) & 0x0F), (FPGA_version & 0x0F));

                    // FPGA 상태를 읽어 본다.
                    if (read_FPGA_systemResgister_1st(&r_FPGA_registerValue))
                    {
                        comparing = r_FPGA_registerValue & FPGA_Status_FlagIndex;
                        if (comparing != FPGA_Status__Reset_value)
                        {
                            // FPGA 초기화
                            change_isd_state(en__isdStatus_PowerIC_OK);
                            errorCodeUpdate(en__FPGA_COMMUNICATION_ERROR, en__FPGA_ResetValueError, __LINE__);

                            ci_printe("[FPGA] NOT INITIALIZED AFTER RESET, SYS_STAT1=0x%02X, MASKER=0x%02X, RESULT=0x%02X \r\n",  //
                                      r_FPGA_registerValue,
                                      FPGA_Status_FlagIndex,
                                      comparing);
                        }
                    }
                }
            }
        }
        break;

        case 12:
            // 프리엠블 진행
            // 0을 1ms 출력, 프리엠블 1ms 출력 --> 2ms이 소요됨
            changePcmOutputMode(PcmBitStream_Mode_Preamble);
            break;

        case 20:  // FPGA PCM 상태 확인
        {
            // FPGA 상태를 읽어 본다.
            if (read_FPGA_systemResgister_1st(&r_FPGA_registerValue))
            {
                comparing = r_FPGA_registerValue & FPGA_Status_FlagIndex;
                if (comparing == FPGA_Status__OK_DisabledRF_value)
                {
                    //////////////
                    // FPGA PCM 수신 정상
                    //////////////
                    changePcmOutputMode(PcmBitStream_Mode_NopStandby);
                    change_isd_state(en__isdStatus_FPGA_Ok);
                    clearErrorFlag(en__FPGA_CONFIGUARATION_ERROR);

                    ci_printd("[FPGA] INIT SUCCESS \r\n");
                }
                else
                {
                    // FPGA 초기화
                    change_isd_state(en__isdStatus_PowerIC_OK);
                    errorCodeUpdate(en__FPGA_CONFIGUARATION_ERROR, en_RegisterConfigError_byPCM, __LINE__);

                    ci_printe("[FPGA] INIT FAILED, SYS_STAT1=0x%02X, MASKER=0x%02X, RESULT=0x%02X \r\n",  //
                              r_FPGA_registerValue,
                              FPGA_Status_FlagIndex,
                              comparing);
                }
            }
        }
        break;

        default:
            break;
    }

    flowControlCounter++;
}

void init_ISD(bool isdControlStateChagedFlag)
{
    static int temporal_registerValue;
    static int flowControlCounter = 0;

    //  int i2c_txBuffer[32];
    //  int i2c_rxBuffer[32];
    int RF_TxPowerValue;
    int r_FPGA_registerValue;
    int w_isd_registerValue;
    int r_isd_registerValue[2];
    int pcm_index;
    int i;

    bool FPGA_FIFO_empty;
    bool FPGA_error;

    if (isdControlStateChagedFlag)
    {
        flowControlCounter = 0;
        clearIsdControlStateChagedFlag();
        changeConnected_isd_num_CFX(0);
    }

    pcm_index = 0;

    switch (flowControlCounter)
    {
        // FPGA에 에러 발생했는지 확인 하는 단계, 에러 없을 시 10Mhz 캐리어 클럭 끔
        case 0:
        {
            check_FPGA_PCM_Error(&FPGA_error);
            if (FPGA_error)
            {
                // FPGA 에러 발생, FPGA 초기화
                change_isd_state(en__isdStatus_PowerIC_OK);
                ci_printe("[FPGA] ERROR OCCURRED \r\n");
            }
            else
            {
                write_FPGA_disable_RF_tx();
                write_change_TxPowerLevel(MaxVoltageControlValue);

                ci_printv("[FPGA] TRY TO DISABLE XFR(RF) \r\n");
            }

            change_i2c_is_free();
        }
        break;

        // 약 200ms 동안 10Mhz 캐리어 클럭을 꺼서 내부기에 전원이 인가되지 않아 꺼져있을 것으로 추정
        // 다시 10Mhz 캐리어 클럭을 켬
        case 200:
        {
            // PCM 출력 모드 변경
            changePcmOutputMode(PcmBitStream_Mode_NopStandby);

            change_i2c_is_busy();

            if (!is_RF_tx_eanble())
            {
                ci_printv("[FPGA] SUCCESS TO DISABLE XFR(RF) THEN, TRY TO ENABLE XFR(RF) \r\n");

                if (write_FPGA_enable_RF_tx())
                {
                    if (!is_RF_tx_eanble())
                    {
                        change_isd_state(en__isdStatus_PowerIC_OK);  // FPGA 초기화
                        errorCodeUpdate(en__FPGA_CONFIGUARATION_ERROR, en_RF_Tx_enableError, __LINE__);
                        ci_printe("[FPGA] FAILED TO ENABLE XFR(RF) \r\n");
                    }
                }
            }
            else
            {
                change_isd_state(en__isdStatus_PowerIC_OK);  // FPGA 초기화
                errorCodeUpdate(en__FPGA_CONFIGUARATION_ERROR, en_RF_Tx_enableError, __LINE__);
                ci_printe("[FPGA] FAILED TO DISABLE XFR(RF) \r\n");
            }
        }
        break;

        // 약 2ms 동안 10Mhz 캐리어 클럭이 입력되고 있는 상태에서
        // FPGA의 펄스 폭을 최소로 설정 (앞으로 PCM 통신으로 이런 저런 설정을 하기 위함)
        case 202:
        {
            chang_PulseWidth_minimum(pcm_index++);  // 펄스 폭 설정

            // 나머지 버퍼는 NopStandby
            for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
            {
                fillSepcificCommndBuffer(i, pcm_Mold_NopStandby);
            }

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        // 여기까지 약 5ms 동안 10Mhz 캐리어 클럭이 입력되고 있는 상태에서
        // FPGA의 펄스 폭이 최소로 설정되었는지 검증
        case 205:
        {
            if (read_FPGA_PulseWidth(&r_FPGA_registerValue))
            {
                if (r_FPGA_registerValue == FPGA_pulsePhaseWidth_minimum)
                {
                    // 펄스 폭 설정이 정상적으로 완료되었기 때문에 펄스폭 값을 업데이트한다.
                    upadte_fpga_pulsePhaseWidth_written_Value(r_FPGA_registerValue);

                    if (write_change_TxPowerLevel(MaxVoltageControlValue))
                    {
                        if (read_txPowerLevel(&RF_TxPowerValue))
                        {
                            if (RF_TxPowerValue != MaxVoltageControlValue)
                            {
                                // R_TxPower 설정 실패, powerIC 초기화
                                errorCodeUpdate(en__RF_PowerIC_ERROR, en__writtenReadVlaueIsNotSame, __LINE__);
                                change_isd_state(en__isdStatus_PowerIC_Reset);

                                ci_printe("[FPGA] FAILED TO SET RF TX POWER TO MAX POWER \r\n");
                            }
                        }
                    }
                }
                else
                {
                    // 펄스폭 설정 실패 , FPGA 초기화
                    errorCodeUpdate(en__FPGA_CONFIGUARATION_ERROR, en_PulseWidthDifferent, __LINE__);
                    change_isd_state(en__isdStatus_PowerIC_OK);

                    ci_printe("[FPGA] FAILED TO SET PULSE PHASE WIDTH TO MINIMUM \r\n");
                }
            }
        }
        break;

#ifndef DisalbedBackTel
        // 여기까지 약 6ms 동안 10Mhz 캐리어 클럭이 입력되고 있는 상태에서
        // FPGA의 백텔 레지스터를 8비트 모드로 설정
        case 206:
        {
            temporal_registerValue = Reset_8bitBacktelConfig(pcm_index++);  // FPGA Backtel - 8bit 레지스터

            // 나머지 버퍼는 NopStandby
            for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
            {
                fillSepcificCommndBuffer(i, pcm_Mold_NopStandby);
            }

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        // 여기까지 약 10ms 동안 10Mhz 캐리어 클럭이 입력되고 있는 상태에서
        // FPGA의 백텔 레지스터가 8비트 모드로 설정되었는지 확인 후 백텔 FIFO 클리어 진행
        case 210:
        {
            if (read_FPGA_backtelConfig(&r_FPGA_registerValue))
            {
                if (temporal_registerValue == r_FPGA_registerValue)
                {
                    // 위에서, PCM으로 설정한 FPGA 백텔 레지스터가 원하는 값으로 쓰여졌는지 확인 후 업데이트
                    upadte_fpga_backtelConfig_written_Value(temporal_registerValue);

                    // FIFO를 지우고 FPGA 상태를 읽어 본다.
                    if (write_FPGA_clear_FIFO())
                    {
                        if (check_FPGA_FIFO_empty(&FPGA_FIFO_empty))  // FIFO 지워 졌는지 확인.
                        {
                            if (!FPGA_FIFO_empty)
                            {
                                errorCodeUpdate(en__FPGA_CONFIGUARATION_ERROR, en_FIFO_NotCleared, __LINE__);
                                change_isd_state(en__isdStatus_PowerIC_OK);  //
                                ci_printe("[FPGA] FIFO IS NOT CLEARED \r\n");
                            }
                        }
                    }
                }
                else
                {
                    // 백텔 레지스터 설정 오류
                    errorCodeUpdate(en__FPGA_CONFIGUARATION_ERROR, en_RegisterConfigError_byPCM, __LINE__);
                    change_isd_state(en__isdStatus_PowerIC_OK);

                    ci_printe("[FPGA] BACKTEL REGISTER IS NOT CONFIGURED \r\n");
                }
            }
        }
        break;

        // 여기까지 약 50ms 동안 10Mhz 캐리어 클럭이 입력되고 있는 상태에서
        // 내부기 칩의 'h0F 레지스터의 SW_SYS_RESET 비트를 1로 설정하여 내부기 칩의 소프트웨어 리셋을 수행
        // 약 50ms 동안 10Mhz 캐리어 클럭이 입력되었으므로 기초 전원 공급은 충분하다고 판단함
        case 250:
        {
            w_isd_registerValue = ISD_registerAddr_SystemClkReset;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
            w_isd_registerValue = w_isd_registerValue << 8;
            w_isd_registerValue = w_isd_registerValue | 0x01;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

            // 나머지 버퍼는 NopStandby
            for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
            {
                fillSepcificCommndBuffer(i, pcm_Mold_NopStandby);
            }

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        // 약  5ms 동안 내부기 칩이 소프트웨어 리셋이 걸렸으리라 판단
        // 참고:
        // 아래 순서 255 단계에서
        // 'h10 레지스터의 SYSCLK_OE을 1로 설정하는 패킷을 구성하지만 실제로 출력은 하지 않는다.
        case 255:
        {
#if defined(EEPROM_LSK_Error)
            // ISD LSK 설정
            w_isd_registerValue = ISD_registerAddr_LSK_Clk_Config;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
            w_isd_registerValue = w_isd_registerValue << 8;

            w_isd_registerValue = w_isd_registerValue | 0x08;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;
            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

            w_isd_registerValue = ISD_registerAddr_PPSK_Config;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
            w_isd_registerValue = w_isd_registerValue << 8;

            w_isd_registerValue = w_isd_registerValue | 0x20;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;
            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
#endif
            // ISD SYSCLK_OE 설정
            w_isd_registerValue = ISD_registerAddr_IO_Config;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
            w_isd_registerValue = w_isd_registerValue << 8;
            w_isd_registerValue = w_isd_registerValue | 0x08;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

            // 나머지 버퍼는 NopStandby
            for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
            {
                fillSepcificCommndBuffer(i, pcm_Mold_NopStandby);
            }

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);  // 다음 출력 모드 : NopStandby
            // changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        // 내부기 칩의 소프트웨어 리셋 이후 약 6ms 뒤에 백텔을 사용해서 내부기 칩의 전원 상태를 읽는다.
        case 256:
        {
            // ISD Power 읽기
            w_isd_registerValue = ISD_registerAddr_PowerCheck;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_readRegister;
            w_isd_registerValue = w_isd_registerValue << 8;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
            fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
            fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
            fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);

            // 나머지 버퍼는  NOP standby
            for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
            {
                fillSepcificCommndBuffer(i, pcm_Mold_NopStandby);
            }

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        // 백텔로 내부기 전원 상태 확인 응답이 왔는지 확인한다.
        // 백텔 데이터 수가 1이면 응답을 받은 것이고 1이 아니면,
        // 응답이 없거나 많아도 문제인 것이므로 내부기 연결 과정 처음부터 다시 진행한다.
        case 260:
        {
            // FIFO 카운터를 읽어 본다.
            if (read_FPGA_FIFO_counter(&r_FPGA_registerValue))
            {
                if (r_FPGA_registerValue == 1)
                {
                    read_FPGA_backtel_FIFO(r_isd_registerValue, 2);
                    change_isd_state(en__isdStatus_ISD_Power_Ok);

                    clearErrorFlag(en__RF_PowerIC_ERROR);
                    clearErrorFlag(en__FPGA_CONFIGUARATION_ERROR);
                    clearErrorFlag(en__EN__ISD_ERROR);

                    ci_printd("[FPGA] SUCCESS TO GET POWER LEVEL BACKTEL FOR INITIAL CONNECTION \r\n");
                }
                else
                {
                    read_FPGA_backtelError_Flag(&r_FPGA_registerValue);

                    // 백텔 에러는 발생하지 않았으나 백텔이 들어 오지 않았다. -> 내부기 전송 파워 설정 부터 다시.
                    errorCodeUpdate(en__EN__ISD_ERROR, en__BackTelCounterZero, __LINE__);
                    change_isd_state(en__isdStatus_FPGA_Ok);  // RF PMIC MAX POWER 설정을 FPGA_OK 상태에서도 진행한다.
                    // change_isd_state(en__isdStatus_PowerIC_OK);

                    ci_printv("[FPGA] NO POWER LEVEL BACKTEL RESPONSE FOR INITIAL CONNECTION \r\n");
                }
            }
        }
        break;
#else // if defind(DisalbedBackTel)
                case 275  :
                    change_isd_state(en__isdStatus_ISD_Power_Ok);
                    break;
#endif

                default :
                    break;
    }

    flowControlCounter++;
}
