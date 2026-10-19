/**
 * @file initialize.h
 */

#ifndef __tdc_sys_init_h__
#define __tdc_sys_init_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <tdc_util.h>

#include <tdc_isd_init_fpga.h>
#include <tdc_isd_fpga.h>

void tdc_sys_init(void);
void tdc_sys_reset_nrf(void);

#endif  // __tdc_sys_init_h__
