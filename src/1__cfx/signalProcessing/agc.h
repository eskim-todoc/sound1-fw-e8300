/**
 * @file agc.h
 */

#ifndef __agc_h__
#define __agc_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

// clang-format off
#include <sk5_cfx_dsp_types.h>            // include 순서 1
#include <sk5_cfx_dsp_math_accelerator.h> // include 순서 2
#include <sk5_cfx_dsp_data_conversions.h> // include 순서 3
#include <sk5_cfx_dsp_type_conversions.h> // include 순서 4
// clang-format on

#include <OTE_1_5_gen_UART.h>
#include <definitionsForAlgorithm.h>
#include <microcode.h>
#include <shared_memory.h>

void audio_agc(void);

int audio_agc_attack_release(int current_agc_10db_gain);
int fn_agc_function_fixedVolume(int input_max_value);

int  calculate_agc_gain(int INT__agc_input);
void apply_agc_gain(int agc_gain_linear);

int get_QI8F16_dB(int pwr);
int get_QI1F23_pwr(int QI8F16_dB);

int get_QI1F23_linear_gain(int QI8F16_dB);
int get_QI1F23_exp2(int in);

extern int chess_storage(XMEM) m_attenuation_region_ybias_db_Q8_16[10];
extern int chess_storage(XMEM) m_amplify_region_ybias_db_Q8_16[10];
extern int chess_storage(XMEM) m_noise_region_ybias_db_Q12_12[10];
extern int chess_storage(XMEM) m_rotation_point_db_Q8_16;
extern int chess_storage(XMEM) m_noise_gate_db_Q8_16;
extern int chess_storage(XMEM) m_attenuation_region_slope_Q8_16[10];
extern int chess_storage(XMEM) m_noise_region_slope_Q8_16;
extern int chess_storage(XMEM) m_attack_coeff_Q8_16;
extern int chess_storage(XMEM) m_one_minus_attack_coeff_Q8_16;
extern int chess_storage(XMEM) m_release_coeff_Q8_16;
extern int chess_storage(XMEM) m_one_minus_release_coeff_Q8_16;
extern int chess_storage(XMEM) m_mathlib_gain_db_Q8_16;
extern int chess_storage(XMEM) m_mathlib_gain_linear_Q12_12;
extern int chess_storage(XMEM) m_prev_agc_10db_gain_Q8_16;
extern int chess_storage(XMEM) m_audio_volume;

extern int chess_storage(XMEM) m_agc_output_buffer[df_inputADC_DataBuffLength];

#endif // __agc_h__
