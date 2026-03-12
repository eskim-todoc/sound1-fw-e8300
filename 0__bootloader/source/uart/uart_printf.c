
#include <uart_printf.h>

#if ENABLE_UART

void uart_init(void)
{
    Sys_UART_Config(UART,
                    SystemCoreClock,
                    UART_BAUDRATE,
                    (UART_TX_DMA_DISABLE | UART_RX_DMA_DISABLE | UART_TX_END_INT_DISABLE | UART_TX_START_INT_DISABLE |
                     UART_RX_INT_DISABLE | UART_OVERRUN_INT_DISABLE));

    Sys_UART_DIOConfig(UART, (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL), DIO_UART_TX, DIO_UART_RX);

    UART->CTRL = UART_ENABLE;
}

void uart_printf(const char *p_fmt, ...)
{
    char    buf[64];
    int     len;
    va_list ap;

    va_start(ap, p_fmt);
    len = vsnprintf(buf, 64, p_fmt, ap);
    va_end(ap);

    for (volatile int i = 0; i < len;)
    {
        if ((UART->STATUS & (UART_TX_BUSY | UART_TX_REQ)) == (UART_TX_IDLE | UART_TX_REQ))
        {
            UART->TX_DATA = buf[i++];
        }
    }
}

int uart_getch(char *buf)
{
    if ((UART->STATUS & UART_RX_REQ) == UART_RX_REQ)
    {
        if ((UART->STATUS & UART_OVERRUN_TRUE) == UART_OVERRUN_TRUE)
        {
            UART->STATUS = UART_OVERRUN_CLEAR;
        }

        buf[0] = UART->RX_DATA;
        return 1;
    }

    return 0;
}

#endif
