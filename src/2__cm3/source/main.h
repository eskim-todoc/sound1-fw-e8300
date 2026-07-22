/**
 * @file main.h
 */

#ifndef __main_h__
#define __main_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <tdc_sys_control.h>

#include <tdc_pwr_battery.h>
#include <tdc_ble_communication.h>
#include <tdc_shm.h>
#include <tdc_fs.h>
#include <tdc_fs_map.h>
#include <tdc_fs_fft.h>
#include <tdc_fs_event_log.h>
#include <tdc_isd.h>
#include <tdc_led_output.h>  //ok
#include <tdc_cfx_eeprom_erase.h>

// 개발 진행 시 부여되는 버전 정보
#define DEV_FW_VER_BETA    0
#define DEV_FW_VER_RELEASE 1
#define DEV_FW_VER_RND     2
#define DEV_FW_VER_HW_TEST 4
#define DEV_FW_VER_SW_TEST 3

#define RX_BATT_LEVEL_TIME_OUT_MS 3000

#define OTE_1_5_GEN_TEST_WITHOUT_CFX 1

#define ENABLE_MAIN_DEBUG_PRINT 1

void CFX_0_IRQHandler(void);

void reset_global_variables_in_main(void);

void enable_iteration(void);
void disable_iteration(void);

void update_mapNum(void);

void debugging_for_AGC_and_LogMapping(void);

int func_sleep(void);
int func_normal(void);

//
// For debugging
//

bool checkCharList(char *p_inList, int len, char *p_retCh);
bool checkChar(char in);
void debugMode(void);
bool calculate_48bit_QInFn(uint32_t QIn, uint32_t Fn, int32_t val_H, int32_t val_L, int *p_ret_QI_H, int *p_ret_F_H, int *p_ret_QI_L, int *p_ret_F_L, bool *p_isMinus);
bool calculate_24bit_QInFn(uint32_t QIn, uint32_t Fn, int32_t val, int *p_ret_QI, int *p_ret_F, bool *p_isMinus);
#if ENABLE_MAIN_DEBUG_PRINT
void debug_printer_for_mcuErrorCode(tdc_sys_error_code_t *p_mcuErrorCode);
#endif
void debugPrint_ADCRegs(int num);
void debugPrint_ADCInput(void);

/* fake sleep mode API 제거(2026-07-20): 터치센서 계측용 임시 코드.
 * 상세: docs/tasks/main/20260720_fake-sleep-removal/ */

#endif  // __main_h__
