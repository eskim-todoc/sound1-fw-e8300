
#ifndef __rtt_printf_h__
#define __rtt_printf_h__

#include <ci_initialize.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <SEGGER_RTT.h>

#if ENABLE_SEGGER_RTT
void rtt_printf(const char *p_fmt, ...);
int  rtt_getch(char *buf);
#else
#define rtt_printf(...)
#define rtt_getch(...)
#endif

#endif // __rtt_printf_h__
