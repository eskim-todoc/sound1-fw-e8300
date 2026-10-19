/**
 * @file OTE_1_5gen_DIO.h
 */

#ifndef __tdc_hal_dio_h__
#define __tdc_hal_dio_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <board.h>
#include <tdc_hal_spi.h>
#include <tdc_pwr_clock.h>
#include <processorDirective.h>

#include <tdc_qcc.h>

// DIO list

/* 이어피스·충전기·케이스·케이스덮개용 DIO 설정 매크로 9종(NORMAL 4 + LP 5)은
 * 2차 리팩토링에서 제거했다. 유일한 소비자였던 tdc_hal_dio_configure_sleep() 이
 * 사장 함수로 확인돼 G2 에서 함께 제거됐기 때문이다.
 * 실제로 참조되는 가속도센서용 ACCEL_INT 만 남긴다. */
#define OTE_1_5_GEN_DIO_CFG_NORMAL_ACCEL_INT (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_IN)

// Function header

void tdc_hal_dio_set_int_flag_case_lid_open(void);
void tdc_hal_dio_set_int_flag_acc_sensor(void);

void DIO_0_IRQHandler(void);
void DIO_1_IRQHandler(void);

int tdc_hal_dio_configure_normal(void);

#endif  // __tdc_hal_dio_h__
