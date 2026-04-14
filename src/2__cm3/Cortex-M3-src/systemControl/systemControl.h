#ifndef SYSTEM_CONTROL_H__
#define SYSTEM_CONTROL_H__

#include <hw.h>
#include <stdbool.h>
#include "board.h"
#include "cfx_cm3_sharedMemory.h"
#include "batteryNPowerControl.h"
#include "LedOutput.h"
#include "error.h"
#include "isd_interface.h"

typedef struct
{

    EN__LED_PATTERN Led_Pattern;
    bool            enable_ISD;
    bool            enablePMIC;
    bool            BLE_Off; // 충전기 연결 시 시스템에서 BLE를 끄기 위함
    bool            StimulationIndicatorTriggerLowPower;
    bool            systemOff;

} ST__SYSTEM_STATE;

ST__SYSTEM_STATE systemControl(EN__LED_PATTERN   current_led_pattern,
                               ST__ERROR_CODE    mcuErrorCode,
                               ST__USB_CONNECTOR chargerState,
                               EN__BATTERY_LEVEL batteryLevel,
                               bool              powerButtonPushed,
                               bool              conneded_ISD,
                               bool              mappingConnected);

void NRF_On_OFF(ST__ISD_STATUS isd_state, bool global_BLE_Off, bool mappingConnection, bool Mapping_BLE_Off);

void NRF_Off_Command(void);

void NRF_ON_Command(void);

#endif
