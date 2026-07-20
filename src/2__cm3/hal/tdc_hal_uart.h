/**
 * @file OTE_1_5gen_UART.h
 */

#ifndef __tdc_hal_uart_h__
#define __tdc_hal_uart_h__

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <processorDirective.h>
#include <definitionsForAlgorithm.h>

#include <board.h>

#define TDC_HAL_UART_DIO_INIT_CFG   (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL)
#define TDC_HAL_UART_DIO_UNINIT_CFG (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_250K_PULL_UP | DIO_MODE_DISABLE)

#define TDC_HAL_UART_DIO_TX DIO_PIN_INDEX_forUART_TX
#define TDC_HAL_UART_DIO_RX DIO_PIN_INDEX_forUART_RX

#define TDC_HAL_UART_TX_BUF_LEN 256

#define TDC_HAL_UART_BAUDRATE_115200 115200
#define TDC_HAL_UART_BAUDRATE_921600 921600

#define TDC_HAL_UART_BAUDRATE TDC_HAL_UART_BAUDRATE_921600

#define TDC_HAL_UART_CONFIG                                                                                                                                         \
    (UART_TX_DMA_DISABLE | UART_RX_DMA_DISABLE | UART_TX_END_INT_DISABLE | UART_TX_START_INT_DISABLE | UART_RX_INT_DISABLE | UART_OVERRUN_INT_DISABLE)

#define FS_MEM_UART_BUF_LEN 256

#define FS_MEM_UART_STATE_RESET 0x00
#define FS_MEM_UART_STATE_INIT  0x11
#define FS_MEM_UART_STATE_IDLE  0x22
#define FS_MEM_UART_STATE_CM3   0x33
#define FS_MEM_UART_STATE_CFX   0x44

#define FS_MEM_DISABLE_CFX 0x00
#define FS_MEM_ENABLE_CFX  0xAA

typedef struct
{
    int use_bypass_data_reg;
    int use_custom_adc_config;
    int use_custom_dec_gain_factor;
    int use_dec_dc_remove;

    int adc_sel;
    int adc_mode;
    int adc_ref;
    int adc_ain_rbias;

    int dec_gain_factor;

} DebugMode_t;

#define FS_MEM_DEBUG_AGC_STATE_IDLE        0
#define FS_MEM_DEBUG_AGC_STATE_INPUT_DONE  1
#define FS_MEM_DEBUG_AGC_STATE_CALCULATING 2
#define FS_MEM_DEBUG_AGC_STATE_OUTPUT_DONE 3
#define FS_MEM_DEBUG_AGC_STATE_PRINTING    4

typedef struct
{
    int state;
    int audio_volume;
    int INT__agc_input;
    int QI08F16_INT__agc_input_db;
    int QI12F12_INT__agc_input_db;
    int QI08F16_INT__agc_gain_db;
    int QI12F12_INT__agc_gain_linear;
} debug_agc_t;

typedef struct
{
    int state;
    int flag[32];
    int buffer[512];
#if 0
    int enableCFX;

    int print_flag_FIFO_A0_0;
    int FIFO_A0_0[32];

    int print_flag_inputAudio_Mix;
    int inputAudio_Mix[df_inputADC_DataBuffLength];

    int print_flag_CIS_result;
    int freqBandOrder[32];
    int electrodIndex[32];
    int stimulusLevel[32];

    int print_flag_inputAudioMix_MaxValue;
    int inputAudioMix_MaxValue;

    int print_flag_agcFunction_result;
    int m_fixedVolume_QI8F16_db_rotationPoint_inputPoint;
    int m_fixedVolume_QI8F16_attenuationRegion_Slope;
    int m_fixedVolume_QI8F16_db_attenuationRegion_yBias;
    int m_fixedVolume_QI8F16_db_noiseGate_inputPoint;
    int m_fixedVolume_QI8F16_noiseRegion_Slope;
    int m_fixedVolume_QI8F16_db_noiseRegion_yBias;
    int m_fixedVolume_QI8F16_db_amplifyRegion_yBias;
    int QI8F16_agc_input_db;
    int QI8F16_agc_output_db;
    int QI8F16_target_gain_db;
    int QI1F23_pwr_output;
    int QI12F12_linear_gain;

    int print_flag_agcApply;
    int agcOutData[df_inputADC_DataBuffLength];

    int print_flag_vMagResult;
    int vMag[256];

    int print_flag_channelRepresentiveValue;
    int channelRepresentiveValue[32];

    int test_cfx_pwr_to_dB;
    int test_cfx_dB_to_pwr_H;
    int test_cfx_dB_to_pwr_L;

    DebugMode_t debugMode;
    debug_agc_t debug_agc;
#endif
} FS_MEM_UART_T;

int  tdc_hal_uart_uninit(void);
int  tdc_hal_uart_printf(const char* p_fmt, ...);

#define FS_MEM_UART ((volatile FS_MEM_UART_T*) DSP_PRAM1_REMAP_BASE)

#endif  // __tdc_hal_uart_h__
