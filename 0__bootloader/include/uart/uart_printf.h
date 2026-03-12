
#ifndef __uart_printf_h__
#define __uart_printf_h__

#include <ci_initialize.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>


#define UART_BAUDRATE 921600

#define DIO_UART_TX DIO8
#define DIO_UART_RX DIO15

#if ENABLE_UART
void uart_printf(const char *p_fmt, ...);
void uart_init(void);
int  uart_getch(char *buf);
#else
#define uart_printf(...)
#define uart_init(...)
#define uart_getch(...)
#endif

#endif // __uart_printf_h__
