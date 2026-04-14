/**
 * @file lib_PCM.h
 */

#ifndef __lib_PCM_h__
#define __lib_PCM_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#define LIB_PCM_DIO_CFG   (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_250K_PULL_UP)
#define LIB_PCM_DIO_RESET (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_250K_PULL_UP | DIO_MODE_DISABLE)

// clang-format off
#define LIB_PCM_CFG ( PCM_TX_ACK_DATA0       | PCM_RX_ACK_DATA0        | PCM_TX_DMA_DISABLE       \
                    | PCM_RX_DMA_DISABLE     | PCM_TX_IOC_ENABLE       | PCM_RX_IOC_DISABLE       \
                    | PCM_RX_TX_INT_DISABLE  | PCM_OVERRUN_INT_DISABLE | PCM_UNDERRUN_INT_DISABLE \
                    | PCM_SAMPLE_RISING_EDGE | PCM_TX_DATA_ALIGN_LSB   | PCM_RX_DATA_ALIGN_LSB    \
                    | PCM_WORD_SIZE_20       | PCM_BIT_ORDER_MSB_FIRST | PCM_FRAME_ALIGN_FIRST    \
                    | PCM_FRAME_WIDTH_LONG   | PCM_MULTIWORD_2         | PCM_SUBFRAME_DISABLE     \
                    | PCM_SELECT_MASTER )
// clang-format on

void lib_init_PCM(uint32_t dioClk, uint32_t dioFrame, uint32_t dioSero);

void lib_enable_PCM(void);
void lib_disable_PCM(void);

void LIB_PCM_DIOConfig(const PCM_Type *pcm, uint32_t slave, uint32_t cfg, uint32_t clk, uint32_t frame, uint32_t sero, uint32_t clksrc);

#endif // __lib_PCM_h__
