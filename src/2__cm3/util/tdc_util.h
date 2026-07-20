/**
 * @file tdc_util.h
 */

#ifndef __tdc_util_h__
#define __tdc_util_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <board.h>
#include <definitionsForAlgorithm.h>

#include <tdc_hal_uart.h>

void tdc_util_indicate_critical_error(void);
void tdc_util_assert(int a);
void tdc_util_delay_ms(uint32_t ms);

#endif  // __tdc_util_h__
