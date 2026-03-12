#ifndef BLE_REMOTECONTROL_H__
#define BLE_REMOTECONTROL_H__

#include <hw.h>
#include <stdbool.h>
#include "ble_commonProtocol.h"
#include "isd_interface.h"

#include <ci_printf.h>
#include <ci_event_log.h>
#include <ci_timer.h>

#include <ci_stim_mute.h>

typedef struct
{
    int slot_index;
    int map_index;

} ST__REMOTE_CONTROL_PAYLOD_READWRITEDATA_FLASH;

typedef struct
{
    // EN__REMOTE_CONTROL_COMMAND fetched_command;
    EN__REMOTE_CONTROL_COMMAND                    command;
    ST__REMOTE_CONTROL_PAYLOD_READWRITEDATA_FLASH remocon_ReadWriteMapData_Flash;
    int                                           data[todoc_PayloadSize];

} ST__REMOTECONTROL_PACKET;

// #pragma pack(4)
typedef struct
{
    EN__ISD_CONTROL_STATE isdControlCommand;
    bool                  BLE_Off;
    bool                  remoconConnection;

} ST__REMOTECONTROL_STATE;

void                    fetch_remoteControlPacket(const int *Rx_dataPacket);
ST__REMOTECONTROL_STATE remoteControl(bool isdConnection);

void                       set_isd_passKeyMatchResult(void);
void                       clear_isd_passKeyMatchResult(void);
bool                       is_isd_passKeyMatch(void);
void                       clearRemoteColtrolCommand(void);
void                       changeRemoteCommandWaitingForBleOff(void);
EN__REMOTE_CONTROL_COMMAND getRemoteCommand(void);

#endif
