/**
 * @file ci_util.h
 */

#ifndef __ci_util_h__
#define __ci_util_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <board.h>
#include <definitionsForAlgorithm.h>

#include <ci_uart.h>

void ci_util_indicate_critical_error(void);
void ci_util_assert(int a);
void delay_ms(uint32_t ms);
void delay_ms_long_time(uint32_t ms, uint32_t cnt);
void print_sysvar_manu_table(void);

#endif  // __ci_util_h__
