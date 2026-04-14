
#ifndef __uart_printf_h__
#define __uart_printf_h__

#include <ci_initialize.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#define TDC_UART_CONFIG      (UART_TX_DMA_DISABLE | UART_RX_DMA_DISABLE | UART_TX_END_INT_DISABLE | UART_TX_START_INT_DISABLE | UART_RX_INT_DISABLE | UART_OVERRUN_INT_DISABLE)
#define TDC_UART_BAUDRATE    921600
#define TDC_UART_TX_BUF_SIZE 128

// BAUDRATE 설정에 따른 1 char 전송 시간 (us)
// 115200 = 86.8
// 921600 = 10.85
#define TDC_UART_TX_WAIT_CNT_MAX 160  // 115200 기준 2 char 전송 시간 조금 안되는 정도

#define TDC_UART_DIO_TX DIO8
#define TDC_UART_DIO_RX DIO15

void tdc_uart_set_enable(bool enable);
bool tdc_uart_get_enable(void);
void tdc_uart_printf(const char *p_fmt, ...);
void tdc_uart_init(void);
int  tdc_uart_getch(char *buf);

#endif  // __uart_printf_h__
