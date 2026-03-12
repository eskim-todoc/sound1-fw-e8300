/**
 * @file snd1_qcc_control_pins.h
 */

#ifndef __snd1_qcc_control_pins_h__
#define __snd1_qcc_control_pins_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#define SND1_QCC_SHUTDOWN_ACTIVE_LEVEL 0
#define SND1_QCC_ISD_CONN_ACTIVE_LEVEL 1

#define SND1_PIN_QCC_CTRL  DIO24  // V0.35 Net Name: QCC_CTRL
#define SND1_PIN_ISD_CHECK DIO13  // V0.35 ISD Check

#define SND1_PIN_GPIO_OUT_CFG (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT)

void snd1_qcc_init_control_pins(void);
void snd1_qcc_isd_connection(bool connection);
void snd1_qcc_shutdown(bool shutdown);

#endif  // __snd1_qcc_control_pins_h__
