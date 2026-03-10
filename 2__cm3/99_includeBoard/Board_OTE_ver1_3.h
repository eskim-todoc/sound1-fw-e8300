#ifndef Board_OTE_VER_1_3_H__
#define Board_OTE_VER_1_3_H__

#include "DIO_PIN_Config.h"





/////////////////////////////////////////////////////////////////////////////////////////////////////////
// CFX, CM3에서 공통으로 사용되는 핀
//////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////
// CFX에서 사용되는 핀
//////////////






		// 배터리 전압 측정 핀
		#define DIO_PIN_INDEX_for_devidedBatteryLevel		5



		// 교정용 클럭 입력 핀, 3.3V 전원 관리
		#define DIO_PIN_INDEX_forCalibration				6
		#define DIO_PIN_INDEX_for_3V_onOff					DIO_PIN_INDEX_forCalibration

		// LED 출력 핀  매핑
		#define DIO_PIN_INDEX_for_LED_color_R             	8
		#define DIO_PIN_INDEX_for_LED_color_G              	9
		#define DIO_PIN_INDEX_for_LED_color_B              	20


		// 가속도 센서
		#define DIO_PIN_INDEX_for_Accelerometer			 	21


		// CM3 I2C

		#define DIO_PIN_INDEX_for_CM3_SDA              		22
		#define DIO_PIN_INDEX_for_CM3_SCL             	 	23




		// PCM용 PIN 매핑
		//프레임  출력과 입력 핀의 삭제가 가능한 지 차후에 확인 할 것.
		#define DIO_PIN_INDEX_forPCM_CLK                    25	// 밖으로 나오지 않은 핀
		#define DIO_PIN_INDEX_forPCM_SERI                   26	// 밖으로 나오지 않은 핀
		#define DIO_PIN_INDEX_forPCM_SERO                   24
		#define DIO_PIN_INDEX_forPCM_FR                     29


		////////////////
		// 공용핀
		////////////////

		// CFX UART
		#define	DIO_PIN_INDEX_forUART_TX							DIO_PIN_INDEX_for_LED_color_R
		#define	DIO_PIN_INDEX_forUART_RX							DIO_PIN_INDEX_for_LED_color_G

		#define	DIO_PIN_INDEX_for_ChargerConnectorPluggedIn			DIO_PIN_INDEX_for_Accelerometer
		#define	DIO_PIN_INDEX_for_CarryingCasePluggedIn				DIO_PIN_INDEX_forPCM_SERO
		#define	DIO_PIN_INDEX_for_CarryingCaseCoverOpen				DIO_PIN_INDEX_forPCM_FR



////////////////////////////////////////////////////////////////////////////////////////////

		#define LED_IS_ACTIVELOW


		#define IO_Shared_CarringCase

		#define Ezairo_Input_Votage_13

		#define SystemClk_1536

#endif
