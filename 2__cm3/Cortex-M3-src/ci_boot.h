/**
 * @file ci_boot.h
 */

#ifndef __ci_boot_h__
#define __ci_boot_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <ff.h>
#include <nvmctrl.h>

#include <sdk_ci_boot.h>
#include <ci_printf.h>

#include <ci_filesystem.h>
#include <ci_util.h>

typedef enum
{
    BOOT_RET_TRUE = 0,
    BOOT_RET_FAIL = 1,
} EN__BOOT_RET;

void         ci_boot_init_fp(FIL *fp);
EN__BOOT_RET ci_boot_get_status(snd_boot_status_t *p_status);
EN__BOOT_RET ci_boot_update_status(snd_boot_status_t *p_status);
void         ci_boot_handle_fsm(void);

#endif  // __ci_boot_h__
