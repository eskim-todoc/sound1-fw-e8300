#ifndef __tdc_ble_remote_h__
#define __tdc_ble_remote_h__

#include <hw.h>
#include <stdbool.h>
#include <tdc_ble_protocol.h>
#include <tdc_isd.h>

#include <tdc_printf.h>
#include <tdc_fs_event_log.h>
#include <tdc_hal_timer.h>

#include <tdc_fs_stim_mute.h>

#include <tdc_touch.h>

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

void                    tdc_ble_remote_fetch_packet(const uint8_t *Rx_dataPacket);
ST__REMOTECONTROL_STATE tdc_ble_remote_step(bool isdConnection);

void                       tdc_ble_remote_set_passkey_match(void);
void                       tdc_ble_remote_clear_passkey_match(void);
bool                       tdc_ble_remote_is_passkey_match(void);
void                       tdc_ble_remote_clear_command(void);

#endif
