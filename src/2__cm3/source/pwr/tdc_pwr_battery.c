
#include <stdbool.h>

#include <hw.h>
#include <stdbool.h>
#include <FPGA.h>

#include <tdc_shm.h>
#include <tdc_pwr_battery.h>
#include <tdc_stim_common.h>
#include <tdc_isd.h>

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Sound1
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// 배터리 관련
static volatile int                     s_tdc_pwr_battery_percent = 0;
static volatile tdc_pwr_battery_state_t s_tdc_pwr_battery_state   = TDC_PWR_BATTERY_STATE_RESET;

// 충전 상태 관련
/* 구 코드는 충전기 상태를 배터리 enum(EN__SND_BATT_STATE_RESET)으로 초기화했다.
 * 두 enum 모두 RESET=0 이라 동작은 같았으나 타입이 어긋나 있었다 - 정정(2026-07-21). */
static volatile tdc_pwr_charger_state_t s_tdc_pwr_charger_state = TDC_PWR_CHARGER_STATE_RESET;

// 크래들 뚜껑 상태
static int s_tdc_cradle_cover_state = df_Defalut;

tdc_pwr_battery_state_t tdc_pwr_battery_get_state(void)
{
    return s_tdc_pwr_battery_state;
}

void tdc_pwr_battery_set_state(tdc_pwr_battery_state_t state)
{
    s_tdc_pwr_battery_state = state;
}

int tdc_pwr_battery_get_percent(void)
{
    return s_tdc_pwr_battery_percent;
}

void tdc_pwr_battery_set_percent(int percent)
{
    s_tdc_pwr_battery_percent = percent;
}

ST__USB_CONNECTOR tdc_pwr_charger_get_state(void)
{
    return cfx_cm3_sharedMemoryAll.chargerState;
}

void tdc_pwr_charger_set_state(tdc_pwr_charger_state_t state)
{
    // 충전 케이블 연결 상태 디버깅 메시지 출력
#if 1
    if (s_tdc_pwr_charger_state != state)
    {
        TDC_PRINTF_D("[CHARGER] %s -> %s \r\n",
                     // 이전 상태
                     (s_tdc_pwr_charger_state == TDC_PWR_CHARGER_STATE_RESET)          ? "RESET"  //
                     : (s_tdc_pwr_charger_state == TDC_PWR_CHARGER_STATE_CONNECTED)    ? "CONNECTED"
                     : (s_tdc_pwr_charger_state == TDC_PWR_CHARGER_STATE_DISCONNECTED) ? "DISCONNECTED"
                                                                                       : "INVALID",
                     // 현재 상태
                     (state == TDC_PWR_CHARGER_STATE_RESET)          ? "RESET"  //
                     : (state == TDC_PWR_CHARGER_STATE_CONNECTED)    ? "CONNECTED"
                     : (state == TDC_PWR_CHARGER_STATE_DISCONNECTED) ? "DISCONNECTED"
                                                                     : "INVALID");
    }
#endif

    s_tdc_pwr_charger_state = state;

    switch (state)
    {
        case TDC_PWR_CHARGER_STATE_CONNECTED:
        {
            cfx_cm3_sharedMemoryAll.chargerState.chargerConnectorPluggedIn = df_Connected;
            cfx_cm3_sharedMemoryAll.chargerState.carryingCasePluggedIn     = df_Connected;
            /* carryingCaseCoverOpen: BLE 0x34 data[2] 수신값 유지 - tdc_pwr_cradle_set_cover_state()가 관리 */
        }
        break;

        case TDC_PWR_CHARGER_STATE_DISCONNECTED:
        {
            cfx_cm3_sharedMemoryAll.chargerState.chargerConnectorPluggedIn = df_Disconnected;
            cfx_cm3_sharedMemoryAll.chargerState.carryingCasePluggedIn     = df_Disconnected;
            /* carryingCaseCoverOpen: BLE 0x34 data[2] 수신값 유지 - tdc_pwr_cradle_set_cover_state()가 관리 */
        }
        break;

        default:  // TDC_PWR_CHARGER_STATE_RESET
        {
            cfx_cm3_sharedMemoryAll.chargerState.chargerConnectorPluggedIn = df_Defalut;
            cfx_cm3_sharedMemoryAll.chargerState.carryingCasePluggedIn     = df_Defalut;
            /* carryingCaseCoverOpen: BLE 0x34 data[2] 수신값 유지 - tdc_pwr_cradle_set_cover_state()가 관리 */
        }
        break;
    }
}

void tdc_pwr_cradle_set_cover_state(int state)
{
    if (state == 2)
    {
        s_tdc_cradle_cover_state = df_Disconnected; /* df_Closed == 2 */
    }
    else
    {
        s_tdc_cradle_cover_state = df_Connected; /* 1=열림, else=열림 처리 */
    }
    cfx_cm3_sharedMemoryAll.chargerState.carryingCaseCoverOpen = s_tdc_cradle_cover_state;
    TDC_PRINTF_D("[CRADLE] COVER STATE: %s\r\n", (s_tdc_cradle_cover_state == df_Connected) ? "OPENED" : "CLOSED");
}

int tdc_pwr_cradle_get_cover_state(void)
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

/* 구 전압분배 계수로 배터리 경계전압을 잡던 값들은 제거했다(#if 0 사장).
 * 현행 값은 실측 기반이며 아래 주석에 측정치가 적혀 있다. */

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

int tdc_pwr_battery_read_percentage(void)
{
    // return battery_percentage;
    return s_tdc_pwr_battery_percent;
}

/* updateBatteryLevel(): E8300 자체 LSAD 측정 기반 배터리 등급 함수였으나, QCC가 배터리 정보(0x34)를
 * 제공하는 구조로 전환되며 dead code가 되어 제거함(2026-07-14). percent -> LED 등급 매핑은
 * main.c 의 tdc_led_request_battery(경계 테이블)로 이관. 이력: docs/tasks/main/20260714_battery-led-refactor. */

/* Sullivan 유산 제거(2026-07-15): readUsbConnectorState() 기반 GPIO 3함수
 * (read_BatteryChargerConnectinStatus / isCarryingCaseConnected /
 * isCarryingCaseCoverOpen)와 FPGA I2C 기반 #else 벌을 삭제했다.
 *
 * Sullivan 은 USB 케이블(charger)과 캐링케이스(carryingCase)가 독립 신호였으나,
 * Sound1 은 포고핀 크래들 단일 경로로 바뀌며 tdc_pwr_charger_set_state() 가 두 필드를
 * 항상 동시 설정한다. GPIO 3함수는 호출처가 0 이었고, #else 벌(FPGA I2C)은 보드
 * define 이 하나라도 있으면 컴파일되지 않는다(processorDirective.h:10 에서
 * Board_is_OTE_VER_1_5 활성).
 * 상세: docs/tasks/main/20260715_systemcontrol-fsm-decompose/분석-부록-sullivan유산.md */
