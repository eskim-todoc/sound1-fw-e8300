#ifndef Board_ALASKA_3_H__
#define Board_ALASKA_3_H__


#include <DIO_PIN_Config.h>





/////////////////////////////////////////////////////////////////////////////////////////////////////////
// CFX, CM3에서 공통으로 사용되는 핀
//////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////
// CFX에서 사용되는 핀
//////////////

		// 교정용 클럭 입력 핀
		#define DIO_PIN_INDEX_forCalibration				6

		// PCM용 PIN 매핑
		//프레임  출력과 입력 핀의 삭제가 가능한 지 차후에 확인 할 것.
		#define DIO_PIN_INDEX_forPCM_CLK                    25
		#define DIO_PIN_INDEX_forPCM_SERI                   26
		#define DIO_PIN_INDEX_forPCM_SERO                   24
		#define DIO_PIN_INDEX_forPCM_FR                     23


		// LED 출력 핀  매핑
		#define DIO_PIN_INDEX_for_LED_color_R              9
		#define DIO_PIN_INDEX_for_LED_color_G              6
		#define DIO_PIN_INDEX_for_LED_color_B              8


		// 배터리 전압 측정 핀
		#define DIO_PIN_INDEX_for_devidedBatteryLevel	5


		// CFX UART
		#define	DIO_PIN_INDEX_forUART_TX					DIO_PIN_INDEX_for_LED_color_B
		#define	DIO_PIN_INDEX_forUART_RX					DIO_PIN_INDEX_for_LED_color_R

////////////////////////////////////////////////////////////////////////////////////////////
// CM3에서 사용되는 핀
/////////////
		#define DIO_PIN_INDEX_for_CM3_SCL             	 	22
		#define DIO_PIN_INDEX_for_CM3_SDA              		21


		#define DIO_PIN_INDEX_for_TEST				20

		#define DIO_PIN_INDEX_for_TEST_2					29

		#define DIO_PIN_INDEX_for_Accelerometer			DIO_PIN_INDEX_for_TEST_2

		#define DIO_PIN_INDEX_for_3V_onOff					6


		#define LED_IS_ACTIVELOW
#endif
