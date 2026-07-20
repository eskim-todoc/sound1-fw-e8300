#ifndef BLE_setting_H__
#define BLE_setting_H__

#include <tdc_hal_uart.h>
#include <stdbool.h>

#include "isd_interface.h"  //ok
#include "mappingControl.h" //ok


typedef struct
{
    EN__ISD_CONTROL_STATE isdControlCommand;
    bool                  mappingConnection;
    bool                  StimulationIndicatorTrigger;
    bool                  BLE_Off_Command;

} ST__BLE_COMMUNICATION_STATE;

void reset_global_variables_in_ble_communication(void);

ST__BLE_COMMUNICATION_STATE bleCommunication(ST__ISD_STATUS isd_state);

#endif
