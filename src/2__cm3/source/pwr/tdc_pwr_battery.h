#ifndef __tdc_pwr_battery_h__
#define __tdc_pwr_battery_h__

#include <stdbool.h>
#include <processorDirective.h>
#include <tdc_hal_i2c_isd.h>
#include <tdc_led_output.h>

#include <tdc_shm.h>
#include <tdc_printf.h>

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Sullivan
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/* EN__BATTERY_LEVEL(7단계 레벨 enum) 제거(2026-07-14): tdc_sys_control_step()이 배터리 percent를
 * 직접 비교하도록 전환. 배터리 표현은 percent(tdc_pwr_battery_get_percent) 단일 소스로 통일. */

/* ST__CARRINGCASE_STATE 및 캐링케이스 / 충전기 상태 조회 API 제거(2026-07-15):
 * Sullivan 유산. 충전 상태는 QCC 0x34 기반 tdc_pwr_charger_get_state() 로 단일화됐다.
 * 상세: docs/tasks/main/20260715_systemcontrol-fsm-decompose/분석-부록-sullivan유산.md */

int  tdc_pwr_battery_read_percentage(void);

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Sound1
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/* ===================================================================
 * 배터리 퍼센트 임계 (2026-08-05 상수화)
 *
 * 흩어져 있던 매직넘버를 한 곳에 모았다. 값 자체는 하나도 바꾸지 않았다.
 * 모으는 것이 목적이다 - 값이 떨어져 있으면 아래 "순서" 가 안 보인다.
 *
 *   CUTOFF(40) > STIM_ALERT(20) = MAP_LOW(20) > LED_CRITICAL(10)
 *
 * 네 임계는 서로 독립이며 참조 관계가 없다. 다만 순서 때문에 아래가 성립한다:
 *   - 방전 중에는 CUTOFF(40) 에서 먼저 절전에 들어간다.
 *     따라서 LED CRITICAL(10) 은 방전 경로에서 사실상 볼 수 없고,
 *     충전 경로(handle_charging - battery_percent 를 아예 받지 않는다)에서만 보인다.
 *   - "LED 가 빨개지면 곧 꺼진다" 는 성립하지 않는다. 실제로는 LED 가
 *     MID(10~80) 인 상태에서 40 을 지나며 꺼진다.
 *
 * 임계를 바꿀 때는 이 순서 관계가 깨지지 않는지 함께 볼 것.
 * =================================================================== */

#define TDC_BATT_CUTOFF_PCT       40 /* 미만 -> 절전(SYSTEM OFF) 진입. tdc_sys_control.c 진입·탈출 양쪽에서 읽는다 */
#define TDC_BATT_STIM_ALERT_PCT   20 /* 미만 -> 저배터리 자극 알림 (10분 주기, ISD 연결 시에만). LED 와 무관 */
#define TDC_BATT_MAP_LOW_PCT      20 /* 이하 -> 매핑앱에 배터리 LOW 보고. 마진 없는 단일 컷 */
#define TDC_BATT_LED_READY_PCT    80 /* 이상 -> LED READY */
#define TDC_BATT_LED_CRITICAL_PCT 10 /* 미만 -> LED CRITICAL */

typedef enum
{
    TDC_PWR_BATTERY_STATE_RESET = 0,
    TDC_PWR_BATTERY_STATE_DISCHARGING,
    TDC_PWR_BATTERY_STATE_CHARGING
} tdc_pwr_battery_state_t;

typedef enum
{
    TDC_PWR_CHARGER_STATE_RESET = 0,
    TDC_PWR_CHARGER_STATE_CONNECTED,
    TDC_PWR_CHARGER_STATE_DISCONNECTED
} tdc_pwr_charger_state_t;

tdc_pwr_battery_state_t tdc_pwr_battery_get_state(void);
void               tdc_pwr_battery_set_state(tdc_pwr_battery_state_t state);
int                tdc_pwr_battery_get_percent(void);
void               tdc_pwr_battery_set_percent(int percent);

ST__USB_CONNECTOR tdc_pwr_charger_get_state(void);
void              tdc_pwr_charger_set_state(tdc_pwr_charger_state_t state);

void tdc_pwr_cradle_set_cover_state(int state);
int  tdc_pwr_cradle_get_cover_state(void);

#endif  // __tdc_pwr_battery_h__
