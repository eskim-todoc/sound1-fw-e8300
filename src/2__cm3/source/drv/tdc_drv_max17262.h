/**
 * @file tdc_drv_max17262.h
 */

#ifndef __tdc_drv_max17262_h__
#define __tdc_drv_max17262_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>
#include <tdc_hal_i2c.h>
#include <tdc_printf.h>

#define TDC_DRV_MAX17262_SLAVE_ADDR 0x36
#define TDC_DRV_MAX17262_REG_VCELL  0x09

uint32_t tdc_drv_max17262_get_mv(void);
void     tdc_drv_max17262_update_mv(void);

#endif  // __tdc_drv_max17262_h__
