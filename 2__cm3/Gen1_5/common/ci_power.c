/**
 * @file OTE_1P5_power_manager.c
 */

#include <ci_power.h>

#define NVM_BOOT_INFO_OFFSET        BOOTSTRAP_NVM_BOOTINFORMATION_OFFSET
#define NVM_BOOT_INFO_OFFSET_OCTETS BOOTSTRAP_NVM_BOOTINFORMATION_OFFSET_OCTETS

#define NVM_BOOT_INFO_SIZE        BOOTSTRAP_NVM_BOOTINFORMATION_SIZE
#define NVM_BOOT_INFO_SIZE_OCTETS BOOTSTRAP_NVM_BOOTINFORMATION_SIZE_OCTETS

#define NVM_MANUF_TABLE_SIZE        MANU_TABLE_SIZE
#define NVM_MANUF_TABLE_SIZE_OCTETS MANU_TABLE_SIZE_OCTETS

static uint32_t s_boot_info[NVM_BOOT_INFO_SIZE];
static uint32_t s_manu_table[MANU_TABLE_SIZE];

static void delay_ms(uint32_t ms)
{
    Sys_Delay((SystemCoreClock / 1000) * ms);
}

int ci_power_normal(void)
{
    int ret;
    int br;

    ret = f_open(&g_snd_fatfs_ohdl, "/MANUF_TABLE", FA_OPEN_EXISTING | FA_READ);

    if (ret != FR_OK)
    {
        ci_printe("[POWER] FAILED TO OPEN '%s' (RES=%d) \r\n", "/MANUF_TABLE", ret);
        return df_False;
    }

    f_lseek(&g_snd_fatfs_ohdl, 0);

    ret = f_read(&g_snd_fatfs_ohdl, s_manu_table, MANU_TABLE_SIZE_OCTETS, &br);

    if (ret != FR_OK)
    {
        ci_printe("[POWER] FAIELD TO READ '%s' \r\n", "/MANUF_TABLE");
        return df_False;
    }

    f_close(&g_snd_fatfs_ohdl);

    if (MANU_TABLE_SIZE_OCTETS != br)
    {
        ci_printe("[POWER] INVALID READ SIZE (READ=%d, GOT=%d) \r\n", MANU_TABLE_SIZE_OCTETS, br);
        return df_False;
    }

    Sys_Trims_LoadManuTable(s_manu_table);

    if (Sys_Trims_SetVREGAndLSAD() != SYS_ERRNO_NO_ERROR)
    {
        ci_printe("[POWER] NO CALIBRATE FOR VREG AND LSAD \r\n");
        return df_False;
    }

    if (Sys_Trims_SetVDDIF((VDDIF_ACTIVE_TARGET / 10)) != SYS_ERRNO_NO_ERROR)
    {
        ci_printe("[POWER] NO CALIBRATE FOR VDDIF \r\n");
        return df_False;
    }

    if (Sys_Trims_SetVDDA((VDDA_ACTIVE_TARGET / 10)) != SYS_ERRNO_NO_ERROR)
    {
        ci_printe("[POWER] NO CALIBRATE FOR VDDA \r\n");
        return df_False;
    }

    if (Sys_Trims_SetVDDC((VDDC_ACTIVE_TARGET / 10)) != SYS_ERRNO_NO_ERROR)
    {
        ci_printe("[POWER] NO CALIBRATE FOR VDDC \r\n");
        return df_False;
    }

    if (Sys_Trims_SetVDDC_CP(VDDCM_MIN_CP_DELTA_TARGET) != SYS_ERRNO_NO_ERROR)
    {
        ci_printe("[POWER] NO CALIBRATE FOR VDDC_CP \r\n");
        return df_False;
    }

    if (Sys_Trims_SetVDDM((VDDM_ACTIVE_TARGET / 10)) != SYS_ERRNO_NO_ERROR)
    {
        ci_printe("[POWER] NO CALIBRATE FOR VDDM \r\n");
        return df_False;
    }

    if (Sys_Trims_SetVDDM_CP(VDDCM_MIN_CP_DELTA_TARGET) != SYS_ERRNO_NO_ERROR)
    {
        ci_printe("[POWER] NO CALIBRATE FOR VDDM_CP \r\n");
        return df_False;
    }

    if (Sys_Trims_SetVDDOD((VDDOD_ACTIVE_TARGET / 10)) != SYS_ERRNO_NO_ERROR)
    {
        ci_printe("[POWER] NO CALIBRATE FOR VDDOD \r\n");
        return df_False;
    }

    if (Sys_Trims_SetVMIC((VMIC_ACTIVE_TARGET / 10)) != SYS_ERRNO_NO_ERROR)
    {
        ci_printe("[POWER] NO CALIBRATE FOR VMIC \r\n");
        return df_False;
    }

    if (Sys_Trims_SetADCOffsets() != SYS_ERRNO_NO_ERROR)
    {
        ci_printe("[POWER] NO CALIBRATE FOR ADC OFFSETS \r\n");
        return df_False;
    }

    if (Sys_Trims_SetOperatingFrequency(SYS_FREQ_30M72) != SYS_ERRNO_NO_ERROR)
    {
        ci_printe("[POWER] NO CALIBRATE FOR SYS_FREQ_30M72 \r\n");
        return df_False;
    }

    // SLOWCLK은 반드시 1.28 MHz로 설정
    D_CLK->CFG_1 = (ADCCLK_PRESCALE_8 | ADCCLK_SRC_SYSCLK | SDMCLK_PRESCALE_2 | SLOWCLK_PRESCALE_24 | SLOWCLK_SRC_SYSCLK | UARTCLK_SRC_SYSCLK);
    D_CLK->CFG_2 = (UCLK_PRESCALE_8 | UCLK_SRC_ADCCLK);

    delay_ms(5);  // 시스템 클럭 안정화 대기

    return df_True;
}

int ci_power_sleep(void)
{
#if 1
    LSAD->CFG = LSAD_DISABLE;

    Sys_Trims_SetOperatingFrequency(SYS_FREQ_2M56);

    D_CLK->CFG_1 = (ADCCLK_PRESCALE_32 | ADCCLK_SRC_SYSCLK | SDMCLK_PRESCALE_64 | SLOWCLK_PRESCALE_2 | SLOWCLK_SRC_SYSCLK | UARTCLK_SRC_SYSCLK);
    D_CLK->CFG_2 = (UCLK_PRESCALE_4096 | UCLK_SRC_ADCCLK);

    delay_ms(1);  // 시스템 클럭 안정화 대기

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
