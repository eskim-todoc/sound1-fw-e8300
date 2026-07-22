/**
 * @file OTE_1_5gen_DIO.c
 */

#include <tdc_hal_dio.h>

static volatile int s_int_flag_acc_sensor    = 0;
static volatile int s_int_flag_case_lid_open = 0;

void tdc_hal_dio_set_int_flag_acc_sensor(void)
{
    s_int_flag_acc_sensor = 1;
}

void tdc_hal_dio_set_int_flag_case_lid_open(void)
{
    s_int_flag_case_lid_open = 1;
}

int tdc_hal_dio_configure_normal(void)
{
    // LED
    Sys_DIO_Config(DIO_PIN_INDEX_for_LED_color_R, DIO_PIN_CFG_FOR_GPIO_OUPUT_NOPULLUP);  // R
    Sys_DIO_Config(DIO_PIN_INDEX_for_LED_color_G, DIO_PIN_CFG_FOR_GPIO_OUPUT_NOPULLUP);  // G
    Sys_DIO_Config(DIO_PIN_INDEX_for_LED_color_B, DIO_PIN_CFG_FOR_GPIO_OUPUT_NOPULLUP);  // B

    // LED 초기값은 전부 OFF로 설정
#if defined(LED_IS_ACTIVELOW)
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);  // R
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);  // G
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);  // B
#else
#if 1  // 기존 방식
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);  // R
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);  // G
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);  // B
#else  // 부팅 시간이 길어서 임의로 LED를 켜놓게 해본다.
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);   // R
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);  // G
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);  // B
#endif
#endif

    // FPGA 활성화 핀
    Sys_DIO_Config(DIO_PIN_INDEX_for_FPGA_SLEEP, DIO_PIN_CFG_FOR_GPIO_OUPUT_NOPULLUP);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_FPGA_SLEEP);

#if 0
    Sys_DIO_Config(DIO_PIN_INDEX_for_FPGA_3P3V_ON, DIO_PIN_CFG_FOR_GPIO_OUPUT_NOPULLUP);
    Sys_DIO_Config(DIO_PIN_INDEX_for_FPGA_1P2V_ON, DIO_PIN_CFG_FOR_GPIO_OUPUT_NOPULLUP);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_FPGA_3P3V_ON);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_FPGA_1P2V_ON);
#endif

    // I2C 핀
    Sys_I2C_DIOConfig(I2C0, I2C_PIN_CFG_VAL_forDIO, DIO_PIN_INDEX_for_CM3_SCL, DIO_PIN_INDEX_for_CM3_SDA);

    // NOTE: SPI CS가 플로팅 상태일 때 오류가 발생할 수 있어서, GPIO 출력으로 확실히 1로 설정 후 진행
    Sys_DIO_Config(NRF_SPI_CS_PIN, CM3_DIO_PIN_CFG_FOR_GPIO_OUPUT_NOPULLUP);
    Sys_GPIO_Set_High(NRF_SPI_CS_PIN);

    // SPI 통신 핀 설정: Cortex-M3 <-> nRF
    //Sys_SPI_DIOConfig(SPI1, SPI_SELECT_SLAVE, SPI_DIO_PIN_CFG, NRF_SPI_CLK_PIN, NRF_SPI_CS_PIN, NRF_SPI_MOSI_PIN, NRF_SPI_MISO_PIN);

    // SPI 통신 보조핀
    Sys_DIO_Config(GPIO_PIN_ReadCommandForSPI_Master, CM3_DIO_PIN_CFG_FOR_GPIO_OUPUT_NOPULLUP);  // nRF 칩에서 풀업 설정함
    Sys_GPIO_Set_Low(GPIO_PIN_ReadCommandForSPI_Master);

    // Power charger detection (앞으로 차저 감지를 사용하지 않음)
    // Sys_DIO_Config(DIO_PIN_INDEX_for_ChargerConnectorPluggedIn, OTE_1_5_GEN_DIO_CFG_NORMAL_CHG_DET_N);

    // Acc-sensor interrupt detection
    Sys_DIO_Config(DIO_PIN_INDEX_for_Accelerometer, OTE_1_5_GEN_DIO_CFG_NORMAL_ACCEL_INT);

    // 부트로더에서 DIO 설정, 셧다운 상태 설정, ISD 연결해제 상태 설정을 하고
    // 부팅하기 때문에 사실상 큰 의미 없다.
    tdc_qcc_init();

    return df_True;
}

int tdc_hal_dio_configure_sleep(void)
{
#if 0
    // Earpiece detection
    Sys_DIO_Config(DIO_PIN_INDEX_for_EARPIECE_DET_N, OTE_1_5_GEN_DIO_CFG_LP_EARPIECE_DET_N);

    // Power charger detection
    Sys_DIO_Config(DIO_PIN_INDEX_for_ChargerConnectorPluggedIn, OTE_1_5_GEN_DIO_CFG_LP_CHG_DET_N);

    // Carrying case detection
    Sys_DIO_Config(DIO_PIN_INDEX_for_CarryingCasePluggedIn, OTE_1_5_GEN_DIO_CFG_LP_CASE_DET);

    // Carrying case cover open detection
    Sys_DIO_Config(DIO_PIN_INDEX_for_CarryingCaseCoverOpen, OTE_1_5_GEN_DIO_CFG_LP_CASE_OPEN);

    // Acc-sensor interrupt detection
    Sys_DIO_Config(DIO_PIN_INDEX_for_Accelerometer, OTE_1_5_GEN_DIO_CFG_LP_ACCEL_INT);

    // Configure DIO interrupt for acc-sensor
    Sys_DIO_IntConfig(0, (DIO_INT_SRC_DIO_34 | DIO_INT_DEBOUNCE_DISABLE | DIO_INT_EVENT_FALLING_EDGE), DIO_DEBOUNCE_SLOWCLK_DIV32, 0);

    // Configure DIO interrupt for carrying case cover open detection
    Sys_DIO_IntConfig(1, (DIO_INT_SRC_DIO_28 | DIO_INT_DEBOUNCE_DISABLE | DIO_INT_EVENT_RISING_EDGE), DIO_DEBOUNCE_SLOWCLK_DIV32, 0);
#endif

    /* Reset DIO for LSAD */
    // Sys_DIO_Config(DIO23, (DIO_8X_DRIVE | DIO_LPF_DISABLE | DIO_250K_PULL_UP | DIO_MODE_DISABLE));

    /* Reset DIOs for SPI */
    Sys_DIO_Config(NRF_SPI_CS_PIN, (DIO_8X_DRIVE | DIO_LPF_DISABLE | DIO_250K_PULL_UP | DIO_MODE_DISABLE));
    Sys_DIO_Config(NRF_SPI_CLK_PIN, (DIO_8X_DRIVE | DIO_LPF_DISABLE | DIO_250K_PULL_UP | DIO_MODE_DISABLE));
    Sys_DIO_Config(NRF_SPI_MOSI_PIN, (DIO_8X_DRIVE | DIO_LPF_DISABLE | DIO_250K_PULL_UP | DIO_MODE_DISABLE));
    Sys_DIO_Config(NRF_SPI_MISO_PIN, (DIO_8X_DRIVE | DIO_LPF_DISABLE | DIO_250K_PULL_UP | DIO_MODE_DISABLE));

    /* Reset DIOs for I2C */
    Sys_DIO_Config(DIO_PIN_INDEX_for_CM3_SCL, (DIO_8X_DRIVE | DIO_LPF_DISABLE | DIO_250K_PULL_UP | DIO_MODE_DISABLE));
    Sys_DIO_Config(DIO_PIN_INDEX_for_CM3_SDA, (DIO_8X_DRIVE | DIO_LPF_DISABLE | DIO_250K_PULL_UP | DIO_MODE_DISABLE));

    /* Reset DIOs for UART */
    Sys_DIO_Config(TDC_HAL_UART_DIO_TX, (DIO_8X_DRIVE | DIO_LPF_DISABLE | DIO_250K_PULL_UP | DIO_MODE_DISABLE));
    Sys_DIO_Config(TDC_HAL_UART_DIO_RX, (DIO_8X_DRIVE | DIO_LPF_DISABLE | DIO_250K_PULL_UP | DIO_MODE_DISABLE));

    NVIC_ClearPendingIRQ(DIO_0_IRQn);
    NVIC_ClearPendingIRQ(DIO_1_IRQn);

    NVIC_EnableIRQ(DIO_0_IRQn);
    NVIC_EnableIRQ(DIO_1_IRQn);

    return df_True;
}

/*
 * DIO interrupt handler for acc-sensor
 */
void DIO_0_IRQHandler(void)
{
    tdc_hal_dio_set_int_flag_acc_sensor();
}

/*
 * DIO interrupt handler for carrying case cover open detection
 */
void DIO_1_IRQHandler(void)
{
    tdc_hal_dio_set_int_flag_case_lid_open();
}

