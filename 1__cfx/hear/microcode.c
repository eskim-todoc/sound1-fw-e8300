// ---------------------------------------------------
// HEAR Configuration Tool, ON Semiconductor
// 
// Generated file - DO NOT EDIT!
// ---------------------------------------------------
#include "hw.h"
#include "microcode.h"

//
// Memory Allocation
//
volatile int _XADDR(D_HEAR_A0MEM_BASE + HCT_FIFO_A0_0_START) HCT_A0_0[HCT_FIFO_A0_0_LENGTH];
volatile int _XADDR(D_HEAR_A0MEM_BASE + HCT_FIFO_A0_1_START) HCT_A0_1[HCT_FIFO_A0_1_LENGTH];
volatile int _XADDR(D_HEAR_A0MEM_BASE + HCT_FIFO_A0_3_START) HCT_A0_3[HCT_FIFO_A0_3_LENGTH];
volatile int _XADDR(D_HEAR_A0MEM_BASE + HCT_FIFO_A0_4_START) HCT_A0_4[HCT_FIFO_A0_4_LENGTH];
volatile int _XADDR(D_HEAR_A1MEM_BASE + HCT_FIFO_A1_0_START) HCT_A1_0[HCT_FIFO_A1_0_LENGTH];


//
// HEAR Data Offset for CFX
//
#define HEAR_DATA_OFFSET     ((int *) D_HEAR_SHARED_MEM_BASE)
#define HEAR_FC_OFFSET     (D_HEAR_MICROCODE_MEM_BASE )

//
// Shared Memory Definitions 
//
uint32_t HEAR_FC_agc_preprocessing_entry = (HEAR_FC_OFFSET + 0x4);
uint32_t HEAR_FC_fft_vmag_entry = (HEAR_FC_OFFSET + 0x22);
volatile int *HEAR_FC_agc_preprocessing_abs_input_addr = (volatile int *) (HEAR_DATA_OFFSET + 0x1ff3c);
volatile int *HEAR_FC_agc_preprocessing_abs_output_addr = (volatile int *) (HEAR_DATA_OFFSET + 0x1ff3e);
volatile int *HEAR_FC_agc_preprocessing_abs_K = (volatile int *) (HEAR_DATA_OFFSET + 0x1ff40);
volatile int *HEAR_FC_agc_preprocessing_max_input_addr = (volatile int *) (HEAR_DATA_OFFSET + 0x1ff42);
volatile int *HEAR_FC_agc_preprocessing_max_output_addr = (volatile int *) (HEAR_DATA_OFFSET + 0x1ff44);
volatile int *HEAR_FC_agc_preprocessing_max_K = (volatile int *) (HEAR_DATA_OFFSET + 0x1ff45);
volatile int *HEAR_FC_fft_vmag_vmag_input_addr = (volatile int *) (HEAR_DATA_OFFSET + 0x1ff7a);
volatile int *HEAR_FC_fft_vmag_vmag_output_addr = (volatile int *) (HEAR_DATA_OFFSET + 0x1ff7c);
volatile int *HEAR_FC_fft_vmag_vmag_K = (volatile int *) (HEAR_DATA_OFFSET + 0x1ff7e);


