
#include <ci_uart.h>
#include <hw.h>
#include <stdbool.h>

#include "error.h"
#include "cfx_cm3_sharedMemory.h"

#include "driver_MIS2DH.h"
#include "driver_SPI.h"
#include "driver_cfx_i2c.h"
#include "isd_interface.h"
#include "remoteControl.h"
#include "mappingControl.h"

#include "LedOutput.h"
#include "isd_interface_stimulationStandAlone.h"
#include "batteryNPowerControl.h"
#include "earpieceUpdate.h"
#include "processorDirective.h"
#include "systemControl.h"

#include <ci_printf.h>

ST__SYSTEM_STATE systemStatus = {en__LED_NA, false, false, false, false, false};

#define LED_OnTime_afterCoverClosed 4501

#define BLE_OffTimeAfterISD_Disconnected 1000

void NRF_Off_Command(void)
{
    Sys_GPIO_Set_Low(DIO_NUM_NRF_ON_OFF_COMMAND);
}

void NRF_On_Command(void)
{
    Sys_GPIO_Set_High(DIO_NUM_NRF_ON_OFF_COMMAND);
}

bool ISD_ConnectionHistory = false;

// 수정 필요함.. 리모콘 쪽 연결 끊김. 리모콘 연결 상태 및 타이머 필요할 듯
void NRF_On_OFF(ST__ISD_STATUS isd_state, bool global_BLE_Off, bool mappingConnection, bool Mapping_BLE_Off)
{
    static int deaylCounter = 0;
    bool       BLE_OFF      = false;

    if (isd_state.isd_controlState >= en__isdStatus_stimul_10V_Ok)
    {
        ISD_ConnectionHistory = true;
    }

    if (ISD_ConnectionHistory)
    {
        // 내부기와 연결되고 일정 시간이 지난 후에 BLE를 끈다.
        // 연결된 내부기의 id에 해당하는 매핑데이터(내부기 이름)이 읽어 들여진 이후에 nrf를 켜야한다.
        if (isd_state.isd_controlState < en__isdStatus_stimul_10V_Ok)
        {
            if (deaylCounter > 640)  // 0.6초 후 BLE 끔
            {
                BLE_OFF               = true;
                ISD_ConnectionHistory = false;
            }

            deaylCounter++;
        }
        else
        {
            deaylCounter = 0;
        }
    }
    else  // 내부기가 연결되지 않은 초기 상태 또는 끊어지고 일정 시간이 지난 이후에는 NRF를 끈다.
    {
        BLE_OFF = true;
    }

    //  매핑이 연결되어 있으면 내부기 연결이 끊어지더라도 ble를 끄지 않는다.
    if (mappingConnection)
    {
        BLE_OFF = false;
    }

    // 매핑에서  BLE를 잠시 껐다가 켜는 경우(최초 내부기 이름 설정)
    if (Mapping_BLE_Off)
    {
        BLE_OFF = true;
    }

    //
    if (global_BLE_Off)
    {
        BLE_OFF = true;
    }

    if (BLE_OFF)
    {
        NRF_Off_Command();
    }
    else
    {
        NRF_On_Command();
    }
}

#if 0
void NRF_adv_powerMode(bool mode)
{
    if(mode)
        Sys_GPIO_Set_High(ENABLE_NRF_ADV_LowPower);
    else
        Sys_GPIO_Set_Low(ENABLE_NRF_ADV_LowPower);
}
#endif

// 함수 밖 static int xxxxyyyy=5; ==> .data
// 함수 밖 static int xxxxyyyy=1; ==> .data
// 함수 밖 static int xxxxyyyy=0; ===>.bss
// 함수 밖 static int xxxxyyyy;  ==> .bss

// 함수 밖 static  bool xxxxyyyy=true;  ==>.data
// 함수 밖 static  bool xxxxyyyy=false;  ==> .bss
// 함수 밖 static  bool xxxxyyyy; ==> .bss

// 전역 int xxxxyyy=1;   ==>.data
// 전역 int xxxxyyy=0;   ==>.bss
// 전역 int xxxxyyy;     ==>.bss

// 전역 bool xxxxyyyy=true;  ==>.data
// 전역 bool xxxxyyyy=false; ==>.bss
// 전역 bool xxxxyyyy;    ==>.bss

ST__SYSTEM_STATE systemControl(EN__LED_PATTERN   current_led_pattern,
                               ST__ERROR_CODE    mcuErrorCode,
                               ST__USB_CONNECTOR chargerState,
                               EN__BATTERY_LEVEL batteryLevel,
                               bool              powerButtonPushed,
                               bool              conneded_ISD,
                               bool              mappingConnected)
{
    // static int xxxxyyyy;

    // 함수 안  static int xxxxyyyy=1; ===>.data
    // 함수 안  static int xxxxyyyy=0; ===>.bss
    // 함수 안  static int xxxxyyyy; ===>.bss

    // 함수 안  static bool xxxxyyyy=true; ===>.data
    // 함수 안  static bool xxxxyyyy=false; ===>.bss
    // 함수 안  static bool xxxxyyyy; ===>.bss

    static int PowerOn_StartCounter                = 0;
    static int PowerOff_StartCounter               = 0;
    static int normalModeCounter                   = 0;
    static int CounterAfterCoverClosed             = 0;
    static int ISD_Disconnection_counter           = Df_Disconnection_BLE_Time_ms;
    static int lowBatteryIndicatorCounter          = 0;
    static int prev_batteryChargerConnectionStatus = df_Defalut;

    static bool StartFlag         = false;
    static bool isPowerOffEnabled = false;

    bool veryLowBattery     = false;
    bool stimulationTrigger = false;

    if ((mcuErrorCode.dataProcessingErrorFlag == en__NA)         //
        && (mcuErrorCode.accelerometerErrorFlag == en__NA)       //
        && (mcuErrorCode.FPGA_CommunicationErrorFlag == en__NA)  // &&(mcuErrorCode.PowerIcErrorFlag==en__NA))
        && (mcuErrorCode.data_logging_error == en__NA))
    {
        // 충전기가 꼽히면 하드웨어적으로 리셋이 된다. 따라서 가장 먼저 여기로 들어오게 된다.
        if (chargerState.chargerConnectorPluggedIn == df_Defalut)
        {
            systemStatus.Led_Pattern = en__LED_NA;
        }
        // 완충이 되어도 charging connection 은 항상 유지되는지 확인
        else if (chargerState.chargerConnectorPluggedIn == df_Connected)
        {
            // NRF를 꺼진 상태로 변경 유지
            systemStatus.BLE_Off = true;

            // 상시전원을 제외하고 전원을 끄도록 CFX에 전달
            systemStatus.enablePMIC = false;

            // ci_printf("[CHARGER] CONNECTOR PLUGGED IN \r\n");

            //
            changePcmOutputMode(PcmBitStream_Mode_FillZero);

            if (chargerState.carryingCasePluggedIn == df_Connected)  // 충전 케이스가 연결된 경우
            {
                // ci_printf("[CHARGER] CASE PLUGGED IN \r\n");

                // 충전기 케이이 커버가 열린 경우에만 LED를 켠다.
                if (chargerState.carryingCaseCoverOpen == df_Connected)
                {
                    // ci_printf("[CHARGER] COVER OPENED \r\n");

                    CounterAfterCoverClosed = 0;

                    switch (batteryLevel)
                    {
                        case en__batteryPower_0per:
                            systemStatus.Led_Pattern = en__LED_BatteryChargingLevel_0per;
                            break;
                        case en__batteryPower_0btw20:
                            systemStatus.Led_Pattern = en__LED_BatteryChargingLevel_0btw20;
                            break;
                        case en__batteryPower_20btw40:
                            systemStatus.Led_Pattern = en__LED_BatteryChargingLevel_20btw40;
                            break;
                        case en__batteryPower_40btw60:
                            systemStatus.Led_Pattern = en__LED_BatteryChargingLevel_40btw60;
                            break;
                        case en__batteryPower_60btw80:
                            systemStatus.Led_Pattern = en__LED_BatteryChargingLevel_60btw80;
                            break;
                        case en__batteryPower_80btw100:
                            systemStatus.Led_Pattern = en__LED_BatteryChargingLevel_80btw100;
                            break;
                        case en__batteryPower_100per:
                            systemStatus.Led_Pattern = en__LED_BatteryChargingLevel_100per;
                            break;
                    }
                }
                else
                {
                    // ci_printf("[CHARGER] COVER CLOSED \r\n");

                    if (CounterAfterCoverClosed == LED_OnTime_afterCoverClosed)
                    {
                        systemStatus.Led_Pattern = en__LED_NA;
                    }
                    else if (CounterAfterCoverClosed > LED_OnTime_afterCoverClosed)
                    {
                        CounterAfterCoverClosed = 0;
                        // 저전력 모드 진입하도록 CFX전달
                        systemStatus.systemOff  = true;
                        CounterAfterCoverClosed = 0;

                        ci_printi("[SYSTEM] GO TO SYSTEM OFF \r\n");
                    }
                    CounterAfterCoverClosed++;
                }
            }
            else  // 충전 케이스가 연결되지 않고 자극기에 직접 충전기가 꼽힌 경우.
            {
                CounterAfterCoverClosed = 0;

                switch (batteryLevel)
                {
                    case en__batteryPower_0per:
                        systemStatus.Led_Pattern = en__LED_BatteryChargingLevel_0per;
                        break;
                    case en__batteryPower_0btw20:
                        systemStatus.Led_Pattern = en__LED_BatteryChargingLevel_0btw20;
                        break;
                    case en__batteryPower_20btw40:
                        systemStatus.Led_Pattern = en__LED_BatteryChargingLevel_20btw40;
                        break;
                    case en__batteryPower_40btw60:
                        systemStatus.Led_Pattern = en__LED_BatteryChargingLevel_40btw60;
                        break;
                    case en__batteryPower_60btw80:
                        systemStatus.Led_Pattern = en__LED_BatteryChargingLevel_60btw80;
                        break;
                    case en__batteryPower_80btw100:
                        systemStatus.Led_Pattern = en__LED_BatteryChargingLevel_80btw100;
                        break;
                    case en__batteryPower_100per:
                        systemStatus.Led_Pattern = en__LED_BatteryChargingLevel_100per;
                        break;
                }
            }

            StartFlag = false;
        }
        else if (chargerState.chargerConnectorPluggedIn == df_Disconnected)  // 충전기가 연겨되지 않은 상태.
        {
            systemStatus.BLE_Off = false;

            // 전원이 켜지고 충전기가 연결되지 않은 상태
            // 충충전기의 연결이 끊어지면 저전력 모드로 진입.
            if (prev_batteryChargerConnectionStatus == chargerState.carryingCasePluggedIn)
            {
                // update_CM3Status_toCFX(__LINE__);

                if (StartFlag == false)
                {
                    turnOffLED();
                    systemStatus.Led_Pattern = en__LED_POWER_On;
                    PowerOn_StartCounter     = 0;
                    StartFlag                = true;
                    ci_printi("[SYSTEM] LED PATTERN IS POWER ON \r\n");
                }
                else
                {
                    if (current_led_pattern != en__LED_POWER_On)
                    {
                        systemStatus.enable_ISD = true;
                        systemStatus.enablePMIC = true;
                        normalModeCounter++;
                        normalModeCounter = normalModeCounter & 0xFFFF;

                        // 매핑이 연결되어있으면 전원 버튼은 무시한다.
                        if (mappingConnected)
                        {
                            powerButtonPushed = false;
                        }
                        else if (ISD_Disconnection_counter < 300)
                        {
                            powerButtonPushed = false;
                        }

                        // 배터리 방전 상태 확인
                        if (batteryLevel == en__batteryPower_0per)
                        {
                            veryLowBattery = true;
                        }
                        else
                        {
                            veryLowBattery = false;
                        }

                        // 전원 끄기 시작
                        if (veryLowBattery || powerButtonPushed)  // 전원 끄기
                        {
                            if (isPowerOffEnabled == false)  // 1회 설정
                            {
                                if (veryLowBattery)
                                {
                                    ci_printw("[SYSTEM] VERY LOW BATTERY \r\n");
                                }

                                if (powerButtonPushed)
                                {
                                    ci_printd("[SYSTEM] POWER BUTTON PUSHED \r\n");
                                }

                                systemStatus.Led_Pattern = en__LED_POWER_Off;
                                systemStatus.enable_ISD  = false;
                                PowerOff_StartCounter    = 0;
                                isPowerOffEnabled        = true;

                                ci_printi("[SYSTEM] LED PATTERN IS POWER OFF \r\n");
                            }
                        }

                        if (isPowerOffEnabled)
                        {
                            // LED 출력이 완료되었다.
                            if ((current_led_pattern != en__LED_POWER_Off) && (PowerOff_StartCounter != 0))
                            {
                                isPowerOffEnabled         = false;
                                ISD_Disconnection_counter = 0;
                                // update_CM3Status_toCFX(__LINE__);
                                //  상시전원을 제외하고 전원을 끄도록 CFX에 전달
                                systemStatus.enablePMIC = false;
                                // 저전력 모드 진입하도록 CFX전달
                                systemStatus.systemOff = true;
                                ci_printi("[SYSTEM] GO TO SYSTEM OFF \r\n");
                            }
                            else
                            {
                                // 내부기 부착 과정에서 버튼이 활성화 되어서 전원이 꺼지는 루틴에 들어왔을 경우, 탈출
                                if (!veryLowBattery)
                                {
                                    if (conneded_ISD)
                                    {
                                        isPowerOffEnabled = false;
                                    }
                                }
                            }

                            PowerOff_StartCounter++;
                        }
                        else
                        {
                            if (mappingConnected)
                            {
                                if (conneded_ISD)
                                {
                                    if (batteryLevel > en__batteryPower_0btw20)
                                    {
                                        systemStatus.Led_Pattern = en__LED_MappingConneted_ISD_Connected_BatteryNormal;
                                    }
                                    else
                                    {
                                        systemStatus.Led_Pattern = en__LED_MappingConneted_ISD_Connected_BatteryLow;
                                    }
                                }
                                else
                                {
                                    if (batteryLevel > en__batteryPower_0btw20)
                                    {
                                        systemStatus.Led_Pattern = en__LED_MappingConneted_ISD_Unconnected_BatteryNormal;
                                    }
                                    else
                                    {
                                        systemStatus.Led_Pattern = en__LED_MappingConneted_ISD_Unconnected_BatteryLow;
                                    }
                                }
                            }
                            else  // 매핑이 연결되지 않았을 경우
                            {
                                if (conneded_ISD)
                                {
                                    // LED 출력 및 NRF 칩 설정
                                    if (batteryLevel > en__batteryPower_0btw20)
                                    {
                                        systemStatus.Led_Pattern = en__LED_ISD_StimulationOut_batteryNormal;

                                        lowBatteryIndicatorCounter = 0;
                                    }
                                    else
                                    {
                                        systemStatus.Led_Pattern = en__LED_ISD_StimulationOut_batteryLow;
#if 1
                                        if (lowBatteryIndicatorCounter == 0)  // 10분마다 자극 알림을 출력한다.
                                        {
                                            stimulationTrigger         = true;
                                            lowBatteryIndicatorCounter = df_lowbatteryIndicationPeriod_ms;
                                        }
#endif
                                        lowBatteryIndicatorCounter--;
                                    }
                                }
                                else
                                {
                                    // LED 출력 및 NRF 칩 설정
                                    if (batteryLevel > en__batteryPower_0btw20)
                                    {
                                        systemStatus.Led_Pattern   = en__LED_StandbyForconneded_ISD_batteryNormal;
                                        lowBatteryIndicatorCounter = 0;
                                    }
                                    else
                                    {
                                        systemStatus.Led_Pattern   = en__LED_StandbyForconneded_ISD_batteryLow;
                                        lowBatteryIndicatorCounter = 0;
                                    }
                                }

                                if (conneded_ISD)
                                {
                                    ISD_Disconnection_counter = 0;
                                }
                                else
                                {
                                    ISD_Disconnection_counter++;
                                }

                                if (ISD_Disconnection_counter > 180000)  // 3분
                                {

                                    ISD_Disconnection_counter = 0;

                                    systemStatus.Led_Pattern = en__LED_POWER_Off;
                                    systemStatus.enable_ISD  = false;
                                    PowerOff_StartCounter    = 0;
                                    isPowerOffEnabled        = true;
                                    // update_CM3tempValue2_toCFX(4);
                                }
                                // if (ISD_Disconnection_counter>1800000) // 30분

                            }  // 매핑.연결
                        }
                        ////
                    }
                }
                PowerOn_StartCounter++;
            }
            else  // 충전기가 꼽혔다가 빠진 상태.
            {
                if (prev_batteryChargerConnectionStatus == df_Disconnected)
                {
                    // 상시전원을 제외하고 전원을 끄도록 CFX에 전달
                    systemStatus.enablePMIC = false;
                    // 저전력 모드 진입하도록 CFX전달
                    systemStatus.systemOff = true;
                    ci_printi("[SYSTEM] GO TO SYSTEM OFF \r\n");
                }
            }

        }  // end battery Charging
        else
        {
        }
        prev_batteryChargerConnectionStatus = chargerState.carryingCasePluggedIn;

    }  // 에러
    else
    {
        // 에러 코드 확인..
        systemStatus.Led_Pattern=en__LED_MCU_Error;

        if(mcuErrorCode.dataProcessingErrorFlag!=en__NA)
        {
            systemStatus.Led_Pattern=en__LED_Map_Error;
        }
    }

    systemStatus.StimulationIndicatorTriggerLowPower=stimulationTrigger;

    return systemStatus;
}
