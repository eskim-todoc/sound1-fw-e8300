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

typedef enum
{
    en__batteryPower_0per = 0,
    en__batteryPower_0btw20,
    en__batteryPower_20btw40,
    en__batteryPower_40btw60,
    en__batteryPower_60btw80,
    en__batteryPower_80btw100,
    en__batteryPower_100per
} EN__BATTERY_LEVEL;

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
EN__BATTERY_LEVEL  snd_batt_get_level(void);

ST__USB_CONNECTOR snd_charger_get_state(void);
void              snd_charger_set_state(EN__SND_CHARGER_STATE state);

void tdc_charger_set_cradle_cover_state(int state);
int  tdc_cradle_get_cover_state(void);

#endif  // BATTERY_N_POWERCONTROL_H__
