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

/* LED 판정은 Arbiter(led_request / LED_SRC_ 계열)로 이관됨(Rev.3).
 * 구 Led_Pattern 필드와 current_led_pattern 입력은 소비자가 없어 제거했다. */
typedef struct
{

    bool enable_ISD;
    bool enablePMIC;
    bool BLE_Off; // 충전기 연결 시 시스템에서 BLE를 끄기 위함
    bool StimulationIndicatorTriggerLowPower;
    bool systemOff;
    bool cradleLidClosed;

} ST__SYSTEM_STATE;

ST__SYSTEM_STATE systemControl(ST__ERROR_CODE    mcuErrorCode,
                               ST__USB_CONNECTOR chargerState,
                               int               battery_percent,
                               bool              powerButtonPushed,
                               bool              conneded_ISD,
                               bool              mappingConnected);

void NRF_On_OFF(ST__ISD_STATUS isd_state, bool global_BLE_Off, bool mappingConnection, bool Mapping_BLE_Off);

void NRF_Off_Command(void);

void NRF_ON_Command(void);

#endif
