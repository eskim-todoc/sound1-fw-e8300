// clang-format off

#ifndef DIO_PIN_Configuraion_H__
#define DIO_PIN_Configuraion_H__

#ifdef Board_is_OTE_VER_1_5

    //////////////////////////////////////////////////////////////////////////////////
    // CFX, CM3 공용
    //////////////////

    // SPI로 사용될 때 DIO 핀 설정값
    #define SPI_DIO_PIN_CFG (DIO_2X_DRIVE | DIO_LPF_DISABLE | DIO_60K_PULL_UP)

    // UART로 사용될 때 DIO 핀 설정값
    #define UART_DIO_PIN_CFG (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_250K_PULL_UP)

    /////////////////////////////////////////////////////////////////////////////////////////
    // CFX 전용
    /////////////

    // GPIO로 사용될 때 DIO 핀 설정값

    // DIO_MODE_GPIO_OUT  : 초기값의 변화가 없다.
    // DIO_MODE_GPIO_OUT  : 초기값의 변화가 없다.

    #define DIO_PIN_CFG_FOR_GPIO_OUPUT_NOPULLUP (DIO_2X_DRIVE | DIO_LPF_DISABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT)

    #define DIO_PIN_CFG_FOR_GPIO_OUPUT_PULLUP (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_250K_PULL_UP | DIO_MODE_GPIO_OUT)

    #define DIO_PIN_CFG_FOR_GPIO_INPUT_NOPULLUP (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_NO_PULL | DIO_MODE_GPIO_IN)

    #define DIO_PIN_CFG_FOR_GPIO_INPUT_PULLUP (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_250K_PULL_UP | DIO_MODE_GPIO_IN)

    // USER_CLK로 사용될 때 DIO 핀 설정값
    #define DIO_PIN_CFG_FOR_USER_CLK (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_250K_PULL_UP | DIO_MODE_USRCLK)

    // SYSCLK로 사용될 때 DIO 핀 설정값
    #define DIO_PIN_CFG_FOR_SYSCLK_OUPUT (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_250K_PULL_UP | DIO_MODE_SYSCLK)

    // SLOWCLK로 사용될 때 DIO 핀 설정값
    #define DIO_PIN_CFG_FOR_SLOWCLK_OUPUT (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_250K_PULL_UP | DIO_MODE_SLOWCLK)
    // LSAD
    #define DIO_PIN_CFG_LSAD (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_NO_PULL | DIO_MODE_INPUT)

    // Enable a 1K I2C bus pull-up resistor and enable filtering at the I2C pads.

    #define I2C_PIN_CFG_VAL_forDedicatedPin (I2C_PIN_CFG_PULLUP_ENABLE | I2C_1K_PULL_UP | I2C_PIN_CFG_FILTER_ENABLE)

    // CFX에서  DIO핀을 I2C핀으로 설정하는 것은, 전용 SDA, SCL 핀의 동작을 강화 시키기 위한 목적이며 DIO핀 단독으로는 통신이 불가능하다.
    #define I2C_SCL_PIN_CFG_VAL_forDIO (DIO_4X_DRIVE | DIO_LPF_ENABLE | DIO_1K_PULL_UP | DIO_MODE_SCL)
    #define I2C_SDA_PIN_CFG_VAL_forDIO (DIO_4X_DRIVE | DIO_LPF_ENABLE | DIO_1K_PULL_UP | DIO_MODE_SDA)

    #define I2C_PIN_CFG_VAL_forDIO (DIO_8X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL) // __KIM: 신규 추가

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // CM3 전용
    //////////////

    // CM3에서 DIO 핀을 I2C로 사용하기 위한 설정값
    #define CM3_I2C_MASTER_PIN_CFG_VAL (DIO_4X_DRIVE | DIO_LPF_ENABLE | DIO_1K_PULL_UP)

    // CM3에서 DIO 핀을 I2C로 사용하기 위한 설정값
    #define CM3_I2C_SLAVE_PIN_CFG_VAL (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_1K_PULL_UP)

    // GPIO로 사용될 때 DIO 핀 설정값
    // DIO_MODE_GPIO_OUT  초기 값이 0
    // DIO_MODE_GPIO_OUT  초기 값이 1

    #define CM3_DIO_PIN_CFG_FOR_GPIO_OUPUT_NOPULLUP (DIO_4X_DRIVE | DIO_LPF_DISABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT)

    #define CM3_DIO_PIN_CFG_FOR_GPIO_OUPUT_PULLUP (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_250K_PULL_UP | DIO_MODE_GPIO_OUT)

    #define CM3_DIO_PIN_CFG_FOR_GPIO_INPUT_NOPULLUP (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_NO_PULL | DIO_MODE_GPIO_IN)

    #define CM3_DIO_PIN_CFG_FOR_GPIO_INPUT_PULLUP (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_250K_PULL_UP | DIO_MODE_GPIO_IN)

#else // 'Board_is_OTE_1_5gen_Test_board' is not defined.

    //////////////////////////////////////////////////////////////////////////////////
    // CFX, CM3 공용
    //////////////////

    // SPI로 사용될 때 DIO 핀 설정값
    #define SPI_DIO_PIN_CFG (DIO_LOW_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PU_ENABLE | DIO_STRONG_PU_DISABLE)

    // UART로 사용될 때 DIO 핀 설정값
    #define UART_DIO_PIN_CFG (DIO_LOW_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PU_ENABLE | DIO_STRONG_PU_DISABLE)

    /////////////////////////////////////////////////////////////////////////////////////////
    // CFX 전용
    /////////////

    // GPIO로 사용될 때 DIO 핀 설정값

    // DIO_MODE_GPIO_OUT_0	: 초기값의 변화가 없다.
    // DIO_MODE_GPIO_OUT_1  : 초기값의 변화가 없다.

    #define DIO_PIN_CFG_FOR_GPIO_OUPUT_NOPULLUP (DIO_HIGH_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PU_DISABLE | DIO_STRONG_PU_DISABLE | DIO_MODE_GPIO_OUT_0)

    #define DIO_PIN_CFG_FOR_GPIO_OUPUT_PULLUP (DIO_HIGH_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PU_ENABLE | DIO_STRONG_PU_DISABLE | DIO_MODE_GPIO_OUT_0)

    #define DIO_PIN_CFG_FOR_GPIO_INPUT_NOPULLUP (DIO_LOW_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PU_DISABLE | DIO_STRONG_PU_DISABLE | DIO_MODE_GPIO_IN_0)

    #define DIO_PIN_CFG_FOR_GPIO_INPUT_PULLUP (DIO_LOW_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PU_ENABLE | DIO_STRONG_PU_DISABLE | DIO_MODE_GPIO_IN_0)

    // DIO_MODE_GPIO_IN_0 , DIO_MODE_GPIO_IN_1의 차이를 모르겠음.. 입력이 데이터의 high low에 따라 값이 변경됨..
    // 따라서 설정값의 의미가 없어 보임

    // USER_CLK로 사용될 때 DIO 핀 설정값
    #define DIO_PIN_CFG_FOR_USER_CLK (DIO_LOW_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PU_ENABLE | DIO_STRONG_PU_DISABLE | DIO_MODE_UCLK)

    // SYSCLK로 사용될 때 DIO 핀 설정값
    #define DIO_PIN_CFG_FOR_SYSCLK_OUPUT (DIO_LOW_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PU_ENABLE | DIO_STRONG_PU_DISABLE | DIO_MODE_SYSCLK)

    // SLOWCLK로 사용될 때 DIO 핀 설정값
    #define DIO_PIN_CFG_FOR_SLOWCLK_OUPUT (DIO_LOW_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PU_ENABLE | DIO_STRONG_PU_DISABLE | DIO_MODE_SLOWCLK)
    // LSAD
    #define DIO_PIN_CFG_LSAD (DIO_LOW_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PU_DISABLE | DIO_STRONG_PU_DISABLE | DIO_MODE_LSAD)

    // Enable a 1K I2C bus pull-up resistor and enable filtering at the I2C pads.
    #define I2C_PIN_CFG_VAL_forDedicatedPin (I2C_PIN_CFG_PULLUP_ENABLE | I2C_PIN_CFG_PULLUP_1K | I2C_PIN_CFG_FILTER_ENABLE)

    // CFX에서  DIO핀을 I2C핀으로 설정하는 것은, 전용 SDA, SCL 핀의 동작을 강화 시키기 위한 목적이며 DIO핀 단독으로는 통신이 불가능하다.
    #define I2C_SCL_PIN_CFG_VAL_forDIO (DIO_HIGH_DRIVE | DIO_LPF_ENABLE | DIO_WEAK_PU_DISABLE | DIO_STRONG_PU_ENABLE | DIO_MODE_SCL)

    #define I2C_SDA_PIN_CFG_VAL_forDIO (DIO_HIGH_DRIVE | DIO_LPF_ENABLE | DIO_WEAK_PU_DISABLE | DIO_STRONG_PU_ENABLE | DIO_MODE_SDA)

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // CM3 전용
    //////////////

    // CM3에서 DIO 핀을 I2C로 사용하기 위한 설정값
    #define CM3_I2C_MASTER_PIN_CFG_VAL (DIO_HIGH_DRIVE | DIO_LPF_ENABLE | DIO_WEAK_PU_DISABLE | DIO_STRONG_PU_ENABLE)

    // CM3에서 DIO 핀을 I2C로 사용하기 위한 설정값
    #define CM3_I2C_SLAVE_PIN_CFG_VAL (DIO_LOW_DRIVE | DIO_LPF_ENABLE | DIO_WEAK_PU_DISABLE | DIO_STRONG_PU_ENABLE)

    // GPIO로 사용될 때 DIO 핀 설정값
    // DIO_MODE_C_GPIO_OUT_0  초기 값이 0
    // DIO_MODE_C_GPIO_OUT_1  초기 값이 1

    #define CM3_DIO_PIN_CFG_FOR_GPIO_OUPUT_NOPULLUP (DIO_HIGH_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PU_DISABLE | DIO_STRONG_PU_DISABLE | DIO_MODE_C_GPIO_OUT_0)

    #define CM3_DIO_PIN_CFG_FOR_GPIO_OUPUT_PULLUP (DIO_HIGH_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PU_ENABLE | DIO_STRONG_PU_DISABLE | DIO_MODE_C_GPIO_OUT_0)

    #define CM3_DIO_PIN_CFG_FOR_GPIO_INPUT_NOPULLUP (DIO_LOW_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PU_DISABLE | DIO_STRONG_PU_DISABLE | DIO_MODE_C_GPIO_IN_0)

    #define CM3_DIO_PIN_CFG_FOR_GPIO_INPUT_PULLUP (DIO_LOW_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PU_ENABLE | DIO_STRONG_PU_DISABLE | DIO_MODE_C_GPIO_IN_0)

#endif // 'Board_is_OTE_1_5gen_Test_board' is not defined.

#endif // DIO_PIN_Configuraion_H__

// clang-format on
