
#include <ci_stim_mute.h>

int ci_stim_mute_init(void)
{
    FIL           *fp;
    uint8_t       *fname;
    bool           is_validated_file;
    int            br;  // byte read
    int            bw;  // byte written
    int            ret;
    CI_STIM_MUTE_T stim_mute;

    fp    = &g_ci_filesystem_ohdl;
    fname = CI_STIM_MUTE_FILE_NAME;

    // 파일이 없으면 생성하는 옵션으로 연다.
    ret = f_open(fp, fname, (FA_OPEN_ALWAYS | FA_READ | FA_WRITE));
    if (ret != FR_OK)
    {
        TDC_PRINTF_E("[MUTE] OPEN FAIL '%s' (RES=%d) \r\n", fname, ret);
        return -1;
    }

    SYS_WATCHDOG_REFRESH();

    // 파일은 존재하는데, 유효한 상태인지 검증한다.
    is_validated_file = true;

    f_lseek(&g_ci_filesystem_ohdl, 4096);  // FATFS에게 4KB로 고정된 파일을 생성할 수 있게 의도적으로 파일 포지션을 4096으로 설정
    f_lseek(&g_ci_filesystem_ohdl, 0);
    TDC_PRINTF_V("[MUTE] PERFORMED : FILE (%s) LSEEK --> 4096 --> 0 \r\n", fname);

    br  = 0;
    ret = f_read(fp, (void *) &stim_mute, sizeof(CI_STIM_MUTE_T), &br);

    // 읽기 실패하면 유효한 상태가 아니다.
    if (ret != FR_OK)
    {
        is_validated_file = false;
    }

    // 읽은 크기가 일치 하지 않다면 유효한 상태가 아니다.
    if (br != sizeof(CI_STIM_MUTE_T))
    {
        is_validated_file = false;
    }
    else
    {
        // 이미 파일이 써진 적이 있어서, 읽은 크기는 일치한 경우 IDENT를 검사한다.

        if ((stim_mute.file_ident_begin != CI_STIM_MUTE_FILE_IDNET_BEGIN)  // IDENT가 하나라도 일치 하지 않으면 에러
            || (stim_mute.file_ident_end != CI_STIM_MUTE_FILE_IDENT_END))
        {
            is_validated_file = false;
        }

        // is_enabled_mute_stimulation_under_t_level, mute_stimulation_t_level_offset 값의 유효성 확인
        if ((stim_mute.is_enabled_mute_stimulation_under_t_level != CI_STIM_MUTE_UNDER_T_LEVEL_ENABLE)  //
            && (stim_mute.is_enabled_mute_stimulation_under_t_level != CI_STIM_MUTE_UNDER_T_LEVEL_DISABLE))
        {
            is_validated_file = false;
        }

        // T LEVEL OFFSET MIN 이상이 아니면 유효성 에러
        if (!(CI_STIM_MUTE_T_LEVEL_OFFSET_MIN <= stim_mute.mute_stimulation_t_level_offset))
        {
            is_validated_file = false;
        }
        else
        {
            // T LEVEL OFFSET MAX 이하가 아니면 유효성 에러
            if (!(stim_mute.mute_stimulation_t_level_offset <= CI_STIM_MUTE_T_LEVEL_OFFSET_MAX))
            {
                is_validated_file = false;
            }
        }
    }

    SYS_WATCHDOG_REFRESH();

    // 파일이 유효하지 않은 경우 초기화 시킨다.
    if (!is_validated_file)
    {
        TDC_PRINTF_E("[MUTE] INVALID '%s', SO FULLY RESET \r\n", fname);

        // 구조체 내용 초기화
        // CI_STIM_MUTE_T_LEVEL_OFFSET_DEFAULT 값은 CFX 프로젝트에서 참조
        stim_mute.file_ident_begin                          = CI_STIM_MUTE_FILE_IDNET_BEGIN;
        stim_mute.is_enabled_mute_stimulation_under_t_level = CI_STIM_MUTE_UNDER_T_LEVEL_DISABLE;
        stim_mute.mute_stimulation_t_level_offset           = CI_STIM_MUTE_T_LEVEL_OFFSET_DEFAULT;
        stim_mute.file_ident_end                            = CI_STIM_MUTE_FILE_IDENT_END;

        f_lseek(fp, 0);
        ret = f_write(fp, (void *) &stim_mute, sizeof(CI_STIM_MUTE_T), &bw);

        if ((ret != FR_OK) || (bw != sizeof(CI_STIM_MUTE_T)))
        {
            TDC_PRINTF_E("[MUTE] INIT FAIL '%s' (RES=%d, WRITTEN=%d) \r\n", fname, ret, bw);
            f_close(fp);
            return -1;
        }

        TDC_PRINTF_D("[MUTE] SUCCESS TO FULLY RESET '%s' \r\n", fname);
    }

    // 안전을 위한 flush
    f_sync(&g_ci_filesystem_ohdl);

#if 1
    FILINFO fno;
    ret = f_stat(fname, &fno);
    if (ret == FR_OK)
    {
        TDC_PRINTF_V("[FS] NAME : %s, TOTAL SIZE : %u BYTES, START CLUSTER : %u \r\n",  //
                  fno.fname,
                  fno.fsize,
                  g_ci_filesystem_ohdl.obj.sclust);
    }
#endif

    f_close(fp);
    SYS_WATCHDOG_REFRESH();

    /* 파일이 유효하거나,
     * 유효하지 않아서 초기화가 완료된 후에는 공유 메모리에 값을 저장하여 CFX가 사용하도록 한다. */
    cfx_cm3_sharedMemoryAll.is_enabled_mute_stimulation_under_t_level = stim_mute.is_enabled_mute_stimulation_under_t_level;
    cfx_cm3_sharedMemoryAll.mute_stimulation_t_level_offset           = stim_mute.mute_stimulation_t_level_offset;

    return 0;
}

int ci_stim_mute_update(uint32_t enable, uint32_t level)
{
    FIL           *fp;
    uint8_t       *fname;
    int            bw;  // byte written
    int            ret;
    CI_STIM_MUTE_T stim_mute;

    // 매개변수 유효성 확인
    if ((CI_STIM_MUTE_UNDER_T_LEVEL_ENABLE == enable) || (CI_STIM_MUTE_UNDER_T_LEVEL_DISABLE == enable))
    {
        TDC_PRINTF_D("[MUTE] NEW STIM MUTE FLAG : %u \r\n", enable);
    }
    else
    {
        TDC_PRINTF_E("[MUTE] INVALID STIM MUTE FLAG : %u \r\n", enable);
        return CI_STIM_MUTE_RET_FALSE;
    }

    if ((CI_STIM_MUTE_T_LEVEL_OFFSET_MIN <= level) && (level <= CI_STIM_MUTE_T_LEVEL_OFFSET_MAX))
    {
        TDC_PRINTF_D("[MUTE] NEW T LEVEL OPTION VALUE : %u \r\n", level);
    }
    else
    {
        TDC_PRINTF_E("[MUTE] INVALID T LEVEL OPTION VALUE : %u \r\n", level);
        return CI_STIM_MUTE_RET_FALSE;
    }

    fp    = &g_ci_filesystem_ohdl;
    fname = CI_STIM_MUTE_FILE_NAME;

    // 파일이 없으면 생성하는 옵션으로 연다.
    ret = f_open(fp, fname, (FA_OPEN_APPEND | FA_READ | FA_WRITE));
    if (ret != FR_OK)
    {
        TDC_PRINTF_E("[MUTE] OPEN FAIL '%s' (RES=%d) \r\n", fname, ret);
        return CI_STIM_MUTE_RET_FALSE;
    }

    SYS_WATCHDOG_REFRESH();

    // 구조체 내용 초기화
    stim_mute.file_ident_begin                          = CI_STIM_MUTE_FILE_IDNET_BEGIN;
    stim_mute.is_enabled_mute_stimulation_under_t_level = enable;
    stim_mute.mute_stimulation_t_level_offset           = level;
    stim_mute.file_ident_end                            = CI_STIM_MUTE_FILE_IDENT_END;

    f_lseek(fp, 0);
    ret = f_write(fp, (void *) &stim_mute, sizeof(CI_STIM_MUTE_T), &bw);

    if ((ret != FR_OK) || (bw != sizeof(CI_STIM_MUTE_T)))
    {
        TDC_PRINTF_E("[MUTE] UPDATE FAIL '%s' (RES=%d, WRITTEN=%d) \r\n", fname, ret, bw);
        f_close(fp);
        return CI_STIM_MUTE_RET_FALSE;
    }

    TDC_PRINTF_D("[MUTE] SUCCESS TO UPDATE '%s' \r\n", fname);

    f_close(fp);
    SYS_WATCHDOG_REFRESH();

    /* 업데이트가 완료된 후에는 공유 메모리에 값을 저장하여 CFX가 사용하도록 한다. */
    cfx_cm3_sharedMemoryAll.mute_stimulation_t_level_offset           = stim_mute.mute_stimulation_t_level_offset;
    cfx_cm3_sharedMemoryAll.is_enabled_mute_stimulation_under_t_level = stim_mute.is_enabled_mute_stimulation_under_t_level;

    return CI_STIM_MUTE_RET_TRUE;
}
