/* SYS-050/051: 실제 감사기록과 같은 필드/이벤트 코드의 RAM 표시 시험.
 * 생성 데이터의 출처는 매 행 TEST로 식별한다. 제품 시험 합격을 출력하지 않는다. */
#include "tdc_sys_test_audit.h"

#if TDC_SYS_TEST_AUDIT_ENABLE
#include <stdio.h>
#include <string.h>
#include <SEGGER_RTT.h>
#include "tdc_fs_event_log.h"

#define TDC_AUDIT_LINE_BYTES 60u
#define TDC_AUDIT_PREFIX "[SYS-%03u][TEST] "

/* time은 기존 감사 형식과 같이 YY/MM/DD/hh/mm/ss의 이진 정수다.
 * 02로 시작하는 주소는 로컬 관리/유니캐스트 시험 주소이며 실제 장치가 아니다.
 * 51은 연결 대상이 없는 오류 표시이므로 주소를 모두 0으로 둔다. */
static const TDC_FS_EVENT_LOG_ENTITY_T s_tdc_connection_records[] = {
    {{26, 5, 29, 10, 0, 0}, {{0x02, 0x24, 0xA0, 0x00, 0x00, 0x50}}, CI_EVENT_LOG_TYPE_CONNECTED},
    {{26, 5, 29, 10, 0, 5}, {{0x02, 0x24, 0xA0, 0x00, 0x00, 0x50}}, CI_EVENT_LOG_TYPE_DISCONNECTED}
};
static const TDC_FS_EVENT_LOG_ENTITY_T s_tdc_integrity_record = {
    {26, 5, 29, 10, 1, 0}, {{0, 0, 0, 0, 0, 0}}, CI_EVENT_LOG_TYPE_INTEGRITY_ERROR
};

/* UI 명령과 poll만 접근한다. ISR/타이머/파일 시스템과 공유하지 않는다. */
static unsigned s_tdc_test_id;
static unsigned s_tdc_next_line;
static bool s_tdc_stop_requested;

bool tdc_sys_test_audit_pending(void)
{
    return s_tdc_test_id != 0u;
}

int tdc_sys_test_audit_command(unsigned test_id, int argc, char *argv[])
{
    if ((test_id != 50u && test_id != 51u) || argv == NULL || argv[0] == NULL ||
        argc < 1 || argc > 2)
        return -1;
    if (strcmp(argv[0], test_id == 50u ? "sys50" : "sys51") != 0)
        return -1;
    if (argc == 2 && (argv[1] == NULL ||
        (strcmp(argv[1], "show") != 0 && strcmp(argv[1], "stop") != 0)))
        return -1;
    const bool stop = argc == 2 && strcmp(argv[1], "stop") == 0;
    if (tdc_sys_test_audit_pending())
    {
        if (!stop || s_tdc_test_id != test_id)
            return 1;
        s_tdc_stop_requested = true;
        return 0;
    }
    s_tdc_test_id = test_id;
    s_tdc_next_line = 0u;
    s_tdc_stop_requested = stop;
    return 0;
}

void tdc_sys_test_audit_poll(void)
{
    if (!tdc_sys_test_audit_pending())
        return;
    char line[TDC_AUDIT_LINE_BYTES + 1u];
    int length;
    bool last = false;
    const unsigned id = s_tdc_test_id;
    const unsigned count = id == 50u ? 2u : 1u;
    if (s_tdc_stop_requested)
    {
        length = snprintf(line, sizeof(line), TDC_AUDIT_PREFIX "STOPPED\r\n", id);
        last = true;
    }
    else if (s_tdc_next_line == 0u)
        length = snprintf(line, sizeof(line), TDC_AUDIT_PREFIX "RECORDS=%u STORAGE=RAM\r\n", id, count);
    else if (s_tdc_next_line <= count * 3u)
    {
        const unsigned index = (s_tdc_next_line - 1u) / 3u;
        const unsigned field = (s_tdc_next_line - 1u) % 3u;
        const TDC_FS_EVENT_LOG_ENTITY_T *entry = id == 50u ? &s_tdc_connection_records[index] : &s_tdc_integrity_record;
        if (field == 0u)
            length = snprintf(line, sizeof(line), TDC_AUDIT_PREFIX "<%u> TIME {%02u-%02u-%02u-%02u-%02u-%02u}\r\n",
                id, index, (unsigned)entry->time[0], (unsigned)entry->time[1], (unsigned)entry->time[2],
                (unsigned)entry->time[3], (unsigned)entry->time[4], (unsigned)entry->time[5]);
        else if (field == 1u)
            length = snprintf(line, sizeof(line), TDC_AUDIT_PREFIX "<%u> ADDR {%02X:%02X:%02X:%02X:%02X:%02X}\r\n",
                id, index, (unsigned)entry->bt_addr.bt_addr[0], (unsigned)entry->bt_addr.bt_addr[1],
                (unsigned)entry->bt_addr.bt_addr[2], (unsigned)entry->bt_addr.bt_addr[3],
                (unsigned)entry->bt_addr.bt_addr[4], (unsigned)entry->bt_addr.bt_addr[5]);
        else
            length = snprintf(line, sizeof(line), TDC_AUDIT_PREFIX "<%u> EVENT {%08lX} %s\r\n",
                id, index, (unsigned long)entry->event,
                entry->event == CI_EVENT_LOG_TYPE_CONNECTED ? "CONNECTED" :
                entry->event == CI_EVENT_LOG_TYPE_DISCONNECTED ? "DISCONNECTED" : "INTEGRITY_ERROR");
    }
    else
    {
        length = snprintf(line, sizeof(line), TDC_AUDIT_PREFIX "END\r\n", id);
        last = true;
    }
    if (length < 0 || (unsigned)length > TDC_AUDIT_LINE_BYTES)
    {
        length = snprintf(line, sizeof(line), TDC_AUDIT_PREFIX "FORMAT_ERROR\r\n", id);
        last = true;
    }
    unsigned written;
    SEGGER_RTT_LOCK();
    written = SEGGER_RTT_WriteSkipNoLock(0, line, (unsigned)length);
    SEGGER_RTT_UNLOCK();
    if (written == 1u)
    {
        if (last)
        {
            s_tdc_test_id = 0u;
            s_tdc_stop_requested = false;
        }
        else
            ++s_tdc_next_line;
    }
}
#endif
