
#include <ci_boot.h>
#include <ci_initialize.h>
#include <rtt_printf.h>
#include <uart_printf.h>

//
// private macros
//
#define _INFINITE_LOOP()                                                                                                                                                                                                                                                                                                       \
    while (1)                                                                                                                                                                                                                                                                                                                  \
    {                                                                                                                                                                                                                                                                                                                          \
        (void) 0;                                                                                                                                                                                                                                                                                                              \
    }

#define _DELAY_MS(ms) Sys_Delay((SystemCoreClock / 1000) * ms)

//
// private function headers
//
static void _handle_debug_mode(void);
static void _print_fw_file(char *p_path);
static bool _is_boot_file_exist(void);
static void _read_boot_file(void);
static void _initialize_boot_file(void);
static void _update_boot_file(void);
static void _determine_slot_num(void);
static void _validate_alt_boot(void);
static bool _debug_mode_countdown(uint8_t sec);

//
// private variables
//
static uint8_t                _g_slot_num    = 0;
static ST__CI_LIB_BOOT_STATUS _g_boot_status = {0};
static FIL                   *_g_fp;

//
// functions
//
uint8_t ci_boot_get_slot_num(void)
{
    return _g_slot_num;
}

void ci_boot_set_fp(FIL *fp)
{
    _g_fp = fp;
}

void ci_boot_handle_boot_file(void)
{
    // 상태 파일 존재 유무 확인
    if (_is_boot_file_exist())
    {
        // 상태 파일 있음
        _read_boot_file();  // 읽기
    }
    else
    {
        // 상태 파일 없음
        uart_printf("There is no boot file. \r\n");
        _initialize_boot_file();  // 초기화
        _update_boot_file();      // 업데이트
    }

    _determine_slot_num();  // 상태 정보를 토대로 부팅 슬롯 결정

    SYS_WATCHDOG_REFRESH();
}

void ci_boot_print_boot_file(void)
{
    uart_printf("Boot file information: \r\n");
    uart_printf("version = %u.%u \r\n", _g_boot_status.major_ver, _g_boot_status.minor_ver);
    uart_printf("state = %u \r\n", _g_boot_status.state);
    uart_printf("sub_state = %u \r\n", _g_boot_status.sub_state);
    uart_printf("boot_slot_num = %u \r\n", _g_boot_status.boot_slot_num);
    uart_printf("last_boot_slot_num = %u \r\n", _g_boot_status.last_boot_slot_num);
    uart_printf("alt_boot_slot_num = %u \r\n", _g_boot_status.alt_boot_slot_num);
    uart_printf("alt_boot_try_count = %u \r\n", _g_boot_status.alt_boot_try_count);
    uart_printf("alt_boot_result = %u \r\n", _g_boot_status.alt_boot_result);
    uart_printf("slot_1_state = %u \r\n", _g_boot_status.slot_1_state);
    uart_printf("slot_2_state = %u \r\n", _g_boot_status.slot_2_state);
    uart_printf("slot_3_state = %u \r\n", _g_boot_status.slot_3_state);
    uart_printf("slot_4_state = %u \r\n", _g_boot_status.slot_4_state);
    uart_printf("reserved = 0x");
    for (int i = 0; i < 47; i++)
    {
        uart_printf("%02X", _g_boot_status.reserved[i]);
    }
    uart_printf("\r\n");
    uart_printf("crc32 = 0x%08X \r\n", _g_boot_status.crc32);
}

void ci_boot_debug_mode(void)
{
    if (!_debug_mode_countdown(CI_BOOT_COUNT_DOWN_SEC))
    {
        return;
    }

    _handle_debug_mode();
}

//
// private functions
//
static void _handle_debug_mode(void)
{
    uint8_t buf[64] = {0};
    int     idx     = 0;
    char    ch;

    uart_printf("\r\nDebug mode entered. \r\n");

    while (1)
    {
        SYS_WATCHDOG_REFRESH();

        if (0 < uart_getch(&ch))
        {
            if (ch == '\n')
            {
                uart_printf("\r\n");

                if (strncmp((const char *) buf, "--quit", 6) == 0)
                {
                    break;
                }
                else if (strncmp((const char *) buf, "--print=mf", 10) == 0)
                {
                    _print_fw_file("/MANIFEST.TXT");
                    idx      = 0;
                    buf[idx] = 0;
                }
                else if (strncmp((const char *) buf, "--print=a0", 10) == 0)
                {
                    _print_fw_file("/APP000.FEZ");
                    idx      = 0;
                    buf[idx] = 0;
                }
                else if (strncmp((const char *) buf, "--print=a1", 10) == 0)
                {
                    _print_fw_file("/APP001.FEZ");
                    idx      = 0;
                    buf[idx] = 0;
                }
                else if (strncmp((const char *) buf, "--print=a2", 10) == 0)
                {
                    _print_fw_file("/APP002.FEZ");
                    idx      = 0;
                    buf[idx] = 0;
                }
                else
                {
                    if (idx != 0)
                    {
                        uart_printf("invalid command. \r\n");
                    }
                }

                idx      = 0;
                buf[idx] = 0;
            }
            else if (ch == '\b')
            {
                if (0 < idx)
                {
                    idx--;
                }

                buf[idx] = 0;

                uart_printf("\b \b");
            }
            else
            {
                if (idx < 62)
                {
                    buf[idx]     = ch;
                    buf[idx + 1] = 0;
                    idx++;
                }

                uart_printf("%c", ch);
            }
        }
    }
}

static void _print_fw_file(char *p_path)
{
    uint8_t rtt_buffer[64];
    UINT    br;
    FRESULT res;

    res = f_open(_g_fp, p_path, (FA_OPEN_EXISTING | FA_READ));

    if (res != FR_OK)
    {
        uart_printf("[ERROR] Failed to open '%s' file. \r\n", p_path);
        return;
    }

    rtt_printf("%s=<<<<<\r\n", p_path);

    while (1)
    {
        SYS_WATCHDOG_REFRESH();

        res = f_read(_g_fp, rtt_buffer, 64, &br);

        if (res != FR_OK)
        {
            uart_printf("[ERROR] Failed to read '%s' file. \r\n", p_path);
            f_close(_g_fp);
            break;
        }

        if (br == 0)
        {
            rtt_printf(">>>>>\r\n");
            uart_printf("print done. \r\n");
            break;
        }

        for (int i = 0; i < br; i++)
        {
            rtt_printf("%02X ", rtt_buffer[i]);
        }

        rtt_printf("\r\n");
    }

    f_close(_g_fp);
}

static bool _is_boot_file_exist(void)
{
    FILINFO fno;
    FRESULT res;

    res = f_stat(SDK_CI_BOOT_FILE_PATH, &fno);

    if (res != FR_OK)
    {
        return false;
    }

    if (fno.fsize != sizeof(ST__CI_LIB_BOOT_STATUS))
    {
        return false;
    }

    return true;
}

static void _read_boot_file(void)
{
    UINT    br;
    FRESULT res;

    res = f_open(_g_fp, SDK_CI_BOOT_FILE_PATH, (FA_OPEN_EXISTING | FA_READ | FA_WRITE));

    if (res == FR_OK)
    {
        res = f_read(_g_fp, &_g_boot_status, sizeof(ST__CI_LIB_BOOT_STATUS), &br);

        if (res == FR_OK)
        {
            if (br == sizeof(ST__CI_LIB_BOOT_STATUS))
            {
                f_close(_g_fp);
                return;
            }
        }
    }

    uart_printf("[ERROR] Failed to read '%s' file. \r\n", SDK_CI_BOOT_FILE_PATH);
    _INFINITE_LOOP();
}

static void _initialize_boot_file(void)
{
    _g_boot_status.major_ver          = SDK_CI_BOOT_VER_MAJOR;
    _g_boot_status.minor_ver          = SDK_CI_BOOT_VER_MINOR;
    _g_boot_status.state              = SDK_CI_BOOT_STATE_BOOT;
    _g_boot_status.sub_state          = SDK_CI_BOOT_SUB_STATE_IDLE;
    _g_boot_status.boot_slot_num      = 0xFF;
    _g_boot_status.last_boot_slot_num = 0xFF;
    _g_boot_status.alt_boot_slot_num  = 0;
    _g_boot_status.alt_boot_try_count = 0;
    _g_boot_status.alt_boot_result    = SDK_CI_BOOT_ALT_BOOT_RESULT_NONE;
    _g_boot_status.slot_1_state       = SDK_CI_BOOT_SLOT_STATE_NONE;
    _g_boot_status.slot_2_state       = SDK_CI_BOOT_SLOT_STATE_NONE;
    _g_boot_status.slot_3_state       = SDK_CI_BOOT_SLOT_STATE_NONE;
    _g_boot_status.slot_4_state       = SDK_CI_BOOT_SLOT_STATE_NONE;

    for (int i = 0; i < 47; i++)
    {
        _g_boot_status.reserved[i] = 0;
    }

    _g_boot_status.crc32 = 0;
}

static void _update_boot_file(void)
{
    UINT    btw;
    FRESULT res;

    res = f_open(_g_fp, SDK_CI_BOOT_FILE_PATH, (FA_OPEN_ALWAYS | FA_READ | FA_WRITE));

    if (res == FR_OK)
    {
        f_lseek(_g_fp, 4096);  // FATFS에게 4KB로 고정된 파일을 생성할 수 있게 의도적으로 파일 포지션을 4096으로 설정
        f_lseek(_g_fp, 0);

        rtt_printf("[FATFS] PERFORMED : FILE (%s) LSEEK --> 4096 --> 0 \r\n", SDK_CI_BOOT_FILE_PATH);

        res = f_write(_g_fp, &_g_boot_status, sizeof(ST__CI_LIB_BOOT_STATUS), &btw);

        if (res == FR_OK)
        {
            // 안전을 위한 flush
            f_sync(_g_fp);

#if 1
            FILINFO fno;
            res = f_stat(SDK_CI_BOOT_FILE_PATH, &fno);
            if (res == FR_OK)
            {
                rtt_printf("[FATFS] NAME : %s, TOTAL SIZE : %u BYTES, START CLUSTER : %u \r\n",  //
                           fno.fname,
                           fno.fsize,
                           _g_fp->obj.sclust);
            }
#endif

            if (btw == sizeof(ST__CI_LIB_BOOT_STATUS))
            {
                f_close(_g_fp);
                return;
            }
        }
    }

    uart_printf("[ERROR] Failed to update '%s' file. \r\n", SDK_CI_BOOT_FILE_PATH);
    _INFINITE_LOOP();
}

static void _determine_slot_num(void)
{
    switch (_g_boot_status.state)
    {
        case SDK_CI_BOOT_STATE_BOOT:
            _g_slot_num = _g_boot_status.boot_slot_num;
            break;

        case SDK_CI_BOOT_STATE_ALT_BOOT:
            _validate_alt_boot();
            break;

        default:  // 존재할 수 없는 상태
        {
            _g_slot_num              = _g_boot_status.boot_slot_num;
            _g_boot_status.sub_state = SDK_CI_BOOT_SUB_STATE_UNKNOWN;
            _update_boot_file();
        }
        break;
    }
}

static void _validate_alt_boot(void)
{
    switch (_g_boot_status.sub_state)
    {
        case SDK_CI_BOOT_SUB_STATE_BOOT_TRY:
        {
            _g_boot_status.alt_boot_try_count++;

            if (_g_boot_status.alt_boot_try_count > SDK_CI_BOOT_ALT_BOOT_TRY_MAX)
            {
                _g_slot_num                    = _g_boot_status.boot_slot_num;
                _g_boot_status.sub_state       = SDK_CI_BOOT_SUB_STATE_BOOT_TRY_DONE;
                _g_boot_status.alt_boot_result = SDK_CI_BOOT_ALT_BOOT_RESULT_FAIL;
                _g_boot_status.state           = SDK_CI_BOOT_STATE_BOOT;

                uart_printf("[ERROR] Can not boot with the 'alt_slot'. ");
                uart_printf("Therefore, boot with the 'boot_slot'. \r\n");
            }
            else
            {
                _g_slot_num = _g_boot_status.alt_boot_slot_num;
            }

            _update_boot_file();
        }
        break;

        case SDK_CI_BOOT_SUB_STATE_BOOT_TRY_DONE:
        {
            if (_g_boot_status.alt_boot_result == SDK_CI_BOOT_ALT_BOOT_RESULT_SUCCESS)
            {
                _g_slot_num = _g_boot_status.alt_boot_slot_num;
            }
            else
            {
                _g_slot_num              = _g_boot_status.boot_slot_num;
                _g_boot_status.sub_state = SDK_CI_BOOT_SUB_STATE_UNKNOWN;
                _update_boot_file();
            }
        }
        break;

        default:
        {
            _g_slot_num              = _g_boot_status.boot_slot_num;
            _g_boot_status.sub_state = SDK_CI_BOOT_SUB_STATE_UNKNOWN;
            _update_boot_file();
        }
        break;
    }
}

static bool _debug_mode_countdown(uint8_t sec)
{
    bool ret = false;
    char ch;

    uart_printf("Debug mode count down : ");

    for (int i = 0; i < sec; i++)
    {
        uart_printf("\b%u", sec - i);

        for (int j = 0; j < 100; j++)
        {
            if (ret)
            {
                break;
            }

            if ((volatile int) 0 < uart_getch(&ch))
            {
                if (ch == '\n')
                {
                    ret = true;
                    break;
                }
            }
            SYS_WATCHDOG_REFRESH();
            _DELAY_MS(10);
        }
    }

    uart_printf("\b0 \r\n");

    return ret;
}
