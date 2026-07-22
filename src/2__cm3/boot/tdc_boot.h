/**
 * @file tdc_boot.h
 */

#ifndef __tdc_boot_h__
#define __tdc_boot_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <ff.h>
#include <nvmctrl.h>

#include <tdc_dfu_sdk_boot.h>
#include <tdc_printf.h>

#include <tdc_fs.h>
#include <tdc_util.h>

typedef enum
{
    BOOT_RET_TRUE = 0,
    BOOT_RET_FAIL = 1,
} EN__BOOT_RET;

void         tdc_boot_init_fp(FIL *fp);
EN__BOOT_RET tdc_boot_get_status(snd_boot_status_t *p_status);
EN__BOOT_RET tdc_boot_update_status(snd_boot_status_t *p_status);
void         tdc_boot_handle_fsm(void);

#endif  // __tdc_boot_h__
