/* SYS-073: 사설 RAM 파일의 I/O 실패와 실제 제품 오류 반응을 구분한다. */
#include "tdc_sys_test_73.h"

#if TDC_SYS_TEST_73_ENABLE
#include <string.h>
#include "ci_event_log.h"
#include "error.h"
#include "tdc_sys_test_storage.h"

typedef enum
{
    TDC_SYS73_OPEN,
    TDC_SYS73_READ,
    TDC_SYS73_SHORT_READ,
    TDC_SYS73_WRITE,
    TDC_SYS73_SHORT_WRITE,
    TDC_SYS73_SYNC,
    TDC_SYS73_CLOSE,
    TDC_SYS73_FAULT_COUNT
} tdc_sys73_fault_t;

static const char *const s_tdc_fault_names[TDC_SYS73_FAULT_COUNT] = {
    "open", "read", "short-read", "write", "short-write", "sync", "close"
};

/* 큰 FIL/파일/작업 버퍼는 스택에 두지 않는다. 실제 FS 전역과 독립이다. */
static struct
{
    FIL file;
    CI_EVENT_LOG_T work;
    CI_EVENT_LOG_T disk;
    FSIZE_t position;
    UINT size;
    BYTE mode;
    bool opened;
    tdc_sys73_fault_t fault;
    unsigned reached;
    FRESULT injected_result;
} s_tdc_ram;

_Static_assert(sizeof(CI_EVENT_LOG_T) == 4096, "SYS-073 requires the 4096-byte audit format");

static bool inject(tdc_sys73_fault_t fault)
{
    if (s_tdc_ram.fault != fault || s_tdc_ram.reached != 0u)
        return false;
    s_tdc_ram.reached = 1u;
    s_tdc_ram.injected_result = (fault == TDC_SYS73_SHORT_READ || fault == TDC_SYS73_SHORT_WRITE) ?
                               FR_OK : FR_DISK_ERR;
    return true;
}

static bool is_open(FIL *file)
{
    return file == &s_tdc_ram.file && s_tdc_ram.opened;
}

static FRESULT ram_open(FIL *file, const TCHAR *name, BYTE mode)
{
    if (file != &s_tdc_ram.file || strcmp(name, CI_EVENT_LOG_IDENT) != 0 || s_tdc_ram.opened)
        return FR_INVALID_OBJECT;
    if (inject(TDC_SYS73_OPEN))
        return FR_DISK_ERR;
    s_tdc_ram.mode = mode;
    s_tdc_ram.position = 0;
    s_tdc_ram.opened = true;
    return FR_OK;
}

static FRESULT ram_read(FIL *file, void *data, UINT count, UINT *read_count)
{
    *read_count = 0;
    if (!is_open(file) || !(s_tdc_ram.mode & FA_READ) || s_tdc_ram.position > s_tdc_ram.size)
        return FR_INVALID_OBJECT;
    if (inject(TDC_SYS73_READ))
        return FR_DISK_ERR;
    UINT available = s_tdc_ram.size - (UINT)s_tdc_ram.position;
    if (count > available)
        count = available;
    if (inject(TDC_SYS73_SHORT_READ) && count != 0u)
        --count;
    memcpy(data, (const unsigned char *)&s_tdc_ram.disk + s_tdc_ram.position, count);
    *read_count = count;
    s_tdc_ram.position += count;
    return FR_OK;
}

static FRESULT ram_write(FIL *file, const void *data, UINT count, UINT *written)
{
    *written = 0;
    if (!is_open(file) || !(s_tdc_ram.mode & FA_WRITE) || s_tdc_ram.position > sizeof(s_tdc_ram.disk) ||
        count > sizeof(s_tdc_ram.disk) - s_tdc_ram.position)
        return FR_INVALID_OBJECT;
    if (inject(TDC_SYS73_WRITE))
        return FR_DISK_ERR;
    if (inject(TDC_SYS73_SHORT_WRITE) && count != 0u)
        --count;
    memcpy((unsigned char *)&s_tdc_ram.disk + s_tdc_ram.position, data, count);
    *written = count;
    s_tdc_ram.position += count;
    if (s_tdc_ram.position > s_tdc_ram.size)
        s_tdc_ram.size = (UINT)s_tdc_ram.position;
    return FR_OK;
}

static FRESULT ram_seek(FIL *file, FSIZE_t offset)
{
    if (!is_open(file) || offset > sizeof(s_tdc_ram.disk))
        return FR_INVALID_OBJECT;
    if (offset > s_tdc_ram.size && (s_tdc_ram.mode & FA_WRITE))
        s_tdc_ram.size = (UINT)offset;
    s_tdc_ram.position = offset;
    return FR_OK;
}

static FRESULT ram_sync(FIL *file)
{
    if (!is_open(file))
        return FR_INVALID_OBJECT;
    return inject(TDC_SYS73_SYNC) ? FR_DISK_ERR : FR_OK;
}

static FRESULT ram_close(FIL *file)
{
    if (!is_open(file))
        return FR_INVALID_OBJECT;
    if (inject(TDC_SYS73_CLOSE))
        return FR_DISK_ERR;
    /* 제품 함수의 직접 호출 경계를 관찰한다. write에 없는 f_sync 호출을
     * 여기서 만들어 sync 결함에 도달한 것처럼 표시하지 않는다. */
    s_tdc_ram.opened = false;
    return FR_OK;
}

static FRESULT ram_stat(const TCHAR *name, FILINFO *info)
{
    if (strcmp(name, CI_EVENT_LOG_IDENT) != 0)
        return FR_INVALID_NAME;
    memset(info, 0, sizeof(*info));
    info->fsize = s_tdc_ram.size;
    return FR_OK;
}

static const tdc_event_log_test_ops_t s_tdc_ops = {
    .file = &s_tdc_ram.file,
    .buffer = &s_tdc_ram.work,
    .time = {26, 5, 29, 12, 0, 0},
    .address = {{0x02, 0, 0, 0, 0, 0x73}},
    .open = ram_open,
    .read = ram_read,
    .write = ram_write,
    .seek = ram_seek,
    .sync = ram_sync,
    .close = ram_close,
    .stat = ram_stat
};

int tdc_sys_test_73_execute(int argc, char *argv[])
{
    bool init;
    unsigned fault;
    if (argv == NULL || argc < 2 || argv[0] == NULL || argv[1] == NULL || strcmp(argv[0], "sys73") != 0)
        return -1;
    if (argc == 2 && strcmp(argv[1], "led") == 0)
    {
        tdc_sys_storage_log("SRC=EXPLICIT_LED_ERROR");
        errorCodeUpdate(en__MAJOR_ERRORCODE_DATA_LOGGING_ERROR, en__data_logging_error_open, __LINE__);
        tdc_sys_storage_log("ERROR=10/%d", (int)readErrorCode().data_logging_error);
        tdc_sys_storage_log("OBS=LED_DEMO_ONLY");
        tdc_sys_storage_log("TEST_HOLD=1 PRODUCT_STOP=NOT_PROVEN");
        tdc_sys_storage_log("RESULT=NOT_SYSTEM_PASS");
        return 0;
    }
    if (argc != 3 || argv[2] == NULL || (strcmp(argv[1], "init") != 0 && strcmp(argv[1], "write") != 0))
        return -1;
    init = strcmp(argv[1], "init") == 0;
    for (fault = 0; fault < TDC_SYS73_FAULT_COUNT; ++fault)
        if (strcmp(argv[2], s_tdc_fault_names[fault]) == 0)
            break;
    if (fault == TDC_SYS73_FAULT_COUNT)
        return -1;

    /* usage 검증 뒤 한 번만 실행한다. 정상 create/write를 실제 파일에
     * 시도하지 않으며, 유효 파일/신규 파일도 전부 사설 RAM에서 준비한다. */
    if (!tdc_event_log_test_begin(&s_tdc_ops))
    {
        tdc_sys_storage_log("REJECT=IO_BOUNDARY_BUSY");
        return 0;
    }
    memset(&s_tdc_ram, 0, sizeof(s_tdc_ram));
    s_tdc_ram.fault = (tdc_sys73_fault_t)fault;
    if (!init || (fault != TDC_SYS73_WRITE && fault != TDC_SYS73_SHORT_WRITE))
    {
        memcpy(s_tdc_ram.disk.ident, CI_EVENT_LOG_IDENT, sizeof(s_tdc_ram.disk.ident));
        s_tdc_ram.size = sizeof(s_tdc_ram.disk);
    }
    const int result = init ? ci_event_log_init() : ci_event_log_write(CI_EVENT_LOG_TYPE_INTEGRITY_ERROR);
    const int actual_error = (int)readErrorCode().data_logging_error;
    tdc_event_log_test_end();

    const int expected_error = fault == TDC_SYS73_OPEN ? en__data_logging_error_open :
                              (fault == TDC_SYS73_READ || fault == TDC_SYS73_SHORT_READ) ?
                              en__data_logging_error_not_inited : en__data_logging_error_write;
    const char *check = s_tdc_ram.reached == 0u ? "NOT_REACHED" :
                        (result == -1 && actual_error == expected_error) ? "MATCH" : "GAP";
    tdc_sys_storage_log("SRC=RAM_IO OP=%s FAULT=%s", argv[1], s_tdc_fault_names[fault]);
    tdc_sys_storage_log("RET=%d EXPECT=-1", result);
    tdc_sys_storage_log("ERROR=%d/%d EXPECT=10/%d", actual_error ? 10 : 0, actual_error, expected_error);
    tdc_sys_storage_log("FAULT_REACHED=%u IO_RES=%d", s_tdc_ram.reached, (int)s_tdc_ram.injected_result);
    tdc_sys_storage_log("CHECK=%s", check);
    tdc_sys_storage_log("OBS=%s", actual_error ? "PRODUCT_ERROR" : "NO_PRODUCT_ERROR");
    tdc_sys_storage_log("PRODUCT_DIAG=SUPPRESSED");
    tdc_sys_storage_log("NVM=UNTOUCHED TEST_HOLD=1");
    tdc_sys_storage_log("PRODUCT_STOP=NOT_PROVEN");
    tdc_sys_storage_log("RESULT=NOT_SYSTEM_PASS");
    return 0;
}
#endif
