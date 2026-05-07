
#ifndef __ota_h__
#define __ota_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <ff.h>
#include <nvmctrl.h>

#include <bootloader_internal.h>  // bootloader_boot_information

#include <sdk_ci_boot.h>

#define CI_BOOT_COUNT_DOWN_SEC 0  // 1

uint8_t tdc_boot_get_slot_num(void);
void    snd_boot_set_fp(FIL *fp);
void    snd_boot_handle_file(void);
void    tdc_boot_print_boot_file(void);
void    tdc_boot_debug_mode(void);

int  tdc_boot_storage_init(bootloader_boot_information *out_boot_info);
FIL *tdc_boot_get_ohdl(void);

#endif  // __ota_h__
