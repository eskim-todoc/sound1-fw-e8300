#ifndef LED_OUTPUT_H__
#define LED_OUTPUT_H__

#include <stdbool.h>

#include <ci_printf.h>

typedef enum
{
    en__LED_NA = 0,
    en__LED_Map_Error,
    en__LED_MCU_Error,
    en__LED_MCU_Accelerometer_Error,
    en__LED_MCU_FPGA_Error,
    en__LED_MCU_RF_PMIC_Error,
    en__LED_POWER_On,
    en__LED_ISD_StimulationOut_batteryNormal,
    en__LED_ISD_StimulationOut_batteryLow,
    en__LED_StandbyForconneded_ISD_batteryNormal,
    en__LED_StandbyForconneded_ISD_batteryLow,
    en__LED_MappingConneted_ISD_Connected_BatteryNormal,
    en__LED_MappingConneted_ISD_Connected_BatteryLow,
    en__LED_MappingConneted_ISD_Unconnected_BatteryNormal,
    en__LED_MappingConneted_ISD_Unconnected_BatteryLow,
    en__LED_BatteryChargingLevel_0per,
    en__LED_BatteryChargingLevel_0btw20,
    en__LED_BatteryChargingLevel_20btw40,
    en__LED_BatteryChargingLevel_40btw60,
    en__LED_BatteryChargingLevel_60btw80,
    en__LED_BatteryChargingLevel_80btw100,
    en__LED_BatteryChargingLevel_100per,
    en__LED_POWER_Off

} EN__LED_PATTERN;

typedef enum
{
    en__LED_BLACK = 0,
    en__LED_RED,
    en__LED_GREEN,
    en__LED_BLUE,
    en__LED_ORANGE,
    en__LED_SKYBLUE,
    en__LED_PURPLE,
    en__LED_WHITE
} EN__LED_COLOR;

void enabletestLED_Trigger(void);
void disabletestLED_Trigger(void);
bool isTestTriggerEanbled(void);

EN__LED_PATTERN geteLED_OutputPattern(void);
void            LedPatternOut(EN__LED_PATTERN ledOutputPattern);

void LED_black(void);
void LED_White(void);
void turnOffLED(void);
void LED_Memory_error(void);
void LED_clock_error(void);

void turnON_RedLED(void);

void turnON_GreenLED(void);

void turnON_BlueLED(void);

#endif
