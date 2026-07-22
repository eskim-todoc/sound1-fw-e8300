#ifndef Board_Evaluation_H__
#define Board_Evaluation_H__


#include <DIO_PIN_Config.h>




///////////////////////////////////////////////////////////////////////////////////////////////////////
// CFX에서 사용되는 핀
//////////////

		// PCM용 PIN 매핑
		//프레임  출력과 입력 핀의 삭제가 가능한 지 차후에 확인 할 것.
		#define DIO_PIN_INDEX_forPCM_CLK                    25
		#define DIO_PIN_INDEX_forPCM_SERI                   26
		#define DIO_PIN_INDEX_forPCM_SERO                   20
		#define DIO_PIN_INDEX_forPCM_FR                     21


		// 배터리 전압 체크용 PIN 매핑
		#define DIO_PIN_INDEX_for_devidedBatteryLevel       5


		// LED 출력 핀  매핑
		#define DIO_PIN_INDEX_for_LED_color_R              6
		#define DIO_PIN_INDEX_for_LED_color_G              8
		#define DIO_PIN_INDEX_for_LED_color_B              9

////////////////////////////////////////////////////////////////////////////////////////////
// CM3에서 사용되는 핀
/////////////
		#define DIO_PIN_INDEX_for_CM3_SCL             	 	22
		#define DIO_PIN_INDEX_for_CM3_SDA              		23





#endif



