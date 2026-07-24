/**
 * @file OTE_1_5gen_UART.c
 */

#include <tdc_hal_uart.h>
#include <tdc_shm_debug.h>

static char m_tx_buf[TDC_HAL_UART_TX_BUF_LEN];

int tdc_hal_uart_uninit(void)
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

    Sys_DIO_Config(TDC_HAL_UART_DIO_TX, TDC_HAL_UART_DIO_UNINIT_CFG);  // UART TX DIO 해제
    Sys_DIO_Config(TDC_HAL_UART_DIO_RX, TDC_HAL_UART_DIO_UNINIT_CFG);  // UART TX DIO 해제

    FS_MEM_UART->state = FS_MEM_UART_STATE_RESET;

    return df_True;
}

int tdc_hal_uart_printf(const char *p_fmt, ...)
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

    len = vsnprintf(m_tx_buf, TDC_HAL_UART_TX_BUF_LEN, p_fmt, ap);

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
