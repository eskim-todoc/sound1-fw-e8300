
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

#define CI_BOOT_COUNT_DOWN_SEC 0  // 1

/* forward declaration — 정의는 <bootloader_internal.h> 에 있음.
 * tdc_boot_storage_init() 의 인자 타입을 노출하기 위함. ci_boot.h 가 transitive 로
 * bootloader_internal.h 를 끌어오면 다른 컴파일 단위 (ci_boot.c / service.c) 에서
 * aes.h / bootloader_application_file.h 등 부수 의존이 충돌하므로 분리. */
struct _bootloader_boot_information;

uint8_t tdc_boot_get_slot_num(void);
void    snd_boot_set_fp(FIL *fp);
void    snd_boot_handle_file(void);
void    tdc_boot_print_boot_file(void);

int  tdc_boot_storage_init(struct _bootloader_boot_information *out_boot_info);
FIL *tdc_boot_get_ohdl(void);

#endif  // __ota_h__
