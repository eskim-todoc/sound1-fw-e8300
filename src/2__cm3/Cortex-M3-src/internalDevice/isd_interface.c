
#include <hw.h>
#include <isdExecution/driver_PCM.h>
#include <stdbool.h>

#include "FPGA.h"
#include "internalStimulationChip.h"
#include "isd_interface.h"
#include "isd_interface_FPGA.h"
#if 0
#include "driver_cfx_i2c.h"
#else
#include "driver_i2c_for_ISD.h"
#endif

#include "definitionsForAlgorithm.h"
#include "isd_interface.h"
#include "isd_interface_init_FPGA.h"
#include "isd_interface_init_ISD.h"
#include "isd_interface_stimulationStandAlone.h"
#include "isd_interface_stimulationParaSetting.h"
#include "mappingControl.h"
#include <ci_ble_control_ota.h>

#include "cfx_cm3_sharedMemory.h"

#include "remoteControl.h"
#include "error.h"

#if defined(Board_is_OTE_VER_1_2)
#include "driver_REN_ISL91128.h"
#elif defined(Board_is_TD_DEV_ver_1_4) || defined(Board_is_OTE_VER_1_4) || defined(Board_is_OTE_VER_1_5)
#include "driver_REN_ISL9122.h"
#elif defined(Board_is_OTE_VER_1_3)
#include "driver_REN_ISL98608.h"
#else
#error Link PMIC is NOT selected.
#endif

#include <snd_qcc.h>

static bool i2c_is_freeS_for_10msec = false;

bool is_i2c_free(void)
{
    return i2c_is_freeS_for_10msec;
}

void change_i2c_is_busy(void)
{
    i2c_is_freeS_for_10msec = false;
}

void change_i2c_is_free(void)
{
    i2c_is_freeS_for_10msec = true;
}

void fill_pcmBuff_writeData_checkPathOpen_normalValue(int *p_pcm_index)
{
    int w_isd_registerSlice;
    int w_isd_registerValue;

    // ISD path 확인용 임의의 값
    w_isd_registerSlice = ISD_registerAddr_forwardPath_check;
    w_isd_registerSlice = w_isd_registerSlice << 1;
    w_isd_registerSlice = w_isd_registerSlice | ISD_writeRegister;
    w_isd_registerSlice = w_isd_registerSlice << 8;

    w_isd_registerValue = w_isd_registerSlice | df_forwardPathCheck_arbitraryValue;
    w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;
    fillSepcificCommndBuffer((*p_pcm_index)++, w_isd_registerValue);
}

void fill_pcmBuff_writeData_checkPathOpen_duplicateZeroData(int *p_pcm_index)
{
    int w_isd_registerSlice;
    int w_isd_registerValue;

    // ISD path 확인용 임의의 값
    w_isd_registerSlice = ISD_registerAddr_forwardPath_check;
    w_isd_registerSlice = w_isd_registerSlice << 1;
    w_isd_registerSlice = w_isd_registerSlice | ISD_writeRegister;
    w_isd_registerSlice = w_isd_registerSlice << 8;

    w_isd_registerValue = w_isd_registerSlice | df_duplicateZeroValue;
    w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;
    fillSepcificCommndBuffer((*p_pcm_index)++, w_isd_registerValue);
}

void fill_pcmBuff_readData_checkPathOpen(int *p_pcm_index)
{
    // ISD path 확인 읽기
    int w_isd_registerSlice;
    int w_isd_registerValue;

    w_isd_registerSlice = ISD_registerAddr_forwardPath_check;
    w_isd_registerSlice = w_isd_registerSlice << 1;
    w_isd_registerSlice = w_isd_registerSlice | ISD_readRegister;
    w_isd_registerSlice = w_isd_registerSlice << 8;

    w_isd_registerValue = w_isd_registerSlice;
    w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

    fillSepcificCommndBuffer((*p_pcm_index)++, w_isd_registerValue);
}

void fill_pcmBuff_check_ISD_PathOpen_normalValue(int *p_pcm_index)
{
    fill_pcmBuff_writeData_checkPathOpen_normalValue(p_pcm_index);
    fill_pcmBuff_readData_checkPathOpen(p_pcm_index);

    // 백텔 값 읽기
    fillSepcificCommndBuffer((*p_pcm_index)++, pcm_Mold_NopBacktel);
    fillSepcificCommndBuffer((*p_pcm_index)++, pcm_Mold_NopBacktel);
    fillSepcificCommndBuffer((*p_pcm_index)++, pcm_Mold_NopBacktel);
}

void fill_pcmBuff_lastSimulationOut(int *p_pcm_index)
{
    fill_pcmBuff_writeData_checkPathOpen_normalValue(p_pcm_index);
}

// 콜라 프로토콜 상에서 0의 중복이 많은 데이터에 대해서 백텔 에러가 발생한 경우가 있었다. 이러한 데이터에 대한 백텔 확인용.
void fill_pcmBuff_check_ISD_PathOpen_duplicateZeroData(int *p_pcm_index)
{
    fill_pcmBuff_writeData_checkPathOpen_duplicateZeroData(p_pcm_index);
    fill_pcmBuff_readData_checkPathOpen(p_pcm_index);

    // 백텔 값 읽기
    fillSepcificCommndBuffer((*p_pcm_index)++, pcm_Mold_NopBacktel);
    fillSepcificCommndBuffer((*p_pcm_index)++, pcm_Mold_NopBacktel);
    fillSepcificCommndBuffer((*p_pcm_index)++, pcm_Mold_NopBacktel);
}

static ST__ISD_STATUS s_isd_state                     = {en__isdStatus_PowerIC_Reset, false};
static bool           s_isd_control_state_chaged_flag = true;

ST__ISD_STATUS snd_isd_interface_get_state(void)
{
    return s_isd_state;
}

void change_isd_state(EN__ISD_CONTROL_STATE ISD_controlState)
{
    int i                           = 0;
    s_isd_state.isd_controlState    = ISD_controlState;
    s_isd_control_state_chaged_flag = true;

    changePcmOutputMode(PcmBitStream_Mode_NopStandby);

    if (ISD_controlState == 8)
    {
        i = 1;
    }
}

void clearIsdControlStateChagedFlag(void)
{
    s_isd_control_state_chaged_flag = false;
}

ST__ISD_STATUS isd_interface(bool isd_enable, bool mappingConnection, EN__ISD_CONTROL_STATE isdControlCommand)
{
    bool stimulationParameterSettingIsDone = false;

    if (isd_enable)
    {
        // 매핑 기능에서 명령 또는 특정한 상태에 따라 내부기를 제어하는 명령이 적용되는 경우가 있다.
        // 예를 들면, 매핑 앱의 블루투스 연결 해제 시 en__isdStatus_PowerIC_Reset 부터 수행하는 경우가 있다.
        if (isdControlCommand != en__isdStatus_NA)
        {
            change_isd_state(isdControlCommand);
        }

        // 매핑 기능에서 명령 또는 특정한 상태에 따라 내부기를 제어하는 경우가 아니면,
        // 전역 변수로 저장된 내부기 상태에 따라서 내부기와 연결될 때 까지 내부기 연결 과정을 반복적으로 시도한다.
        switch (s_isd_state.isd_controlState)
        {
            case en__isdStatus_PowerIC_Reset:
                // 링크 5V PMIC를 최소 전압에서 최대전압으로 단계적 증가
                init_txPowerIC(s_isd_control_state_chaged_flag);
                break;

            case en__isdStatus_PowerIC_OK:
                // FPGA의 소프트웨어 리셋, PCM Abort, Preamble, Nop 패킷 전송 시퀀스 수행 후,
                // 마지막으로 FPGA에 에러가 없는지 검증한다. (싱크 로스트 등)
                init_FPGA(s_isd_control_state_chaged_flag);
                break;

            case en__isdStatus_FPGA_Ok:
                // RF Tx (10Mhz 캐리어 클럭)을 일정기간 죽인 후, 내부기 전송 시작,
                // 내부기의 전원 안정화 여부는 관계 없이 내부기 칩의 전원 레벨이 읽히는지 여부까지만 수행한다.
                init_ISD(s_isd_control_state_chaged_flag);
                break;

            case en__isdStatus_ISD_Power_Ok:
                // 내부기 칩의 시리얼을 백텔로 읽고, 외부기에 저장된 맵 데이터와 일치하는 내부기인지 검증한다.
                // 마스터 키 내부기의 경우 첫 번째 저장된 맵 데이터를 사용한다.
                // 백텔로 읽은 시리얼을 사용해 내부기 키 복호화를 수행한다.
                // 키 복호화 성공 여부를 Forward Check 레지스터를 사용해 검증한다.
                // 특정 값으로 한 번, 0으로 채워진 데이터로 한 번, 총 두번이 모두 성공하면
                // 외부기에 저장된 맵 데이터의 해당 ISD 번호에 해당하는 내부기가 올바르게 인식된 것으로 간주한다.

                // 중요사항: 모든 과정 성공 시 마지막에 ISD 번호 업데이트 후 CFX가 처리도록 함
                // 순서 1: 파일 읽고 맵 데이터 메모리 영역에 로드 : ISD 정보, 사용자 설정 값, 매핑 일자, 프로그램 1, 2, 3, 4
                // 순서 2: 맵 데이터 메모리 영역에서 공유 메모리 영역으로 ISD 번호에 해당하는 정보 모두 복사
                // 순서 3: 공유 메모리의 현재 연결 중인 ISD 번호 업데이트하여 CFX가 처리하도록 함
                isd_path_Open(s_isd_control_state_chaged_flag);
                break;

            case en__isdStatus_ISD_pathOpen_Ok:
                // 자극 출력을 위한 내부기 칩 외부의 10V를 켜고, 이 10V를 활용하도록 VTG_LOCK_ENABLE을 설정하여
                // 자극발생부가 잘 활성화 되도록 설정 및 검증하는 단계이다.
                enableStimul_10v(s_isd_control_state_chaged_flag);
                break;

            default:
                break;
        }

        // 내부기 칩의 자극발생부 활성화까지 전부 문제가 없으면, 맵 데이터를 사용해 내부기 칩의 자극 방식을 설정하는 단계를 수행한다.
        // 자극 방식 설정까지 모두 완료되면, 주기적으로 링크 연결 상태 체크를 위해 백텔 데이터를 주고 받는다.
        if (s_isd_state.isd_controlState == en__isdStatus_stimul_10V_Ok)
        {
            /* 매핑 연결이 아닐 때 */
            if (!mappingConnection)
            {
                stimulationParameterSettingIsDone = stimulationStandAlone();

                // OTA DFU 모드에 따른 Link backtel 체크 유무 결정
#if 0
                update_isd_LinkConnection_byBacktel_withLiveStimulation();
#else
                // OTA DFU 모드 (Link backtel 체크 X) 사용 중일 때는 FIFO clear + 상태 초기화만 반복한다.
                if (tdc_get_ota_dfu_conn_state() == TDC_OTA_DFU_CONN_ST_CONN)
                {
                    if (BackelCircuitDisabled_readPcmFired_duringLiveStimulation == readConnectionCheckPcmState())
                    {
                        if (!write_FPGA_clear_FIFO())
                        {
                            change_isd_state(en__isdStatus_PowerIC_OK);
                        }

                        clearConnectionCheckPcmFiredFlag();  // 기록을 지운다.
                    }
                }
                else
                {
                    update_isd_LinkConnection_byBacktel_withLiveStimulation();
                }
#endif
            }
        }
    }
    else  // if (isd_enable)에 대한 else
    {
        // main 함수의 systemControl() 함수에서 획득한 isd_enable 상태가,
        // 내부기 연결 과정을 진행하지 않게 false 인 경우에는
        // 내부기 연결 과정을 RF PMIC 5V를 리셋하는 것부터 다시 시작하도록
        // 내부기 상태를 지속적으로 en__isdStatus_PowerIC_Reset 값으로 초기화 시킨다.

        change_isd_state(en__isdStatus_PowerIC_Reset);
    }

    if (mappingConnection)
    {
        if (s_isd_state.isd_controlState >= en__isdStatus_stimul_10V_Ok)
        {
            s_isd_state.conneded_ISD = true;
            snd_qcc_set_isd(SND_QCC_ISD_CONNECTED);
        }
        else
        {
            s_isd_state.conneded_ISD = false;
            snd_qcc_set_isd(SND_QCC_ISD_DISCONNECTED);
        }
    }
    else
    {
        if (stimulationParameterSettingIsDone)
        {
            s_isd_state.conneded_ISD = true;
            snd_qcc_set_isd(SND_QCC_ISD_CONNECTED);
        }
        else
        {
            s_isd_state.conneded_ISD = false;
            snd_qcc_set_isd(SND_QCC_ISD_DISCONNECTED);
        }
    }

    return s_isd_state;
}

void update_isd_LinkConnection_byBacktel_withLiveStimulation(void)
{
    static int link_connection_success_counter = 0;
    static int r_isd_registerValue             = 0;
    static int TxPowerLevel;
    static int TxPowerLevel_bak = 0;
    static int flowCounter      = 0;

    int temp;

    // CFX에서 주기적으로 연결확인용 PCM을 출력하고 출력이 완료되었음을 공유메모리에 기록한다.
    // CFX에서 PCM 모드가 실시간 자극일 때, 자동으로 업데이트 된다.
    if (BackelCircuitDisabled_readPcmFired_duringLiveStimulation == readConnectionCheckPcmState())
    {
#ifndef DisalbedBackTel
        // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
#if defined(conneded_ISDCheck_byForwardPath)

        if (is_arbitraryValue_matched_normalValue())
        {
            update_isd_Link_is_Connected();
            intervalCounter = df_connectionCheckPeriod_ms;
        }
        else
        {
            read_FPGA_systemResgister_1st(&temp);
            read_FPGA_systemError_Flag(&temp);
            read_FPGA_backtelError_Flag(&temp);
            read_FPGA_PulseWidth(&temp);
            read_FPGA_FIFO_counter(&temp);
            // change_isd_state(en__isdStatus_FPGA_Ok); // 내부기 전송 파워 설정 부터 다시.
            update_isd_Link_is_Disconnected();
            // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
        }

        clearConnectionCheckPcmFiredFlag();  // 기록을 지운다.
#elif defined(conneded_ISDCheck_byISDPower)
        switch (flowCounter)
        {
            case 0:
            {
                change_i2c_is_busy();
                r_isd_registerValue = read_isd_Power_State();
            }
            break;

            case 1:
            {
                read_txPowerLevel(&TxPowerLevel);
            }
            break;

            case 2:  // 전원 IC 출력을 조정하여 쓴다.
            {
#if 1  // 링크 디버깅
                {
#if 1  // 링크 파워 변경시에만 출력
                    static EN_ISD_PowerState debug_isd_power_state = ISD_Power_NA;

                    if (debug_isd_power_state != r_isd_registerValue)
                    {
                        debug_isd_power_state = r_isd_registerValue;

                        if (r_isd_registerValue == NoBacktel || r_isd_registerValue == BackTelNumTooMuch || r_isd_registerValue == ISD_Power_NA)
                        {
                            TDC_PRINTF_V("[LINK] STATE : %s \r\n",                                                     //
                                      r_isd_registerValue == ISD_Power_LowUnstable    ? "ISD POWER LOW, UNSTABLE"   //
                                      : r_isd_registerValue == ISD_Power_LowStable    ? "ISD POWER LOW, STABLE"     //
                                      : r_isd_registerValue == ISD_Power_HighUnstable ? "ISD POWER HIGH, UNSTABLE"  //
                                      : r_isd_registerValue == ISD_Power_HighStable   ? "ISD POWER HIGH, STABLE"    //
                                      : r_isd_registerValue == NoBacktel              ? "NO BACKTEL"                //
                                      : r_isd_registerValue == BackTelNumTooMuch      ? "BACKTEL NUM TOO MUCH"      //
                                                                                      : "ISD POWER N/A");
                        }
                    }
#else  // 항상 출력
                    TDC_PRINTF_V("[LINK] STATE : %s \r\n",                                                     //
                              r_isd_registerValue == ISD_Power_LowUnstable    ? "ISD POWER LOW, UNSTABLE"   //
                              : r_isd_registerValue == ISD_Power_LowStable    ? "ISD POWER LOW, STABLE"     //
                              : r_isd_registerValue == ISD_Power_HighUnstable ? "ISD POWER HIGH, UNSTABLE"  //
                              : r_isd_registerValue == ISD_Power_HighStable   ? "ISD POWER HIGH, STABLE"    //
                              : r_isd_registerValue == NoBacktel              ? "NO BACKTEL"                //
                              : r_isd_registerValue == BackTelNumTooMuch      ? "BACKTEL NUM TOO MUCH"      //
                                                                              : "ISD POWER N/A");
#endif
                }
#endif

                link_connection_success_counter++;

                switch (r_isd_registerValue)
                {
#if !defined(disable_Tx_PowerControl)
                    case ISD_Power_HighStable:
                    {
                        // 전송 파워 감소 시킴
                        if (TxPowerLevel > MinTxPowerValue)
                        {
                            int current_TxPowerLevel;

                            current_TxPowerLevel = TxPowerLevel;
                            TxPowerLevel_bak     = TxPowerLevel;

                            TxPowerLevel--;

                            // TDC_PRINTF_V("[LINK] HIGH, STABLE : TX POWER > MIN POWER (CURR=%4d, NEXT=%4d) \r\n", current_TxPowerLevel, TxPowerLevel);

                            write_change_TxPowerLevel(TxPowerLevel);
                        }
                        else
                        {
                            if (TxPowerLevel_bak != TxPowerLevel)
                            {
                                TxPowerLevel_bak = TxPowerLevel;
                                // TDC_PRINTF_V("[LINK] HIGH, STABLE : TX POWER <= MIN POWER (CURR=%4d) \r\n", TxPowerLevel);
                            }

                            temp = 0;
                            temp++;
                        }
                    }
                    break;

                    case ISD_Power_HighUnstable:
                    {
                        if (TxPowerLevel_bak != TxPowerLevel)
                        {
                            TxPowerLevel_bak = TxPowerLevel;
                            // TDC_PRINTF_V("[LINK] HIGH, UNSTABLE (CURR=%4d) \r\n", TxPowerLevel);
                        }

                        temp = 0;
                        temp++;
                    }
                    break;

                    case ISD_Power_LowStable:  // 증가가 가능할 때까지 증가. 증가가 더이상 불가능한 경우. 상태 유지
                    {
                        // 전송 파워 감소 시킴
                        if (TxPowerLevel < MaxVoltageControlValue)
                        {
                            int current_TxPowerLevel;

                            current_TxPowerLevel = TxPowerLevel;
                            TxPowerLevel_bak     = TxPowerLevel;

                            TxPowerLevel++;

                            // TDC_PRINTF_V("[LINK] LOW, STABLE : TX POWER < MAX CONTROL POWER (CURR=%4d, NEXT=%4d) \r\n", current_TxPowerLevel, TxPowerLevel);

                            write_change_TxPowerLevel(TxPowerLevel);
                        }
                        else
                        {
                            if (TxPowerLevel_bak != TxPowerLevel)
                            {
                                TxPowerLevel_bak = TxPowerLevel;
                                // TDC_PRINTF_V("[LINK] LOW, STABLE : TX POWER >= MAX CONTROL POWER (CURR=%4d) \r\n", TxPowerLevel);
                            }
                        }
                    }
                    break;

                    case ISD_Power_LowUnstable:  // 증가가 가능할 때까지 증가. 증가가 더이상 불가능한 경우. 연결 상태 끊고 다시 시작.
                    {
                        if (TxPowerLevel_bak != TxPowerLevel)
                        {
                            TxPowerLevel_bak = TxPowerLevel;
                            // TDC_PRINTF_V("[LINK] LOW, UNSTABLE (CURR=%4d) \r\n", TxPowerLevel);
                        }

                        temp = 0;
                        temp++;
                    }
                    break;
#endif
                    case NoBacktel:
                    {
#if 0
                        read_FPGA_systemResgister_1st(&temp);
                        read_FPGA_systemError_Flag(&temp);
                        read_FPGA_backtelError_Flag(&temp);
                        read_FPGA_PulseWidth(&temp);
                        read_FPGA_FIFO_counter(&temp);
#endif

#if 0
                        int readValue[6];
                        read_ISD_by_CM3_I2C(i2cAddr_FPGA_systemResgister_1st, readValue, 6);
                        TDC_PRINTF_V("system 1st   : 0x%02X \r\n", readValue[0]);
                        TDC_PRINTF_V("system 2nd   : 0x%02X \r\n", readValue[1]);
                        TDC_PRINTF_V("Error Flag   : 0x%02X \r\n", readValue[2]);
                        TDC_PRINTF_V("Backel Error : 0x%02X \r\n", readValue[3]);
                        TDC_PRINTF_V("Duration     : 0x%02X \r\n", readValue[4]);
                        TDC_PRINTF_V("FIFO Count   : 0x%02X \r\n", readValue[5]);
#endif
                        // TDC_PRINTF("CM3_tempValue1 : %u <- must be 2 \r\n", cfx_cm3_sharedMemoryAll.CM3_tempValue1);
                        // TDC_PRINTF("CM3_tempValue2 : %u <- must be 1 \r\n", cfx_cm3_sharedMemoryAll.CM3_tempValue2);

                        TDC_PRINTF_W("[LINK] DISCONNECTED, NO BACKTEL, LINK CONNECTION SUCCESS COUNTER : %d \r\n", link_connection_success_counter - 1);
                        link_connection_success_counter = 0;

                        errorCodeUpdate(en__EN__ISD_ERROR, en__BackTelCounterZero, __LINE__);
                        change_isd_state(en__isdStatus_FPGA_Ok);  // 내부기 파워 조정 시작
                    }
                    break;

                    case BackTelNumTooMuch:
                    {
                        TDC_PRINTF_W("[LINK] DISCONNECTED, TOO MUCH BACKTEL, LINK CONNECTION SUCCESS COUNTER : %d \r\n", link_connection_success_counter - 1);
                        link_connection_success_counter = 0;

                        errorCodeUpdate(en__EN__ISD_ERROR, en__BackterDataLengthError, __LINE__);
                        change_isd_state(en__isdStatus_FPGA_Ok);  // 내부기 파워 조정 시작
                    }
                    break;
                }

                if (!write_FPGA_clear_FIFO())
                {
                    change_isd_state(en__isdStatus_PowerIC_OK);
                }

                change_i2c_is_free();

                clearConnectionCheckPcmFiredFlag();  // 기록을 지운다.
            }
            break;

            default:
            {
            }
            break;
        }

        flowCounter++;
#endif
#else
        // update_isd_Link_is_Connected();
        // update_isd_Link_is_Disconnected();
        clearConnectionCheckPcmFiredFlag();  // 기록을 지운다.
#endif
        // Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
    }
    else
    {
        flowCounter = 0;
    }
}

bool isINGconnectionCheck_WithMapping = false;

void update_isd_LinkConnection_byBacktel_withMapping(int connectionCheckCOUNTER)
{
    static int r_isd_registerValue = 0;
    static int TxPowerLevel;
    static int flowCounter = 0;

    int pcm_index = 0;
    int w_isd_registerValue;
    int i;

#ifndef DisalbedBackTel
    if (connectionCheckCOUNTER == 0)
    {
        flowCounter                      = 0;
        isINGconnectionCheck_WithMapping = true;
    }

#if 0
    if (flowCounter < 7)
    {
        TDC_PRINTF_V("[MAPPING] LINK CONNECTION CHECK FLOW COUNTER : %d \r\n", flowCounter);
    }
#endif

    if (flowCounter < 7)
    {
        switch (flowCounter)
        {
                // case 0:  // backtel write and read pcm
            case 0:
            {
                changePcmOutputMode(PcmBitStream_Mode_NopStandby);  // 김은수 추가 2026.02.19
            }
            break;

            case 2:  // backtel write and read pcm
            {
                change_i2c_is_busy();

                read_txPowerLevel(&TxPowerLevel);

                // 펄스폭 최소화
                chang_PulseWidth_minimum(pcm_index++);
                upadte_fpga_pulsePhaseWidth_written_Value(FPGA_pulsePhaseWidth_minimum);

                // 백텔 8bit 모드
                change_8BitBacktel_mode(pcm_index++);

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

                // Nop Backtel 추가 by 김은수 (2026.02.19)
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);

                // 나머지 버퍼는  NOP standby
                // for (i=16; i<df_MaxNumTransferableChannel; i++)
                for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
                {
                    fillSepcificCommndBuffer(i, pcm_Mold_NopStandby);
                }

                // 다음 순서의 PCM 동작 모드
                changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
                changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
            }
            break;

            case 5:
            {
                r_isd_registerValue = read_isd_Power_State();
            }
            break;

            case 6:
            {
                switch (r_isd_registerValue)
                {
#if !defined(disable_Tx_PowerControl)
                    case ISD_Power_HighStable:
                    {
                        // 전송 파워 감소 시킴
                        if (TxPowerLevel > MinTxPowerValue)
                        {
                            TxPowerLevel--;

                            // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                            write_change_TxPowerLevel(TxPowerLevel);
                            // Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                        }
                    }
                    break;

                    case ISD_Power_HighUnstable:
                    {
                    }
                    break;

                    case ISD_Power_LowStable:  // 증가가 가능할 때까지 증가. 증가가 더이상 불가능한 경우. 상태 유지
                    {
                        // 전송 파워 감소 시킴
                        if (TxPowerLevel < MaxVoltageControlValue)
                        {
                            TxPowerLevel++;

                            // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                            write_change_TxPowerLevel(TxPowerLevel);
                            // Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                        }
                    }
                    break;

                    case ISD_Power_LowUnstable:  // 증가가 가능할 때까지 증가. 증가가 더이상 불가능한 경우. 연결 상태 끊고 다시 시작.
                    {
                    }
                    break;
#endif

                    case NoBacktel:
                    {
#if 0
                        read_FPGA_systemResgister_1st(&temp);
                        read_FPGA_systemError_Flag(&temp);
                        read_FPGA_backtelError_Flag(&temp);
                        read_FPGA_PulseWidth(&temp);
                        read_FPGA_FIFO_counter(&temp);
#endif
                        errorCodeUpdate(en__EN__ISD_ERROR, en__BackTelCounterZero, __LINE__);
                        change_isd_state(en__isdStatus_FPGA_Ok);  // 내부기 파워 조정 시작

                        TDC_PRINTF_W("[MAPPING] NO BACKTEL DURING MAPPING LINK CONNECTION \r\n");
                    }
                    break;

                    case BackTelNumTooMuch:
                    {
                        errorCodeUpdate(en__EN__ISD_ERROR, en__BackterDataLengthError, __LINE__);
                        change_isd_state(en__isdStatus_FPGA_Ok);  // 내부기 파워 조정 시작
                        TDC_PRINTF_W("[MAPPING] BACKTEL TOO MUCH DURING MAPPING LINK CONNECTOIN \r\n");
                    }
                    break;
                }

                if (!write_FPGA_clear_FIFO())
                {
                    change_isd_state(en__isdStatus_PowerIC_OK);
                }

                change_i2c_is_free();

                isINGconnectionCheck_WithMapping = false;
            }
            break;

            default:
            {
            }
            break;
        }
    }

    flowCounter++;

#else

        //update_isd_Link_is_Connected();
        isINGconnectionCheck_WithMapping=false;
#endif
}

bool isING_connectionCheckWithMapping(void)
{
    return isINGconnectionCheck_WithMapping;
}



