/**
 * @file OTE_1_5gen_UART.c
 */

#include <ci_uart.h>

static char m_tx_buf[CI_UART_TX_BUF_LEN];

int ci_uart_init(void)
{
    uint32_t cfg;

#if 1
    // 출력 중이던 TX 데이터가 있다면 출력이 완료될 때 까지 대기 (Last 1 char)
    while ((UART->STATUS & UART_TX_BUSY) == UART_TX_BUSY)
    {
        (void) 0;
    }

    UART->CTRL = UART_DISABLE;  // UART 비활성화

    // UART 비활성화 검증 (계속 비활성화 되지 않는다면 워치독이 발생할 수 있음)
    while ((UART->CTRL & UART_STATUS_ENABLED) == UART_STATUS_ENABLED)
    {
        (void) 0;
    }

    UART->CTRL       = UART_RESET;              // UART 리셋
    DIO->SRC_UART[0] = UART_RX_SRC_CONST_HIGH;  // UART RX DIO 레벨을 HIGH로 고정

    FS_MEM_UART->state = FS_MEM_UART_STATE_RESET;
#endif

    // UART 설정
    Sys_UART_Config(UART, SystemCoreClock, CI_UART_BAUDRATE, CI_UART_CONFIG);

    // DIO 설정
    Sys_DIO_Config(CI_UART_DIO_TX, ((DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_DISABLE)));
    Sys_DIO_Config(CI_UART_DIO_RX, ((DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_DISABLE)));

    Sys_UART_DIOConfig(UART, CI_UART_DIO_INIT_CFG, /*CI_UART_DIO_TX*/ DIO33, /*CI_UART_DIO_RX*/ DIO32);
    // Sys_DIO_Config(DIO_PIN_INDEX_for_LED_color_R, DIO_PIN_CFG_FOR_GPIO_OUPUT_NOPULLUP);  // R
    // DIO->SRC_UART[0] = UART_RX_SRC_CONST_HIGH;

    // UART 활성화
    UART->CTRL = UART_ENABLE;

    // UART 활성화 대기
    while ((UART->CTRL & UART_STATUS_ENABLED) != UART_STATUS_ENABLED)
    {
        (void) 0;
    }

    FS_MEM_UART->state = FS_MEM_UART_STATE_INIT;
    FS_MEM_UART->state = FS_MEM_UART_STATE_IDLE;

    return df_True;
}

int ci_uart_uninit(void)
{
    // 출력 중이던 TX 데이터가 있다면 출력이 완료될 때 까지 대기 (Last 1 char)
    while ((UART->STATUS & UART_TX_BUSY) == UART_TX_BUSY)
    {
        (void) 0;
    }

    UART->CTRL       = UART_DISABLE;            // UART 비활성화
    DIO->SRC_UART[0] = UART_RX_SRC_CONST_HIGH;  // UART RX DIO 레벨을 HIGH로 고정

    // UART 비활성화 검증 (계속 비활성화 되지 않는다면 워치독이 발생할 수 있음)
    while ((UART->CTRL & UART_STATUS_ENABLED) == UART_STATUS_ENABLED)
    {
        (void) 0;
    }

    Sys_DIO_Config(CI_UART_DIO_TX, CI_UART_DIO_UNINIT_CFG);  // UART TX DIO 해제
    Sys_DIO_Config(CI_UART_DIO_RX, CI_UART_DIO_UNINIT_CFG);  // UART TX DIO 해제

    FS_MEM_UART->state = FS_MEM_UART_STATE_RESET;

    return df_True;
}

int ci_uart_getch(char *p_ch)
{
    if ((UART->CTRL & UART_STATUS_ENABLED) != UART_STATUS_ENABLED)
    {
        return df_False;
    }

    if ((UART->STATUS & UART_OVERRUN_TRUE) == UART_OVERRUN_TRUE)
    {
        UART->STATUS = UART_OVERRUN_CLEAR;
    }

    if ((UART->STATUS & UART_RX_REQ) != UART_RX_REQ)
    {
        return df_False;
    }

    *p_ch = UART->RX_DATA;

    return df_True;
}

int ci_uart_printf(const char *p_fmt, ...)
{
    int     len;
    va_list ap;

    while (FS_MEM_UART->state != FS_MEM_UART_STATE_IDLE)
        ;

    FS_MEM_UART->state = FS_MEM_UART_STATE_CM3;

    if ((UART->CTRL & UART_STATUS_ENABLED) != UART_STATUS_ENABLED)
    {
        FS_MEM_UART->state = FS_MEM_UART_STATE_IDLE;
        return df_False;
    }

    va_start(ap, p_fmt);

    len = vsnprintf(m_tx_buf, CI_UART_TX_BUF_LEN, p_fmt, ap);

    va_end(ap);

    // TX 데이터 출력
    for (int i = 0; i < len;)
    {
        // TX BUSY 상태가 아니며 TX 전송이 가능할 때 데이터 전송
        if ((UART->STATUS & UART_TX_BUSY) == UART_TX_BUSY)
        {
            continue;
        }

        if ((UART->STATUS & UART_TX_REQ) != UART_TX_REQ)
        {
            continue;
        }

        UART->TX_DATA = m_tx_buf[i++];
    }  // 끝, for : TX 데이터 출력

    // 마지막 TX까지 전부 전송되었는지 확인
    while (1)
    {
        if ((UART->STATUS & UART_TX_BUSY) != UART_TX_BUSY)
        {
            if ((UART->STATUS & UART_TX_REQ) == UART_TX_REQ)
            {
                break;
            }
        }
    }  // 끝, while : 마지막 TX까지 전부 전송되었는지 확인

    FS_MEM_UART->state = FS_MEM_UART_STATE_IDLE;

    return df_True;
}

void ci_uart_set_color(uint32_t color)
{
    ci_uart_printf("\033[38:5:%um", color);
}

void ci_uart_clear_color(void)
{
    ci_uart_printf("\033[0m");
}
