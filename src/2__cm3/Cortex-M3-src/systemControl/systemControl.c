
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

#ifdef ENABLE_UI_CMD
#include "tdc_ui_command.h"
#endif

#include "tdc_touch.h"

#include <ci_printf.h>

ST__SYSTEM_STATE systemStatus = {en__LED_NA, false, false, false, false, false, false};

#define LED_OnTime_afterCoverClosed 4501

#define BLE_OffTimeAfterISD_Disconnected 1000

void NRF_Off_Command(void)
{
    // Sys_GPIO_Set_Low(DIO_NUM_NRF_ON_OFF_COMMAND);
}

void NRF_On_Command(void)
{
    // Sys_GPIO_Set_High(DIO_NUM_NRF_ON_OFF_COMMAND);
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
        // NRF_Off_Command();
    }
    else
    {
        // NRF_On_Command();
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

ST__SYSTEM_STATE systemControl(EN__LED_PATTERN   current_led_pattern,  //
                               ST__ERROR_CODE    mcuErrorCode,
                               ST__USB_CONNECTOR chargerState,
                               int               battery_percent,
                               bool              powerButtonPushed,
                               bool              conneded_ISD,
                               bool              mappingConnected)
{
    static int PowerOn_StartCounter                = 0;
    static int PowerOff_StartCounter               = 0;
    static int normalModeCounter                   = 0;
    static int  CounterAfterCoverClosed             = 0;
    static bool s_tdc_cradle_cover_closed_edge      = false;
    static int ISD_Disconnection_counter           = Df_Disconnection_BLE_Time_ms;
    static int lowBatteryIndicatorCounter          = 0;
    static int prev_batteryChargerConnectionStatus = df_Defalut;

    static bool StartFlag         = false;
    static bool isPowerOffEnabled = false;

    bool veryLowBattery     = false;
    bool stimulationTrigger = false;

    if ((mcuErrorCode.dataProcessingErrorFlag == en__NA)         //
        && (mcuErrorCode.accelerometerErrorFlag == en__NA)       //
        && (mcuErrorCode.FPGA_CommunicationErrorFlag == en__NA)  //
        && (mcuErrorCode.data_logging_error == en__NA))
    {
        /* 에러 해제 시 ERROR 소스 클리어 */
#ifdef ENABLE_UI_CMD
        if (!tdc_ui_command_is_led_override(LED_SRC_ERROR))
#endif
            led_request(LED_SRC_ERROR, LED_ST_NONE);

        // 충전기가 꼽히면 하드웨어적으로 리셋이 된다. 따라서 가장 먼저 여기로 들어오게 된다.
        if (chargerState.chargerConnectorPluggedIn == df_Defalut)
        {
            systemStatus.Led_Pattern = en__LED_NA;
        }
        // 충전기가 연결된 상태 -- LED 충전 레벨 표시 폐지 (Rev.3 이슈 #1)
        else if (chargerState.chargerConnectorPluggedIn == df_Connected)
        {
            // NRF를 꺼진 상태로 변경 유지
            systemStatus.BLE_Off = true;

            // 상시전원을 제외하고 전원을 끄도록 CFX에 전달
            systemStatus.enablePMIC = false;

            changePcmOutputMode(PcmBitStream_Mode_FillZero);

            if (chargerState.carryingCasePluggedIn == df_Connected)  // 충전 케이스가 연결된 경우
            {
                if (chargerState.carryingCaseCoverOpen == df_Connected)
                {
                    CounterAfterCoverClosed        = 0;
                    s_tdc_cradle_cover_closed_edge = false;  /* 뚜껑 열림 → 엣지 플래그 리셋 */
                    /* 충전 중 LED: 배터리 레벨 판정은 Arbiter가 처리 (led_request 불필요) */
                }
                else
                {
                    if (!s_tdc_cradle_cover_closed_edge)
                    {
                        s_tdc_cradle_cover_closed_edge = true;
                        CounterAfterCoverClosed        = 0;
                        systemStatus.cradleLidClosed   = true;
                        ci_printi("[SYSTEM] CRADLE LID CLOSED FIRST DETECT\r\n");
                    }
                }
            }
            else  // 충전 케이스가 연결되지 않고 자극기에 직접 충전기가 꼽힌 경우.
            {
                CounterAfterCoverClosed = 0;
                /* 충전 중 LED: 배터리 레벨 판정은 Arbiter가 처리 */
            }

#if TDC_DBG_LONG_TOUCH_IGNORE_LED
            if (powerButtonPushed) { led_request(LED_SRC_DBG, LED_ST_DBG_LONG_TOUCH_IGNORE); }
#endif
            StartFlag = false;
        }
        else if (chargerState.chargerConnectorPluggedIn == df_Disconnected)  // 충전기가 연결되지 않은 상태.
        {
            systemStatus.BLE_Off = false;

            if (prev_batteryChargerConnectionStatus == chargerState.carryingCasePluggedIn)
            {
                if (StartFlag == false)
                {
                    /* turnOffLED · led_request(POWER, POWER_ON) · tdc_touch_init_begin
                     * 은 Initialize() P3-Early 에서 직접 수행 (Rev.4 이관). 본 분기는
                     * 부팅 후 첫 진입 시 systemStatus 마커와 카운터만 셋업.
                     * StartFlag · PowerOn_StartCounter 변수 자체 정리는 별도 cleanup
                     * 작업으로 위임 (사용처 dead 확인됨). */
                    systemStatus.Led_Pattern = en__LED_POWER_On;

                    PowerOn_StartCounter = 0;
                    StartFlag            = true;
                    ci_printi("[SYSTEM] FIRST POWER-ON SYSTEM CONTROL TICK \r\n");
                }
                else
                {
                    /* burst pending flag 직접 조회 - `current_led_pattern` 은 LED arbiter
                     * ISR 가 갱신하므로 main loop iter 와 1-tick stale race 가능 (커밋
                     * cecbc3d 의 ISR 책임 분리로 노출). pending flag 는 `led_request()`
                     * 가 set, `led_engine_run()` burst 완료 시 clear → timer/tick 무관 정확. */
                    if (!tdc_led_is_burst_pending())
                    {
                        systemStatus.enable_ISD = true;
                        systemStatus.enablePMIC = true;
                        normalModeCounter++;
                        normalModeCounter = normalModeCounter & 0xFFFF;

                        // 매핑이 연결되어있으면 전원 버튼은 무시한다.
                        if (mappingConnected)
                        {
#if TDC_DBG_LONG_TOUCH_IGNORE_LED
                            if (powerButtonPushed) { led_request(LED_SRC_DBG, LED_ST_DBG_LONG_TOUCH_IGNORE); }
#endif
                            powerButtonPushed = false;
                        }
                        else if (ISD_Disconnection_counter < 300)
                        {
#if TDC_DBG_LONG_TOUCH_IGNORE_LED
                            if (powerButtonPushed) { led_request(LED_SRC_DBG, LED_ST_DBG_LONG_TOUCH_IGNORE); }
#endif
                            powerButtonPushed = false;
                        }

                        // 배터리 방전 상태 확인 (전기기계적안정성 시험을 위해 저전력 범위 변경)
                        // 40per 미만이면 저전력 (구: EN__BATTERY_LEVEL 0per/0btw20/20btw40 비교 → percent 직접 비교)
                        if (battery_percent < 40)
                        {
                            veryLowBattery = true;
                        }
                        else
                        {
                            veryLowBattery = false;
                        }

                        // 전원 끄기 시작
                        if (veryLowBattery || powerButtonPushed)
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
                                led_request(LED_SRC_POWER, LED_ST_POWER_OFF);
                                systemStatus.enable_ISD = false;
                                PowerOff_StartCounter   = 0;
                                isPowerOffEnabled       = true;

                                ci_printi("[SYSTEM] LED PATTERN IS POWER OFF \r\n");
                            }
                        }

                        if (isPowerOffEnabled)
                        {
                            /* burst 종료 검출 - burst pending flag 직접 조회.
                             * pending flag set/clear 책임 분리: `led_request()` 가 요청 시점
                             * 즉시 set, `led_engine_run()` 이 burst 자가 해제 시 clear.
                             * timer/tick 무관 정확. (`current_led_pattern` 은 LED arbiter ISR
                             * 갱신이라 main loop iter 와 stale race 가능 - 부정확.) */
                            if (!tdc_led_is_burst_pending() && (PowerOff_StartCounter != 0))
                            {
                                isPowerOffEnabled         = false;
                                ISD_Disconnection_counter = 0;
                                systemStatus.enablePMIC   = false;
                                systemStatus.systemOff    = true;
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
                            /* ===================================================
                             * LED 판정은 Arbiter로 이관됨 (Rev.3)
                             * Battery/ISD/Mapping 요청은 main.c에서 led_request() 호출
                             * =================================================== */

                            /* 저배터리 자극 알림 (10분 주기) -- LED와 독립된 기능 */
                            /* 20per 미만 (구: batteryLevel <= en__batteryPower_0btw20 → percent 직접 비교) */
                            if (conneded_ISD && battery_percent < 20)
                            {
                                if (lowBatteryIndicatorCounter == 0)
                                {
                                    stimulationTrigger         = true;
                                    lowBatteryIndicatorCounter = df_lowbatteryIndicationPeriod_ms;
                                }
                                lowBatteryIndicatorCounter--;
                            }
                            else
                            {
                                lowBatteryIndicatorCounter = 0;
                            }

                            if (conneded_ISD)
                            {
                                ISD_Disconnection_counter = 0;
                            }
                            else
                            {
                                ISD_Disconnection_counter++;  // 내부기 미연결 시 해제 카운트 증가하는 부분
                            }

                            if (ISD_Disconnection_counter > 180000)  // 3분
                            {
                                ISD_Disconnection_counter = 0;

                                systemStatus.Led_Pattern = en__LED_POWER_Off;
                                led_request(LED_SRC_POWER, LED_ST_POWER_OFF);
                                systemStatus.enable_ISD = false;
                                PowerOff_StartCounter   = 0;
                                isPowerOffEnabled       = true;
                            }
                        }
                    }
                }
                PowerOn_StartCounter++;
            }
            else  // 충전기가 꼽혔다가 빠진 상태.
            {
                if (prev_batteryChargerConnectionStatus == df_Disconnected)
                {
                    systemStatus.enablePMIC = false;
                    systemStatus.systemOff  = true;
                    ci_printi("[SYSTEM] GO TO SYSTEM OFF \r\n");
                }
            }

        }  // end battery Charging
        else
        {
        }
        prev_batteryChargerConnectionStatus = chargerState.carryingCasePluggedIn;

    }  // 에러 없음
    else
    {
        /* 에러 발생 -- led_request(ERROR, ...) (Rev.3 SS4.2) */
#ifdef ENABLE_UI_CMD
        if (!tdc_ui_command_is_led_override(LED_SRC_ERROR))
        {
#endif
            led_request(LED_SRC_ERROR, LED_ST_ERROR_MCU);
            systemStatus.Led_Pattern = en__LED_MCU_Error;

            if (mcuErrorCode.dataProcessingErrorFlag != en__NA)
            {
                led_request(LED_SRC_ERROR, LED_ST_ERROR_MAP);
                systemStatus.Led_Pattern = en__LED_Map_Error;
            }
            if (mcuErrorCode.accelerometerErrorFlag != en__NA)
            {
                led_request(LED_SRC_ERROR, LED_ST_ERROR_ACCEL);
            }
            if (mcuErrorCode.FPGA_CommunicationErrorFlag != en__NA)
            {
                led_request(LED_SRC_ERROR, LED_ST_ERROR_FPGA);
            }
#ifdef ENABLE_UI_CMD
        }
#endif
    }

    systemStatus.StimulationIndicatorTriggerLowPower = stimulationTrigger;

    return systemStatus;
}
