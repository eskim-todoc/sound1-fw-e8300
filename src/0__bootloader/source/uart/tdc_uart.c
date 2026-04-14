
#include <tdc_uart.h>
#include <tdc_util.h>

static bool gs_tdc_uart_enable = false;

static inline bool tdc_uart_is_tx_ready(void)
{
    uint32_t status = UART->STATUS;

    return ((status & UART_TX_BUSY) != UART_TX_BUSY) /* line wrapping */
           && ((status & UART_TX_REQ) == UART_TX_REQ);
}

static inline bool tdc_uart_is_rx_ready(void)
{
    return (UART->STATUS & UART_RX_REQ) == UART_RX_REQ;
}

static inline void tdc_uart_clear_overrun(void)
{
    if ((UART->STATUS & UART_OVERRUN_TRUE) == UART_OVERRUN_TRUE)
    {
        UART->STATUS = UART_OVERRUN_CLEAR;
    }
}

void tdc_uart_set_enable(bool enable)
{
    gs_tdc_uart_enable = enable;
}

bool tdc_uart_get_enable(void)
{
    return gs_tdc_uart_enable;
}

void tdc_uart_init(void)
{
    // 활성화 상태인 경우 비활성화 후 설정할 수 있도록 수정함
#if 1
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
#endif

    Sys_UART_Config(UART, SystemCoreClock, TDC_UART_BAUDRATE, TDC_UART_CONFIG);

    if (gs_tdc_uart_enable)
    {
        // IMPORTANT: 현재 레벨 3.3V 시프터가 없어서 1.8V에 강제로 내부 풀업을 사용하는 중임 (2026.04.07)
        // Sys_UART_DIOConfig(UART, (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL), TDC_UART_DIO_TX, TDC_UART_DIO_RX);
        Sys_UART_DIOConfig(UART, (DIO_4X_DRIVE | DIO_LPF_ENABLE | DIO_STRONG_PULL_UP), TDC_UART_DIO_TX, TDC_UART_DIO_RX);
    }

    UART->CTRL = UART_ENABLE;
}

void tdc_uart_printf(const char *p_fmt, ...)
{
    char    buf[TDC_UART_TX_BUF_SIZE];
    int     len;
    va_list ap;
    int     wait_cnt;

    if (!gs_tdc_uart_enable)
    {
        return;
    }

    va_start(ap, p_fmt);
    len = vsnprintf(buf, TDC_UART_TX_BUF_SIZE, p_fmt, ap);
    va_end(ap);

    if (len < 0)
    {
        return;
    }

    wait_cnt = 0;

    for (int i = 0; i < len;)
    {
        if (tdc_uart_is_tx_ready())
        {
            UART->TX_DATA = buf[i++];  // 인덱스 증가
            wait_cnt      = 0;
        }
        else
        {
            if (TDC_UART_TX_WAIT_CNT_MAX <= wait_cnt)
            {
                break;
            }
            else
            {
                tdc_delay_us(1);
                wait_cnt++;
            }
        }
    }  // end, for
}

int tdc_uart_getch(char *buf)
{
    if (!gs_tdc_uart_enable)
    {
        return 0;
    }

    if (tdc_uart_is_rx_ready())
    {
        tdc_uart_clear_overrun();

        buf[0] = UART->RX_DATA;

        return 1;
    }

    return 0;
}
