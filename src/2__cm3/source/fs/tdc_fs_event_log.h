#ifndef __tdc_fs_event_log_h__
#define __tdc_fs_event_log_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <ff.h>
#include <nvmctrl.h>
#include <nvmlib.h>

#include <tdc_fs.h>  // FIL 포인터 extern 선언된 헤더
#include <tdc_hal_timer.h>

#define TDC_FS_EVENT_LOG_IDENT        "EVENTLOG.TXT"  // 12 bytes
#define TDC_FS_EVENT_LOG_ENTITY_COUNT ((FF_MAX_SS / 16) - 1)

#define TDC_FS_EVENT_LOG_TYPE_CONNECTED       0x40
#define TDC_FS_EVENT_LOG_TYPE_DISCONNECTED    0x32
#define TDC_FS_EVENT_LOG_TYPE_INTEGRITY_ERROR 0xF0

typedef struct
{
    uint8_t bt_addr[6];
} TDC_FS_EVENT_LOG_BT_ADDR_T;

typedef struct
{
    uint8_t                    time[6];  // 6 bytes (년, 월, 일, 시, 분, 초)
    TDC_FS_EVENT_LOG_BT_ADDR_T bt_addr;  // 6 bytes
    uint32_t                   event;    // 4 bytes
} TDC_FS_EVENT_LOG_ENTITY_T;             // total 16 bytes

typedef struct
{
    uint8_t                   ident[12];                                // 12 bytes
    uint16_t                  write_index;                              // 2 bytes
    uint16_t                  entity_count;                             // 2 bytes
    TDC_FS_EVENT_LOG_ENTITY_T entities[TDC_FS_EVENT_LOG_ENTITY_COUNT];  // 16 bytes array
} TDC_FS_EVENT_LOG_T;

void                       tdc_fs_event_log_update_bt_addr(TDC_FS_EVENT_LOG_BT_ADDR_T *p_bt_addr);
TDC_FS_EVENT_LOG_BT_ADDR_T tdc_fs_event_log_get_bt_addr(void);

int tdc_fs_event_log_init(void);
int tdc_fs_event_log_write(uint32_t event_type);
int tdc_fs_event_log_read(void);

#endif /* __tdc_fs_event_log_h__ */
