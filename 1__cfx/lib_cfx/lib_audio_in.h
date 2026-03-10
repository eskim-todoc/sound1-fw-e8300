/**
 * @file lib_audio_in.h
 */

#ifndef __lib_audio_in_h__
#define __lib_audio_in_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <microcode.h>

// ADC 샘플링 주파수 설정 (ADCCLK : 3.84 MHz)
#define LIB_AUDIO_SFCR_16K ADC_MODDIV_BY30
#define LIB_AUDIO_SFCR_32K ADC_MODDIV_BY15
#define LIB_AUDIO_SFCR_48K ADC_MODDIV_BY10

#define LIB_AUDIO_IN_PATH_ADC  0
#define LIB_AUDIO_IN_PATH_DMIC 1

/* Disable ADCs */
#define LIB_ADC_CTRL_0_DISABLE_VAL (ADC_DISABLE | ADC_SEL_AI0)
#define LIB_ADC_CTRL_1_DISABLE_VAL (ADC_DISABLE | ADC_SEL_AI1)
#define LIB_ADC_CTRL_2_DISABLE_VAL (ADC_DISABLE | ADC_SEL_AI2)
#define LIB_ADC_CTRL_3_DISABLE_VAL (ADC_DISABLE | ADC_SEL_AI3)

/* Disable output drivers */
#define LIB_OUTPUT_CTRL_DISABLE_VAL (OD0_DISABLE | OD1_DISABLE)

/* Disable the interface between the ADCs and the IOC */
#define LIB_IOC_ADC_CFG_DISABLE_VAL (IOC_INPUT_CFG_IN0_NONE | IOC_INPUT_CFG_IN1_NONE | IOC_INPUT_CFG_IN2_NONE | IOC_INPUT_CFG_IN3_NONE)

/* Disable the interface between the PCM input and the IOC */
#define LIB_IOC_PCM_CFG_DISABLE_VAL (IOC_INPUT_CFG_PCM_RX0_NONE | IOC_INPUT_CFG_PCM_RX1_NONE)

/* Disable the interface between the output (DACs and PCM) and the IOC */
#define LIB_IOC_OUTPUT_CFG_DISABLE_VAL (IOC_OUTPUT_CFG_OUT0_NONE | IOC_OUTPUT_CFG_OUT1_NONE | IOC_OUTPUT_CFG_PCM_TX0_NONE | IOC_OUTPUT_CFG_PCM_TX1_NONE)

/* Define the interface between the ADCs input and the FIFOs. */
//                           DMIC Left (Rising)        AMIC (Earpiece)
#define LIB_IOC_ADC_CFG_VAL (IOC_INPUT_CFG_IN0_FA0_0 | IOC_INPUT_CFG_IN1_FA0_1 | IOC_INPUT_CFG_IN2_NONE | IOC_INPUT_CFG_IN3_NONE)

/* Define the interface between the PCM input and the IOC */
#define LIB_IOC_PCM_CFG_VAL (IOC_INPUT_CFG_PCM_RX0_NONE | IOC_INPUT_CFG_PCM_RX1_NONE)

/* Enable double-access mode for the FIFOs.
 * When a single source/destination is used as input/output for a FIFO,
 * double-access mode is recommended. When multiple sources/destinations share
 * the same FIFO, double-access mode must be disabled for proper functionality.
 * (double-access mode not enabled for PCM out) */
#define LIB_IOC_FIFO_ACCESS_VALUE (FIFO_A0_0_DBL_ACC_EN | FIFO_A0_1_DBL_ACC_EN | FIFO_A0_3_DBL_ACC_EN)

// Define the interface between the FIFOs and the PCM out
//                                                         DAC1 (FA0_3)                PCM0 TX0 (FA0_4)               PCM0 TX1 (FA0_4)
#define LIB_IOC_OUTPUT_CFG_VAL (IOC_OUTPUT_CFG_OUT0_NONE | IOC_OUTPUT_CFG_OUT1_FA0_3 | IOC_OUTPUT_CFG_PCM_TX0_FA0_4 | IOC_OUTPUT_CFG_PCM_TX1_FA0_4)

/* Configuration the audio mux to use ADCs, decimation filters as the IOCs' source, IOCs as the interpolation filters' source,
 * and, interpolation filters as the SDMs' source. */
#define LIB_ADC_AUDIO_MUX_CFG_INPUT_CH_SRC (ADC3_OUT | ADC2_OUT | ADC1_OUT | ADC0_OUT)
#define LIB_ADC_AUDIO_MUX_CFG_IOC_SRC      (IOC0_SRC3_DEC_FILTER | IOC0_SRC2_DEC_FILTER | IOC0_SRC1_DEC_FILTER | IOC0_SRC0_DEC_FILTER)
#define LIB_ADC_AUDIO_MUX_CFG_INT_FILTER   (INT_FILTER1_IOC0 | INT_FILTER0_IOC0)
#define LIB_ADC_AUDIO_MUX_CFG_SDM          (SDM1_INT_FILTER | SDM0_INT_FILTER)

#define LIB_ADC_AUDIO_MUX_CFG_VAL                                                                                                                              \
    (LIB_ADC_AUDIO_MUX_CFG_INPUT_CH_SRC | LIB_ADC_AUDIO_MUX_CFG_IOC_SRC | LIB_ADC_AUDIO_MUX_CFG_INT_FILTER | LIB_ADC_AUDIO_MUX_CFG_SDM)

/* Configuration the audio mux to use DMICs, decimation filters as the IOCs' source,
 * IOCs as the interpolation filters' source, and, interpolation filters as the SDMs' source. */
#define LIB_DMIC_AUDIO_MUX_CFG_INPUT_CH_SRC (ADC3_OUT | ADC2_OUT | ADC1_OUT | DMIC0_DATA_RE)
#define LIB_DMIC_AUDIO_MUX_CFG_IOC_SRC      (IOC0_SRC3_DEC_FILTER | IOC0_SRC2_DEC_FILTER | IOC0_SRC1_DEC_FILTER | IOC0_SRC0_DEC_FILTER)
#define LIB_DMIC_AUDIO_MUX_CFG_INT_FILTER   (INT_FILTER1_IOC0 | INT_FILTER0_IOC0)
#define LIB_DMIC_AUDIO_MUX_CFG_SDM          (SDM1_INT_FILTER | SDM0_INT_FILTER)
#define LIB_DMIC_AUDIO_MUX_CFG_VAL                                                                                                                             \
    (LIB_DMIC_AUDIO_MUX_CFG_INPUT_CH_SRC | LIB_DMIC_AUDIO_MUX_CFG_IOC_SRC | LIB_DMIC_AUDIO_MUX_CFG_INT_FILTER | LIB_DMIC_AUDIO_MUX_CFG_SDM)

/* Configuration the ADC_CFG */
#define LIB_ADC_CFG_VAL (ADC_FBDAC_DEM_ENABLE | ADC_FBDAC_MAX_DRIVE_64 | ADC_FBDAC_MIN_DRIVE_1 | ADC_RIN_4K57 | ADC_COMP_CURRENT_2UA | ADC_CURRENT_14P5UA)

/* Define the decimation filter configuration - enable the decimation filter,
 * unmute the ADC, select the frequency band, set the DC removal cutoff
 * frequency, the sampling delay, the fractional delay and the gain factor */

/* Gain factor given SFCR of 40 (sampling frequency of 24 kHz with ADCCLK 3.84MHz) */
#define LIB_SYS_CALC_GF 1092  // 1092//1979

#define LIB_SAMPLE_FRACTIONAL_DELAY 0
#define LIB_ADC_FRACTIONAL_DELAY_0  (LIB_SAMPLE_FRACTIONAL_DELAY << AUDIO_ADC_DEC_CTRL_DELAY_FRACTIONAL_Pos)
#define LIB_ADC_DEC_CTRL_VAL                                                                                                                                   \
    (BAND_SELECT_ADC_0K_8K | ADC_INTEGER_DELAY_0 | LIB_ADC_FRACTIONAL_DELAY_0 | ADC_UNMUTE | ADC_DEC_ENABLE | ADC_DC_REMOVE_CUTOFF_80HZ | LIB_SYS_CALC_GF)

#define LIB_ADC_CTRL_VAL (ADC_ENABLE | ADC_COMP_ENABLE | ADC_MODE_MAX_1P6V | ADC_REF_VSSA | ADC_AIN_RBIAS_VREG)

// 함수

void disable_audio_path_all(void);
void configure_FIFO_all(void);
void configure_audio_IOC(void);
void configure_audio_path_all(void);
void enable_AMIC(void);
void disable_AMIC(void);
void enable_DMIC(void);
void disable_DMIC(void);
void reset_FFT_FIFO(void);
void clear_FIFO(volatile int _XMEM *p_fifo, int len);
void clear_FIFO_all(void);
void disable_FIFO_all(void);

#endif  // __lib_audio_in_h__
