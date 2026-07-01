/**
 * @file main.h
 */

#ifndef __main_h__
#define __main_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <custom_types.h>
#include <hw.h>

#include <definitionsForAlgorithm.h>

#include <interrupt_service_routine.h>

#include <OTE_1_5_gen_UART.h>
#include <audioMixer.h>
#include <microcode.h>
#include <shared_memory.h>
#include <system_control.h>

#include <lib_PCM.h>
#include <lib_audio_in.h>
#include <lib_audio_mixer.h>
#include <lib_i2s.h>

#include <driver_PCM.h>

/* NOTE:
 * 왜 인지, HEAR를 활성화 시켜도 활성화가 안 되는 경우 있음.
 * 이를 위해 Function Chain 명령 시작 전에 지속적으로 HEAR를 활성화 시키도록 코드 구현.
 * HEAR 활성화 코드는 어셈블리 코드 디버깅 결과 1 명령어로 처리되는 것을 확인 하였음.
 * 매번 활성화를 해도 전체 처리 시간에 큰 로드는 없을 것으로 판단함. */
#define USE_PERIODICALLY_ENABLE_HEAR 1

#if USE_PERIODICALLY_ENABLE_HEAR
#define CALL_FUNCTION_CHAIN(FC)                                                                                                                                                                                                                                                                                                \
    SYS_HEAR_FUNCTIONCHAIN_ENABLE(D_HEAR_FC, HCT_HEAR_FC_ENABLE_VAL);                                                                                                                                                                                                                                                          \
    Sys_HEAR_Command(D_HEAR, FC)
#else
#define CALL_FUNCTION_CHAIN(FC) Sys_HEAR_Command(D_HEAR, FC)
#endif

/* Define the system priority value to give priority to the CFX when accessing the FIFOs and shared memory */
#define SYSTEM_PRIORITY_VAL SYSTEM_PRIORITY_CFX

/* Set the memory arbitration order and maximum wait times before high priority access is given */
#define SYSTEM_MEM_ARBITER (CM3_CFX_DSP_PRIORITY | DSP_MAX_WAIT_7_CYCLES | CFX_MAX_WAIT_3_CYCLES | DMA_MAX_WAIT_7_CYCLES | CM3_MAX_WAIT_15_CYCLES)

/*
 * Configuration for the clock
 */

/* Configure ADC sampling to provide the desired sampling frequency, when ADCCLK is 3.84 MHz */
#define SFCR_16K ADC_MODDIV_BY30
#define SFCR_32K ADC_MODDIV_BY15
#define SFCR_48K ADC_MODDIV_BY10

/* Disable ADCs */
#define ADC_CTRL_0_DISABLE_VAL (ADC_DISABLE | ADC_SEL_AI0)
#define ADC_CTRL_1_DISABLE_VAL (ADC_DISABLE | ADC_SEL_AI1)
#define ADC_CTRL_2_DISABLE_VAL (ADC_DISABLE | ADC_SEL_AI2)
#define ADC_CTRL_3_DISABLE_VAL (ADC_DISABLE | ADC_SEL_AI3)

/* Disable output drivers */
#define OUTPUT_CTRL_DISABLE_VAL (OD0_DISABLE | OD1_DISABLE)

/* Disable the DACs and configure all DAC settings with default values */
#define DAC_AO_CTRL_DISABLE_VAL (AO0_UNMUTE | AO0_ATTENUATE_BYPASS | AO0_CURRENT_1_0X | AO0_DISABLE | AO0_LFP_OUT_ACTIVE_12KHZ | AO1_UNMUTE | AO1_ATTENUATE_BYPASS | AO1_CURRENT_1_0X | AO1_DISABLE | AO1_LFP_OUT_ACTIVE_12KHZ | DIFF_OUT_DISABLE | DIFF_OUT_CH0)

/* Disable the DMIC output interface */
#define DMIC_OUTPUT_CTRL_DISABLE_VAL (DMIC0_OUT_DISABLE | DMIC1_OUT_DISABLE | DMIC0_LOW_SRC_CH0 | DMIC0_HIGH_SRC_CH0 | DMIC1_LOW_SRC_CH0 | DMIC1_HIGH_SRC_CH0)

/* Disable the interface between the ADCs and the IOC */
#define IOC_ADC_CFG_DISABLE_VAL (IOC_INPUT_CFG_IN0_NONE | IOC_INPUT_CFG_IN1_NONE | IOC_INPUT_CFG_IN2_NONE | IOC_INPUT_CFG_IN3_NONE)

/* Disable the interface between the PCM input and the IOC */
#define IOC_PCM_CFG_DISABLE_VAL (IOC_INPUT_CFG_PCM_RX0_NONE | IOC_INPUT_CFG_PCM_RX1_NONE)

/* Disable the interface between the output (DACs and PCM) and the IOC */
#define IOC_OUTPUT_CFG_DISABLE_VAL (IOC_OUTPUT_CFG_OUT0_NONE | IOC_OUTPUT_CFG_OUT1_NONE | IOC_OUTPUT_CFG_PCM_TX0_NONE | IOC_OUTPUT_CFG_PCM_TX1_NONE)

///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////

/* Configuration the audio mux to use ADCs, decimation filters as the IOCs' source, IOCs as the interpolation filters' source,
 * and, interpolation filters as the SDMs' source. */
#define AUDIO_MUX_CFG_INPUT_CH_SRC (ADC3_OUT | ADC2_OUT | ADC1_OUT | ADC0_OUT)
#define AUDIO_MUX_CFG_IOC_SRC      (IOC0_SRC3_DEC_FILTER | IOC0_SRC2_DEC_FILTER | IOC0_SRC1_DEC_FILTER | IOC0_SRC0_DEC_FILTER)
#define AUDIO_MUX_CFG_INT_FILTER   (INT_FILTER1_IOC0 | INT_FILTER0_IOC0)
#define AUDIO_MUX_CFG_SDM          (SDM1_INT_FILTER | SDM0_INT_FILTER)

#define AUDIO_MUX_CFG_VAL (AUDIO_MUX_CFG_INPUT_CH_SRC | AUDIO_MUX_CFG_IOC_SRC | AUDIO_MUX_CFG_INT_FILTER | AUDIO_MUX_CFG_SDM)

/* Configuration the ADC_CFG */
#define ADC_CFG_VAL (ADC_FBDAC_DEM_ENABLE | ADC_FBDAC_MAX_DRIVE_64 | ADC_FBDAC_MIN_DRIVE_1 | ADC_RIN_4K57 | ADC_COMP_CURRENT_2UA | ADC_CURRENT_14P5UA)

#define ADC_CTRL_VAL (ADC_ENABLE | ADC_COMP_ENABLE | ADC_MODE_MAX_1P6V | ADC_REF_VSSA | ADC_AIN_RBIAS_VREG)
/* Enable ADC, selecting the desired input */

/* Define decimation filter gain value for each channel to be unity gain.
 * The decimation filter gain value is specified as (desired gain)*10^7
 * and is used by the macro Sys_Calc_Gain_Factor_Val() to calculate the proper
 * value for the decimation filter gain factor. */
#define DF_UNITY_GAIN 10000000

/* Define the decimation filter configuration - enable the decimation filter,
 * unmute the ADC, select the frequency band, set the DC removal cutoff
 * frequency, the sampling delay, the fractional delay and the gain factor */

/* Gain factor given SFCR of 40 (sampling frequency of 24 kHz with ADCCLK 3.84MHz) */
#define SYS_CALC_GF 1979

#define SAMPLE_FRACTIONAL_DELAY 0
#define ADC_FRACTIONAL_DELAY_0  (SAMPLE_FRACTIONAL_DELAY << AUDIO_ADC_DEC_CTRL_DELAY_FRACTIONAL_Pos)
#define ADC_DEC_CTRL_VAL        (BAND_SELECT_ADC_0K_8K | ADC_INTEGER_DELAY_0 | ADC_FRACTIONAL_DELAY_0 | ADC_UNMUTE | ADC_DEC_ENABLE | ADC_DC_REMOVE_CUTOFF_20HZ | SYS_CALC_GF)

#define ADC_DEC_CTRL_1_VAL ADC_DEC_CTRL_0_VAL
#define ADC_DEC_CTRL_2_VAL ADC_DEC_CTRL_0_VAL
#define ADC_DEC_CTRL_3_VAL ADC_DEC_CTRL_0_VAL

#define AUDIO_SDM_CTRL_VAL (QUANT_LEVEL_6 | TOGGLE_OPT_DISABLE | SD_2_LEVEL | CLK_RATIO_4 | XSDM0_ENABLE | OD_DELAY_DISABLE)

/* 1.9990234375 gain for audio output */
#define OUTPUT_GAIN_VAL ((((uint32_t) 0x7FF << AUDIO_OUTPUT_GAIN_OUTPUT0_GAIN_Pos) & AUDIO_OUTPUT_GAIN_OUTPUT0_GAIN_Mask) | (((uint32_t) 0x7FF << AUDIO_OUTPUT_GAIN_OUTPUT1_GAIN_Pos) & AUDIO_OUTPUT_GAIN_OUTPUT1_GAIN_Mask))

/** @brief Disable OD0, Enable OD1 */
#define OUTPUT_CTRL_VAL (OD0_DISABLE | OD1_ENABLE)

/*
 * Configuration for the IOC
 */

// Define the interface between the ADCs input and the FIFOs.

// ADC0 (MIC 0) : FA0_0
// ADC1 (MIC 1) : FA0_1
// ADC2         : NONE
// ADC3         : NONE

#define IOC_ADC_CFG_VAL (IOC_INPUT_CFG_IN0_FA0_0 | IOC_INPUT_CFG_IN1_FA0_1 | IOC_INPUT_CFG_IN2_NONE | IOC_INPUT_CFG_IN3_NONE)

/* Define the interface between the PCM input and the IOC */
#define IOC_PCM_CFG_VAL (IOC_INPUT_CFG_PCM_RX0_NONE | IOC_INPUT_CFG_PCM_RX1_NONE)

// Define the interface between the FIFOs and the PCM out
// DAC0 : FA0_2
// DAC1 : FA0_3
// PCM  : FA0_4
#define IOC_OUTPUT_CFG_VAL (IOC_OUTPUT_CFG_OUT0_FA0_2 | IOC_OUTPUT_CFG_OUT1_FA0_3 | IOC_OUTPUT_CFG_PCM_TX0_FA0_4 | IOC_OUTPUT_CFG_PCM_TX1_FA0_4)

/* Enable double-access mode for the FIFOs.
 * When a single source/destination is used as input/output for a FIFO,
 * double-access mode is recommended. When multiple sources/destinations share
 * the same FIFO, double-access mode must be disabled for proper functionality.
 * (double-access mode not enabled for PCM out) */
#define IOC_FIFO_ACCESS_VALUE (FIFO_A0_0_DBL_ACC_EN | FIFO_A0_1_DBL_ACC_EN | FIFO_A0_2_DBL_ACC_EN | FIFO_A0_3_DBL_ACC_EN | FIFO_A0_5_DBL_ACC_EN | FIFO_A0_6_DBL_ACC_EN)

/*
 * Configuration for the PCM
 */

/* Define the PCM DIO configuration */
#define OTE1_5GEN_PCM_DIO_CFG   (DIO_8X_DRIVE | DIO_LPF_DISABLE | DIO_1K_PULL_UP)
#define OTE1_5GEN_PCM_DIO_RESET (DIO_8X_DRIVE | DIO_LPF_DISABLE | DIO_250K_PULL_UP | DIO_MODE_DISABLE)

// PCM 관련 DIO 번호
#define DIO_PCM_CLK   DIO34  // NC
#define DIO_PCM_FRAME DIO5   // NET: PCM_FRAME
#define DIO_PCM_MOSI  DIO6   // NET: PCM_DATA

// I2S 관련 DIO 번호
#define DIO_I2S_CLK   DIO25  // NET: I2S_CLK
#define DIO_I2S_FRAME DIO19  // NET: I2S_FRAME
#define DIO_I2S_MISO  DIO14  // NET: I2S_SERO
#define DIO_I2S_FLAG  DIO29  // NET: I2S_FLAG

/* Define the PCM configuration */
#define OTE1_5GEN_PCM_CFG                                                                                                                                                                                                                                                                                                      \
    (PCM_TX_ACK_DATA0 | PCM_RX_ACK_DATA0 | PCM_TX_DMA_DISABLE | PCM_RX_DMA_DISABLE | PCM_TX_IOC_ENABLE | PCM_RX_IOC_DISABLE | PCM_RX_TX_INT_DISABLE | PCM_OVERRUN_INT_DISABLE | PCM_UNDERRUN_INT_DISABLE | PCM_SAMPLE_RISING_EDGE | PCM_TX_DATA_ALIGN_LSB | PCM_RX_DATA_ALIGN_LSB | PCM_WORD_SIZE_20 | PCM_BIT_ORDER_MSB_FIRST \
     | PCM_FRAME_ALIGN_FIRST | PCM_FRAME_WIDTH_LONG | PCM_MULTIWORD_2 | PCM_SUBFRAME_DISABLE | PCM_SELECT_MASTER)

#define EARPIECE_DETECT_ACTIVE_LEVEL 1

/*
 * function.
 */
void normal_init(void);
void normal_loop(void);

void normal(void);
void standby(void);

void PCM_LiveStimulation_Mode(void);
void HEAR_LiveStimulation_Mode(void);

void tdc_copy_DMIC_buffers(void);

/* extern variable */
extern volatile int _XMEM g_pcm_mode;

#endif // __main_h__
