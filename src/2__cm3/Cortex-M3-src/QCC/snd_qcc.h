/**
 * @file snd1_qcc_control_pins.h
 */

#ifndef __snd_qcc_h__
#define __snd_qcc_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>
#include <ci_printf.h>

#define SND_QCC_DEBUG_ENABLE 1

#if SND_QCC_DEBUG_ENABLE
#define snd_qcc_printe(fmt, ...) ci_printe("[QCC] " fmt, ##__VA_ARGS__)
#define snd_qcc_printw(fmt, ...) ci_printw("[QCC] " fmt, ##__VA_ARGS__)
#define snd_qcc_printi(fmt, ...) ci_printi("[QCC] " fmt, ##__VA_ARGS__)
#define snd_qcc_printd(fmt, ...) ci_printd("[QCC] " fmt, ##__VA_ARGS__)
#define snd_qcc_printv(fmt, ...) ci_printv("[QCC] " fmt, ##__VA_ARGS__)
#else
#define snd_qcc_printe(fmt, ...)
#define snd_qcc_printw(fmt, ...)
#define snd_qcc_printi(fmt, ...)
#define snd_qcc_printd(fmt, ...)
#define snd_qcc_printv(fmt, ...)
#endif

#define SND_QCC_DIO_QCC_CTRL  DIO24  // V0.35: NET NAME: QCC_CTRL
#define SND_QCC_DIO_ISD_CHECK DIO13  // V0.35: NET NAME: ISD_CHECK

#define SND_QCC_DIO_CFG_GPIO_OUT (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT)

typedef enum
{
    SND_QCC_MODE_SHUTDOWN = 0,
    SND_QCC_MODE_NORMAL,
} snd_qcc_mode_t;

typedef enum
{
    SND_QCC_ISD_DISCONNECTED = 0,
    SND_QCC_ISD_CONNECTED,
} snd_qcc_isd_t;

void snd_qcc_init(void);
void snd_qcc_set_isd(snd_qcc_isd_t state);
void snd_qcc_set_mode(snd_qcc_mode_t mode);

#endif  // __snd_qcc_h__
