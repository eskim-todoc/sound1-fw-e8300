/**
 * @file OTE_1_5gen_UART.h
 */

#ifndef __tdc_hal_uart_h__
#define __tdc_hal_uart_h__

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <processorDirective.h>
#include <tdc_stim_definitions.h>

#include <board.h>

#define TDC_HAL_UART_DIO_INIT_CFG   (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL)
#define TDC_HAL_UART_DIO_UNINIT_CFG (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_250K_PULL_UP | DIO_MODE_DISABLE)

#define TDC_HAL_UART_DIO_TX DIO_PIN_INDEX_forUART_TX
#define TDC_HAL_UART_DIO_RX DIO_PIN_INDEX_forUART_RX

#define TDC_HAL_UART_TX_BUF_LEN 256

#define TDC_HAL_UART_BAUDRATE_115200 115200
#define TDC_HAL_UART_BAUDRATE_921600 921600

#define TDC_HAL_UART_BAUDRATE TDC_HAL_UART_BAUDRATE_921600

#define TDC_HAL_UART_CONFIG                                                                                                                                         \
    (UART_TX_DMA_DISABLE | UART_RX_DMA_DISABLE | UART_TX_END_INT_DISABLE | UART_TX_START_INT_DISABLE | UART_RX_INT_DISABLE | UART_OVERRUN_INT_DISABLE)


int  tdc_hal_uart_uninit(void);
int  tdc_hal_uart_printf(const char* p_fmt, ...);


#endif  // __tdc_hal_uart_h__
