/**
 * @file snd1_qcc_control_pins.h
 */

#ifndef __tdc_qcc_h__
#define __tdc_qcc_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>
#include <tdc_printf.h>

#define TDC_QCC_DEBUG_ENABLE 1

#if TDC_QCC_DEBUG_ENABLE
#define tdc_qcc_printe(fmt, ...) TDC_PRINTF_E("[QCC] " fmt, ##__VA_ARGS__)
#define tdc_qcc_printw(fmt, ...) TDC_PRINTF_W("[QCC] " fmt, ##__VA_ARGS__)
#define tdc_qcc_printi(fmt, ...) TDC_PRINTF_I("[QCC] " fmt, ##__VA_ARGS__)
#define tdc_qcc_printd(fmt, ...) TDC_PRINTF_D("[QCC] " fmt, ##__VA_ARGS__)
#define tdc_qcc_printv(fmt, ...) TDC_PRINTF_V("[QCC] " fmt, ##__VA_ARGS__)
#else
#define tdc_qcc_printe(fmt, ...)
#define tdc_qcc_printw(fmt, ...)
#define tdc_qcc_printi(fmt, ...)
#define tdc_qcc_printd(fmt, ...)
#define tdc_qcc_printv(fmt, ...)
#endif

#define TDC_QCC_DIO_QCC_CTRL  DIO24  // V0.35: NET NAME: QCC_CTRL
#define TDC_QCC_DIO_ISD_CHECK DIO13  // V0.35: NET NAME: ISD_CHECK

#define TDC_QCC_DIO_CFG_GPIO_OUT (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT)

typedef enum
{
    TDC_QCC_MODE_SHUTDOWN = 0,
    TDC_QCC_MODE_NORMAL,
} tdc_qcc_mode_t;

typedef enum
{
    TDC_QCC_ISD_DISCONNECTED = 0,
    TDC_QCC_ISD_CONNECTED,
} tdc_qcc_isd_t;

void tdc_qcc_init(void);
void tdc_qcc_set_isd(tdc_qcc_isd_t state);
void tdc_qcc_set_mode(tdc_qcc_mode_t mode);

#endif  // __tdc_qcc_h__
