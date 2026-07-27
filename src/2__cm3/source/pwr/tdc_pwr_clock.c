/**
 * @file OTE_1P5_power_manager.c
 */

#include <tdc_pwr_clock.h>

/* 부팅정보(boot info) 관련 정의는 2차 리팩토링에서 전부 제거했다.
 *   - bootloader_CRC_calc()  : NVM 부팅정보 영역의 CRC-CCITT 를 계산하던 함수. 호출처 0.
 *   - s_boot_info[]          : 그 함수에 넘길 부팅정보 버퍼. 읽기·쓰기 0.
 *   - NVM_BOOT_INFO_SIZE 등  : 위 버퍼 크기·오프셋 매크로.
 *   - NVM_MANUF_TABLE_SIZE 등: 제조정보 크기 매크로(미참조. 실제 코드는 MANU_TABLE_SIZE 직접 사용).
 * CM3 는 부팅정보를 직접 읽지 않는다. 부팅정보 처리는 부트로더(0__bootloader) 몫이며
 * 구조체 원본도 bootloader_internal.h 에 있다. 필요해지면 git 이력에서 복원한다. */

static uint32_t s_manu_table[MANU_TABLE_SIZE];

// IMPORTANT: 절대 이 함수의 내용을 함부로 수정하지 마십시오.
// 타이밍 이슈가 커서 코드 수정 시 동작하지 않을 수 있습니다.
int tdc_pwr_clock_normal(void)
{
    FRESULT fr;
    int     br;

    memset(s_manu_table, 0, MANU_TABLE_SIZE_OCTETS);  // 버퍼 초기화

    fr = f_open(&g_tdc_fs_ohdl, TDC_PWR_MANUF_TABLE_FILE, (FA_OPEN_EXISTING | FA_READ));
    if (fr != FR_OK)
    {
        TDC_PRINTF_E("[POWER] FAIL : OPEN '%s' (FR : %d) \r\n", TDC_PWR_MANUF_TABLE_FILE, fr);
        return df_False;
    }

    f_lseek(&g_tdc_fs_ohdl, 0);

    // Manufacturing Information (Manufacturing area)의 크기는 256 바이트
    fr = f_read(&g_tdc_fs_ohdl, s_manu_table, MANU_TABLE_SIZE_OCTETS, &br);
    if (fr != FR_OK)
    {
        TDC_PRINTF_E("[POWER] FAIL : READ '%s' (FR : %d) \r\n", TDC_PWR_MANUF_TABLE_FILE, fr);
        return df_False;
    }

    fr = f_close(&g_tdc_fs_ohdl);
    if (fr != FR_OK)
    {
        TDC_PRINTF_E("[POWER] FAIL : CLESE '%s' (FR : %d) \r\n", TDC_PWR_MANUF_TABLE_FILE, fr);
        return df_False;
    }

    if (MANU_TABLE_SIZE_OCTETS != br)
    {
        TDC_PRINTF_E("[POWER] INVALID READ SIZE (%d OF %d) \r\n", br, MANU_TABLE_SIZE_OCTETS);
        return df_False;
    }

    if (!NVMSync())
    {
        TDC_PRINTF_E("[POWER] TIMEOUT : NVM SYNC \r\n");
        return df_False;
    }

    SYS_WATCHDOG_REFRESH();

    // 여기부터 실제 전원과 클럭 설정하는 부분
    tdc_hal_trims_load_manu_table(s_manu_table);

    if (tdc_hal_trims_set_vreg_and_lsad() != SYS_ERRNO_NO_ERROR)
    {
        TDC_PRINTF_E("[POWER] NO CALIBRATE FOR VREG AND LSAD \r\n");
        return df_False;
    }

    if (tdc_hal_trims_set_vddif((VDDIF_ACTIVE_TARGET / 10)) != SYS_ERRNO_NO_ERROR)
    {
        TDC_PRINTF_E("[POWER] NO CALIBRATE FOR VDDIF \r\n");
        return df_False;
    }

    if (tdc_hal_trims_set_vdda((VDDA_ACTIVE_TARGET / 10)) != SYS_ERRNO_NO_ERROR)
    {
        TDC_PRINTF_E("[POWER] NO CALIBRATE FOR VDDA \r\n");
        return df_False;
    }

    if (tdc_hal_trims_set_vddc((VDDC_ACTIVE_TARGET / 10)) != SYS_ERRNO_NO_ERROR)
    {
        TDC_PRINTF_E("[POWER] NO CALIBRATE FOR VDDC \r\n");
        return df_False;
    }

    if (tdc_hal_trims_set_vddc_cp(VDDCM_MIN_CP_DELTA_TARGET) != SYS_ERRNO_NO_ERROR)
    {
        TDC_PRINTF_E("[POWER] NO CALIBRATE FOR VDDC_CP \r\n");
        return df_False;
    }

    if (tdc_hal_trims_set_vddm((VDDM_ACTIVE_TARGET / 10)) != SYS_ERRNO_NO_ERROR)
    {
        TDC_PRINTF_E("[POWER] NO CALIBRATE FOR VDDM \r\n");
        return df_False;
    }

    if (tdc_hal_trims_set_vddm_cp(VDDCM_MIN_CP_DELTA_TARGET) != SYS_ERRNO_NO_ERROR)
    {
        TDC_PRINTF_E("[POWER] NO CALIBRATE FOR VDDM_CP \r\n");
        return df_False;
    }

    if (tdc_hal_trims_set_vddod((VDDOD_ACTIVE_TARGET / 10)) != SYS_ERRNO_NO_ERROR)
    {
        TDC_PRINTF_E("[POWER] NO CALIBRATE FOR VDDOD \r\n");
        return df_False;
    }

    if (tdc_hal_trims_set_vmic((VMIC_ACTIVE_TARGET / 10)) != SYS_ERRNO_NO_ERROR)
    {
        TDC_PRINTF_E("[POWER] NO CALIBRATE FOR VMIC \r\n");
        return df_False;
    }

    if (tdc_hal_trims_set_adc_offsets() != SYS_ERRNO_NO_ERROR)
    {
        TDC_PRINTF_E("[POWER] NO CALIBRATE FOR ADC OFFSETS \r\n");
        return df_False;
    }

    if (tdc_hal_trims_set_operating_frequency(SYS_FREQ_30M72) != SYS_ERRNO_NO_ERROR)
    {
        TDC_PRINTF_E("[POWER] NO CALIBRATE FOR SYS_FREQ_30M72 \r\n");
        return df_False;
    }

    // SLOWCLK은 반드시 1.28 MHz로 설정
    D_CLK->CFG_1 = (ADCCLK_PRESCALE_8 | ADCCLK_SRC_SYSCLK | SDMCLK_PRESCALE_2 | SLOWCLK_PRESCALE_24 | SLOWCLK_SRC_SYSCLK | UARTCLK_SRC_SYSCLK);
    D_CLK->CFG_2 = (UCLK_PRESCALE_8 | UCLK_SRC_ADCCLK);

    tdc_util_delay_ms(5);  // 시스템 클럭 안정화 대기

    return df_True;
}

/* ci_fake_power_sleep() 제거(2026-07-20): SYSCLK 를 30.72MHz 로 유지해 터치센서
 * 계측을 가능케 하던 '가짜 절전'용. fake_func_sleep() 이 유일한 호출자였다.
 * 상세: docs/tasks/main/20260720_fake-sleep-removal/ */

int tdc_pwr_clock_sleep(void)
{
#if 1
    LSAD->CFG = LSAD_DISABLE;

    tdc_hal_trims_set_operating_frequency(SYS_FREQ_2M56);

    D_CLK->CFG_1 = (ADCCLK_PRESCALE_32 | ADCCLK_SRC_SYSCLK | SDMCLK_PRESCALE_64 | SLOWCLK_PRESCALE_2 | SLOWCLK_SRC_SYSCLK | UARTCLK_SRC_SYSCLK);
    D_CLK->CFG_2 = (UCLK_PRESCALE_4096 | UCLK_SRC_ADCCLK);

    tdc_util_delay_ms(1);  // 시스템 클럭 안정화 대기

#else

    /* Disable the LSAD by writing LSAD_DISABLE to the FREQ bitfield of LSAD_CFG. */
    LSAD->CFG = LSAD_DISABLE;

    /* Automatically connect VDDIF to VDDA when VDDIF is disabled manually or when the entire analog part is disabled (standby mode). */
    ANALOG->CP_VDDIF_CTRL |= VDDIF_VDDA_AUTO_CONNECT;

    /* Configure the VDDA charge pump in low-power mode and apply desired voltage trimming for standby mode. */
    ANALOG->CP_VDDA_CTRL = (VDDA_HIGH_POWER_DISABLE | VDDA_RUN | VDDA_ITRIM_LP_0P5MA | VDDA_VTRIM_1P8V);

    /* If adaptive voltage scaling is used, then disable it and restore the high VDDC trimming value */
    if ((ANALOG->CP_VDDC_AUTO_CTRL & VDDC_TRIM_AUTO) == VDDC_TRIM_AUTO)
    {
        ANALOG->CP_VDDC_AUTO_CTRL &= ~VDDC_TRIM_AUTO;
        ANALOG->CP_VDDC_TRIM = (ANALOG->CP_VDDC_AUTO_LIMITS >> ANALOG_CP_VDDC_AUTO_LIMITS_MAX_TRIM_Pos) & ANALOG_CP_VDDM_TRIM_LDO_TRIM_Mask;
    }

    /* Disable the VDDC charge pump. */
    if ((ANALOG->CP_VDDC_CTRL & VDDC_ENABLE) == VDDC_ENABLE)
    {
        ANALOG->CP_VDDC_CTRL &= ~VDDC_ENABLE;
    }

    /* Disable the VDDM charge pump. */
    if ((ANALOG->CP_VDDM_CTRL & VDDM_ENABLE) == VDDM_ENABLE)
    {
        ANALOG->CP_VDDM_CTRL &= ~VDDM_ENABLE;
    }

    /* Temporarily change the VDDM voltage trimming to that for standby mode in case the current trimming is lower. */
    if ((ANALOG->CP_VDDM_TRIM & ANALOG_CP_VDDM_TRIM_LDO_TRIM_Mask) < VDDM_LDO_0P80V)
    {
        ANALOG->CP_VDDM_TRIM = (ANALOG->CP_VDDM_TRIM & ~ANALOG_CP_VDDM_TRIM_LDO_TRIM_Mask) | VDDM_LDO_0P80V;
    }

    /* Prepare for standby mode by disabling the analog blocks. */
    ANALOG->PWR_CTRL = ANALOG_DISABLE;

    /* Disable the internal oscillator multiplier. */
    ANALOG->OSC_CTRL_1 = OSC_MULTIPLY_BY_1;

    /* Change clock prescale and source. */
    D_CLK->CFG_1 = (UARTCLK_SRC_SYSCLK | SLOWCLK_SRC_SYSCLK | SLOWCLK_PRESCALE_1 | SDMCLK_PRESCALE_1 | ADCCLK_SRC_SYSCLK | ADCCLK_PRESCALE_1);

    /* Change system clock source to standby clock (this will activate the standby mode and power down the bandgap, VREG regulator and VDDIF charge pump). */
    D_CLK->CFG_0 = ((D_CLK->CFG_0 & ~D_CLK_CFG_0_SYSCLK_SEL_Mask) | SYSCLK_SEL_STANDBY);

#if 1
    /* Change the VDDA voltage trimming to that for standby mode. */
    // Sys_Trims_SetVDDA((VDDA_ACTIVE_TARGET / 10));

    /* Change the VDDC voltage trimming to that for standby mode. */
    Sys_Trims_SetVDDC_PMURef((VDDC_RETENTION_TARGET / 10));

    /* Change the VDDM voltage trimming to that for standby mode. */
    Sys_Trims_SetVDDM_PMURef((VDDM_RETENTION_TARGET / 10));
#else
    /* Change the VDDC voltage trimming to that for standby mode. */
    ANALOG->CP_VDDC_TRIM = (VDDC_CP_DELTA_0MV | VDDC_LDO_0P45V);

    /* Change the VDDM voltage trimming to that for standby mode. kes0481@to-doc.com */
    ANALOG->CP_VDDM_TRIM = (VDDM_CP_DELTA_20MV | VDDM_LDO_0P45V);
#endif
#endif
    return 0;
}
