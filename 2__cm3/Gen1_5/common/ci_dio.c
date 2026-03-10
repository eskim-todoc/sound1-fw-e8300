/**
 * @file OTE_1_5gen_DIO.c
 */

#include <ci_dio.h>

static volatile int s_int_flag_acc_sensor    = 0;
static volatile int s_int_flag_case_lid_open = 0;

bool ci_dio_is_set_int_flag_acc_sensor(void)
{
    return (s_int_flag_acc_sensor == 1);
}

bool ci_dio_is_set_int_flag_case_lid_open(void)
{
    return (s_int_flag_case_lid_open == 1);
}

void ci_dio_set_int_flag_acc_sensor(void)
{
    s_int_flag_acc_sensor = 1;
}

void ci_dio_set_int_flag_case_lid_open(void)
{
    s_int_flag_case_lid_open = 1;
}

void ci_dio_clear_int_flag_acc_sensor(void)
{
    s_int_flag_acc_sensor = 0;
}

void ci_dio_clear_int_flag_case_lid_open(void)
{
    s_int_flag_case_lid_open = 0;
}

int ci_dio_configure_normal(void)
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
    // __KIM: 테스트 단계이므로 항상 켜놓는 상태로 만듬
    //      : FPGA_SLEEP 핀의 레벨이 FPGA 내부에서 V_LINK_ON 핀으로 바이패스 출력되도록 설정되었음

#if 0
    Sys_DIO_Config(DIO_PIN_INDEX_for_FPGA_3P3V_ON, DIO_PIN_CFG_FOR_GPIO_OUPUT_NOPULLUP);
    Sys_DIO_Config(DIO_PIN_INDEX_for_FPGA_1P2V_ON, DIO_PIN_CFG_FOR_GPIO_OUPUT_NOPULLUP);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_FPGA_3P3V_ON);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_FPGA_1P2V_ON);
#endif

    // I2C 핀
    Sys_I2C_DIOConfig(I2C0, I2C_PIN_CFG_VAL_forDIO, DIO_PIN_INDEX_for_CM3_SCL, DIO_PIN_INDEX_for_CM3_SDA);

    // SPI 통신 핀 설정: Cortex-M3 <-> nRF
    Sys_SPI_DIOConfig(SPI1, SPI_SELECT_SLAVE, SPI_DIO_PIN_CFG, NRF_SPI_CLK_PIN, NRF_SPI_CS_PIN, NRF_SPI_MOSI_PIN, NRF_SPI_MISO_PIN);

    // SPI 통신 보조핀
    Sys_DIO_Config(GPIO_PIN_ReadCommandForSPI_Master,
                   CM3_DIO_PIN_CFG_FOR_GPIO_OUPUT_NOPULLUP);  // nRF 칩에서 풀업 설정함

    // nRF의 BLE On/Off 핀
    Sys_DIO_Config(DIO_NUM_NRF_ON_OFF_COMMAND, CM3_DIO_PIN_CFG_FOR_GPIO_OUPUT_NOPULLUP);
    // Sys_GPIO_Set_High(DIO_NUM_NRF_ON_OFF_COMMAND);
    Sys_GPIO_Set_Low(DIO_NUM_NRF_ON_OFF_COMMAND);

    // nRF의 BLE 광고 전력 모드
    Sys_DIO_Config(ENABLE_NRF_ADV_LowPower, CM3_DIO_PIN_CFG_FOR_GPIO_OUPUT_NOPULLUP);
    Sys_GPIO_Set_High(ENABLE_NRF_ADV_LowPower);

    // nRF 리셋핀
    Sys_DIO_Config(DIO_NUM_NRF_SWDIO_NRESET, DIO_PIN_CFG_FOR_GPIO_OUPUT_NOPULLUP);
    Sys_GPIO_Set_High(DIO_NUM_NRF_SWDIO_NRESET);  // 초기 값은 High 설정하여 nRF가 리셋되지 않도록 설정

    // Earpiece detection
    Sys_DIO_Config(DIO_PIN_INDEX_for_EARPIECE_DET_N, OTE_1_5_GEN_DIO_CFG_NORMAL_EARPIECE_DET_N);

    // Power charger detection
    Sys_DIO_Config(DIO_PIN_INDEX_for_ChargerConnectorPluggedIn, OTE_1_5_GEN_DIO_CFG_NORMAL_CHG_DET_N);

    // Carrying case detection
    Sys_DIO_Config(DIO_PIN_INDEX_for_CarryingCasePluggedIn, OTE_1_5_GEN_DIO_CFG_NORMAL_CASE_DET);

    // Carrying case cover open detection
    Sys_DIO_Config(DIO_PIN_INDEX_for_CarryingCaseCoverOpen, OTE_1_5_GEN_DIO_CFG_NORMAL_CASE_OPEN);
    // Sys_DIO_Config(DIO_PIN_INDEX_for_CarryingCaseCoverOpen, (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_60K_PULL_DOWN | DIO_MODE_GPIO_IN));

    // Acc-sensor interrupt detection
    Sys_DIO_Config(DIO_PIN_INDEX_for_Accelerometer, OTE_1_5_GEN_DIO_CFG_NORMAL_ACCEL_INT);

    // Configure DIO interrupt for acc-sensor
#if 1  // Sullivan 1.5
    Sys_DIO_IntConfig(0, (DIO_INT_SRC_DIO_34 | DIO_INT_DEBOUNCE_DISABLE | DIO_INT_EVENT_FALLING_EDGE), DIO_DEBOUNCE_SLOWCLK_DIV32, 0);

    // Configure DIO interrupt for carrying case cover open detection
    // CASE_OPEN_n 핀이 25.09.30일 잠수함 패치 회로에서 DIO28에서 DIO27로 변경됨
    // 커버 닫히면 Low, 열리면 High 신호가 들어오므로 Rising edge를 감지하도록 한다.
    Sys_DIO_IntConfig(1, (DIO_INT_SRC_DIO_27 | DIO_INT_DEBOUNCE_DISABLE | DIO_INT_EVENT_RISING_EDGE), DIO_DEBOUNCE_SLOWCLK_DIV32, 0);
    // Sys_DIO_IntConfig(1, (DIO_INT_SRC_DIO_27 | DIO_INT_DEBOUNCE_DISABLE | DIO_INT_EVENT_FALLING_EDGE), DIO_DEBOUNCE_SLOWCLK_DIV32, 0);
#else  // Sound1 Test
    // acc sensor
    Sys_DIO_IntConfig(0, (DIO_INT_SRC_DIO_19 | DIO_INT_DEBOUNCE_DISABLE | DIO_INT_EVENT_FALLING_EDGE), DIO_DEBOUNCE_SLOWCLK_DIV32, 0);
#endif

    NVIC_ClearPendingIRQ(DIO_0_IRQn);
    NVIC_ClearPendingIRQ(DIO_1_IRQn);

    NVIC_DisableIRQ(DIO_0_IRQn);
    NVIC_DisableIRQ(DIO_1_IRQn);

#if 1  // CM3 디버깅 용도의 DIO 설정 (TDI_E8300 사용)
    Sys_DIO_Config(DIO32, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT));
    Sys_GPIO_Set_Low(DIO32);
#endif

#if 1  // CFX 디버깅 용도의 DIO 설정 (CALIBRATION 사용)
    Sys_GPIO_Set_High(DIO19);
    Sys_DIO_Config(DIO19, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT));
#endif

    return df_True;
}

int ci_dio_configure_sleep(void)
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
    Sys_DIO_Config(CI_UART_DIO_TX, (DIO_8X_DRIVE | DIO_LPF_DISABLE | DIO_250K_PULL_UP | DIO_MODE_DISABLE));
    Sys_DIO_Config(CI_UART_DIO_RX, (DIO_8X_DRIVE | DIO_LPF_DISABLE | DIO_250K_PULL_UP | DIO_MODE_DISABLE));

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
    ci_dio_set_int_flag_acc_sensor();
}

/*
 * DIO interrupt handler for carrying case cover open detection
 */
void DIO_1_IRQHandler(void)
{
    ci_dio_set_int_flag_case_lid_open();
}

