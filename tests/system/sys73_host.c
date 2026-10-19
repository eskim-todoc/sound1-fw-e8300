/* 실제 감사 함수/오류 상태/시험 모듈을 링크한다. 사례마다 새 프로세스다.
 * 이 fixture의 FatFs/시간 fallback은 호출 즉시 실패하여 실경로 누출을 잡는다.
 * 공통 HOLD/LED 처리 및 실제 NVM 내구성은 이 호스트 검사의 범위가 아니다. */
#include <assert.h>
#include <stdarg.h>
#include <fcntl.h>
#include <io.h>
#include "ci_event_log.h"
#include "error.h"
#include "tdc_sys_test_73.h"

FIL g_ci_filesystem_ohdl;
static FIL s_saved_file;
static char s_log[20][61];
static unsigned s_lines;
static unsigned s_diagnostics;
static unsigned s_max_line;

void tdc_sys_storage_log(const char *format, ...)
{
    char body[160];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(body, sizeof(body), format, args);
    va_end(args);
    assert(length >= 0 && length <= 48 && s_lines < 20);
    assert(strchr(body, '\r') == NULL && strchr(body, '\n') == NULL);
    length = snprintf(s_log[s_lines++], 61, "[SYS-073] %s\r\n", body);
    assert(length >= 12 && length <= 60);
    if ((unsigned)length > s_max_line)
        s_max_line = (unsigned)length;
}

int SEGGER_RTT_printf(unsigned index, const char *format, ...)
{
    (void)index;
    (void)format;
    ++s_diagnostics;
    return 0;
}

void ci_print_file_func_line(const char *file, const char *func, int line)
{
    (void)file;
    (void)func;
    (void)line;
    ++s_diagnostics;
}

CI_TIMER_TIME_T ci_timer_get_reference_time_after_self_update(void) { abort(); }
FRESULT f_open(FIL *fp, const TCHAR *name, BYTE mode) { (void)fp; (void)name; (void)mode; abort(); }
FRESULT f_read(FIL *fp, void *data, UINT size, UINT *count) { (void)fp; (void)data; (void)size; (void)count; abort(); }
FRESULT f_write(FIL *fp, const void *data, UINT size, UINT *count) { (void)fp; (void)data; (void)size; (void)count; abort(); }
FRESULT f_lseek(FIL *fp, FSIZE_t offset) { (void)fp; (void)offset; abort(); }
FRESULT f_sync(FIL *fp) { (void)fp; abort(); }
FRESULT f_close(FIL *fp) { (void)fp; abort(); }
FRESULT f_stat(const TCHAR *name, FILINFO *info) { (void)name; (void)info; abort(); }

static bool has(const char *body)
{
    char expected[61];
    int size = snprintf(expected, sizeof(expected), "[SYS-073] %s\r\n", body);
    assert(size >= 0 && size <= 60);
    for (unsigned i = 0; i < s_lines; ++i)
        if (strcmp(s_log[i], expected) == 0)
            return true;
    return false;
}

static void check_usage(void)
{
    char *bad[] = {"sys73", "init", "invalid"};
    char *extra[] = {"sys73", "led", "extra"};
    char *wrong[] = {"sys54", "init", "open"};
    char *missing[] = {"sys73", "write", NULL};
    char *null_name[] = {NULL, "init", "open"};
    char *null_action[] = {"sys73", NULL, "open"};
    assert(tdc_sys_test_73_execute(3, bad) == -1);
    assert(tdc_sys_test_73_execute(2, bad) == -1);
    assert(tdc_sys_test_73_execute(0, NULL) == -1);
    assert(tdc_sys_test_73_execute(3, extra) == -1);
    assert(tdc_sys_test_73_execute(3, wrong) == -1);
    assert(tdc_sys_test_73_execute(3, missing) == -1);
    assert(tdc_sys_test_73_execute(3, null_name) == -1);
    assert(tdc_sys_test_73_execute(3, null_action) == -1);
    assert(s_lines == 0 && readErrorCode().data_logging_error == 0);
    puts("USAGE_CHECKED=8 NO_ACTION=1");
}

static void check_busy(void)
{
    static FIL file;
    static CI_EVENT_LOG_T buffer;
    tdc_event_log_test_ops_t ops = {
        .file = &file, .buffer = &buffer,
        .open = f_open, .read = f_read, .write = f_write,
        .seek = f_lseek, .sync = f_sync, .close = f_close, .stat = f_stat
    };
    assert(!tdc_event_log_test_begin(NULL));
    tdc_event_log_test_ops_t incomplete = ops;
    incomplete.close = NULL;
    assert(!tdc_event_log_test_begin(&incomplete));
    incomplete = ops;
    incomplete.buffer = NULL;
    assert(!tdc_event_log_test_begin(&incomplete));
    assert(tdc_event_log_test_begin(&ops));
    assert(!tdc_event_log_test_begin(&ops));
    char *args[] = {"sys73", "write", "open"};
    assert(tdc_sys_test_73_execute(3, args) == 0);
    assert(has("REJECT=IO_BOUNDARY_BUSY") && s_lines == 1);
    assert(readErrorCode().data_logging_error == 0);
    assert(!tdc_event_log_test_begin(&ops));
    tdc_event_log_test_end();
    assert(tdc_event_log_test_begin(&ops));
    tdc_event_log_test_end();
    puts("BOUNDARY_CHECKED=1 BUSY_PRESERVED=1");
}

int main(int argc, char *argv[])
{
    assert(_setmode(_fileno(stdout), _O_BINARY) != -1);
    assert(argc >= 2);
    assert(readErrorCode().data_logging_error == 0);
    CI_EVENT_LOG_BT_ADDR_T address = {{2, 0, 0, 0, 0, 0x55}};
    ci_event_log_update_bt_addr(&address);
    memset(&g_ci_filesystem_ohdl, 0xa7, sizeof(g_ci_filesystem_ohdl));
    s_saved_file = g_ci_filesystem_ohdl;
    const unsigned diagnostics = s_diagnostics;

    /* 원래 작업 버퍼 주소에 PC 전용 보호 페이지를 놓는다. 실제 보드 접근이
     * 아니며, 시험 경계가 새면 읽기 전용 페이지의 쓰기 위반으로 실패한다. */
    const uintptr_t work_address = (uintptr_t)CI_FILESYSTEM_BASE_ADDR_FOR_EVENT_LOG;
    void *const page_base = (void *)(work_address & ~(uintptr_t)0xffffu);
    void *const page = VirtualAlloc(page_base, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    assert(page == page_base && work_address + sizeof(CI_EVENT_LOG_T) <= (uintptr_t)page + 0x10000);
    memset((void *)work_address, 0x5a, sizeof(CI_EVENT_LOG_T));
    DWORD previous;
    assert(VirtualProtect(page, 0x10000, PAGE_READONLY, &previous));

    if (strcmp(argv[1], "usage") == 0)
        check_usage();
    else if (strcmp(argv[1], "boundary") == 0)
        check_busy();
    else
    {
        argv[0] = "sys73";
        /* 전경은 기존 오류를 거절한다. 여기서는 모듈이 다른 오류를 지우지
         * 않는지도 따로 검사하며, 이 입력을 정상 UI 진입으로 취급하지 않는다. */
        errorCodeUpdate(en__RF_PowerIC_ERROR, en__NON_RESETTABLE, 1);
        assert(tdc_sys_test_73_execute(argc, argv) == 0);
        assert(readErrorCode().PowerIcErrorFlag == en__NON_RESETTABLE);
        const int actual = (int)readErrorCode().data_logging_error;
        if (strcmp(argv[1], "led") == 0)
        {
            assert(argc == 2 && actual == 1 && s_lines == 5);
            assert(has("OBS=LED_DEMO_ONLY") && !has("CHECK=MATCH"));
        }
        else
        {
            assert(argc == 3 && s_lines == 10);
            char expected[49];
            int detail = strcmp(argv[2], "open") == 0 ? 1 :
                         (strcmp(argv[2], "read") == 0 || strcmp(argv[2], "short-read") == 0) ? 3 : 2;
            snprintf(expected, sizeof(expected), "ERROR=%d/%d EXPECT=10/%d", actual ? 10 : 0, actual, detail);
            assert(has(expected));
            assert(has(actual ? "OBS=PRODUCT_ERROR" : "OBS=NO_PRODUCT_ERROR"));
            assert(has("RESULT=NOT_SYSTEM_PASS"));
        }
        printf("ACTUAL_DETAIL=%d\n", actual);
    }
    assert(memcmp(&s_saved_file, &g_ci_filesystem_ohdl, sizeof(s_saved_file)) == 0);
    CI_EVENT_LOG_BT_ADDR_T after = ci_event_log_get_bt_addr();
    assert(memcmp(&after, &address, sizeof(address)) == 0);
    assert(s_diagnostics == diagnostics);
    for (unsigned i = 0; i < sizeof(CI_EVENT_LOG_T); ++i)
        assert(((const unsigned char *)work_address)[i] == 0x5a);
    assert(VirtualFree(page, 0, MEM_RELEASE));
    for (unsigned i = 0; i < s_lines; ++i)
        fputs(s_log[i], stdout);
    printf("HOST_OK lines=%u max_line=%u\n", s_lines, s_max_line);
    puts("PRESERVED=FILE_HANDLE,BUFFER,BT,OTHER_ERROR");
    puts("REAL_IO_TIME_CALLS=0 PRODUCT_DIAGNOSTICS=0");
    return 0;
}
