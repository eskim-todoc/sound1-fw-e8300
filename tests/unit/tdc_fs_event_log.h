// 호스트 테스트용 shim - src/2__cm3/source/fs/tdc_fs_event_log.h 를 가린다.
//
// 왜 필요한가
//   진짜 헤더는 SDK 의 <ff.h> 를 include 하고, MinGW 에서 ff.h 가 <windows.h> 를
//   끌어온다. 그러면 같은 SDK 의 NVMLIB.h 가 정의한 STORAGE_DEVICE_DESCRIPTOR 와
//   Windows 의 것이 충돌하고, spi_driver.h 의 SPI_Type 도 호스트에 없어 컴파일이
//   무너진다. ble/remote/ 를 호스트에서 컴파일하지 못하게 막던 유일한 장벽이다.
//
//   tests/unit 이 첫 include 경로이므로 이 파일이 진짜 헤더를 대신한다.
//   src/ 는 한 줄도 건드리지 않는다 (tests/unit 의 src/ 무변경 원칙).
//
// 무엇을 담는가
//   ble/remote/tdc_ble_remote.c 가 실제로 쓰는 것만 담는다. 프로토타입은
//   진짜 헤더(fs/tdc_fs_event_log.h:24~49)에서 그대로 옮겼다. 시그니처가
//   어긋나면 링크 단계에서 드러난다.
//
//   진짜 헤더에만 있고 여기 없는 것: get_bt_addr · init · read ·
//   TDC_FS_EVENT_LOG_ENTITY_T · TDC_FS_EVENT_LOG_T (FF_MAX_SS 의존).
//   remote 가 쓰지 않으므로 넣지 않는다. 필요해지면 그때 추가한다.

#ifndef __tdc_fs_event_log_h__
#define __tdc_fs_event_log_h__

#include <stdbool.h>
#include <stdint.h>

#define TDC_FS_EVENT_LOG_TYPE_CONNECTED       0x40
#define TDC_FS_EVENT_LOG_TYPE_DISCONNECTED    0x32
#define TDC_FS_EVENT_LOG_TYPE_INTEGRITY_ERROR 0xF0

typedef struct
{
    uint8_t bt_addr[6];
} TDC_FS_EVENT_LOG_BT_ADDR_T;

void tdc_fs_event_log_update_bt_addr(TDC_FS_EVENT_LOG_BT_ADDR_T *p_bt_addr);
int  tdc_fs_event_log_write(uint32_t event_type);

#endif  // __tdc_fs_event_log_h__
