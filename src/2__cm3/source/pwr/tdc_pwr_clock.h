/**
 * @file OTE_1P5_power_manager.h
 */

#ifndef __tdc_pwr_clock_h__
#define __tdc_pwr_clock_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>
#include <sk5_map_nvm.h>
#include <calibrate_power.h>
#include <nvmctrl.h>
#include <nvmlib.h>

#include <tdc_stim_definitions.h>

#include <tdc_util.h>
#include <tdc_fs.h>
#include <tdc_printf.h>

#include <tdc_hal_trims.h>

/* TDC_PWR_CLOCK_NORMAL/STANDBY/PREHEAT 는 2차 리팩토링에서 제거했다(참조 0).
 * 클럭 모드는 tdc_pwr_clock_normal()/_sleep() 함수 호출로 직접 구분한다. */

#define TDC_PWR_MANUF_TABLE_FILE "/MANUF_TABLE"

/* tdc_pwr_clock_backup_t 구조체는 2차 리팩토링에서 제거했다.
 * 클럭/전원 레지스터를 백업하려던 타입인데 인스턴스가 0 이었다. */

/* bootloader_boot_information 구조체(문서 주석 포함)는 2차 리팩토링에서 제거했다.
 * CM3 에서 이 타입으로 선언된 변수가 하나도 없는 미사용 사본이었다.
 * 원본은 0__bootloader/include/bootloader/bootloader_internal.h:87 에 있다. */

int tdc_pwr_clock_normal(void);
int tdc_pwr_clock_sleep(void);
/* ci_fake_power_sleep() 제거(2026-07-20) - fake sleep mode 삭제 */

#endif  // __tdc_pwr_clock_h__
