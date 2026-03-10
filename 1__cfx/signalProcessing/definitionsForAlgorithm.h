
#ifndef DEFINITION_ALGORITHM_H__
#define DEFINITION_ALGORITHM_H__

////////////////////////////////////////////
// 동작 환경 선택
//////////////////

/////////////////
// AGC 계수 선택
#define AGC_COEFF_TYPE_E7150       1
#define AGC_COEFF_TYPE_E8300_SLOPE 2
#define AGC_COEFF_TYPE_OTE_1_5_GEN 3

#define AGC_COEFF_TYPE AGC_COEFF_TYPE_OTE_1_5_GEN  // AGC_COEFF_TYPE_E8300_SLOPE

#if 1
// 자극 묵음 기능
#define df_muteStimulationUnder_TLevel
#define T_levelOffset                           2
#define ISD_registerAddr_forwardPath_check_Data 0x50783

#endif

///////////
// 내부이식장치 버전

#if 1
#define conneded_ISDCheck_byISDPower
#else
#define conneded_ISDCheck_byForwardPath
#endif

///////////////////////////////////////////////////////////////////////////////////////////
// CFX 관련
/////////////////

// 자료형 범위 관련
#define INT24_MAX ((long) 0x7FFFFF)   // +8,388,607
#define INT24_MIN (-(long) 0x800000)  // -8,388,608

// I2C

#define df_i2c_Tx_buffer_size 64
#define df_i2c_Rx_buffer_size 64

// UART

///////////////////////////////////////////////////////////////////////////////////////////
// CM3 관련

///////////////////
// 신호처리 기본

// 오디오 신호 입출력
#define df_inputADC_DataBuffLength  16
#define df_outputDAC_DataBuffLength df_inputADC_DataBuffLength

#define df_maxAudioForLogarithm 21892  // 약 100dB SPL
// #define df_maxAudioForLogarithm   65535  //FFT 입력

#define df_minAudioForLogarithm 8

// nOFm 주파수 밴드 수
#define df_MaxNum_nOFm 16

// 사용자수
#define MAX_NUM_USER 4

// 사용자별 맵 개수
#define MAX_NUM_MAP 4

//
#define ManufacturingDefault_ISD_No 1

///////////////////////
// 논리
#define df_True  1
#define df_False 0
#define df_NA    99

#define df_Default      0
#define df_Connected    1
#define df_Disconnected 2

// 오디오 신호 입출력

#define df_audioDetectionSampleSize_64msec     1024
#define df_audioDetectionSampleSize_64msecHalf (1024 / 2)

#define df_maxMicVloumeLevel 10

// AGC

// #define df_normalizeShiftBitNumFor10DBLibrary 0
#define NORMALIZE_SHIFT_BIT_NUM_FOR_10DB_LIBRARY 5

// 주파수 분석

#define FFT_SIZE      512
#define HALF_FFT_SIZE (FFT_SIZE / 2)

/*
 * NOTE : 오디오 입력 값에 대해서 우측 쉬프트를 몇 번 할것인가?
 */
#define AUDIO_INPUT_RSHIFT 5

#define HEAR_FC_DFT_TYPE_NO_WINDOW 0
#define HEAR_FC_DFT_TYPE_WINDOW    1

#define HEAR_FC_DFT_TYPE HEAR_FC_DFT_TYPE_WINDOW  // 어떤 DFT 방식을 사용할 것인지 선택

#if (HEAR_FC_DFT_TYPE == HEAR_FC_DFT_TYPE_WINDOW)
#define RIGHT_SHIFT_MAX_MAG_FREQ_SCALE 4
#elif (HEAR_FC_DFT_TYPE == HEAR_FC_DFT_TYPE_NO_WINDOW)
#define RIGHT_SHIFT_MAX_MAG_FREQ_SCALE 5
#else
#error "HEAR_FC_DFT_TYPE is invalid in 'definitionsForAlgorithm.h' file."
#endif

// 일반적으로 시간 영역의 크기 A인 신호를 FFT 변환을 하면 주파수 영역에서 크기는 FFT_SIZE 배로 커진다.
// 시간 영역의 신호가 사인파형일 경우에는 두 개의 피크로 크기가 분산되기 때문에 1/2이 적용된다.
// 토닥 사운드 신호처리는 FFT변환전에 윈도윙을 적용하기 때문에 윈도우 게인이 1/2이 적용된다.
// Ezario fft 라이브러리의 Block Pointer 연산의 게인은 1/4 적용된다.
// 사인파형의 Win_DFT_R_Forwar연산 최종 게인은 FFT_size * 1/2 * 1/2 * 1/4 = FFT_size/16이된다.
// vMag 라이브러리 연산의 gain은 1/2이다.
// 모든 게인의 총합은 FFT_size / 32 = 512 / 32 = 16 = 2^4 이다.
// 따라서 주파수 영역에서 구해진 크기 값을 4비트 오른쪽 쉬프트하면 시간영역에서 구한 크기가 된다.

#define RightShitSumFreqScale 5
// 일반적으로 시간 영역의 크기 A인 신호를  FFT변환을 하면 주파수 영역에서 크기는 FFT_size배로 커진다.
// 시간영역의 신호가 반쪽만 밴드 섬을 할것이기 때문에 1/2이 적용된다.
// 밴드 섬에서는 윈도우 효과가 없다.
// Ezario fft 라이브러리의 Block Pointer 연산의 게인은 1/4 적용된다. (사인파형의 Win_DFT_R_Forwar연산 최종 게인은
// FFT_size*1/2*1/4 = FFT_size/8이된다. vMag 라이브러리 연산의 gain은 1/2이다. 모든 게인의 총합은
// FFT_size/8=512/16=32=2^5이다. 따라서 주파형 영역에서 구해진 크기 값을 5비트 왼쪽 쉬프트하면 시간영역에서 구한 크기가
// 된다.

// PCM 입/출력
#define df_MaxNumTransferableChannel          24
#define df_wordLengthPerOneChannelDataForFPGA 1  // BTE 버전의 FPGA에서는 한 채널당 2word, 40bit를 전송했으나, 현재는 한 채널당 1word, 20bit를 전송한다.
#define df_ouputDataBufferLength              (df_MaxNumTransferableChannel * df_wordLengthPerOneChannelDataForFPGA)
#define df_outputPCM_DataBuffLength           df_ouputDataBufferLength

// 전극
#define df_MaxNumOfElectrode 32

// 자극

#define df_maxStimulationVloumeLevel 4

#define df_stimulation_Min 0
#define df_stimulation_Max 255

#define df_stimulationStrategy_CIS    1
#define df_stimulationStrategy_nOFm   2
#define df_stimulationStrategy_medium 3

// ISD 연결 체크 주기
#define df_connectionCheckPeriod_ms 300

// 최대 전달 전하량
    #define df_MaxDeliveryCharge_nC         75
    #define df_MaxDeliveryCharge_pC         75000




// eCAP 측정시 최대 반복 횟수
    #define df_maxIterationNum_eCAP                     30


// 로우배터리 알림 주기
    #define df_lowbatteryIndicationPeriod_ms            600000

#endif
