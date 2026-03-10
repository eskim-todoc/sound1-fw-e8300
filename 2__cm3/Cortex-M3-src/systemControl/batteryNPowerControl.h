#ifndef BATTERY_N_POWERCONTROL_H__
#define BATTERY_N_POWERCONTROL_H__

#include <stdbool.h>
#include "processorDirective.h"
#include "dirver_i2c_for_ISD.h"
#include "LedOutput.h"

#include <ci_printf.h>
#include <driver_MAX17262.h>

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
EN__BATTERY_LEVEL updateBatteryLevel(int chargingState, EN__LED_PATTERN ledPattern);
int               readBatteryPercentage(void);
int               read_BatteryChargerConnectinStatus(void);
void              updateCarryingCaseStatus(void);
bool              isCarryingCaseConnected(void);
bool              isCarryingCaseCoverOpen(void);

#endif
