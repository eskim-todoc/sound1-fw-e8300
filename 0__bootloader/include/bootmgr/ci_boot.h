
#ifndef __ota_h__
#define __ota_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <ff.h>
#include <nvmctrl.h>

#include <sdk_ci_boot.h>

#define CI_BOOT_COUNT_DOWN_SEC 3

uint8_t ci_boot_get_slot_num(void);
void    ci_boot_set_fp(FIL *fp);
void    ci_boot_handle_boot_file(void);
void    ci_boot_print_boot_file(void);
void    ci_boot_debug_mode(void);

#endif // __ota_h__
