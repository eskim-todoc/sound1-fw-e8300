/**
 * @file lib_i2s.h
 */

#ifndef __lib_i2s_h__
#define __lib_i2s_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>
#include <microcode.h>

#include <shared_memory.h>

#include <lib_i2s_bridge.h>

#define LIB_I2S_STATE_RESET    0
#define LIB_I2S_STATE_ENABLED  1
#define LIB_I2S_STATE_DISABLED 2

#define I2S_BUFFER_STATE_INIT     0
#define I2S_BUFFER_STATE_READY    1
#define I2S_BUFFER_STATE_UNDERRUN 2

#define I2S_BUFFER_SUB_STATE_FADE_IN_STEP_0 0
#define I2S_BUFFER_SUB_STATE_FADE_IN_STEP_1 1
#define I2S_BUFFER_SUB_STATE_FADE_IN_STEP_2 2
#define I2S_BUFFER_SUB_STATE_FADE_IN_STEP_3 3
#define I2S_BUFFER_SUB_STATE_FADE_IN_STEP_4 4
#define I2S_BUFFER_SUB_STATE_FADE_IN_DONE   5

#define I2S_BUFFER_SUB_STATE_FADE_OUT_STEP_0 6
#define I2S_BUFFER_SUB_STATE_FADE_OUT_STEP_1 7
#define I2S_BUFFER_SUB_STATE_FADE_OUT_STEP_2 8
#define I2S_BUFFER_SUB_STATE_FADE_OUT_STEP_3 9
#define I2S_BUFFER_SUB_STATE_FADE_OUT_STEP_4 10
#define I2S_BUFFER_SUB_STATE_FADE_OUT_DONE   11

#define I2S_BUFFER_UNDERRUN_CNT   4
#define I2S_BUFFER_FULL_READY_CNT (I2S_BUFFER_UNDERRUN_CNT + I2S_BUFFER_UNDERRUN_CNT)

#define Q24_MAX ((long) 0x7FFFFF)
#define Q24_MIN (-(long) 0x800000)

// NPP(PCM) : IOC0, PCM0
// I2S      : IOC1, PCM1

#define LIB_I2S_IOC D_IOC1
#define LIB_I2S     PCM1

#define LIB_I2S_ENABLE_CHECK_CNT  10
#define LIB_I2S_DISABLE_CHECK_CNT 10
#define LIB_I2S_DATA_BUF_LEN      16

#define I2S_IOC_INPUT_CFG_NONE  (IOC_INPUT_CFG_IN0_NONE | IOC_INPUT_CFG_IN1_NONE | IOC_INPUT_CFG_IN2_NONE | IOC_INPUT_CFG_IN3_NONE)
#define I2S_IOC_OUTPUT_CFG_NONE (IOC_OUTPUT_CFG_OUT0_NONE | IOC_OUTPUT_CFG_OUT1_NONE | IOC_OUTPUT_CFG_PCM_TX0_NONE | IOC_OUTPUT_CFG_PCM_TX1_NONE)
#define I2S_IOC_PCM_CFG_NONE    (IOC_INPUT_CFG_PCM_RX0_NONE | IOC_INPUT_CFG_PCM_RX1_NONE)

#define I2S_IOC_OUTPUT_CFG_VAL (IOC_OUTPUT_CFG_OUT0_NONE | IOC_OUTPUT_CFG_OUT1_NONE | IOC_OUTPUT_CFG_PCM_TX0_NONE | IOC_OUTPUT_CFG_PCM_TX1_NONE)
#define I2S_IOC_PCM_CFG_VAL    (IOC_INPUT_CFG_PCM_RX0_FA0_5 | IOC_INPUT_CFG_PCM_RX1_NONE)

#define LIB_I2S_DIO_CFG       (DIO_4X_DRIVE | DIO_LPF_ENABLE | DIO_60K_PULL_UP)
#define LIB_I2S_DIO_CFG_RESET (DIO_8X_DRIVE | DIO_LPF_DISABLE | DIO_250K_PULL_UP | DIO_MODE_DISABLE)

// clang-format off
#define LIB_I2S_CFG ( PCM_TX_ACK_DATA0       | PCM_RX_ACK_DATA0        | PCM_TX_DMA_DISABLE       \
                    | PCM_RX_DMA_DISABLE     | PCM_TX_IOC_DISABLE      | PCM_RX_IOC_ENABLE        \
                    | PCM_RX_TX_INT_DISABLE  | PCM_OVERRUN_INT_DISABLE | PCM_UNDERRUN_INT_DISABLE \
                    | PCM_SAMPLE_RISING_EDGE | PCM_TX_DATA_ALIGN_MSB   | PCM_RX_DATA_ALIGN_MSB    \
                    | PCM_WORD_SIZE_24       | PCM_BIT_ORDER_MSB_FIRST | PCM_FRAME_ALIGN_LAST     \
                    | PCM_FRAME_WIDTH_LONG   | PCM_MULTIWORD_2         | PCM_SUBFRAME_DISABLE     \
                    | PCM_SELECT_SLAVE )
// clang-format on

#define I2S_FLAG_ACTIVE_LEVEL 0
#define I2S_FLAG_DIO_NUM      DIO29
#define I2S_FLAG_DIO_CFG      (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_WEAK_PULL_UP | DIO_MODE_GPIO_IN)

void lib_init_I2S(uint32_t clk_dio, uint32_t frame_dio, uint32_t seri_dio, uint32_t flag_dio);

void LIB_I2S_DIOConfig(const PCM_Type *pcm, /**/
                       uint32_t        slave,
                       uint32_t        cfg,
                       uint32_t        clk,
                       uint32_t        frame,
                       uint32_t        seri,
                       uint32_t        clksrc);

void lib_enable_I2S(void);
void lib_i2s_disable(void);

int  lib_i2s_is_buffer_copy_done(void);
void lib_i2s_clear_buffer_copy_done(void);

void tdc_i2s_set_streaming_state(int state);
int  I2S_isStreaming(void);
void I2S_update_state(void);

void lib_i2s_copy_data_from_fifo(int _XMEM *p_fifo);
void lib_i2s_clear_data(void);

int _XMEM *I2S_get_buffer_with_fade_in_process(void);
int _XMEM *I2S_get_buffer_with_fade_out_process(void);

// extern variables
extern volatile int _XMEM lib_g_i2s_interrupt_flag;
extern volatile int _XMEM lib_g_i2s_interrupt_cnt;

extern int _XMEM lib_g_i2s_buffer_state;
extern int _XMEM lib_g_i2s_buffer_sub_state;

extern int _XMEM          lib_g_i2s_buffer_copy_cnt;
extern volatile int _XMEM lib_g_i2s_prev1_offset;

extern volatile int _XMEM lib_g_i2s_click_occurred;
extern volatile int _XMEM lib_g_i2s_last_output_val;

extern int _XMEM lib_g_i2s_buffer_in_pos;
extern int _XMEM lib_g_i2s_buffer_out_pos;

extern int _XMEM lib_g_i2s_buffers[I2S_BUFFER_FULL_READY_CNT][16];

extern int _XMEM lib_g_i2s_buffer[LIB_I2S_DATA_BUF_LEN];
extern int _XMEM lib_g_i2s_buffer_prev1[LIB_I2S_DATA_BUF_LEN];
extern int _XMEM lib_g_i2s_buffer_prev2[LIB_I2S_DATA_BUF_LEN];
extern int _XMEM lib_g_i2s_buffer_output[LIB_I2S_DATA_BUF_LEN];

extern volatile int _XMEM lib_g_i2s_buffer_ready;

#endif  // __lib_i2s_h__
