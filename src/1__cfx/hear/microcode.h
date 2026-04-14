// ---------------------------------------------------
// HEAR Configuration Tool, ON Semiconductor
// 
// Generated file - DO NOT EDIT!
// Created from ../microcode.hct
// ---------------------------------------------------
#ifndef HCT_HEAR_MICROCODE_INCLUDED
#define HCT_HEAR_MICROCODE_INCLUDED

//
// User Defined Constants
//
#define HEAR_FFT_SIZE                      (512)
#define HEAR_HALF_FFT_SIZE                 (256)
#define HEAR_AUDIO_FIFO_SIZE               (32)
#define HEAR_AUDIO_FIFO_BLK_SIZE           (16)
#define HEAR_AUDIO_MIX_SIZE                (16)
#define HEAR_MIC0_FIFO_START_ADDR_BIAS     (0)
#define HEAR_MIC0_FIFO_SIZE                (HEAR_AUDIO_FIFO_SIZE)
#define HEAR_MIC0_FIFO_BLK_SIZE            (HEAR_AUDIO_FIFO_BLK_SIZE)
#define HEAR_MIC0_FIFO_BASE_PTR            (0)
#define HEAR_MIC0_FIFO_IOBLK_PTR           (0)
#define HEAR_MIC1_FIFO_START_ADDR_BIAS     (32)
#define HEAR_MIC1_FIFO_SIZE                (HEAR_AUDIO_FIFO_SIZE)
#define HEAR_MIC1_FIFO_BLK_SIZE            (HEAR_AUDIO_FIFO_BLK_SIZE)
#define HEAR_MIC1_FIFO_BASE_PTR            (0)
#define HEAR_MIC1_FIFO_IOBLK_PTR           (0)
#define HEAR_DAC0_FIFO_START_ADDR_BIAS     (64)
#define HEAR_DAC0_FIFO_SIZE                (HEAR_AUDIO_FIFO_SIZE)
#define HEAR_DAC0_FIFO_BLK_SIZE            (HEAR_AUDIO_FIFO_BLK_SIZE)
#define HEAR_DAC0_FIFO_BASE_PTR            (0)
#define HEAR_DAC0_FIFO_IOBLK_PTR           (0)
#define HEAR_DAC1_FIFO_START_ADDR_BIAS     (96)
#define HEAR_DAC1_FIFO_SIZE                (HEAR_AUDIO_FIFO_SIZE)
#define HEAR_DAC1_FIFO_BLK_SIZE            (HEAR_AUDIO_FIFO_BLK_SIZE)
#define HEAR_DAC1_FIFO_BASE_PTR            (0)
#define HEAR_DAC1_FIFO_IOBLK_PTR           (0)
#define HEAR_PCM_FIFO_START_ADDR_BIAS      (128)
#define HEAR_PCM_FIFO_SIZE                 (64)
#define HEAR_PCM_FIFO_BLK_SIZE             (24)
#define HEAR_PCM_FIFO_BASE_PTR             (0)
#define HEAR_PCM_FIFO_IOBLK_PTR            (0)
#define HEAR_I2S_IN_FIFO_START_ADDR_BIAS   (192)
#define HEAR_I2S_IN_FIFO_SIZE              (HEAR_AUDIO_FIFO_SIZE)
#define HEAR_I2S_IN_FIFO_BLK_SIZE          (HEAR_AUDIO_FIFO_BLK_SIZE)
#define HEAR_I2S_IN_FIFO_BASE_PTR          (0)
#define HEAR_I2S_IN_FIFO_IOBLK_PTR         (0)
#define HEAR_I2S_OUT_FIFO_START_ADDR_BIAS  (224)
#define HEAR_I2S_OUT_FIFO_SIZE             (HEAR_AUDIO_FIFO_SIZE)
#define HEAR_I2S_OUT_FIFO_BLK_SIZE         (HEAR_AUDIO_FIFO_BLK_SIZE)
#define HEAR_I2S_OUT_FIFO_BASE_PTR         (0)
#define HEAR_I2S_OUT_FIFO_IOBLK_PTR        (0)
#define HEAR_FFT_FIFO_START_ADDR_BIAS      (0)
#define HEAR_FFT_FIFO_SIZE                 (HEAR_FFT_SIZE)
#define HEAR_FFT_FIFO_BLK_SIZE             (HEAR_AUDIO_FIFO_BLK_SIZE)
#define HEAR_FFT_FIFO_BASE_PTR             (0)
#define HEAR_FFT_FIFO_IOBLK_PTR            (0)
#define HEAR_ADDR_FIFO_MIC0                ((D_HEAR_A0MEM_BASE + HEAR_MIC0_FIFO_START_ADDR_BIAS))
#define HEAR_ADDR_FIFO_MIC1                ((D_HEAR_A0MEM_BASE + HEAR_MIC1_FIFO_START_ADDR_BIAS))
#define HEAR_ADDR_FIFO_DAC0                ((D_HEAR_A0MEM_BASE + HEAR_DAC0_FIFO_START_ADDR_BIAS))
#define HEAR_ADDR_FIFO_DAC1                ((D_HEAR_A0MEM_BASE + HEAR_DAC1_FIFO_START_ADDR_BIAS))
#define HEAR_ADDR_FIFO_PCM                 ((D_HEAR_A0MEM_BASE + HEAR_PCM_FIFO_START_ADDR_BIAS))
#define HEAR_ADDR_FIFO_I2S_IN              ((D_HEAR_A0MEM_BASE + HEAR_I2S_IN_FIFO_START_ADDR_BIAS))
#define HEAR_ADDR_FIFO_I2S_OUT             ((D_HEAR_A0MEM_BASE + HEAR_I2S_OUT_FIFO_START_ADDR_BIAS))
#define HEAR_ADDR_FIFO_FFT                 ((D_HEAR_A1MEM_BASE + HEAR_FFT_FIFO_START_ADDR_BIAS))
#define HEAR_ADDR_FIFO_PCM_WRITING         ((HEAR_ADDR_FIFO_PCM + (HEAR_PCM_FIFO_BLK_SIZE - 1)))
#define HEAR_ADDR_AUDIO_MIX                ((HEAR_ADDR_FIFO_I2S_OUT + HEAR_I2S_OUT_FIFO_SIZE))
#define HEAR_ADDR_FFT_INPUT                (HEAR_ADDR_FIFO_FFT)
#define HEAR_ADDR_FFT_OUTPUT               (D_HEAR_C0MEM_BASE)
#define HEAR_ADDR_FFT_TEMP01               (D_HEAR_H01MEM_BASE)
#define HEAR_ADDR_FFT_TEMP23               (D_HEAR_H23MEM_BASE)
#define HEAR_ADDR_FFT_WINDOW               ((HEAR_ADDR_FFT_OUTPUT + HEAR_FFT_SIZE))
#define HEAR_ADDR_VMAG_OUTPUT              ((HEAR_ADDR_FFT_INPUT + HEAR_FFT_SIZE))
#define HEAR_ADDR_AUDIO_MIX_ABS            (D_HEAR_H4MEM_BASE)
#define HEAR_ADDR_AUDIO_MIX_ABS_MAX_VALUE  (HEAR_ADDR_AUDIO_MIX_ABS)
#define HEAR_LARGEST_FIFO_SIZE             (HEAR_FFT_FIFO_SIZE)



//
// FIFO Configuration
//
#define HCT_FIFO_A0_0_START       (0)
#define HCT_FIFO_A0_0_LENGTH      (32)
#define HCT_FIFO_A0_0_BASE_PTR    (0)
#define HCT_FIFO_A0_0_IOBLOCK_PTR (0)
#define HCT_FIFO_A0_0_BLOCK_SIZE  (16)

#define HCT_FIFO_A0_1_START       (32)
#define HCT_FIFO_A0_1_LENGTH      (32)
#define HCT_FIFO_A0_1_BASE_PTR    (0)
#define HCT_FIFO_A0_1_IOBLOCK_PTR (0)
#define HCT_FIFO_A0_1_BLOCK_SIZE  (16)

#define HCT_FIFO_A0_2_START       (64)
#define HCT_FIFO_A0_2_LENGTH      (32)
#define HCT_FIFO_A0_2_BASE_PTR    (0)
#define HCT_FIFO_A0_2_IOBLOCK_PTR (0)
#define HCT_FIFO_A0_2_BLOCK_SIZE  (16)

#define HCT_FIFO_A0_3_START       (96)
#define HCT_FIFO_A0_3_LENGTH      (32)
#define HCT_FIFO_A0_3_BASE_PTR    (0)
#define HCT_FIFO_A0_3_IOBLOCK_PTR (0)
#define HCT_FIFO_A0_3_BLOCK_SIZE  (16)

#define HCT_FIFO_A0_4_START       (128)
#define HCT_FIFO_A0_4_LENGTH      (64)
#define HCT_FIFO_A0_4_BASE_PTR    (0)
#define HCT_FIFO_A0_4_IOBLOCK_PTR (0)
#define HCT_FIFO_A0_4_BLOCK_SIZE  (24)

#define HCT_FIFO_A0_5_START       (192)
#define HCT_FIFO_A0_5_LENGTH      (32)
#define HCT_FIFO_A0_5_BASE_PTR    (0)
#define HCT_FIFO_A0_5_IOBLOCK_PTR (0)
#define HCT_FIFO_A0_5_BLOCK_SIZE  (16)

#define HCT_FIFO_A0_6_START       (224)
#define HCT_FIFO_A0_6_LENGTH      (32)
#define HCT_FIFO_A0_6_BASE_PTR    (0)
#define HCT_FIFO_A0_6_IOBLOCK_PTR (0)
#define HCT_FIFO_A0_6_BLOCK_SIZE  (16)

#define HCT_FIFO_A1_0_START       (0)
#define HCT_FIFO_A1_0_LENGTH      (512)
#define HCT_FIFO_A1_0_BASE_PTR    (0)
#define HCT_FIFO_A1_0_IOBLOCK_PTR (0)
#define HCT_FIFO_A1_0_BLOCK_SIZE  (16)



//
// FIFO Configuration Macro
//
#define HCT_FIFO_Configure \
     Sys_FIFO_Configure((D_FIFO_Type *) D_FIFO_A0_0, \
                       HCT_FIFO_A0_0_START, \
                       HCT_FIFO_A0_0_LENGTH, \
                       HCT_FIFO_A0_0_BLOCK_SIZE, \
                       HCT_FIFO_A0_0_BASE_PTR, \
                       HCT_FIFO_A0_0_IOBLOCK_PTR); \
    Sys_FIFO_Configure((D_FIFO_Type *) D_FIFO_A0_1, \
                       HCT_FIFO_A0_1_START, \
                       HCT_FIFO_A0_1_LENGTH, \
                       HCT_FIFO_A0_1_BLOCK_SIZE, \
                       HCT_FIFO_A0_1_BASE_PTR, \
                       HCT_FIFO_A0_1_IOBLOCK_PTR); \
    Sys_FIFO_Configure((D_FIFO_Type *) D_FIFO_A0_2, \
                       HCT_FIFO_A0_2_START, \
                       HCT_FIFO_A0_2_LENGTH, \
                       HCT_FIFO_A0_2_BLOCK_SIZE, \
                       HCT_FIFO_A0_2_BASE_PTR, \
                       HCT_FIFO_A0_2_IOBLOCK_PTR); \
    Sys_FIFO_Configure((D_FIFO_Type *) D_FIFO_A0_3, \
                       HCT_FIFO_A0_3_START, \
                       HCT_FIFO_A0_3_LENGTH, \
                       HCT_FIFO_A0_3_BLOCK_SIZE, \
                       HCT_FIFO_A0_3_BASE_PTR, \
                       HCT_FIFO_A0_3_IOBLOCK_PTR); \
    Sys_FIFO_Configure((D_FIFO_Type *) D_FIFO_A0_4, \
                       HCT_FIFO_A0_4_START, \
                       HCT_FIFO_A0_4_LENGTH, \
                       HCT_FIFO_A0_4_BLOCK_SIZE, \
                       HCT_FIFO_A0_4_BASE_PTR, \
                       HCT_FIFO_A0_4_IOBLOCK_PTR); \
    Sys_FIFO_Configure((D_FIFO_Type *) D_FIFO_A0_5, \
                       HCT_FIFO_A0_5_START, \
                       HCT_FIFO_A0_5_LENGTH, \
                       HCT_FIFO_A0_5_BLOCK_SIZE, \
                       HCT_FIFO_A0_5_BASE_PTR, \
                       HCT_FIFO_A0_5_IOBLOCK_PTR); \
    Sys_FIFO_Configure((D_FIFO_Type *) D_FIFO_A0_6, \
                       HCT_FIFO_A0_6_START, \
                       HCT_FIFO_A0_6_LENGTH, \
                       HCT_FIFO_A0_6_BLOCK_SIZE, \
                       HCT_FIFO_A0_6_BASE_PTR, \
                       HCT_FIFO_A0_6_IOBLOCK_PTR); \
    Sys_FIFO_Configure((D_FIFO_Type *) D_FIFO_A1_0, \
                       HCT_FIFO_A1_0_START, \
                       HCT_FIFO_A1_0_LENGTH, \
                       HCT_FIFO_A1_0_BLOCK_SIZE, \
                       HCT_FIFO_A1_0_BASE_PTR, \
                       HCT_FIFO_A1_0_IOBLOCK_PTR); \


//
// FIFO Memory Allocation Macro
//
extern volatile int HCT_A0_0[HCT_FIFO_A0_0_LENGTH];
extern volatile int HCT_A0_1[HCT_FIFO_A0_1_LENGTH];
extern volatile int HCT_A0_2[HCT_FIFO_A0_2_LENGTH];
extern volatile int HCT_A0_3[HCT_FIFO_A0_3_LENGTH];
extern volatile int HCT_A0_4[HCT_FIFO_A0_4_LENGTH];
extern volatile int HCT_A0_5[HCT_FIFO_A0_5_LENGTH];
extern volatile int HCT_A0_6[HCT_FIFO_A0_6_LENGTH];
extern volatile int HCT_A1_0[HCT_FIFO_A1_0_LENGTH];


//
// Initial Function Chain Controller Configuration
// 
#define HEAR_FC_agc_preprocessing HEAR_FC_CMD0_POS
#define HEAR_FC_fft_vmag HEAR_FC_CMD1_POS

#define HCT_HEAR_FC_ENABLE_VAL (HEAR_FC_EBL_CMD0 | \
                                HEAR_FC_EBL_CMD1)
#define HCT_HEAR_FC_PRIORITY_VAL (0)


//
// Shared Memory Addresses
//
extern volatile int *HEAR_FC_agc_preprocessing_abs_K;
extern volatile int *HEAR_FC_agc_preprocessing_abs_input_addr;
extern volatile int *HEAR_FC_agc_preprocessing_abs_output_addr;
extern uint32_t HEAR_FC_agc_preprocessing_entry;
extern volatile int *HEAR_FC_agc_preprocessing_max_K;
extern volatile int *HEAR_FC_agc_preprocessing_max_input_addr;
extern volatile int *HEAR_FC_agc_preprocessing_max_output_addr;
extern uint32_t HEAR_FC_fft_vmag_entry;
extern volatile int *HEAR_FC_fft_vmag_vmag_K;
extern volatile int *HEAR_FC_fft_vmag_vmag_input_addr;
extern volatile int *HEAR_FC_fft_vmag_vmag_output_addr;


#endif // HCT_HEAR_MICROCODE_INCLUDED

