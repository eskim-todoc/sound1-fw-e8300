#ifndef __tdc_ble_communication_h__
#define __tdc_ble_communication_h__

#include <stdbool.h>

#include <tdc_isd.h>  //ok
#include <tdc_ble_mapping.h> //ok


typedef struct
{
    EN__ISD_CONTROL_STATE isdControlCommand;
    bool                  mappingConnection;
    bool                  StimulationIndicatorTrigger;
    bool                  BLE_Off_Command;

} ST__BLE_COMMUNICATION_STATE;

void tdc_ble_communication_reset_globals(void);

ST__BLE_COMMUNICATION_STATE tdc_ble_communication_step(ST__ISD_STATUS isd_state);

#endif
