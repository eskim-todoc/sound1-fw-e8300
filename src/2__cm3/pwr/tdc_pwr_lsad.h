/**
 * @file Gen1_5_battery.h
 */

#ifndef __tdc_pwr_lsad_h__
#define __tdc_pwr_lsad_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <cfx_cm3_sharedMemory.h>
#include <LedOutput.h>

#include <tdc_fs.h>
#include <tdc_util.h>

#if 1  // Sullivan 1.5
#define TDC_PWR_LSAD_INPUT_SEL_CH_NUM 0
#define TDC_PWR_LSAD_INPUT_DIO_NUM    LSAD_INPUT_DIO23
#define TDC_PWR_LSAD_PRESCALE_NUM     LSAD_PRESCALE_3200
#define TDC_PWR_LSAD_INT_CH_NUM       LSAD_INT_CH0
#else  // Sound1 Test
#define TDC_PWR_LSAD_INPUT_SEL_CH_NUM 0
#define TDC_PWR_LSAD_INPUT_DIO_NUM    LSAD_INPUT_DIO22
#define TDC_PWR_LSAD_PRESCALE_NUM     LSAD_PRESCALE_3200
#define TDC_PWR_LSAD_INT_CH_NUM       LSAD_INT_CH0
#endif

#define TDC_PWR_LSAD_STABLE_CNT 4

void LSAD_IRQHandler(void);
int  tdc_pwr_lsad_get_count(void);
void tdc_pwr_lsad_init(void);
void tdc_pwr_lsad_uninit(void);
void tdc_pwr_lsad_update(void);

// 구조체의 배치되는 주소를 sections.ld 파일을 수정하여 LPDSP32_PRAM5에 위치한다.
extern ST__CFX_CM3_SharedMemory_ALL cfx_cm3_sharedMemoryAll __attribute__((section(".shared_memory")));

#endif  // __tdc_pwr_lsad_h__
