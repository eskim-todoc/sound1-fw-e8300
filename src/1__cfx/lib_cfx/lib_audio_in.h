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

// EZ와 QCC가 마이크를 공유하기 때문에,
// 아직 개발 중인 현 시점에서는 EZ가 마이크 몇 개를 사용할 것인지 설정이 필요함 (2026.03.10)
#define LIB_AUDIO_IN_DMIC_ENABLE_COUNT 2  // 1

// 앞 방향 마이크 정보
#define LIB_AUDIO_FRONT_MIC_NONE  0
#define LIB_AUDIO_FRONT_MIC_LEFT  1
#define LIB_AUDIO_FRONT_MIC_RIGHT 2

// 활성 DMIC 개수 비교용 상수 (tdc_get_enabled_DMIC_count() 반환값과 대조)
#define LIB_AUDIO_DMIC_COUNT_SINGLE 1
#define LIB_AUDIO_DMIC_COUNT_DUAL   2

// g_lib_dmic_in_buffers[]의 마이크 차원 인덱스
#define LIB_DMIC_IDX_DMIC1 0  // U7, EZ/E8300 (FIFO MIC0)
#define LIB_DMIC_IDX_DMIC2 1  // U9, QCC      (FIFO MIC1)

// 입력 링버퍼 블록 수: [0]=현재(최신) 프레임, [1]=직전 프레임.
// 빔포밍 블록 경계 처리(audioMixer.c p_mix[15]=직전 블록[0])가 이 값(=2)에 의존한다.
#define LIB_AUDIO_IN_BUF_MAX_CNT 2

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

// 디지털 마이크 입력과 FIFO 매핑 (빔포밍: 짝수 채널 ch0/ch2 동일 패리티)
// U7, DMIC_CLK1/CAL, DMIC_OUT1 : IN0 : FA0_0 (MIC0)
// U9, DMIC_CLK2,     DMIC_OUT2 : IN2 : FA0_1 (MIC1)
#define LIB_IOC_ADC_CFG_VAL (IOC_INPUT_CFG_IN0_FA0_0 | IOC_INPUT_CFG_IN1_NONE | IOC_INPUT_CFG_IN2_FA0_1 | IOC_INPUT_CFG_IN3_NONE)

/* Define the interface between the PCM input and the IOC */
#define LIB_IOC_PCM_CFG_VAL (IOC_INPUT_CFG_PCM_RX0_NONE | IOC_INPUT_CFG_PCM_RX1_NONE)

/* Enable double-access mode for the FIFOs.
 * When a single source/destination is used as input/output for a FIFO,
 * double-access mode is recommended. When multiple sources/destinations share
 * the same FIFO, double-access mode must be disabled for proper functionality.
 * (double-access mode not enabled for PCM out) */
#define LIB_IOC_FIFO_ACCESS_VALUE (FIFO_A0_0_DBL_ACC_EN | FIFO_A0_1_DBL_ACC_EN | FIFO_A0_2_DBL_ACC_EN | FIFO_A0_3_DBL_ACC_EN | FIFO_A0_5_DBL_ACC_EN | FIFO_A0_6_DBL_ACC_EN)

// Define the interface between the FIFOs and the PCM out
// DAC0 : FA0_2
// DAC1 : FA0_3
// PCM  : FA0_4
#define LIB_IOC_OUTPUT_CFG_VAL (IOC_OUTPUT_CFG_OUT0_FA0_2 | IOC_OUTPUT_CFG_OUT1_FA0_3 | IOC_OUTPUT_CFG_PCM_TX0_FA0_4 | IOC_OUTPUT_CFG_PCM_TX1_FA0_4)

/* Configuration the audio mux to use ADCs, decimation filters as the IOCs' source, IOCs as the interpolation filters' source,
 * and, interpolation filters as the SDMs' source. */
#define LIB_ADC_AUDIO_MUX_CFG_INPUT_CH_SRC (ADC3_OUT | ADC2_OUT | ADC1_OUT | ADC0_OUT)
#define LIB_ADC_AUDIO_MUX_CFG_IOC_SRC      (IOC0_SRC3_DEC_FILTER | IOC0_SRC2_DEC_FILTER | IOC0_SRC1_DEC_FILTER | IOC0_SRC0_DEC_FILTER)
#define LIB_ADC_AUDIO_MUX_CFG_INT_FILTER   (INT_FILTER1_IOC0 | INT_FILTER0_IOC0)
#define LIB_ADC_AUDIO_MUX_CFG_SDM          (SDM1_INT_FILTER | SDM0_INT_FILTER)

#define LIB_ADC_AUDIO_MUX_CFG_VAL (LIB_ADC_AUDIO_MUX_CFG_INPUT_CH_SRC | LIB_ADC_AUDIO_MUX_CFG_IOC_SRC | LIB_ADC_AUDIO_MUX_CFG_INT_FILTER | LIB_ADC_AUDIO_MUX_CFG_SDM)

/* Configuration the audio mux to use DMICs, decimation filters as the IOCs' source,
 * IOCs as the interpolation filters' source, and, interpolation filters as the SDMs' source. */
//                                          (ADC3     | ADC2          | ADC1          | ADC0    )
// #define LIB_DMIC_AUDIO_MUX_CFG_INPUT_CH_SRC (ADC3_OUT | DMIC2_DATA_FE | DMIC1_DATA_RE | ADC0_OUT)
#define LIB_DMIC_AUDIO_MUX_CFG_INPUT_CH_SRC (ADC3_OUT | DMIC2_DATA_FE | ADC1_OUT | DMIC0_DATA_RE)
#define LIB_DMIC_AUDIO_MUX_CFG_IOC_SRC      (IOC0_SRC3_DEC_FILTER | IOC0_SRC2_DEC_FILTER | IOC0_SRC1_DEC_FILTER | IOC0_SRC0_DEC_FILTER)
#define LIB_DMIC_AUDIO_MUX_CFG_INT_FILTER   (INT_FILTER1_IOC0 | INT_FILTER0_IOC0)
#define LIB_DMIC_AUDIO_MUX_CFG_SDM          (SDM1_INT_FILTER | SDM0_INT_FILTER)
#define LIB_DMIC_AUDIO_MUX_CFG_VAL          (LIB_DMIC_AUDIO_MUX_CFG_INPUT_CH_SRC | LIB_DMIC_AUDIO_MUX_CFG_IOC_SRC | LIB_DMIC_AUDIO_MUX_CFG_INT_FILTER | LIB_DMIC_AUDIO_MUX_CFG_SDM)

/* Configuration the ADC_CFG */
#define LIB_ADC_CFG_VAL (ADC_FBDAC_DEM_ENABLE | ADC_FBDAC_MAX_DRIVE_64 | ADC_FBDAC_MIN_DRIVE_1 | ADC_RIN_4K57 | ADC_COMP_CURRENT_2UA | ADC_CURRENT_14P5UA)

/* Define the decimation filter configuration - enable the decimation filter,
 * unmute the ADC, select the frequency band, set the DC removal cutoff
 * frequency, the sampling delay, the fractional delay and the gain factor */

/* Gain factor given SFCR of 40 (sampling frequency of 24 kHz with ADCCLK 3.84MHz) */
#define LIB_SYS_CALC_GF 1092  // 1092//1979

#define LIB_SAMPLE_FRACTIONAL_DELAY 0
#define LIB_ADC_FRACTIONAL_DELAY_0  (LIB_SAMPLE_FRACTIONAL_DELAY << AUDIO_ADC_DEC_CTRL_DELAY_FRACTIONAL_Pos)
#define LIB_ADC_DEC_CTRL_VAL        (BAND_SELECT_ADC_0K_8K | ADC_INTEGER_DELAY_0 | LIB_ADC_FRACTIONAL_DELAY_0 | ADC_UNMUTE | ADC_DEC_ENABLE | ADC_DC_REMOVE_CUTOFF_20HZ | LIB_SYS_CALC_GF)

// HW 소수점 지연: FRAC=6 = 6/240 = 0.025샘플 (decimation 8x30 서브분할 기준).
// 빔포밍에서 front mic 채널에 적용하며, SW 1샘플 지연(믹서 i+1 오프셋)과 합쳐
// 총 1.025샘플(≈64.06µs ≈ 22mm 음향 경로차)을 만든다. rear mic 채널은 지연 0.
#define LIB_ADC_FRACTIONAL_DELAY_6  (6 << AUDIO_ADC_DEC_CTRL_DELAY_FRACTIONAL_Pos)
#define LIB_ADC_DEC_CTRL_VAL_0_0250 (BAND_SELECT_ADC_0K_8K | ADC_INTEGER_DELAY_0 | LIB_ADC_FRACTIONAL_DELAY_6 | ADC_UNMUTE | ADC_DEC_ENABLE | ADC_DC_REMOVE_CUTOFF_20HZ | LIB_SYS_CALC_GF)

#define LIB_ADC_CTRL_VAL (ADC_ENABLE | ADC_COMP_ENABLE | ADC_MODE_MAX_1P6V | ADC_REF_VSSA | ADC_AIN_RBIAS_VREG)

// 함수
void lib_init_audio_in_path_ADC(void);
void tdc_configure_audio_path_all(void);

void enable_AMIC(void);
void enable_DMIC(void);
void disable_DMIC(void);

void clear_FIFO(volatile int _XMEM *p_fifo, int len);
void clear_FIFO_all(void);
void disable_FIFO_all(void);

int  tdc_get_enabled_DMIC_count(void);
void tdc_enable_1_DMIC(void);
void tdc_enable_2_DMICs(void);

void tdc_audio_set_front_mic(int type);
int  tdc_audio_get_front_mic(void);

// DMIC 입력 링버퍼. 인덱스 규약 (빔포밍 지연 방향의 전제):
//   [mic]    : 0 = DMIC1(EZ/U7, MIC0), 1 = DMIC2(QCC/U9, MIC1)  ← LIB_DMIC_IDX_DMIC1/2
//   [block]  : 0 = 현재(최신) 프레임, 1 = 직전 프레임
//   [sample] : 0 = 블록 내 최신 샘플 … 15 = 가장 오래된 샘플
// (셋째 차원 16 = df_inputADC_DataBuffLength(블록 길이). 본 헤더엔 해당 상수 미가시라 리터럴 유지.)
extern int _XMEM g_lib_dmic_in_buffers[LIB_AUDIO_IN_DMIC_ENABLE_COUNT][LIB_AUDIO_IN_BUF_MAX_CNT][16];

#endif  // __lib_audio_in_h__
