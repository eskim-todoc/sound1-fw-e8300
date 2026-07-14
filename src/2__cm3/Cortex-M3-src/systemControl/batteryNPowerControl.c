
#include <stdbool.h>

#include <hw.h>
#include <stdbool.h>
#include "FPGA.h"

#include "cfx_cm3_sharedMemory.h"
#include "batteryNPowerControl.h"
#include "commonDataProcessing.h"
#include "isd_interface.h"

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Sound1
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// 배터리 관련
volatile int                s_snd_batt_percent = 0;
volatile EN__SND_BATT_STATE s_snd_batt_state   = EN__SND_BATT_STATE_RESET;

// 충전 상태 관련
volatile EN__SND_CHARGER_STATE s_snd_charger_state = EN__SND_BATT_STATE_RESET;

// 크래들 뚜껑 상태
static int s_tdc_cradle_cover_state = df_Defalut;

EN__SND_BATT_STATE snd_batt_get_state(void)
{
    return s_snd_batt_state;
}

void snd_batt_set_state(EN__SND_BATT_STATE state)
{
    s_snd_batt_state = state;
}

int snd_batt_get_percent(void)
{
    return s_snd_batt_percent;
}

void snd_batt_set_percent(int percent)
{
    s_snd_batt_percent = percent;
}

EN__BATTERY_LEVEL snd_batt_get_level(void)
{
    int percent;

    percent = snd_batt_get_percent();

    // 100
    if (100 <= percent)
    {
        return en__batteryPower_100per;
    }
    // 80 ~ 99
    else if ((80 <= percent) && (percent < 100))
    {
        return en__batteryPower_80btw100;
    }
    // 60 ~ 79
    else if ((60 <= percent) && (percent < 80))
    {
        return en__batteryPower_60btw80;
    }
    // 40 ~ 59
    else if ((40 <= percent) && (percent < 60))
    {
        return en__batteryPower_40btw60;
    }
    // 20 ~ 39
    else if ((20 <= percent) && (percent < 40))
    {
        return en__batteryPower_20btw40;
    }
    // 1 ~ 19
    else if ((0 < percent) && (percent < 20))
    {
        return en__batteryPower_0btw20;
    }
    // 0
    else
    {
        return en__batteryPower_0per;
    }
}

ST__USB_CONNECTOR snd_charger_get_state(void)
{
    return cfx_cm3_sharedMemoryAll.chargerState;
}

void snd_charger_set_state(EN__SND_CHARGER_STATE state)
{
    // 충전 케이블 연결 상태 디버깅 메시지 출력
#if 1
    if (s_snd_charger_state != state)
    {
        ci_printd("[CHARGER] %s -> %s \r\n",
                  // 이전 상태
                  (s_snd_charger_state == EN__SND_CHARGER_STATE_RESET)          ? "RESET"  //
                  : (s_snd_charger_state == EN__SND_CHARGER_STATE_CONNECTED)    ? "CONNECTED"
                  : (s_snd_charger_state == EN__SND_CHARGER_STATE_DISCONNECTED) ? "DISCONNECTED"
                                                                                : "INVALID",
                  // 현재 상태
                  (state == EN__SND_CHARGER_STATE_RESET)          ? "RESET"  //
                  : (state == EN__SND_CHARGER_STATE_CONNECTED)    ? "CONNECTED"
                  : (state == EN__SND_CHARGER_STATE_DISCONNECTED) ? "DISCONNECTED"
                                                                  : "INVALID");
    }
#endif

    s_snd_charger_state = state;

    switch (state)
    {
        case EN__SND_CHARGER_STATE_CONNECTED:
        {
            cfx_cm3_sharedMemoryAll.chargerState.chargerConnectorPluggedIn = df_Connected;
            cfx_cm3_sharedMemoryAll.chargerState.carryingCasePluggedIn     = df_Connected;
            /* carryingCaseCoverOpen: BLE 0x34 data[2] 수신값 유지 — tdc_charger_set_cradle_cover_state()가 관리 */
        }
        break;

        case EN__SND_CHARGER_STATE_DISCONNECTED:
        {
            cfx_cm3_sharedMemoryAll.chargerState.chargerConnectorPluggedIn = df_Disconnected;
            cfx_cm3_sharedMemoryAll.chargerState.carryingCasePluggedIn     = df_Disconnected;
            /* carryingCaseCoverOpen: BLE 0x34 data[2] 수신값 유지 — tdc_charger_set_cradle_cover_state()가 관리 */
        }
        break;

        default:  // EN__SND_CHARGER_STATE_RESET
        {
            cfx_cm3_sharedMemoryAll.chargerState.chargerConnectorPluggedIn = df_Defalut;
            cfx_cm3_sharedMemoryAll.chargerState.carryingCasePluggedIn     = df_Defalut;
            /* carryingCaseCoverOpen: BLE 0x34 data[2] 수신값 유지 — tdc_charger_set_cradle_cover_state()가 관리 */
        }
        break;
    }
}

void tdc_charger_set_cradle_cover_state(int state)
{
    if (state == 2)
    {
        s_tdc_cradle_cover_state = df_Disconnected;  /* df_Closed == 2 */
    }
    else
    {
        s_tdc_cradle_cover_state = df_Connected;  /* 1=열림, else=열림 처리 */
    }
    cfx_cm3_sharedMemoryAll.chargerState.carryingCaseCoverOpen = s_tdc_cradle_cover_state;
    ci_printd("[CRADLE] COVER STATE: %s\r\n", (s_tdc_cradle_cover_state == df_Connected) ? "OPENED" : "CLOSED");
}

int tdc_cradle_get_cover_state(void)
{
    return s_tdc_cradle_cover_state;
}

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Sullivan
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// 전압 분배 배율
// LSAD 값 = 0~2V를  0~511로 표현

#define df_integerGain 1024  // 2^10

#define df_calibrationVotage 4  // 2^2

#if 0
#define df_battery_boundary_0per_voltage  3072  //(3.0*df_integerGain)     //0
#define df_battery_boundary_20per_voltage 3481  //(3.4*df_integerGain)     //20
#define df_battery_boundary_40per_voltage 3686  //(3.6*df_integerGain)     //40
#define df_battery_boundary_60per_voltage 3891  //(3.8*df_integerGain)     //60
#define df_battery_boundary_80per_voltage 3993  //(3.9*df_integerGain)     //80
#endif

// Full : 4.16, 100: 4.12, 80 : 3.92, 60:3.74, 40 : 3.62  20: 3.53 0: 3.36

#define df_battery_boundary_0per_voltage   3440  //(3.36 * df_integerGain)        //0
#define df_battery_boundary_20per_voltage  3614  //(3.53 * df_integerGain)        //20
#define df_battery_boundary_40per_voltage  3706  //(3.62 * df_integerGain)        //40
#define df_battery_boundary_60per_voltage  3829  //(3.74 * df_integerGain)        //60
#define df_battery_boundary_80per_voltage  4014  //(3.92 * df_integerGain)        //80
#define df_battery_boundary_100per_voltage 4194  //(4.096* df_integerGain)      //100

#define df_battery_boundary_0per_voltage_charging   3584  //(3.50*df_integerGain)        //0
#define df_battery_boundary_20per_voltage_charging  3686  //(3.60*df_integerGain)        //20
#define df_battery_boundary_40per_voltage_charging  3758  //(3.67*df_integerGain)        //40
#define df_battery_boundary_60per_voltage_charging  3880  //(3.3.79*df_integerGain)      //60
#define df_battery_boundary_80per_voltage_charging  4065  //(3.97*df_integerGain)        //80
#define df_battery_boundary_100per_voltage_charging 4218  //(4.12*df_integerGain)        //100

typedef struct
{
    int battery_boundary_0per;
    int battery_boundary_20per;
    int battery_boundary_40per;
    int battery_boundary_60per;
    int battery_boundary_80per;
    int battery_boundary_100per;

} ST__BATTERY_BOUNDARY;

typedef struct
{
    ST__BATTERY_BOUNDARY dischargingBatterBoundary;
    ST__BATTERY_BOUNDARY chargingBatterBoundary;

} ST__SYSTEM_BATTERY_BOUNDARY;

ST__SYSTEM_BATTERY_BOUNDARY batteryBoundary;

int calculated_3V_value;

volatile EN__BATTERY_LEVEL batteryLevelStatus      = en__batteryPower_100per;
volatile EN__BATTERY_LEVEL prev_batteryLevelStatus = en__batteryPower_100per;

void calculationBatteryBoundary(void)
{
    int tempValueA;
    int tempValueB;
    int mesured4V_value;

    batteryLevelStatus      = en__batteryPower_100per;
    prev_batteryLevelStatus = en__batteryPower_100per;

    mesured4V_value = readBatteryCalibrationValue();  // 4v 전압을 인가했을 때 측정된 값(보드 교정 시)

    ///

    //    4*df_integerGain : mesured4V_value = boundary_0per_voltag : x
    //==> x = boundary_0per_voltag*df_integerGain:mesured4V_value/(4*df_integerGain)

    // 충전중이 아닐때 배터리 경계값
    // battery_boundary_0per
    tempValueA                                                      = (int) df_battery_boundary_0per_voltage;
    tempValueB                                                      = tempValueA * mesured4V_value;
    batteryBoundary.dischargingBatterBoundary.battery_boundary_0per = tempValueB >> 12;  // 1024*4 =2^12

    ci_printv("[LSAD] BATTERY DISCHARGING : 3.3600V (A=%u, B=%u,   0%%=%u) \r\n", tempValueA, tempValueB, batteryBoundary.dischargingBatterBoundary.battery_boundary_0per);

    // battery_boundary_20per
    tempValueA                                                       = (int) df_battery_boundary_20per_voltage;
    tempValueB                                                       = tempValueA * mesured4V_value;
    batteryBoundary.dischargingBatterBoundary.battery_boundary_20per = tempValueB >> 12;  // 1024*4 =2^12

    ci_printv("[LSAD] BATTERY DISCHARGING : 3.5300V (A=%u, B=%u,  20%%=%u) \r\n", tempValueA, tempValueB, batteryBoundary.dischargingBatterBoundary.battery_boundary_20per);

    // battery_boundary_40per
    tempValueA                                                       = (int) df_battery_boundary_40per_voltage;
    tempValueB                                                       = tempValueA * mesured4V_value;
    batteryBoundary.dischargingBatterBoundary.battery_boundary_40per = tempValueB >> 12;  // 1024*4 =2^12

    ci_printv("[LSAD] BATTERY DISCHARGING : 3.6200V (A=%u, B=%u,  40%%=%u) \r\n", tempValueA, tempValueB, batteryBoundary.dischargingBatterBoundary.battery_boundary_40per);

    // battery_boundary_60per
    tempValueA                                                       = (int) df_battery_boundary_60per_voltage;
    tempValueB                                                       = tempValueA * mesured4V_value;
    batteryBoundary.dischargingBatterBoundary.battery_boundary_60per = tempValueB >> 12;  // 1024*4 =2^12

    ci_printv("[LSAD] BATTERY DISCHARGING : 3.7400V (A=%u, B=%u,  60%%=%u) \r\n", tempValueA, tempValueB, batteryBoundary.dischargingBatterBoundary.battery_boundary_60per);

    // battery_boundary_80per
    tempValueA                                                       = (int) df_battery_boundary_80per_voltage;
    tempValueB                                                       = tempValueA * mesured4V_value;
    batteryBoundary.dischargingBatterBoundary.battery_boundary_80per = tempValueB >> 12;  // 1024*4 =2^12

    ci_printv("[LSAD] BATTERY DISCHARGING : 3.9200V (A=%u, B=%u,  80%%=%u) \r\n", tempValueA, tempValueB, batteryBoundary.dischargingBatterBoundary.battery_boundary_80per);

    // battery_boundary_100per
    tempValueA                                                        = (int) df_battery_boundary_100per_voltage;
    tempValueB                                                        = tempValueA * mesured4V_value;
    batteryBoundary.dischargingBatterBoundary.battery_boundary_100per = tempValueB >> 12;  // 1024*4 =2^12

    ci_printv("[LSAD] BATTERY DISCHARGING : 4.0096V (A=%u, B=%u, 100%%=%u) \r\n", tempValueA, tempValueB, batteryBoundary.dischargingBatterBoundary.battery_boundary_100per);

    // 충전중일 때 터리 경계값

    // battery_boundary_0per
    tempValueA                                                   = (int) df_battery_boundary_0per_voltage_charging;
    tempValueB                                                   = tempValueA * mesured4V_value;
    batteryBoundary.chargingBatterBoundary.battery_boundary_0per = tempValueB >> 12;  // 1024*4 =2^12

    ci_printv("[LSAD] BATTERY    CHARGING : 3.5000V (A=%u, B=%u,   0%%=%u) \r\n", tempValueA, tempValueB, batteryBoundary.chargingBatterBoundary.battery_boundary_0per);

    // battery_boundary_20per
    tempValueA                                                    = (int) df_battery_boundary_20per_voltage_charging;
    tempValueB                                                    = tempValueA * mesured4V_value;
    batteryBoundary.chargingBatterBoundary.battery_boundary_20per = tempValueB >> 12;  // 1024*4 =2^12

    ci_printv("[LSAD] BATTERY    CHARGING : 3.6000V (A=%u, B=%u,  20%%=%u) \r\n", tempValueA, tempValueB, batteryBoundary.chargingBatterBoundary.battery_boundary_20per);

    // battery_boundary_40per
    tempValueA                                                    = (int) df_battery_boundary_40per_voltage_charging;
    tempValueB                                                    = tempValueA * mesured4V_value;
    batteryBoundary.chargingBatterBoundary.battery_boundary_40per = tempValueB >> 12;  // 1024*4 =2^12

    ci_printv("[LSAD] BATTERY    CHARGING : 3.6700V (A=%u, B=%u,  40%%=%u) \r\n", tempValueA, tempValueB, batteryBoundary.chargingBatterBoundary.battery_boundary_40per);

    // battery_boundary_60per
    tempValueA                                                    = (int) df_battery_boundary_60per_voltage_charging;
    tempValueB                                                    = tempValueA * mesured4V_value;
    batteryBoundary.chargingBatterBoundary.battery_boundary_60per = tempValueB >> 12;  // 1024*4 =2^12

    ci_printv("[LSAD] BATTERY    CHARGING : 3.7900V (A=%u, B=%u,  60%%=%u) \r\n", tempValueA, tempValueB, batteryBoundary.chargingBatterBoundary.battery_boundary_60per);

    // battery_boundary_80per
    tempValueA                                                    = (int) df_battery_boundary_80per_voltage_charging;
    tempValueB                                                    = tempValueA * mesured4V_value;
    batteryBoundary.chargingBatterBoundary.battery_boundary_80per = tempValueB >> 12;  // 1024*4 =2^12

    ci_printv("[LSAD] BATTERY    CHARGING : 3.9700V (A=%u, B=%u,  80%%=%u) \r\n", tempValueA, tempValueB, batteryBoundary.chargingBatterBoundary.battery_boundary_80per);

    // battery_boundary_100per
    tempValueA                                                     = (int) df_battery_boundary_100per_voltage_charging;
    tempValueB                                                     = tempValueA * mesured4V_value;
    batteryBoundary.chargingBatterBoundary.battery_boundary_100per = tempValueB >> 12;  // 1024*4 =2^12

    ci_printv("[LSAD] BATTERY    CHARGING : 4.1200V (A=%u, B=%u, 100%%=%u) \r\n", tempValueA, tempValueB, batteryBoundary.chargingBatterBoundary.battery_boundary_100per);
}

int battery_percentage;

int readBatteryPercentage(void)
{
    // return battery_percentage;
    return s_snd_batt_percent;
}

/* updateBatteryLevel(): E8300 자체 LSAD 측정 기반 배터리 등급 함수였으나, QCC가 배터리 정보(0x34)를
 * 제공하는 구조로 전환되며 dead code가 되어 제거함(2026-07-14). percent -> LED 등급 매핑은
 * main.c 의 tdc_led_request_battery(경계 테이블)로 이관. 이력: docs/tasks/main/20260714_battery-led-refactor. */

#if defined(Board_is_TD_DEV_ver_1_4) || defined(Board_is_OTE_VER_1_3) || defined(Board_is_OTE_VER_1_4) || defined(Board_is_OTE_VER_1_5)

int read_BatteryChargerConnectinStatus(void)
{

    ST__USB_CONNECTOR chargerState;
    chargerState = readUsbConnectorState();

    return chargerState.chargerConnectorPluggedIn;
}

bool isCarryingCaseConnected(void)
{
    ST__USB_CONNECTOR chargerState;
    chargerState = readUsbConnectorState();

#if 1

    if (chargerState.carryingCasePluggedIn == 1)
    {
        return true;
    }
    else
    {
        return false;
    }

#else
    return false;
#endif
}

bool isCarryingCaseCoverOpen(void)
{

    ST__USB_CONNECTOR chargerState;
    chargerState = readUsbConnectorState();

#if 1

    if (chargerState.carryingCaseCoverOpen == 1)
    {
        return true;
    }
    else
    {
        return false;
    }

#else
    return false;
#endif
}

#else

#define df_batteryChargingCheckInterval_ms 100
static int batteryChargerConnected;

void updateBatteryChargerConnection(void)
{

    int readVal;

    if (read_ISD_by_CM3_I2C(i2cAddr_FPGA_systemResgister_1st, &readVal, 1))
    {

        readVal = readVal >> FPGA_BitPosition_BatteryCharging;  // battery Charger 연결 상태
        readVal = readVal & 0x01;

        batteryChargerConnected = readVal;
    }
}

int read_BatteryChargerConnectinStatus(void)
{

    return batteryChargerConnected;
}

#define df_carringCaseStatusCheckInterval_ms 100
static bool carringCaseStatus = false;

static ST__CARRINGCASE_STATE caringCaseStatus;

void updateCarryingCaseStatus(void)
{
    static int intervalCounter = 0;
    int        value;
    int        readVal;

    if (intervalCounter == 0)
    {

        read_ISD_by_CM3_I2C(i2cAddr_FPGA_systemResgister_1st, &readVal, 1);

        value = data_ExtractionAndRigthShift(readVal, FPGA_BitPosition_CarringCaseOpen, 1);  //

        if (value == 1)
            caringCaseStatus.isCoverOpen = true;
        else
            caringCaseStatus.isCoverOpen = false;

        value = data_ExtractionAndRigthShift(readVal, FPGA_BitPosition_CarringCaseConnecton, 1);  //

        if (value == 1)
            caringCaseStatus.isCarryingCaseConnected = true;
        else
            caringCaseStatus.isCarryingCaseConnected = false;

        intervalCounter = df_carringCaseStatusCheckInterval_ms;
    }

    intervalCounter--;
}

bool isCarryingCaseConnected(void)
{
#if 1
    return caringCaseStatus.isCarryingCaseConnected;
#else
    return false;
#endif
}

bool isCarryingCaseCoverOpen(void)
{

#if 1
    return caringCaseStatus.isCoverOpen;
#else
    return false;
#endif


}

#endif





