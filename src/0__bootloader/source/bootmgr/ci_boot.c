
#include <ci_boot.h>
#include <ci_initialize.h>
#include <rtt_printf.h>
#include <tdc_uart.h>

//
// private macros
//
#define _INFINITE_LOOP()                                                                                                                                                                                                                                                                                                       \
    while (1)                                                                                                                                                                                                                                                                                                                  \
    {                                                                                                                                                                                                                                                                                                                          \
        (void) 0;                                                                                                                                                                                                                                                                                                              \
    }

//
// private function headers
//
static bool snd_boot_is_file_exist(void);
static bool snd_boot_read_file(void);
static void snd_boot_initialize_file(void);
static void snd_boot_update_file(void);
static void snd_boot_determine_slot_num(void);
static void snd_boot_validate_alt_boot(void);

//
// private variables
//
static uint8_t           sg_slot_num    = 0;
static snd_boot_status_t sg_boot_status = {0};
static FIL              *sg_fp;

//
// functions
//
uint8_t tdc_boot_get_slot_num(void)
{
    return sg_slot_num;
}

void snd_boot_set_fp(FIL *fp)
{
    sg_fp = fp;
}

void snd_boot_handle_file(void)
{
    bool is_exist = false;
    bool is_valid = false;

    // 상태 파일 존재 유무 확인
    is_exist = snd_boot_is_file_exist();

    // 상태 파일 있음
    if (is_exist)
    {
        tdc_uart_printf("[BOOT] FILE EXISTS \r\n");
        is_valid = snd_boot_read_file();  // 읽기
    }
    else
    {
        tdc_uart_printf("[BOOT] FILE NOT EXISTS \r\n");
    }

    // 상태 파일이 없거나, 상태 파일에 문제가 있을 경우
    if ((!is_exist) || (!is_valid))
    {
        tdc_uart_printf("[BOOT] FILE INIT \r\n");
        snd_boot_initialize_file();  // 초기화
        snd_boot_update_file();      // 업데이트
    }

#if 1
    FILINFO fno;
    FRESULT fr;
    fr = f_open(sg_fp, SND_BOOT_FILE_NAME, (FA_OPEN_EXISTING | FA_READ | FA_WRITE));
    if (fr == FR_OK)
    {
        fr = f_stat(SND_BOOT_FILE_NAME, &fno);
        if (fr == FR_OK)
        {
            tdc_uart_printf("[BOOT] FILE : %s, TOTAL SIZE : %u (BYTE), START CLUSTER : %u \r\n",  //
                            fno.fname,
                            fno.fsize,
                            sg_fp->obj.sclust);
        }
        else
        {
            tdc_uart_printf("[BOOT] GET STATUS FAILED (FR : %d) \r\n", fr);
        }

        f_close(sg_fp);
    }
#endif

    snd_boot_determine_slot_num();  // 상태 정보를 토대로 부팅 슬롯 결정

    SYS_WATCHDOG_REFRESH();
}

void tdc_boot_print_boot_file(void)
{
    tdc_uart_printf("Boot file information: \r\n");
    tdc_uart_printf("version = %u.%u \r\n", sg_boot_status.major_ver, sg_boot_status.minor_ver);
    tdc_uart_printf("state = %u \r\n", sg_boot_status.state);
    tdc_uart_printf("sub_state = %u \r\n", sg_boot_status.sub_state);
    tdc_uart_printf("boot_slot_num = %u \r\n", sg_boot_status.boot_slot_num);
    tdc_uart_printf("last_boot_slot_num = %u \r\n", sg_boot_status.last_boot_slot_num);
    tdc_uart_printf("alt_boot_slot_num = %u \r\n", sg_boot_status.alt_boot_slot_num);
    tdc_uart_printf("alt_boot_try_count = %u \r\n", sg_boot_status.alt_boot_try_count);
    tdc_uart_printf("alt_boot_result = %u \r\n", sg_boot_status.alt_boot_result);
    tdc_uart_printf("slot_1_state = %u \r\n", sg_boot_status.slot_1_state);
    tdc_uart_printf("slot_2_state = %u \r\n", sg_boot_status.slot_2_state);
    tdc_uart_printf("slot_3_state = %u \r\n", sg_boot_status.slot_3_state);
    tdc_uart_printf("slot_4_state = %u \r\n", sg_boot_status.slot_4_state);
    tdc_uart_printf("reserved = 0x");
    for (int i = 0; i < 47; i++)
    {
        tdc_uart_printf("%02X", sg_boot_status.reserved[i]);
    }
    tdc_uart_printf("\r\n");
    tdc_uart_printf("crc32 = 0x%08X \r\n", sg_boot_status.crc32);
}

//
// private functions
//
static bool snd_boot_is_file_exist(void)
{
    FILINFO fno;
    FRESULT res;

    res = f_stat(SND_BOOT_FILE_NAME, &fno);

    if (res != FR_OK)
    {
        return false;
    }

    // NOTE: 파일 생성, 또는 파일 쓰기 시에 f_lseek()를 사용해 파일의 최소 크기를 4KB (1섹터)로 구성한다.
    // 그래서 아래와 같이 파일의 크기를 특정 데이터 구조의 크기와 비교하면 항상 거짓으로 반환될 수 밖에 없다.
#if 0
    if (fno.fsize != sizeof(snd_boot_status_t))
    {
        return false;
    }
#endif
    return true;
}

static bool snd_boot_read_file(void)
{
    UINT    br;
    FRESULT res;

    res = f_open(sg_fp, SND_BOOT_FILE_NAME, (FA_OPEN_EXISTING | FA_READ | FA_WRITE));

    if (res == FR_OK)
    {
        res = f_read(sg_fp, &sg_boot_status, sizeof(snd_boot_status_t), &br);

        f_close(sg_fp);

        if (res == FR_OK)
        {
            if (br == sizeof(snd_boot_status_t))
            {
                return true;
            }
        }
    }

    tdc_uart_printf("[BOOT] '%s' READ FAILED \r\n", SND_BOOT_FILE_NAME);

    return false;
}

static void snd_boot_initialize_file(void)
{
    sg_boot_status.major_ver          = SND_BOOT_VER_MAJOR;
    sg_boot_status.minor_ver          = SND_BOOT_VER_MINOR;
    sg_boot_status.state              = SND_BOOT_STATE_BOOT;
    sg_boot_status.sub_state          = SND_BOOT_SUB_STATE_IDLE;
    sg_boot_status.boot_slot_num      = 0xFF;
    sg_boot_status.last_boot_slot_num = 0xFF;
    sg_boot_status.alt_boot_slot_num  = 0;
    sg_boot_status.alt_boot_try_count = 0;
    sg_boot_status.alt_boot_result    = SND_BOOT_ALT_BOOT_RESULT_NONE;
    sg_boot_status.slot_1_state       = SND_BOOT_SLOT_STATE_NONE;
    sg_boot_status.slot_2_state       = SND_BOOT_SLOT_STATE_NONE;
    sg_boot_status.slot_3_state       = SND_BOOT_SLOT_STATE_NONE;
    sg_boot_status.slot_4_state       = SND_BOOT_SLOT_STATE_NONE;

    for (int i = 0; i < 47; i++)
    {
        sg_boot_status.reserved[i] = 0;
    }

    sg_boot_status.crc32 = 0;
}

static void snd_boot_update_file(void)
{
    UINT    btw;
    FRESULT res;

    res = f_open(sg_fp, SND_BOOT_FILE_NAME, (FA_OPEN_ALWAYS | FA_READ | FA_WRITE));

    if (res == FR_OK)
    {
        f_lseek(sg_fp, 4096);  // FATFS에게 4KB로 고정된 파일을 생성할 수 있게 의도적으로 파일 포지션을 4096으로 설정
        f_lseek(sg_fp, 0);

        rtt_printf("[FATFS] PERFORMED : FILE (%s) LSEEK --> 4096 --> 0 \r\n", SND_BOOT_FILE_NAME);

        res = f_write(sg_fp, &sg_boot_status, sizeof(snd_boot_status_t), &btw);
        if (res == FR_OK)
        {
            // 안전을 위한 flush
            f_sync(sg_fp);
#if 1
            FILINFO fno;

            res = f_stat(SND_BOOT_FILE_NAME, &fno);
            if (res == FR_OK)
            {
                rtt_printf("[FATFS] NAME : %s, TOTAL SIZE : %u BYTES, START CLUSTER : %u \r\n",  //
                           fno.fname,
                           fno.fsize,
                           sg_fp->obj.sclust);
            }
            else
            {
                rtt_printf("[FATFS] FAILED TO GET FILE STATUS (FR: %d) \r\n", res);
            }
#endif
            if (btw == sizeof(snd_boot_status_t))
            {
                rtt_printf("[FATFS] SUCCESS TO WRITE (BTW: %d) \r\n", btw);
                f_close(sg_fp);

                // 끝나기 전에 파일이 정말 생성되었는지 다시 확인
                res = f_open(sg_fp, SND_BOOT_FILE_NAME, (FA_OPEN_EXISTING | FA_READ | FA_WRITE));
                if (res != FR_OK)
                {
                    rtt_printf("[FATFS] CAN NOT OPEN '%s' FILE (RES: %d) \r\n", SND_BOOT_FILE_NAME, res);
                    _INFINITE_LOOP();
                }

                res = f_stat(SND_BOOT_FILE_NAME, &fno);
                if (res == FR_OK)
                {
                    rtt_printf("[FATFS] CONFIRM, NAME : %s, TOTAL SIZE : %u (BYTE), START CLUSTER : %u \r\n",  //
                               fno.fname,
                               fno.fsize,
                               sg_fp->obj.sclust);
                }
                else
                {
                    rtt_printf("[FATFS] CRITICAL ERROR, CAN NOT GET STATUS '%s' FILE (RES : %d) \r\n", SND_BOOT_FILE_NAME, res);
                    _INFINITE_LOOP();
                }

                f_close(sg_fp);
                return;
            }  // 끝, if btw 체크
            else
            {
                rtt_printf("[FATFS] FAILED TO WRITE (BTW: %d) \r\n", btw);
            }
        }  // 끝, f_write
        else
        {
            rtt_printf("[FATFS] WRITE FAILED '%s' FILE \r\n", SND_BOOT_FILE_NAME);
        }
    }  // 끝, f_open
    else
    {
        rtt_printf("[FATFS] OPEN FAILED '%s' FILE \r\n", SND_BOOT_FILE_NAME);
    }

    tdc_uart_printf("[FATFS] UPDATE FAILED, '%s' FILE \r\n", SND_BOOT_FILE_NAME);
    _INFINITE_LOOP();
}

static void snd_boot_determine_slot_num(void)
{
    switch (sg_boot_status.state)
    {
        case SND_BOOT_STATE_BOOT:
            sg_slot_num = sg_boot_status.boot_slot_num;
            break;

        case SND_BOOT_STATE_ALT_BOOT:
            snd_boot_validate_alt_boot();
            break;

        default:  // 존재할 수 없는 상태
        {
            sg_slot_num              = sg_boot_status.boot_slot_num;
            sg_boot_status.sub_state = SND_BOOT_SUB_STATE_UNKNOWN;
            snd_boot_update_file();
        }
        break;
    }
}

static void snd_boot_validate_alt_boot(void)
{
    switch (sg_boot_status.sub_state)
    {
        case SND_BOOT_SUB_STATE_BOOT_TRY:
        {
            sg_boot_status.alt_boot_try_count++;

            if (sg_boot_status.alt_boot_try_count > SND_BOOT_ALT_BOOT_TRY_MAX)
            {
                sg_slot_num                    = sg_boot_status.boot_slot_num;
                sg_boot_status.sub_state       = SND_BOOT_SUB_STATE_BOOT_TRY_DONE;
                sg_boot_status.alt_boot_result = SND_BOOT_ALT_BOOT_RESULT_FAIL;
                sg_boot_status.state           = SND_BOOT_STATE_BOOT;

                tdc_uart_printf("[ERROR] FAILED TO BOOT ALT SLOT %d, BACK TO BOOT SLOT %d. \r\n",  //
                                sg_boot_status.alt_boot_slot_num,
                                sg_boot_status.boot_slot_num);
            }
            else
            {
                sg_slot_num = sg_boot_status.alt_boot_slot_num;
            }

            snd_boot_update_file();
        }
        break;

        case SND_BOOT_SUB_STATE_BOOT_TRY_DONE:
        {
            if (sg_boot_status.alt_boot_result == SND_BOOT_ALT_BOOT_RESULT_SUCCESS)
            {
                sg_slot_num = sg_boot_status.alt_boot_slot_num;
            }
            else
            {
                sg_slot_num              = sg_boot_status.boot_slot_num;
                sg_boot_status.sub_state = SND_BOOT_SUB_STATE_UNKNOWN;
                snd_boot_update_file();
            }
        }
        break;

        default:
        {
            sg_slot_num              = sg_boot_status.boot_slot_num;
            sg_boot_status.sub_state = SND_BOOT_SUB_STATE_UNKNOWN;
            snd_boot_update_file();
        }
        break;
    }
}

