/**
 * @file SEGGER_RTT_Wrapper.h
 */

#ifndef __SEGGER_RTT_Wrapper_h__
#define __SEGGER_RTT_Wrapper_h__

#include <SEGGER_RTT.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#define debug_printf(...) SEGGER_RTT_Wrapper_printf(__VA_ARGS__)
#define RTT_printf(...)  SEGGER_RTT_Wrapper_printf(__VA_ARGS__)
#define RTT_getch(...)   SEGGER_RTT_Wrapper_getch(__VA_ARGS__)

void SEGGER_RTT_Wrapper_Init(void);
void SEGGER_RTT_Wrapper_printf(const char *p_fmt, ...);
int  SEGGER_RTT_Wrapper_getch(char *p_buf);

#endif // __SEGGER_RTT_Wrapper_h__
