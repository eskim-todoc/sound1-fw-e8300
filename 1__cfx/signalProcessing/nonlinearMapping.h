/**
 * @file nonlinearMapping.h
 */

#ifndef __nonlinearMapping_h__
#define __nonlinearMapping_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <definitionsForAlgorithm.h>

// clang-format off
#include <sk5_cfx_dsp_types.h>            // include 순서 1
#include <sk5_cfx_dsp_math_accelerator.h> // include 순서 2
#include <sk5_cfx_dsp_data_conversions.h> // include 순서 3
#include <sk5_cfx_dsp_type_conversions.h> // include 순서 4
// clang-format on

#include <FrequencyAnalysis.h>
#include <OTE_1_5_gen_UART.h>
#include <shared_memory.h>
#include <system_control.h>

void logarithmMapping(void);
void read_C_T_Level_fromCM3(void);
void calculate_newStimulation_maxLevel(int stimulationVolume);
void calculate_logaritmMapping_coeff(void);
void calculate_logaritmMapping_coeff_with_audioVolume(void);
int  calculate_x_power_p(int x);

extern int chess_storage(XMEM) g_coeff_p_Q1_23;
extern int chess_storage(XMEM) g_normalize_factor_frac48_power_minus_p_Q8_16;

extern int chess_storage(XMEM) g_max_limit_stimulus_amplitude_C_level[df_MaxNumOfElectrode];
extern int chess_storage(XMEM) g_logaritmMapping_coeff_A_Q16_8[df_MaxNumOfElectrode];
extern int chess_storage(XMEM) g_logaritmMapping_coeff_B_Q16_8[df_MaxNumOfElectrode];

extern int chess_storage(XMEM) g_pcm_amplitude_level[df_MaxNumOfElectrode];

#endif  // __nonlinearMapping_h__
