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

#include "systemControl.h"

#include <batteryNPowerControl.h>
#include <ble_communication.h>
#include <cfx_cm3_sharedMemory.h>
#include <ci_filesystem.h>
#include <ci_map.h>
#include <ci_fft.h>
#include <ci_event_log.h>
#include <isd_interface.h>
#include <LedOutput.h>  //ok
#include <fn_from_cfx_eeprom_erase.h>

// 개발 진행 시 부여되는 버전 정보
#define DEV_FW_VER_BETA    0
#define DEV_FW_VER_RELEASE 1
#define DEV_FW_VER_RND     2
#define DEV_FW_VER_HW_TEST 4
#define DEV_FW_VER_SW_TEST 3

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
void debug_printer_for_mcuErrorCode(ST__ERROR_CODE *p_mcuErrorCode);
#endif
void debugPrint_ADCRegs(int num);
void debugPrint_ADCInput(void);
void debugging_for_monitoring(ST__USB_CONNECTOR usbConnectorState, bool powerButtonPushed, EN__BATTERY_LEVEL batteryLevel, ST__ISD_STATUS isd_state, ST__SYSTEM_STATE systemState, ST__BLE_COMMUNICATION_STATE BLE_communicationState);

#endif  // __main_h__
