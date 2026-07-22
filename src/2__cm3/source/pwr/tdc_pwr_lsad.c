/**
 * @file GEN1_5_battery.c
 */

#include <tdc_pwr_battery.h>
#include <tdc_pwr_lsad.h>

static volatile int s_sample_count = 0;

void LSAD_IRQHandler(void)
{
    s_sample_count++;

    if (TDC_PWR_LSAD_STABLE_CNT < s_sample_count)
    {
        // LSAD->CFG = (TDC_PWR_LSAD_INT_CH_NUM | LSAD_INT_DISABLE | TDC_PWR_LSAD_PRESCALE_NUM);
        LSAD->CFG = (LSAD_INT_CH1 | LSAD_INT_DISABLE | TDC_PWR_LSAD_PRESCALE_NUM);
        NVIC_DisableIRQ(LSAD_IRQn);
        NVIC_ClearPendingIRQ(LSAD_IRQn);
    }
}

int tdc_pwr_lsad_get_count(void)
{
    return s_sample_count;
}

void tdc_pwr_lsad_init(void)
{
    int32_t val;
    uint8_t rbuf[4];

    s_sample_count = 0;

    Sys_LSAD_InputConfig(0, LSAD_INPUT_DIO23);
    Sys_LSAD_InputConfig(1, LSAD_INPUT_VSSA);

    // LSAD의 샘플링 시작 후 첫 샘플 결과는 유효하지 않기 때문에 무시해야 한다는 내용이 있다.
    // 그래서 초기화 후 샘플링 카운트가 일정 크기가 되는 시점부터
    // 유효한 샘플링 데이터로 처리하기 위해 인터럽트를 사용한다.

    NVIC_ClearPendingIRQ(LSAD_IRQn);
    NVIC_EnableIRQ(LSAD_IRQn);

    // LSAD->CFG = (TDC_PWR_LSAD_INT_CH_NUM | LSAD_INT_ENABLE | LSAD_PRESCALE_3200);
    LSAD->CFG = (LSAD_INT_CH1 | LSAD_INT_ENABLE | LSAD_PRESCALE_3200);

    if (tdc_fs_read("/BATT_CAL", rbuf, 4) < 0)
    {
        tdc_util_indicate_critical_error();
    }

    val = (rbuf[3] << 24) | (rbuf[2] << 16) | (rbuf[1] << 8) | rbuf[0];

    cfx_cm3_sharedMemoryAll.batteryCalibrationValue = val;

    TDC_PRINTF_I("[LSAD] BATTERY CALIBRATION VALUE : %4d \r\n", cfx_cm3_sharedMemoryAll.batteryCalibrationValue);

    tdc_pwr_battery_calculate_boundary();
}

void tdc_pwr_lsad_uninit(void)
{
    s_sample_count = 0;

    // LSAD->CFG = (LSAD_INT_VDDM | LSAD_INT_DISABLE | LSAD_DISABLE);
    LSAD->CFG = (LSAD_INT_CH1 | LSAD_INT_DISABLE | LSAD_DISABLE);

    NVIC_DisableIRQ(LSAD_IRQn);
    NVIC_ClearPendingIRQ(LSAD_IRQn);
}

void tdc_pwr_lsad_update(void)
{
    if (TDC_PWR_LSAD_STABLE_CNT < s_sample_count)
    {
        // cfx_cm3_sharedMemoryAll.systemShare.batteryLevel_CfX_to_CM3 = LSAD->DATA_TRIM_SAT_CH[TDC_PWR_LSAD_INPUT_SEL_CH_NUM];
        cfx_cm3_sharedMemoryAll.systemShare.batteryLevel_CfX_to_CM3 = (LSAD->DATA_TRIM_SAT_CH[0] - LSAD->DATA_TRIM_SAT_CH[1]);  // DIO23_INPUT - VSSA
    }
}
