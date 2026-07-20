/**
 * @file driver_MAX17262.h
 */

#ifndef __driver_MAX17262_h__
#define __driver_MAX17262_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>
#include <driver_i2c.h>
#include <tdc_printf.h>

#define MAX17262_SLAVE_ADDR 0x36
#define MAX17262_REG_VCELL  0x09

uint32_t max17262_get_mv(void);
void     max17262_update_mv(void);

#endif  // __driver_MAX17262_h__
