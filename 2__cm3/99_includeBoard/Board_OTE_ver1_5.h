/**
 * @file Board_OTE_1_5gen_Test_board.h
 */

#ifndef __Board_OTE_1_5gen_Test_board_h__
#define __Board_OTE_1_5gen_Test_board_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include "DIO_PIN_Config.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// CFX, CM3에서 공통으로 사용되는 핀
//////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////
// CFX에서 사용되는 핀
//////////////

#if 1  // Sullivan 1.5

// 배터리 전압 측정 핀
#define DIO_PIN_INDEX_for_devidedBatteryLevel DIO23

// 교정용 클럭 입력 핀
#define DIO_PIN_INDEX_forCalibration          DIO19

// FPGA SLEEP 핀 (활성화 핀)
#define DIO_PIN_INDEX_for_FPGA_SLEEP          DIO9

// 1.5세대 rev1p2 이전
#define DIO_PIN_INDEX_for_FPGA_3P3V_ON        DIO4
#define DIO_PIN_INDEX_for_FPGA_1P2V_ON        DIO11

// LED 출력 핀  매핑
#define DIO_PIN_INDEX_for_LED_color_R         DIO24
#define DIO_PIN_INDEX_for_LED_color_G         DIO22
#define DIO_PIN_INDEX_for_LED_color_B         DIO29

// 가속도 센서
#define DIO_PIN_INDEX_for_Accelerometer       DIO34

// 이어피스 감지
#define DIO_PIN_INDEX_for_EARPIECE_DET_N      DIO12

// CM3 I2C

#define DIO_PIN_INDEX_for_CM3_SDA DIO25
#define DIO_PIN_INDEX_for_CM3_SCL DIO26

// PCM용 PIN 매핑
// 프레임  출력과 입력 핀의 삭제가 가능한 지 차후에 확인 할 것.
#define DIO_PIN_INDEX_forPCM_CLK  DIO32  // 밖으로 나오지 않은 핀 (Eziro8300에서는 임시로 TCK_E8300에 할당)
#define DIO_PIN_INDEX_forPCM_SERI DIO33  // 밖으로 나오지 않은 핀 (Eziro8300에서는 임시로 TMS_E8300에 할당)
#define DIO_PIN_INDEX_forPCM_SERO DIO10
#define DIO_PIN_INDEX_forPCM_FR   DIO6

////////////////
// 공용핀
////////////////

// CFX UART
#define DIO_PIN_INDEX_forUART_TX  DIO20
#define DIO_PIN_INDEX_forUART_RX  DIO21

#define DIO_PIN_INDEX_for_ChargerConnectorPluggedIn DIO17
#define DIO_PIN_INDEX_for_CarryingCasePluggedIn     DIO28 // CASE_DET 핀이 25.09.30일 잠수함 패치 회로에서 DIO27에서 DIO28로 변경됨
#define DIO_PIN_INDEX_for_CarryingCaseCoverOpen     DIO27 // CASE_OPEN_n 핀이 25.09.30일 잠수함 패치 회로에서 DIO28에서 DIO27로 변경됨

#else  // Sound1 Test

// 배터리 전압 측정 핀
#define DIO_PIN_INDEX_for_devidedBatteryLevel DIO22

// 교정용 클럭 입력 핀
#define DIO_PIN_INDEX_forCalibration DIO11

// FPGA SLEEP 핀 (활성화 핀)
#define DIO_PIN_INDEX_for_FPGA_SLEEP DIO26

// 1.5세대 rev1p2 이전
#define DIO_PIN_INDEX_for_FPGA_3P3V_ON DIO9   // I2S_FRAME
#define DIO_PIN_INDEX_for_FPGA_1P2V_ON DIO34  // I2S_CLK

// LED 출력 핀  매핑
#define DIO_PIN_INDEX_for_LED_color_R DIO28
#define DIO_PIN_INDEX_for_LED_color_G DIO18
#define DIO_PIN_INDEX_for_LED_color_B DIO25

// 가속도 센서
#define DIO_PIN_INDEX_for_Accelerometer DIO19

// 이어피스 감지
#define DIO_PIN_INDEX_for_EARPIECE_DET_N DIO24  // I2S_SERO

// CM3 I2C

#define DIO_PIN_INDEX_for_CM3_SDA DIO20
#define DIO_PIN_INDEX_for_CM3_SCL DIO21

// PCM용 PIN 매핑
// 프레임  출력과 입력 핀의 삭제가 가능한 지 차후에 확인 할 것.
#define DIO_PIN_INDEX_forPCM_CLK  DIO32  // 밖으로 나오지 않은 핀 (Eziro8300에서는 임시로 TCK_E8300에 할당)
#define DIO_PIN_INDEX_forPCM_SERI DIO33  // 밖으로 나오지 않은 핀 (Eziro8300에서는 임시로 TMS_E8300에 할당)
#define DIO_PIN_INDEX_forPCM_SERO DIO13
#define DIO_PIN_INDEX_forPCM_FR   DIO10

////////////////
// 공용핀
////////////////

// CFX UART
#define DIO_PIN_INDEX_forUART_TX DIO17  // DMIC_CLK
#define DIO_PIN_INDEX_forUART_RX DIO14  // DMIC_OUT

#define DIO_PIN_INDEX_for_ChargerConnectorPluggedIn DIO27
#define DIO_PIN_INDEX_for_CarryingCasePluggedIn     DIO23  // DMIC_OUT_QCC
#define DIO_PIN_INDEX_for_CarryingCaseCoverOpen     DIO29  // I2S_FLAG
#endif

////////////////////////////////////////////////////////////////////////////////////////////

#define LED_IS_ACTIVEHIGH
// #define LED_IS_ACTIVELOW

#define IO_Shared_CarringCase

#endif // __Board_OTE_1_5gen_Test_board_h__


