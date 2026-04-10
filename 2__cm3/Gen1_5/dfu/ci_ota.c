/**
 * @file ota.c
 */

#include <ci_ota.h>

#define _INFINITE_LOOP()                                                                                                                                                                                                                                                                                                       \
    while (1)                                                                                                                                                                                                                                                                                                                  \
    {                                                                                                                                                                                                                                                                                                                          \
        (void) 0;                                                                                                                                                                                                                                                                                                              \
    }

static bool                  _g_open    = false;
static CI_OTA_PREPARE_FILE_T _g_prepare = {0};

typedef struct __attribute__((packed))
{
    uint8_t  state;           // 0: STABLE, 1: OTA_UPDATE
    uint8_t  fall_back_slot;  // 0 or 1
    uint8_t  sub_state;       // 0: IDLE, 1: BOOT_TRY, 2: BOOT_OK, 3: BOOT_CONFIRM
    uint8_t  retry;           // 0 이상
    uint8_t  result;          // 0: SUCCESS, 1: FAIL
    uint8_t  reserved[3];     // 8바이트 정렬 + 향후 확장용
    uint32_t crc32;           // 구조체 자체에 대한 CRC
} CI_FILE_OTA_STATUS_T;

#ifndef BUFFER_SIZE_UP
static char s_buffer[16] = {
    0,
};
#else
static char s_buffer[BUFFER_SIZE_UP + 16] = {
    0,
};
#endif

static int  s_index = 0;
static char s_byte  = 0;

void ota_update_file(const char *name)
{
    int ret;

    RTT_printf("Update file '%s'. \r\n", name);

    ret = f_open(&g_ci_filesystem_ohdl, name, (FA_CREATE_ALWAYS | FA_READ | FA_WRITE));

    if (ret != FR_OK)
    {
        RTT_printf("Failed to open '%s' \r\n", name);
        return;
    }

    uint8_t      binary;
    uint8_t      parsing;
    uint8_t      ch;
    volatile int is_upper;
    size_t       len;
    volatile int lf;

    lf       = 0;
    is_upper = 1;

    while (1)
    {
        SYS_WATCHDOG_REFRESH();

        if (RTT_getch(&ch) == 1)
        {
            if (ch == '!')
            {
                RTT_printf("Received esacape char '!'. \r\n");
                break;
            }
            else
            {
                if ('0' <= ch && ch <= '9')
                {
                    parsing = ch - '0';

                    if (is_upper == 1)
                    {
                        binary   = parsing << 4;
                        is_upper = 0;
                    }
                    else
                    {
                        binary   = binary | parsing;
                        is_upper = 1;

                        ret = f_write(&g_ci_filesystem_ohdl, &binary, 1, &len);

                        if (ret != FR_OK || len != 1)
                        {
                            RTT_printf("Failed to write data!!! \r\n");
                            f_close(&g_ci_filesystem_ohdl);
                            return;
                        }

                        RTT_printf("%02X ", binary);

                        lf++;

                        if (lf == 64)
                        {
                            lf = 0;
                            RTT_printf("\r\n");
                        }
                    }
                }
                else if ('A' <= ch && ch <= 'F')
                {
                    parsing = 10 + (ch - 'A');

                    if (is_upper == 1)
                    {
                        binary   = parsing << 4;
                        is_upper = 0;
                    }
                    else
                    {
                        binary   = binary | parsing;
                        is_upper = 1;

                        ret = f_write(&g_ci_filesystem_ohdl, &binary, 1, &len);

                        if (ret != FR_OK || len != 1)
                        {
                            RTT_printf("Failed to write data!!! \r\n");
                            f_close(&g_ci_filesystem_ohdl);
                            return;
                        }

                        RTT_printf("%02X ", binary);

                        lf++;

                        if (lf == 64)
                        {
                            lf = 0;
                            RTT_printf("\r\n");
                        }
                    }
                }
            }
        }
    }

    f_close(&g_ci_filesystem_ohdl);

    RTT_printf("File write done. '%s' \r\n", name);
}

void ota_command_parsing(void)
{
    int     len;
    uint8_t boot_val;

    // 입력 데이터가 없으면 종료
    if (RTT_getch(&s_byte) != 1)
    {
        return;
    }

    RTT_printf("%c", s_byte);

    if (s_index < (BUFFER_SIZE_UP + 15))
    {
        s_buffer[s_index++] = s_byte;
        s_buffer[s_index]   = '\0';
    }

    // '\n'이 입력된게 아니면 종료
    if (s_byte != '\n')
    {
        return;
    }

    // '--write=m' 옵션 검사
    if (strncmp(s_buffer, "--write=m", strlen("--write=m")) == 0)
    {
        ota_update_file("/1/MANIFEST.TXT");
    }
    if (strncmp(s_buffer, "--write=a0", strlen("--write=a0")) == 0)
    {
        ota_update_file("/1/APP000.FEZ");
    }
    if (strncmp(s_buffer, "--write=a1", strlen("--write=a1")) == 0)
    {
        ota_update_file("/1/APP001.FEZ");
    }
    if (strncmp(s_buffer, "--write=a2", strlen("--write=a2")) == 0)
    {
        ota_update_file("/1/APP002.FEZ");
    }

    // '--boot=' 옵션 검사
    len = strlen("--boot=");
    if (strncmp(s_buffer, "--boot=", len) == 0)
    {
        boot_val = s_buffer[len] - '0';

        if (boot_val == 0 || boot_val == 1)
        {
            RTT_printf("Input boot value is %u. \r\n", boot_val);

            if (ci_filesystem_write("boot", &boot_val, 1) < 0)
            {
                RTT_printf("Failed to write boot value to 'boot' file. \r\n");
            }
        }
        else
        {
            if (boot_val == 2)
            {
                int                  ret;
                char                *name;
                UINT                 read;
                CI_FILE_OTA_STATUS_T otaStatus_file;

                name = "/ota_status.bin";

                ret = f_open(&g_ci_filesystem_ohdl, name, (FA_OPEN_EXISTING | FA_READ | FA_WRITE));

                if (ret != FR_OK)
                {
                    RTT_printf("[OTA] Failed to open file : %s \r\n", name);
                    return;
                }

                ret = f_read(&g_ci_filesystem_ohdl, &otaStatus_file, sizeof(otaStatus_file), &read);

                if (ret != FR_OK)
                {
                    RTT_printf("[OTA] Failed to read file : %s \r\n", name);
                    f_close(&g_ci_filesystem_ohdl);
                    return;
                }

                RTT_printf("[OTA] Status.state          = %u \r\n", otaStatus_file.state);
                RTT_printf("[OTA] Status.fall_back_slot = %u \r\n", otaStatus_file.fall_back_slot);
                RTT_printf("[OTA] Status.sub_state      = %u \r\n", otaStatus_file.sub_state);
                RTT_printf("[OTA] Status.retry          = %u \r\n", otaStatus_file.retry);
                RTT_printf("[OTA] Status.result         = %u \r\n", otaStatus_file.result);
            }
            else
            {
                RTT_printf("Valid values for the '--boot=' option are 0, 1 and 2 only. \r\n");
            }
        }
    }

    // '--valid' 옵션 검사
    len = strlen("--valid");
    if (strncmp(s_buffer, "--valid", len) == 0)
    {
        int    ret;
        size_t read;
        size_t read_len;
        size_t file_size;

        char *file_list[8] = {
            "/MANIFEST.TXT",
            "/APP000.FEZ",
            "/APP001.FEZ",
            "/APP002.FEZ",

            "/1/MANIFEST.TXT",
            "/1/APP000.FEZ",
            "/1/APP001.FEZ",
            "/1/APP002.FEZ",
        };

        char *p_file;

        for (int i = 0; i < 8; i++)
        {
            p_file = file_list[i];

            ret = f_open(&g_ci_filesystem_ohdl, p_file, (FA_OPEN_EXISTING | FA_READ));

            if (ret != FR_OK)
            {
                RTT_printf("Failed to open '%s' \r\n", p_file);
                // break;
            }
            else
            {

                file_size = f_size(&g_ci_filesystem_ohdl);
                RTT_printf("'%s' file size is %u. \r\n", p_file, file_size);

                f_close(&g_ci_filesystem_ohdl);
            }
        }
    }

    s_index     = 0;
    s_buffer[0] = '\0';
}

static const char *_get_file_name(CI_OTA_FILE_TYPE_E file_type)
{
    switch (file_type)
    {
        case CI_OTA_FILE_TYPE_MANIFEST:
            return CI_OTA_FILE_NAME_MANIFEST;
            break;

        case CI_OTA_FILE_TYPE_APP000:
            return CI_OTA_FILE_NAME_APP000;
            break;

        case CI_OTA_FILE_TYPE_APP001:
            return CI_OTA_FILE_NAME_APP001;
            break;

        case CI_OTA_FILE_TYPE_APP002:
            return CI_OTA_FILE_NAME_APP002;
            break;

        default:
            break;
    }

    return NULL;
}

static void _ensure_dir(const char *p_path)
{
    FRESULT res;
    DIR     dir;

    res = f_opendir(&dir, p_path);

    if ((res == FR_NO_PATH) || (res == FR_NO_FILE))
    {
        res = f_mkdir(p_path);

        if (res != FR_OK)
        {
            _INFINITE_LOOP();
        }
    }

    if (res == FR_OK)
    {
        f_closedir(&dir);
    }
}

CI_OTA_RET_E ci_ota_prepare_file(CI_OTA_PREPARE_FILE_T *p_prepare)
{
    FRESULT res;
    bool    ret;
    char   *p_name;
    char    path[CI_OTA_FILE_PATH_LEN_MAX] = {0};
    FIL    *fp;

    p_name = (char *) _get_file_name(p_prepare->file_type);

    if (p_name == NULL)
    {
        return CI_OTA_RET_ERROR_INVALID_FILE_TYPE;
    }

    path[0] = '0' + p_prepare->slot;
    path[1] = '/';
    path[2] = 0;

    _ensure_dir((const char *) path);

    strcat(path, p_name);

    // fp = ci_filesystem_get_fp();
    fp = ci_fatfs_get_fp();

    res = f_open(fp, path, (FA_CREATE_ALWAYS | FA_READ | FA_WRITE));

    if (res != FR_OK)
    {
        return CI_OTA_RET_ERROR_FILE_OPEN_FAIL;
    }

    res = f_lseek(fp, 0);

    if (res != FR_OK)
    {
        f_close(fp);
        return CI_OTA_RET_ERROR_FILE_SEEK;
    }

    _g_open    = true;
    _g_prepare = *p_prepare;

    return CI_OTA_RET_SUCCESS;
}

CI_OTA_RET_E ci_ota_write_file(uint8_t *p_data, uint32_t len)
{
    FIL    *fp;
    FRESULT res;
    UINT    bw;
    UINT    btw;

    btw = len;
    // fp  = ci_file_system_get_fp();
    fp  = ci_fatfs_get_fp();
    res = f_write(fp, p_data, btw, &bw);

    if ((res != FR_OK) || (bw != len))
    {
        return CI_OTA_RET_ERROR_FILE_WRITE;
    }

    return CI_OTA_RET_SUCCESS;
}
