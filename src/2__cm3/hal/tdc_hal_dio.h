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
#include <driver_SPI.h>
#include <ci_power.h>
#include <tdc_hal_uart.h>
#include <processorDirective.h>

#include <snd_qcc.h>

// DIO list

// CFG for normal mode
#define OTE_1_5_GEN_DIO_CFG_NORMAL_EARPIECE_DET_N (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_WEAK_PULL_UP | DIO_MODE_GPIO_IN)
#define OTE_1_5_GEN_DIO_CFG_NORMAL_CHG_DET_N      (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_IN)
#define OTE_1_5_GEN_DIO_CFG_NORMAL_CASE_DET       (DIO_4X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL /*DIO_60K_PULL_DOWN*/ | DIO_MODE_GPIO_IN)
#define OTE_1_5_GEN_DIO_CFG_NORMAL_CASE_OPEN      (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL /*DIO_60K_PULL_DOWN*/ | DIO_MODE_GPIO_IN)
#define OTE_1_5_GEN_DIO_CFG_NORMAL_ACCEL_INT      (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_IN)

// CFG for lp mode
#define OTE_1_5_GEN_DIO_CFG_LP_EARPIECE_DET_N (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_IN)
#define OTE_1_5_GEN_DIO_CFG_LP_CHG_DET_N      (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_IN)
#define OTE_1_5_GEN_DIO_CFG_LP_CASE_DET       (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_60K_PULL_DOWN | DIO_MODE_GPIO_IN)
#define OTE_1_5_GEN_DIO_CFG_LP_CASE_OPEN      (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL /*DIO_60K_PULL_DOWN*/ | DIO_MODE_GPIO_IN)
#define OTE_1_5_GEN_DIO_CFG_LP_ACCEL_INT      (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_IN)

// Function header
int tdc_hal_dio_set_mode(int pm_mode);

void tdc_hal_dio_set_int_flag_case_lid_open(void);
void tdc_hal_dio_set_int_flag_acc_sensor(void);

void DIO_0_IRQHandler(void);
void DIO_1_IRQHandler(void);

int tdc_hal_dio_configure_normal(void);
int tdc_hal_dio_configure_sleep(void);

#endif  // __tdc_hal_dio_h__
