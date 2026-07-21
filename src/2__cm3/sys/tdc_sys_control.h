#ifndef __tdc_sys_control_h__
#define __tdc_sys_control_h__

#include <hw.h>
#include <stdbool.h>
#include "board.h"
#include "cfx_cm3_sharedMemory.h"
#include "tdc_pwr_battery.h"
#include "tdc_led_output.h"
#include "tdc_sys_error.h"
#include "isd_interface.h"

/* LED 판정은 Arbiter(tdc_led_request / TDC_LED_SRC_ 계열)로 이관됨(Rev.3).
 * 구 Led_Pattern 필드와 current_led_pattern 입력은 소비자가 없어 제거했다. */
typedef struct
{

    bool enable_ISD;
    bool enablePMIC;
    bool BLE_Off; // 충전기 연결 시 시스템에서 BLE를 끄기 위함
    bool StimulationIndicatorTriggerLowPower;
    bool systemOff;
    bool cradleLidClosed;

} tdc_sys_state_t;

tdc_sys_state_t tdc_sys_control_step(tdc_sys_error_code_t    mcuErrorCode,
                               ST__USB_CONNECTOR chargerState,
                               int               battery_percent,
                               bool              powerButtonPushed,
                               bool              conneded_ISD,
                               bool              mappingConnected);

void tdc_sys_control_nrf_on_off(ST__ISD_STATUS isd_state, bool global_BLE_Off, bool mappingConnection, bool Mapping_BLE_Off);

void tdc_sys_control_nrf_off_command(void);

void tdc_sys_control_nrf_on_command(void);

#endif
