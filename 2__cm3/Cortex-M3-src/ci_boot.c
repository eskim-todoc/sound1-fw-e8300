/**
 * @file ci_boot.c
 */

#include <ci_boot.h>
#include <ci_printf.h>

#define _INFINITE_LOOP()                                                                                                                                       \
    while (1)                                                                                                                                                  \
    {                                                                                                                                                          \
        (void) 0;                                                                                                                                              \
    }

static FIL*                   _g_fp     = NULL;
static ST__CI_LIB_BOOT_STATUS _g_status = {0};

static void _open_status_file(void)
{
    FRESULT res;

    if (_g_fp == NULL)
    {
        ci_printf("[%s] [%s] [%d] [BOOT] ERROR \r\n", "ci_boot.c", __func__, __LINE__);
        _INFINITE_LOOP();
    }

    res = f_open(_g_fp, SDK_CI_BOOT_FILE_PATH, (FA_OPEN_EXISTING | FA_READ | FA_WRITE));

    if (res != FR_OK)
    {
        ci_printf("[%s] [%s()] [%d] [BOOT] ERROR RES=%d \r\n", "ci_boot.c", __func__, __LINE__, res);
        _INFINITE_LOOP();
    }
}

static void _close_status_file(void)
{
    FRESULT res;

    if (_g_fp == NULL)
    {
        ci_printf("[%s] [%s] [%d] [BOOT] ERROR \r\n", "ci_boot.c", __func__, __LINE__);
        _INFINITE_LOOP();
    }

    res = f_close(_g_fp);

    if (res != FR_OK)
    {
        ci_printf("[%s] [%s] [%d] [BOOT] ERROR \r\n", "ci_boot.c", __func__, __LINE__);
        _INFINITE_LOOP();
    }
}

static void _load_status(void)
{
    FRESULT result;
    UINT    byte_read = 0;

    if (_g_fp == NULL)
    {
        ci_printf("[%s] [%s] [%d] [BOOT] ERROR \r\n", "ci_boot.c", __func__, __LINE__);
        _INFINITE_LOOP();
    }

    result = f_lseek(_g_fp, 0);

    if (result != FR_OK)
    {
        ci_printf("[%s] [%s] [%d] [BOOT] ERROR \r\n", "ci_boot.c", __func__, __LINE__);
        _INFINITE_LOOP();
    }

    result = f_read(_g_fp, &_g_status, sizeof(ST__CI_LIB_BOOT_STATUS), &byte_read);

    if ((result != FR_OK) || (byte_read != sizeof(ST__CI_LIB_BOOT_STATUS)))
    {
        ci_printf("[%s] [%s] [%d] [BOOT] ERROR \r\n", "ci_boot.c", __func__, __LINE__);
        _INFINITE_LOOP();
    }
}

static void _store_status(void)
{
    FRESULT result;
    UINT    byte_to_write = 0;

    if (_g_fp == NULL)
    {
        ci_printf("[%s] [%s] [%d] [BOOT] ERROR \r\n", "ci_boot.c", __func__, __LINE__);
        _INFINITE_LOOP();
    }

    result = f_lseek(_g_fp, 0);

    if (result != FR_OK)
    {
        ci_printf("[%s] [%s] [%d] [BOOT] ERROR \r\n", "ci_boot.c", __func__, __LINE__);
        _INFINITE_LOOP();
    }

    result = f_write(_g_fp, &_g_status, sizeof(ST__CI_LIB_BOOT_STATUS), &byte_to_write);

    if ((result != FR_OK) || (byte_to_write != sizeof(ST__CI_LIB_BOOT_STATUS)))
    {
        ci_printf("[%s] [%s] [%d] [BOOT] ERROR \r\n", "ci_boot.c", __func__, __LINE__);
        _INFINITE_LOOP();
    }

    result = f_sync(_g_fp);

    if (result != FR_OK)
    {
        ci_printf("[%s] [%s] [%d] [BOOT] ERROR \r\n", "ci_boot.c", __func__, __LINE__);
        _INFINITE_LOOP();
    }
}

static void _set_unexpected_sub_state(void)
{
    _g_status.state     = SDK_CI_BOOT_STATE_BOOT;
    _g_status.sub_state = SDK_CI_BOOT_SUB_STATE_UNKNOWN;
}

static void _set_boot_alt_try_success(void)
{
    _g_status.sub_state       = SDK_CI_BOOT_SUB_STATE_BOOT_TRY_DONE;
    _g_status.alt_boot_result = SDK_CI_BOOT_ALT_BOOT_RESULT_SUCCESS;
}

static void _handle_state_boot(void)
{
    uint8_t sub_state;
    uint8_t alt_result;

    sub_state  = _g_status.sub_state;
    alt_result = _g_status.alt_boot_result;

    if (sub_state == SDK_CI_BOOT_SUB_STATE_IDLE)
    {
        (void) 0;
    }
    else if ((sub_state == SDK_CI_BOOT_SUB_STATE_BOOT_TRY_DONE)    //
             && (alt_result == SDK_CI_BOOT_ALT_BOOT_RESULT_FAIL))  //
    {
        _g_status.sub_state          = SDK_CI_BOOT_SUB_STATE_IDLE;
        _g_status.alt_boot_try_count = 0;
        _g_status.alt_boot_result    = SDK_CI_BOOT_ALT_BOOT_RESULT_NONE;

        if (_g_status.alt_boot_slot_num == 1)
        {
            _g_status.slot_1_state = SDK_CI_BOOT_SLOT_STATE_NONE;
        }
        else if (_g_status.alt_boot_slot_num == 2)
        {
            _g_status.slot_2_state = SDK_CI_BOOT_SLOT_STATE_NONE;
        }
        else if (_g_status.alt_boot_slot_num == 3)
        {
            _g_status.slot_3_state = SDK_CI_BOOT_SLOT_STATE_NONE;
        }
        else if (_g_status.alt_boot_slot_num == 4)
        {
            _g_status.slot_4_state = SDK_CI_BOOT_SLOT_STATE_NONE;
        }

        _g_status.alt_boot_slot_num = 0;
        _store_status();
    }
    else if (sub_state == SDK_CI_BOOT_SUB_STATE_UNKNOWN)
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
    uint8_t temp_slot;

    sub_state  = _g_status.sub_state;
    alt_result = _g_status.alt_boot_result;

    if (((sub_state == SDK_CI_BOOT_SUB_STATE_BOOT_TRY) && (alt_result == SDK_CI_BOOT_ALT_BOOT_RESULT_NONE))
        || ((sub_state == SDK_CI_BOOT_SUB_STATE_BOOT_TRY_DONE) && (alt_result == SDK_CI_BOOT_ALT_BOOT_RESULT_SUCCESS)))
    {
        //_set_boot_alt_try_success();
        //_store_status();
        if (_g_status.alt_boot_slot_num == 1)
        {
            _g_status.slot_1_state = SDK_CI_BOOT_SLOT_STATE_USABLE;
        }
        else if (_g_status.alt_boot_slot_num == 2)
        {
            _g_status.slot_2_state = SDK_CI_BOOT_SLOT_STATE_USABLE;
        }
        else if (_g_status.alt_boot_slot_num == 3)
        {
            _g_status.slot_3_state = SDK_CI_BOOT_SLOT_STATE_USABLE;
        }
        else if (_g_status.alt_boot_slot_num == 4)
        {
            _g_status.slot_4_state = SDK_CI_BOOT_SLOT_STATE_USABLE;
        }

        _g_status.state              = SDK_CI_BOOT_STATE_BOOT;
        _g_status.sub_state          = SDK_CI_BOOT_SUB_STATE_IDLE;
        _g_status.last_boot_slot_num = _g_status.boot_slot_num;
        _g_status.boot_slot_num      = _g_status.alt_boot_slot_num;
        _g_status.alt_boot_slot_num  = 0;
        _g_status.alt_boot_try_count = 0;
        _g_status.alt_boot_result    = SDK_CI_BOOT_ALT_BOOT_RESULT_NONE;

        _store_status();
    }
    else
    {
        _set_unexpected_sub_state();
        _store_status();
    }
}

void ci_boot_init_fp(FIL* fp)
{
    if (fp == NULL)
    {
        ci_printf("[%s] [%s] [%d] [BOOT] ERROR \r\n", "ci_boot.c", __func__, __LINE__);
        _INFINITE_LOOP();
    }

    _g_fp = fp;
}

EN__BOOT_RET ci_boot_get_status(ST__CI_LIB_BOOT_STATUS* p_status)
{
    if ((_g_fp == NULL) || (p_status == NULL))
    {
        ci_printf("[%s] [%s] [%d] [BOOT] ERROR \r\n", "ci_boot.c", __func__, __LINE__);
        return BOOT_RET_FAIL;
    }

    _open_status_file();
    _load_status();
    _close_status_file();

    *p_status = _g_status;

    return BOOT_RET_TRUE;
}

EN__BOOT_RET ci_boot_update_status(ST__CI_LIB_BOOT_STATUS* p_status)
{
    if ((_g_fp == NULL) || (p_status == NULL))
    {
        ci_printf("[%s] [%s] [%d] [BOOT] ERROR \r\n", "ci_boot.c", __func__, __LINE__);
        return BOOT_RET_FAIL;
    }

    _g_status = *p_status;

    _open_status_file();
    _store_status();
    _close_status_file();

    return BOOT_RET_TRUE;
}

void ci_boot_handle_fsm(void)
{
    uint8_t state;

    _open_status_file();
    _load_status();

    state = _g_status.state;

    switch (state)
    {
        case SDK_CI_BOOT_STATE_BOOT:
        {
            _handle_state_boot();
        }
        break;

        case SDK_CI_BOOT_STATE_ALT_BOOT:
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
