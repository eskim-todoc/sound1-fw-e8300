
#ifndef EEPROM_ADDRESS_H__
#define EEPROM_ADDRESS_H__

#include "definitionsForAlgorithm.h"
#include "processorDirective.h"

#define AT_WREN 0x06 // Write enable

#define df_eepromLastData_Adrress               0x3FFFF
#define df_eepromAddrressByteLengthFor24bitWord 3 // 1word(24bit)는 3byte
#define df_MaxNumberOfUsableElectrode           32

//
//
//
//
//                  0x00000    |^^^^^^^^^^^^^^^^^^^^^^
//                             | system Area
//                             |
//                  0x00040    | Manufacturing Area
//                             |
//                  0x00100    | Key Area
//                             |
//                  0x00180    | Boot Loader Area
//                             |
//                             | Aplication Area
//                             |
//                             |        ,,
//                             |
//                             |
//                             |
//                             |
//                             |
//                             |
//                             | 배터리 측정용 교정 값  : 4V 전압에 대한 측정값
//                             |
//                             |
//							   |  ISD 정보 1
//                             |
//                             |  사용자 1 	설정 파라미터(맵 번호,알람 ,LED 등)
//                             |
//                             | (사용자 1 1st Program Data )
//                             |
//                             | (사용자 1 2nd Program Data )
//                             |
//                             | (사용자 1 3th Program Data )
//                             |
//                             | (사용자 1 4th Program Data )
//                             |
//							   |  ISD 정보 2
//                             |
//                             |  사용자 2 	설정 파라미터(맵 번호,알람 ,LED 등)
//                             |
//                             | (사용자 2 1st Program Data )
//                             |
//                             | (사용자 2 1st Program Data )
//                             |
//                             | (사용자 2 2nd Program Data )
//                             |
//                             | (사용자 2 3th Program Data )
//                             |
//                             | (사용자 2 4th Program Data )
//                             |
//							   |  ISD 정보 3
//                             |
//                             |  사용자 3 	설정 파라미터(맵 번호,알람 ,LED 등)
//                             |
//                             | (사용자 3 1st Program Data )
//                             |
//                             | (사용자 3 1st Program Data )
//                             |
//                             | (사용자 3 2nd Program Data )
//                             |
//                             | (사용자 3 3th Program Data )
//                             |
//                             | (사용자 3 4th Program Data )
//							   |
//							   |  ISD 정보 4
//                             |
//                             |  사용자 4 	설정 파라미터(맵 번호,알람 ,LED 등)
//                             |
//                             | (사용자 4 1st Program Data )
//                             |
//                             | (사용자 4 2nd Program Data )
//                             |
//                             | (사용자 4 3th Program Data )
//                             |
//                             | (사용자 4 4th Program Data )
//                             |
//                             | ( MapStamp)
//                             |
//                             | FFT_Window_Coeff
//                             |
//                             | FFT_PassBin_idex
//                0x3FFFF      |__________________

// 밴드 분석용 FFT bin 인덱스

// FFT_Size/2*사용가능한 채널 수=
//  256*32 = 8192
#define df_24bitWordLength_FFT_PassBin_index (Half_FFT_Size * df_MaxNumOfElectrode)
#define df_ByteLength_FFT_PassBin_index      (df_24bitWordLength_FFT_PassBin_index * 3)
#define df_24bitWordLength_Half_FFT_Size     Half_FFT_Size
#define df_ByteLength_Half_FFT_Size          (df_24bitWordLength_Half_FFT_Size * 3)

//  FFT 입력 윈도우 계수

// FFT_Size
#define df_24bitWordLength_FFT_windowCoeff (FFT_Size)
#define df_ByteLength_FFT_windowCoeff      (df_24bitWordLength_FFT_windowCoeff * 3)

// ISD 1과 관련된 데이터.

// ISD 정보
#define df_24bitWordLength_ISD_info 33
#define df_ByteLength_ISD_info      (df_24bitWordLength_ISD_info * 3)

// 사용자 설정 값

#define df_24bitWordLength_UserSettingValues 7
#define df_ByteLength_UserSettingValues      (df_24bitWordLength_UserSettingValues * 3)

// 맵 스템프 일자
#define df_24bitWordLength_MapStemp 6
#define df_ByteLength_MapStemp      (df_24bitWordLength_MapStemp * 3)

// 프로그램데이터 관련 파라미터

// 매핑 일자
#define df_24bitWordLength_mappingDate 6
#define df_ByteLength_mappingDate      (df_24bitWordLength_mappingDate * 3)

// 선행 펄스 형태
#define df_24bitWordLength_stimulationStrategy 1
#define df_ByteLength_stimulationStrategy      (df_24bitWordLength_stimulationStrategy * 3)

// 선행 펄스 형태
#define df_24bitWordLength_firstPulsePhase 1
#define df_ByteLength_firstPulsePhase		                                 (df_24bitWordLength_firstPulsePhase)*3)
// 자극 모드 (모노폴,바이폴,공통)
#define df_24bitWordLength_stimulationMode 1
#define df_ByteLength_stimulationMode		                                 (df_24bitWordLength_stimulationMode)*3)
// 자극 펄스 폭
#define df_24bitWordLength_pulsePhaseWidth 1
#define df_ByteLength_pulsePhaseWidth		                                 (df_24bitWordLength_pulsePhaseWidth)*3)

// 주파수 분석 채널 수
#define df_24bitWordLength_NumberOfFrequencyBand 1
#define df_ByteLength_NumberOfFrequencyBand		                         	 (df_24bitWordLength_NumberOfFrequencyBand)*3)

// 알람 출력 채널
#define df_24bitWordLength_ToneSignalOutputElectrodeIndex 1
#define df_ByteLength_ToneSignalOutputElectrodeIndex                         (df_24bitWordLength_ToneSignalOutputElectrodeIndex)*3)

// 알람 출력 크기
#define df_24bitWordLength_ToneSignalOutputLevel 1
#define df_ByteLength_ToneSignalOutputLevel                       	 		(df_24bitWordLength_ToneSignalOutputLevel)*3)

//  주파수 분석  채널에 할당된 전극 번호      32 word
//  32*3 = 3
#define df_24bitWordLength_StimulusChannelAssignedElectrodIndex df_MaxNumOfElectrode
#define df_ByteLength_StimulusChannelAssignedElectrodIndex      (df_MaxNumOfElectrode * 3)

// 주파수 분석 채널에 할당된 기준 전극 번호(바이폴라 모드용)
//  32*3 = 3
#define df_24bitWordLength_ReferenceChannelAssignedElectrodIndex df_MaxNumOfElectrode
#define df_ByteLength_ReferenceChannelAssignedElectrodIndex      (df_MaxNumOfElectrode * 3)

// 주파수 분석 채녈에 대한 자극 출력 순서
//  32*3 = 3
#define df_24bitWordLength_CIS_FreqBandOrder df_MaxNumOfElectrode
#define df_ByteLength_CIS_FreqBandOrder      (df_MaxNumOfElectrode * 3)

//  최소 자극 레빌(T-Level)    32 word
//  32*3 = 96
#define df_24bitWordLength_T_Level_uA df_MaxNumOfElectrode
#define df_ByteLength_T_Level_uA      (df_MaxNumOfElectrode * 3)

//  최대 자극 레빌(C-Level)    32 word
//  32*3 = 96
#define df_24bitWordLength_C_Level_uA df_MaxNumOfElectrode
#define df_ByteLength_C_Level_uA      (df_MaxNumOfElectrode * 3)

//  최소 입력 audio 신호(X-min 채널별)    32 word
//  32*3 = 96
#define df_24bitWordLength_xMinLevel df_MaxNumOfElectrode
#define df_ByteLength_xMinLevel      (df_MaxNumOfElectrode * 3)

//  최대 입력 audio 신호(X-max 채널별)    32 word
//  32*3 =96
#define df_24bitWordLength_xMaxLevel df_MaxNumOfElectrode
#define df_ByteLength_xMaxLevel      (df_MaxNumOfElectrode * 3)

// 프로그램 관련 데이터 합 :

#define df_24bitWordLength_ProgramData (13 + (7 * df_MaxNumOfElectrode))
#define df_ByteLength_ProgramData      (df_24bitWordLength_ProgramData * 3)

// 내부기 관련 데이터 총합 :

#define df_24bitWordLength_isd_DB (df_24bitWordLength_ISD_info + df_24bitWordLength_UserSettingValues + df_24bitWordLength_MapStemp + (df_24bitWordLength_ProgramData * 4))
#define df_ByteLength_isd_DB      (df_24bitWordLength_isd_DB * 3)

// 테스트 벡터를 사용할 경우 사
#define df_24bitWordLength_TestVector_AGC_input (df_inputADC_DataBuffLength * TestVector_FrameLenght)
#define df_ByteLength_TestVector_AGC_input      (df_24bitWordLength_TestVector_AGC_input * 3)

#define df_24bitWordLength_TestVector_FFT_input (df_inputADC_DataBuffLength * TestVector_FrameLenght)
#define df_ByteLength_TestVector_FFT_input      (df_24bitWordLength_TestVector_FFT_input * 3)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// EEPROM 주소

////////////////
// 밴드 분석용  FFT bin 인덱스, BASE 주소
#define df_EERPROM_BaseAddr_FFT_PassBin_index (df_eepromLastData_Adrress - (df_ByteLength_FFT_PassBin_index))

////////////////
//  FFT 입력 윈도우 계수

#define df_EERPROM_BaseAddr_FFT_windowCoeff (df_EERPROM_BaseAddr_FFT_PassBin_index - df_ByteLength_FFT_windowCoeff)

////////////////
//  ISD info
#define df_EERPROM_BaseAddr_ISD_DB_4 (df_EERPROM_BaseAddr_FFT_windowCoeff - df_ByteLength_isd_DB)
#define df_EERPROM_BaseAddr_ISD_DB_3 (df_EERPROM_BaseAddr_ISD_DB_4 - df_ByteLength_isd_DB)
#define df_EERPROM_BaseAddr_ISD_DB_2 (df_EERPROM_BaseAddr_ISD_DB_3 - df_ByteLength_isd_DB)
#define df_EERPROM_BaseAddr_ISD_DB_1 (df_EERPROM_BaseAddr_ISD_DB_2 - df_ByteLength_isd_DB)

////////////////
//  user  setting value
#define df_EERPROM_BaseAddr_User_1_SettingValues (df_EERPROM_BaseAddr_ISD_DB_1 + df_ByteLength_ISD_info)
#define df_EERPROM_BaseAddr_User_2_SettingValues (df_EERPROM_BaseAddr_ISD_DB_2 + df_ByteLength_ISD_info)
#define df_EERPROM_BaseAddr_User_3_SettingValues (df_EERPROM_BaseAddr_ISD_DB_3 + df_ByteLength_ISD_info)
#define df_EERPROM_BaseAddr_User_4_SettingValues (df_EERPROM_BaseAddr_ISD_DB_4 + df_ByteLength_ISD_info)

//  배터리 측정용 교정값
#define df_EERPROM_BaseAddr_BatteryCalibraionData (df_EERPROM_BaseAddr_ISD_DB_1 - 3) // 1word, 3byte

#ifdef TestVector_AGC_input_Using

#define df_EERPROM_BaseAddr_AGC_input_TestVector (df_EERPROM_BaseAddr_FFT_windowCoeff - df_ByteLength_TestVector_AGC_input)
#define df_EERPROM_BaseAddr_FFT_input_TestVector (df_EERPROM_BaseAddr_AGC_input_TestVector - df_ByteLength_TestVector_FFT_input)

#else

#define df_EERPROM_BaseAddr_FFT_input_TestVector (df_EERPROM_BaseAddr_FFT_windowCoeff - df_ByteLength_TestVector_FFT_input)

#endif





#endif
