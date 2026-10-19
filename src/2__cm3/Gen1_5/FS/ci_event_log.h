#ifndef FS_CI_EVENT_LOG_H_
#define FS_CI_EVENT_LOG_H_

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <ff.h>
#include <nvmctrl.h>
#include <nvmlib.h>

#include <ci_filesystem.h>  // FIL 포인터 extern 선언된 헤더
#include <ci_timer.h>

#define CI_EVENT_LOG_IDENT        "EVENTLOG.TXT"  // 12 bytes
#define CI_EVENT_LOG_ENTITY_COUNT ((FF_MAX_SS / 16) - 1)

#define CI_EVENT_LOG_TYPE_CONNECTED       0x40
#define CI_EVENT_LOG_TYPE_DISCONNECTED    0x32
#define CI_EVENT_LOG_TYPE_INTEGRITY_ERROR 0xF0

typedef struct
{
    uint8_t bt_addr[6];
} CI_EVENT_LOG_BT_ADDR_T;

typedef struct
{
    uint8_t                time[6];  // 6 bytes (년, 월, 일, 시, 분, 초)
    CI_EVENT_LOG_BT_ADDR_T bt_addr;  // 6 bytes
    uint32_t               event;    // 4 bytes
} CI_EVENT_LOG_ENTITY_T;             // total 16 bytes

typedef struct
{
    uint8_t               ident[12];                            // 12 bytes
    uint16_t              write_index;                          // 2 bytes
    uint16_t              entity_count;                         // 2 bytes
    CI_EVENT_LOG_ENTITY_T entities[CI_EVENT_LOG_ENTITY_COUNT];  // 16 bytes array
} CI_EVENT_LOG_T;

void                   ci_event_log_update_bt_addr(CI_EVENT_LOG_BT_ADDR_T *p_bt_addr);
CI_EVENT_LOG_BT_ADDR_T ci_event_log_get_bt_addr(void);

int ci_event_log_init(void);
int ci_event_log_write(uint32_t event_type);
int ci_event_log_read(void);

#if TDC_SYS_TEST_73_ENABLE
/* CM3 전경의 단일 시험 호출에만 적용한다. 실제 파일 핸들/작업 버퍼는
 * 교체하지 않는다. 모든 경계와 사설 버퍼를 제공해야 시험을 시작한다. */
typedef struct
{
    FIL *file;
    CI_EVENT_LOG_T *buffer;
    /* year/month/day/hour/min/sec. ci_timer.h의 순환 include에 의존하지 않는다. */
    uint8_t time[6];
    CI_EVENT_LOG_BT_ADDR_T address;
    FRESULT (*open)(FIL *, const TCHAR *, BYTE);
    FRESULT (*read)(FIL *, void *, UINT, UINT *);
    FRESULT (*write)(FIL *, const void *, UINT, UINT *);
    FRESULT (*seek)(FIL *, FSIZE_t);
    FRESULT (*sync)(FIL *);
    FRESULT (*close)(FIL *);
    FRESULT (*stat)(const TCHAR *, FILINFO *);
} tdc_event_log_test_ops_t;

/* ops와 그 버퍼는 end까지 유효해야 한다. 중첩/불완전 등록은 거절한다.
 * begin부터 end까지 다른 감사 함수 호출을 섞지 않는 것은 호출자 책임이다. */
bool tdc_event_log_test_begin(const tdc_event_log_test_ops_t *ops);
void tdc_event_log_test_end(void);
#endif

#endif /* FS_CI_EVENT_LOG_H_ */
