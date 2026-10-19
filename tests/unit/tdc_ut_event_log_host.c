/* 감사기록 제품 함수의 PC 전용 경계 시험.
 * 실제 FatFs/NVM 대신 4KB RAM 파일과 실패 응답만 제공한다.
 * 제품 함수 본문/구조체/오류 enum은 검사기가 원본에서 그대로 포함한다.
 * 반환/오류/저장 내용을 비교하며 장치 시간 신뢰성이나 인증은 검증하지 않는다.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FR_OK 0
#define FA_OPEN_ALWAYS 16
#define FA_OPEN_EXISTING 0
#define FA_READ 1
#define FA_WRITE 2
#define SYS_WATCHDOG_REFRESH() ((void)0)
#define TDC_PRINTF_V(...) ((void)0)
#define TDC_PRINTF_D(...) ((void)0)
#define TDC_PRINTF_E(...) ((void)0)
#define TDC_PRINTF_I(...) ((void)0)
typedef struct { struct { unsigned sclust; } obj; unsigned position; int flags; bool opened; } FIL;
typedef struct { char fname[32]; unsigned fsize; } FILINFO;
static FIL g_tdc_fs_ohdl;
static union { uint32_t alignment; uint8_t bytes[4096]; } s_tdc_log_work;
#define TDC_FS_BASE_ADDR_FOR_EVENT_LOG (s_tdc_log_work.bytes)
static int f_open(FIL *, const char *, int);
static int f_read(FIL *, void *, unsigned, int *);
static int f_write(FIL *, const void *, unsigned, int *);
static int f_lseek(FIL *, unsigned);
static int f_close(FIL *);
static int f_sync(FIL *);
static int f_stat(const void *, FILINFO *);
#include "event_log_types.inc"
static tdc_hal_timer_time_t tdc_hal_timer_get_reference_time_after_self_update(void);
static void tdc_sys_error_update(tdc_sys_error_major_t, int, int);
#include "event_log_product.inc"

static uint8_t s_tdc_disk[4096];
static unsigned s_tdc_disk_size;
static bool s_tdc_exists;
static int s_tdc_open_error, s_tdc_read_error, s_tdc_write_error;
static int s_tdc_close_error, s_tdc_sync_error, s_tdc_short_read, s_tdc_short_write;
static int s_tdc_opens, s_tdc_closes, s_tdc_writes, s_tdc_error_major, s_tdc_error_minor;
static int s_tdc_case, s_tdc_records, s_tdc_failures;

static int f_open(FIL *fp, const char *name, int flags)
{
    assert(strcmp(name, "EVENTLOG.TXT") == 0);
    ++s_tdc_opens;
    if (s_tdc_open_error) return s_tdc_open_error;
    if (!s_tdc_exists && !(flags & FA_OPEN_ALWAYS)) return 1;
    s_tdc_exists = true; fp->opened = true; fp->flags = flags; fp->position = 0;
    return FR_OK;
}
static int f_lseek(FIL *fp, unsigned position)
{
    assert(fp->opened && position <= sizeof(s_tdc_disk)); fp->position = position; return FR_OK;
}
static int f_read(FIL *fp, void *dst, unsigned count, int *read_count)
{
    unsigned available = s_tdc_disk_size > fp->position ? s_tdc_disk_size - fp->position : 0;
    assert(fp->opened && (fp->flags & FA_READ));
    if (count > available) count = available;
    if (s_tdc_short_read && count) --count;
    *read_count = s_tdc_read_error ? 0 : (int)count;
    if (*read_count) memcpy(dst, s_tdc_disk + fp->position, count);
    fp->position += (unsigned)*read_count; return s_tdc_read_error;
}
static int f_write(FIL *fp, const void *src, unsigned count, int *written)
{
    ++s_tdc_writes;
    assert(fp->opened && (fp->flags & FA_WRITE));
    assert(fp->position + count <= sizeof(s_tdc_disk));
    if (s_tdc_short_write && count) --count;
    *written = s_tdc_write_error ? 0 : (int)count;
    if (*written) memcpy(s_tdc_disk + fp->position, src, count);
    fp->position += (unsigned)*written;
    if (fp->position > s_tdc_disk_size) s_tdc_disk_size = fp->position;
    return s_tdc_write_error;
}
static int f_close(FIL *fp)
{
    assert(fp->opened); ++s_tdc_closes;
    if (!s_tdc_close_error) fp->opened = false;
    return s_tdc_close_error;
}
static int f_sync(FIL *fp) { assert(fp->opened); return s_tdc_sync_error; }
static int f_stat(const void *name, FILINFO *info)
{
    (void)name; memset(info, 0, sizeof(*info)); info->fsize = s_tdc_disk_size; return FR_OK;
}
static tdc_hal_timer_time_t tdc_hal_timer_get_reference_time_after_self_update(void)
{
    const tdc_hal_timer_time_t time = {26, 10, 2, 13, 14, 15}; return time;
}
static void tdc_sys_error_update(tdc_sys_error_major_t major, int minor, int line)
{
    assert(line > 0); s_tdc_error_major = major; s_tdc_error_minor = minor;
}
static void value(const char *field, int actual, int expected)
{
    ++s_tdc_records; s_tdc_failures += actual != expected;
    printf("RECORD {\"case\":%d,\"field\":\"%s\",\"actual\":%d,\"expected\":%d}\n",
           s_tdc_case, field, actual, expected);
}
static void begin(int number, bool valid)
{
    TDC_FS_EVENT_LOG_T log;
    s_tdc_case = number;
    s_tdc_open_error = s_tdc_read_error = s_tdc_write_error = 0;
    s_tdc_close_error = s_tdc_sync_error = s_tdc_short_read = s_tdc_short_write = 0;
    s_tdc_opens = s_tdc_closes = s_tdc_writes = 0;
    s_tdc_error_major = s_tdc_error_minor = 0;
    memset(&g_tdc_fs_ohdl, 0, sizeof(g_tdc_fs_ohdl));
    s_tdc_exists = valid;
    memset(&log, 0, sizeof(log)); memcpy(log.ident, TDC_FS_EVENT_LOG_IDENT, 12);
    memset(s_tdc_log_work.bytes, 0xa5, sizeof(s_tdc_log_work.bytes));
    memset(s_tdc_disk, 0, sizeof(s_tdc_disk));
    if (valid) memcpy(s_tdc_disk, &log, sizeof(log));
    s_tdc_disk_size = valid ? sizeof(log) : 0;
    tdc_fs_event_log_update_bt_addr(NULL);
}
static void failure(int expected_minor, int expected_close)
{
    value("return", tdc_fs_event_log_write(TDC_FS_EVENT_LOG_TYPE_CONNECTED), -1);
    value("error_major", s_tdc_error_major, 10);
    value("error_minor", s_tdc_error_minor, expected_minor);
    value("close_count", s_tdc_closes, expected_close);
}
static TDC_FS_EVENT_LOG_T stored(void)
{
    TDC_FS_EVENT_LOG_T log; memcpy(&log, s_tdc_disk, sizeof(log)); return log;
}
int main(void)
{
    TDC_FS_EVENT_LOG_T log;
    TDC_FS_EVENT_LOG_BT_ADDR_T addr = {{0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc}};
    const uint8_t time[6] = {26, 10, 2, 13, 14, 15};
    assert(sizeof(TDC_FS_EVENT_LOG_T) == 4096 && sizeof(TDC_FS_EVENT_LOG_ENTITY_T) == 16);
    assert(sizeof(int) == 4 && TDC_FS_EVENT_LOG_ENTITY_COUNT == 255);
    begin(1, false);
    value("return", tdc_fs_event_log_init(), 0); log = stored();
    value("ident", memcmp(log.ident, "EVENTLOG.TXT", 12) == 0, 1);
    value("count", log.entity_count, 0); value("index", log.write_index, 0);
    value("file_bytes", s_tdc_disk_size, 4096); value("close_count", s_tdc_closes, 1);
    begin(2, true); log = stored(); log.entity_count = 1; log.write_index = 1;
    log.entities[0].event = 0x40; memcpy(s_tdc_disk, &log, sizeof(log));
    value("return", tdc_fs_event_log_init(), 0); value("no_rewrite", s_tdc_writes, 0);
    value("preserved", memcmp(s_tdc_disk, &log, sizeof(log)) == 0, 1);
    for (int kind = 0; kind < 3; ++kind) {
        begin(3 + kind, true); log = stored();
        if (kind == 0) log.ident[0] = 'X';
        if (kind == 1) log.write_index = 255;
        if (kind == 2) log.entity_count = 256;
        memcpy(s_tdc_disk, &log, sizeof(log));
        value("return", tdc_fs_event_log_init(), 0); log = stored();
        value("index", log.write_index, 0); value("count", log.entity_count, 0);
        value("ident", memcmp(log.ident, "EVENTLOG.TXT", 12) == 0, 1);
    }
    begin(6, false); s_tdc_open_error = 1; value("return", tdc_fs_event_log_init(), -1);
    value("no_close_unopened", s_tdc_closes, 0);
    begin(7, false); s_tdc_write_error = 1; value("return", tdc_fs_event_log_init(), -1);
    value("close_count", s_tdc_closes, 1);
    begin(8, true); s_tdc_open_error = 1; failure(1, 0);
    begin(9, true); s_tdc_read_error = 1; failure(3, 1); value("no_write", s_tdc_writes, 0);
    begin(10, true); s_tdc_short_read = 1; failure(3, 1); value("no_write", s_tdc_writes, 0);
    begin(11, true); s_tdc_write_error = 1; failure(2, 1);
    begin(12, true); s_tdc_short_write = 1; failure(2, 1);
    for (int kind = 0; kind < 3; ++kind) {
        begin(13 + kind, true); log = stored();
        if (kind == 0) log.ident[0] = 'X';
        if (kind == 1) log.write_index = 255;
        if (kind == 2) log.entity_count = 256;
        memcpy(s_tdc_disk, &log, sizeof(log)); failure(4, 1); value("no_write", s_tdc_writes, 0);
    }
    begin(16, true); tdc_fs_event_log_update_bt_addr(&addr);
    value("return", tdc_fs_event_log_write(0x40), 0); log = stored();
    value("count", log.entity_count, 1); value("index", log.write_index, 1);
    value("event", log.entities[0].event, 0x40);
    value("time", memcmp(log.entities[0].time, time, 6) == 0, 1);
    value("address", memcmp(&log.entities[0].bt_addr, &addr, 6) == 0, 1);
    value("close_count", s_tdc_closes, 1); value("no_error", s_tdc_error_major, 0);
    s_tdc_case = 17; value("return", tdc_fs_event_log_write(0x32), 0); log = stored();
    value("count", log.entity_count, 2); value("index", log.write_index, 2);
    value("first_event", log.entities[0].event, 0x40); value("second_event", log.entities[1].event, 0x32);
    begin(18, true); log = stored(); log.write_index = 254; log.entity_count = 255;
    log.entities[0].event = 0x40; memcpy(s_tdc_disk, &log, sizeof(log));
    value("return", tdc_fs_event_log_write(0xf0), 0); log = stored();
    value("index_wrap", log.write_index, 0); value("count_saturates", log.entity_count, 255);
    value("last_event", log.entities[254].event, 0xf0); value("first_preserved", log.entities[0].event, 0x40);
    s_tdc_case = 19; value("return", tdc_fs_event_log_write(0x32), 0); log = stored();
    value("index", log.write_index, 1); value("count", log.entity_count, 255);
    value("oldest_replaced", log.entities[0].event, 0x32);
    begin(20, true); tdc_fs_event_log_update_bt_addr(&addr); tdc_fs_event_log_update_bt_addr(NULL);
    addr = tdc_fs_event_log_get_bt_addr(); value("address_zero", memcmp(&addr, "\0\0\0\0\0\0", 6) == 0, 1);
    begin(21, true); value("return", tdc_fs_event_log_read(), 0); value("no_write", s_tdc_writes, 0);
    begin(22, true); s_tdc_read_error = 1; value("return", tdc_fs_event_log_read(), -1);
    value("close_count", s_tdc_closes, 1);
    begin(23, true); log = stored(); log.write_index = 255; memcpy(s_tdc_disk, &log, sizeof(log));
    value("return", tdc_fs_event_log_read(), -1); value("no_write", s_tdc_writes, 0);
    /* flush/close 실패는 제품의 실제 반환을 그대로 남긴다.
     * 아래 기대값은 저장 성공을 확정할 수 없을 때 실패를 보고한다는 오류 계약이다.
     * 기존 제품의 0 반환을 기대값으로 복제하지 않는다. */
    begin(24, false); s_tdc_sync_error = 1; value("return", tdc_fs_event_log_init(), -1);
    begin(25, true); s_tdc_close_error = 1; value("return", tdc_fs_event_log_write(0x40), -1);
    printf("SUMMARY records=%d mismatches=%d\n", s_tdc_records, s_tdc_failures);
    return 0; /* Python 검사기가 실제/기대값과 알려진 실패 위치를 검사한다. */
}
