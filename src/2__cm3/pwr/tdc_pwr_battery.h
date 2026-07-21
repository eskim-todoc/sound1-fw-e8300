#ifndef __tdc_pwr_battery_h__
#define __tdc_pwr_battery_h__

#include <stdbool.h>
#include "processorDirective.h"
#include "tdc_hal_i2c_isd.h"
#include "tdc_led_output.h"

#include <cfx_cm3_sharedMemory.h>
#include <tdc_printf.h>
#include <tdc_drv_max17262.h>

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Sullivan
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/* EN__BATTERY_LEVEL(7단계 레벨 enum) 제거(2026-07-14): tdc_sys_control_step()이 배터리 percent를
 * 직접 비교하도록 전환. 배터리 표현은 percent(tdc_pwr_battery_get_percent) 단일 소스로 통일. */

/* ST__CARRINGCASE_STATE 및 캐링케이스 / 충전기 상태 조회 API 제거(2026-07-15):
 * Sullivan 유산. 충전 상태는 QCC 0x34 기반 tdc_pwr_charger_get_state() 로 단일화됐다.
 * 상세: docs/tasks/main/20260715_systemcontrol-fsm-decompose/분석-부록-sullivan유산.md */

void tdc_pwr_battery_calculate_boundary(void);
int  tdc_pwr_battery_read_percentage(void);

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Sound1
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

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
