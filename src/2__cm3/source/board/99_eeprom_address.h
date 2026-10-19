
#ifndef EEPROM_ADDRESS_H__
#define EEPROM_ADDRESS_H__

#include <tdc_stim_definitions.h>
#include <processorDirective.h>

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

//  FFT 입력 윈도우 계수

// FFT_Size

// ISD 1과 관련된 데이터.

// ISD 정보
#define df_24bitWordLength_ISD_info 33

// 사용자 설정 값

#define df_24bitWordLength_UserSettingValues 7

// 맵 스템프 일자

// 프로그램데이터 관련 파라미터

// 매핑 일자
#define df_24bitWordLength_mappingDate 6

// 선행 펄스 형태

// 선행 펄스 형태
// 자극 모드 (모노폴,바이폴,공통)
// 자극 펄스 폭

// 주파수 분석 채널 수

// 알람 출력 채널

// 알람 출력 크기

//  주파수 분석  채널에 할당된 전극 번호      32 word
//  32*3 = 3

// 주파수 분석 채널에 할당된 기준 전극 번호(바이폴라 모드용)
//  32*3 = 3

// 주파수 분석 채녈에 대한 자극 출력 순서
//  32*3 = 3

//  최소 자극 레빌(T-Level)    32 word
//  32*3 = 96

//  최대 자극 레빌(C-Level)    32 word
//  32*3 = 96

//  최소 입력 audio 신호(X-min 채널별)    32 word
//  32*3 = 96

//  최대 입력 audio 신호(X-max 채널별)    32 word
//  32*3 =96

// 프로그램 관련 데이터 합 :

#define df_24bitWordLength_ProgramData (13 + (7 * df_MaxNumOfElectrode))

// 내부기 관련 데이터 총합 :

// 테스트 벡터를 사용할 경우 사

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// EEPROM 주소

////////////////
// 밴드 분석용  FFT bin 인덱스, BASE 주소

////////////////
//  FFT 입력 윈도우 계수

////////////////
//  ISD info

////////////////
//  user  setting value

//  배터리 측정용 교정값

#ifdef TestVector_AGC_input_Using

#else

#endif

#endif
