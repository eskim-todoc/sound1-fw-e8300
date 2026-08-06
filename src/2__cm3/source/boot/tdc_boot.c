/**
 * @file tdc_boot.c
 */

#include <tdc_boot.h>
#include <tdc_printf.h>

#define _INFINITE_LOOP()                                                                                                                                       \
    while (1)                                                                                                                                                  \
    {                                                                                                                                                          \
        (void) 0;                                                                                                                                              \
    }

static FIL              *_g_fp     = NULL;
static tdc_boot_status_t _g_status = {0};

static void _open_status_file(void)
{
    FRESULT res;

    if (_g_fp == NULL)
    {
        TDC_PRINTF("[%s] [%s] [%d] [BOOT] ERROR \r\n", "tdc_boot.c", __func__, __LINE__);
        _INFINITE_LOOP();
    }

    res = f_open(_g_fp, SND_BOOT_FILE_NAME, (FA_OPEN_EXISTING | FA_READ | FA_WRITE));

    if (res != FR_OK)
    {
        TDC_PRINTF("[%s] [%s()] [%d] [BOOT] ERROR RES=%d \r\n", "tdc_boot.c", __func__, __LINE__, res);
        _INFINITE_LOOP();
    }
}

static void _close_status_file(void)
{
    FRESULT res;

    if (_g_fp == NULL)
    {
        TDC_PRINTF("[%s] [%s] [%d] [BOOT] ERROR \r\n", "tdc_boot.c", __func__, __LINE__);
        _INFINITE_LOOP();
    }

    res = f_close(_g_fp);

    if (res != FR_OK)
    {
        TDC_PRINTF("[%s] [%s] [%d] [BOOT] ERROR \r\n", "tdc_boot.c", __func__, __LINE__);
        _INFINITE_LOOP();
    }
}

static void _load_status(void)
{
    FRESULT result;
    UINT    byte_read = 0;

    if (_g_fp == NULL)
    {
        TDC_PRINTF("[%s] [%s] [%d] [BOOT] ERROR \r\n", "tdc_boot.c", __func__, __LINE__);
        _INFINITE_LOOP();
    }

    result = f_lseek(_g_fp, 0);

    if (result != FR_OK)
    {
        TDC_PRINTF("[%s] [%s] [%d] [BOOT] ERROR \r\n", "tdc_boot.c", __func__, __LINE__);
        _INFINITE_LOOP();
    }

    result = f_read(_g_fp, &_g_status, sizeof(tdc_boot_status_t), &byte_read);

    if ((result != FR_OK) || (byte_read != sizeof(tdc_boot_status_t)))
    {
        TDC_PRINTF("[%s] [%s] [%d] [BOOT] ERROR \r\n", "tdc_boot.c", __func__, __LINE__);
        _INFINITE_LOOP();
    }
}

static void _store_status(void)
{
    FRESULT result;
    UINT    byte_to_write = 0;

    if (_g_fp == NULL)
    {
        TDC_PRINTF("[%s] [%s] [%d] [BOOT] ERROR \r\n", "tdc_boot.c", __func__, __LINE__);
        _INFINITE_LOOP();
    }

    result = f_lseek(_g_fp, 0);

    if (result != FR_OK)
    {
        TDC_PRINTF("[%s] [%s] [%d] [BOOT] ERROR \r\n", "tdc_boot.c", __func__, __LINE__);
        _INFINITE_LOOP();
    }

    result = f_write(_g_fp, &_g_status, sizeof(tdc_boot_status_t), &byte_to_write);

    if ((result != FR_OK) || (byte_to_write != sizeof(tdc_boot_status_t)))
    {
        TDC_PRINTF("[%s] [%s] [%d] [BOOT] ERROR \r\n", "tdc_boot.c", __func__, __LINE__);
        _INFINITE_LOOP();
    }

    result = f_sync(_g_fp);

    if (result != FR_OK)
    {
        TDC_PRINTF("[%s] [%s] [%d] [BOOT] ERROR \r\n", "tdc_boot.c", __func__, __LINE__);
        _INFINITE_LOOP();
    }
}

static void _set_unexpected_sub_state(void)
{
    _g_status.state     = SND_BOOT_STATE_BOOT;
    _g_status.sub_state = SND_BOOT_SUB_STATE_UNKNOWN;
}

static void _handle_state_boot(void)
{
    uint8_t sub_state;
    uint8_t alt_result;

    sub_state  = _g_status.sub_state;
    alt_result = _g_status.alt_boot_result;

    if (sub_state == SND_BOOT_SUB_STATE_IDLE)
    {
        (void) 0;
    }
    else if ((sub_state == SND_BOOT_SUB_STATE_BOOT_TRY_DONE)    //
             && (alt_result == SND_BOOT_ALT_BOOT_RESULT_FAIL))  //
    {
        _g_status.sub_state          = SND_BOOT_SUB_STATE_IDLE;
        _g_status.alt_boot_try_count = 0;
        _g_status.alt_boot_result    = SND_BOOT_ALT_BOOT_RESULT_NONE;

        if (_g_status.alt_boot_slot_num == 1)
        {
            _g_status.slot_1_state = SND_BOOT_SLOT_STATE_NONE;
        }
        else if (_g_status.alt_boot_slot_num == 2)
        {
            _g_status.slot_2_state = SND_BOOT_SLOT_STATE_NONE;
        }
        else if (_g_status.alt_boot_slot_num == 3)
        {
            _g_status.slot_3_state = SND_BOOT_SLOT_STATE_NONE;
        }
        else if (_g_status.alt_boot_slot_num == 4)
        {
            _g_status.slot_4_state = SND_BOOT_SLOT_STATE_NONE;
        }

        _g_status.alt_boot_slot_num = 0;
        _store_status();
    }
    else if (sub_state == SND_BOOT_SUB_STATE_UNKNOWN)
    {
        (void) 0;
    }
    else  // 존재할 수 없는 상태
    {
        _set_unexpected_sub_state();
        _store_status();
    }
}

static void _handle_state_alt_boot(void)
{
    uint8_t sub_state;
    uint8_t alt_result;

    sub_state  = _g_status.sub_state;
    alt_result = _g_status.alt_boot_result;

    if (((sub_state == SND_BOOT_SUB_STATE_BOOT_TRY) && (alt_result == SND_BOOT_ALT_BOOT_RESULT_NONE))  //
        || ((sub_state == SND_BOOT_SUB_STATE_BOOT_TRY_DONE) && (alt_result == SND_BOOT_ALT_BOOT_RESULT_SUCCESS)))
    {
        //_set_boot_alt_try_success();
        //_store_status();
        if (_g_status.alt_boot_slot_num == 1)
        {
            _g_status.slot_1_state = SND_BOOT_SLOT_STATE_USABLE;
        }
        else if (_g_status.alt_boot_slot_num == 2)
        {
            _g_status.slot_2_state = SND_BOOT_SLOT_STATE_USABLE;
        }
        else if (_g_status.alt_boot_slot_num == 3)
        {
            _g_status.slot_3_state = SND_BOOT_SLOT_STATE_USABLE;
        }
        else if (_g_status.alt_boot_slot_num == 4)
        {
            _g_status.slot_4_state = SND_BOOT_SLOT_STATE_USABLE;
        }

        _g_status.state              = SND_BOOT_STATE_BOOT;
        _g_status.sub_state          = SND_BOOT_SUB_STATE_IDLE;
        _g_status.last_boot_slot_num = _g_status.boot_slot_num;
        _g_status.boot_slot_num      = _g_status.alt_boot_slot_num;
        _g_status.alt_boot_slot_num  = 0;
        _g_status.alt_boot_try_count = 0;
        _g_status.alt_boot_result    = SND_BOOT_ALT_BOOT_RESULT_NONE;

        _store_status();
    }
    else
    {
        _set_unexpected_sub_state();
        _store_status();
    }
}

void tdc_boot_init_fp(FIL *fp)
{
    if (fp == NULL)
    {
        TDC_PRINTF("[%s] [%s] [%d] [BOOT] ERROR \r\n", "tdc_boot.c", __func__, __LINE__);
        _INFINITE_LOOP();
    }

    _g_fp = fp;
}

EN__BOOT_RET tdc_boot_get_status(tdc_boot_status_t *p_status)
{
    if ((_g_fp == NULL) || (p_status == NULL))
    {
        TDC_PRINTF("[%s] [%s] [%d] [BOOT] ERROR \r\n", "tdc_boot.c", __func__, __LINE__);
        return BOOT_RET_FAIL;
    }

    tdc_util_assert(tdc_fs_fatfs_remount(SND_FATFS_LDRV_NUM_BOOT));

    _open_status_file();
    _load_status();
    _close_status_file();

    tdc_util_assert(tdc_fs_fatfs_remount(SND_FATFS_LDRV_NUM_USER_DATA));

    *p_status = _g_status;

    return BOOT_RET_TRUE;
}

EN__BOOT_RET tdc_boot_update_status(tdc_boot_status_t *p_status)
{
    if ((_g_fp == NULL) || (p_status == NULL))
    {
        TDC_PRINTF("[%s] [%s] [%d] [BOOT] ERROR \r\n", "tdc_boot.c", __func__, __LINE__);
        return BOOT_RET_FAIL;
    }

    _g_status = *p_status;

    tdc_util_assert(tdc_fs_fatfs_remount(SND_FATFS_LDRV_NUM_BOOT));

    _open_status_file();
    _store_status();
    _close_status_file();

    tdc_util_assert(tdc_fs_fatfs_remount(SND_FATFS_LDRV_NUM_USER_DATA));

    return BOOT_RET_TRUE;
}

void tdc_boot_handle_fsm(void)
{
    uint8_t state;

    _open_status_file();
    _load_status();

    state = _g_status.state;

    switch (state)
    {
        // 상태가 BOOT인 경우
        // 보조 상태가 IDLE이면 (DFU 관련 진행 상태가 아닌) 통상적인 부팅 상태를 의미
        // 단, 보조 상태가 UNKNOWN인 경우는 무시
        // 보조 상태가 BOOT TRY DONE인 경우, 알트 부트 결과가 FAIL이면 DFU 실패이다.
        // DFU 실패인 경우 보조 상태를 IDLE로 초기화 한다. 또한, 부팅 결과도 NONE 상태로 초기화 한다.
        // 해당하는 부팅 시도 알트 번호도 NONE으로 초기화 한다.
        // 결과적으로 마지막에 사용되던 정상 부팅 이미지가 다시 사용되는 상태로 복원한다.
        // DFU 성공, 실패 여부는 리모트 앱에서 현재 동작 중인 부팅 슬롯 번호를 확인하여,
        // 부팅 시도를 했던 번호와 일치하는지 여부를 통해 DFU 성공 여부를 판단한다.
        case SND_BOOT_STATE_BOOT:
        {
            _handle_state_boot();
        }
        break;

        // 상태가 ALT BOOT인 경우
        // 보조 상태가 BOOT TRY이며 부팅 결과가 NONE 상태이면, DFU가 성공 후 처음으로 부팅된 상태를 의미한다.
        // 보조 상태가 BOOT TRY DONE이며 부팅 결과가 SUCCESS 상태이면,
        // DFU가 성공 후, 상태를 업데이트 하기 전 어떠한 이유로 시스템이 재부팅된 상태이다.
        // 위 두 결과 모두 DFU가 성공한 상태를 의미하므로,
        // 상태를 BOOT로 업데이트, 라스트 부트 슬롯은 부트 슬롯 번호로, 부트 슬롯은 알트 부트 슬롯 번호로 업데이트한다.
        // 해당하는 알트 부트 슬롯에 대해서 USABLE로 상태를 표시하고, 부트 결과는 NONE으로 변경한다.
        case SND_BOOT_STATE_ALT_BOOT:
        {
            _handle_state_alt_boot();
        }
        break;

        default:
        {
            _set_unexpected_sub_state();
            _store_status();
        }
        break;
    }

    _close_status_file();
}
