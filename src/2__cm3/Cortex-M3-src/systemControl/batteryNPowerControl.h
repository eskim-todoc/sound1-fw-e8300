#ifndef BATTERY_N_POWERCONTROL_H__
#define BATTERY_N_POWERCONTROL_H__

#include <stdbool.h>
#include "processorDirective.h"
#include "dirver_i2c_for_ISD.h"
#include "LedOutput.h"

#include <cfx_cm3_sharedMemory.h>
#include <ci_printf.h>
#include <driver_MAX17262.h>

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Sullivan
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/* EN__BATTERY_LEVEL(7단계 레벨 enum) 제거(2026-07-14): systemControl이 배터리 percent를
 * 직접 비교하도록 전환. 배터리 표현은 percent(snd_batt_get_percent) 단일 소스로 통일. */

typedef struct
{
    bool isCarryingCaseConnected;
    bool isCoverOpen;
} ST__CARRINGCASE_STATE;

void              calculationBatteryBoundary(void);
int               readBatteryPercentage(void);
int               read_BatteryChargerConnectinStatus(void);
void              updateCarryingCaseStatus(void);
bool              isCarryingCaseConnected(void);
bool              isCarryingCaseCoverOpen(void);

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Sound1
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

typedef enum
{
    EN__SND_BATT_STATE_RESET = 0,
    EN__SND_BATT_STATE_DISCHARGING,
    EN__SND_BATT_STATE_CHARGING
} EN__SND_BATT_STATE;

typedef enum
{
    EN__SND_CHARGER_STATE_RESET = 0,
    EN__SND_CHARGER_STATE_CONNECTED,
    EN__SND_CHARGER_STATE_DISCONNECTED
} EN__SND_CHARGER_STATE;

EN__SND_BATT_STATE snd_batt_get_state(void);
void               snd_batt_set_state(EN__SND_BATT_STATE state);
int                snd_batt_get_percent(void);
void               snd_batt_set_percent(int percent);

ST__USB_CONNECTOR snd_charger_get_state(void);
void              snd_charger_set_state(EN__SND_CHARGER_STATE state);

void tdc_charger_set_cradle_cover_state(int state);
int  tdc_cradle_get_cover_state(void);

#endif  // BATTERY_N_POWERCONTROL_H__
