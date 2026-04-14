/**
 * @file FrequencyAnalysis.h
 */

#ifndef __FrequencyAnalysis_h__
#define __FrequencyAnalysis_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <agc.h>
#include <definitionsForAlgorithm.h>
#include <system_control.h>
#include <microcode.h>

#define DMA7_CFG0_FOR_FFT_MEM_COPY                                                                                                     \
    (DMA_LITTLE_ENDIAN | SRC_TRANS_LENGTH_SEL | DMA_PRIORITY_1 | DMA_SRC_ALWAYS_ON | DMA_DEST_ALWAYS_ON | WORD_SIZE_24BITS_TO_24BITS | \
     DMA_SRC_ADDR_DECR_1 | DMA_DEST_ADDR_INCR_1 | DMA_SRC_ADDR_LSB_TOGGLE_DISABLE | DMA_DEST_ADDR_LSB_TOGGLE_DISABLE |                 \
     DMA_CNT_INT_DISABLE | DMA_COMPLETE_INT_ENABLE)

#define DMA7_CTRL_FOR_FFT_MEM_COPY (DMA_DISABLE | DMA_CLEAR_CNTS | DMA_CLEAR_BUFFER)

#define DMA7_STATUS_FOR_FFT_MEM_COPY (DMA_COMPLETE_INT_CLEAR | DMA_CNT_INT_CLEAR)

#define HEAR_FIFO_MEM_LSB_UNSIGNED_BASE 0x20180000
#define HEAR_A1MEM_LSB_UNSIGNED_BASE    (HEAR_FIFO_MEM_LSB_UNSIGNED_BASE + 0x28000)

#define SRC_ADDR_FOR_TOP_OF_HCT_A1_0 (HEAR_A1MEM_LSB_UNSIGNED_BASE + ((HCT_FIFO_A1_0_START + FFT_SIZE - 1) << 2))
#define DST_ADDR_FOR_FFT_inputBuff \
    (HEAR_A1MEM_LSB_UNSIGNED_BASE + ((AudioBuff_FIFO_START_Addr_Bias + AudioBuff_FIFO_Size + (AudioBuff_FIFO_Size / 2)) << 2))

extern int chess_storage(XMEM) g_pass_bin_index[HALF_FFT_SIZE];
extern int chess_storage(XMEM) g_freq_rep_values_scaled[df_MaxNumOfElectrode];
extern int chess_storage(XMEM) g_freq_rep_values[df_MaxNumOfElectrode];

void update_FFT_inputData(void);

void find_freq_rep_value(void);

void read_FFT_PassBin_index(void);

#endif // __FrequencyAnalysis_h__
