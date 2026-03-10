#ifndef Board_ALASKA_3_H__
#define Board_ALASKA_3_H__


#include "DIO_PIN_Config.h"



/////////////////////////////////////////////////////////////////////////////////////////////////////////
// CFX, CM3에서 공통으로 사용되는 핀
//////////////////////////

		#define DIO_PIN_INDEX_TestGPIO                  	8
		#define DIO_PIN_INDEX_TestCKL	                	9

///////////////////////////////////////////////////////////////////////////////////////////////////////
// CFX에서 사용되는 핀
//////////////

		// 교정용 클럭 입력 핀
		#define DIO_PIN_INDEX_forCalibration				5

		// PCM용 PIN 매핑
		//프레임  출력과 입력 핀의 삭제가 가능한 지 차후에 확인 할 것.
		#define DIO_PIN_INDEX_forPCM_CLK                    25
		#define DIO_PIN_INDEX_forPCM_SERI                   26
		#define DIO_PIN_INDEX_forPCM_SERO                   20
		#define DIO_PIN_INDEX_forPCM_FR                     21

		// 내부 EEPROM용 PIN 매핑
		#define DIO_PIN_INDEX_for_EEPROM_SPI_CLK            0
		#define DIO_PIN_INDEX_for_EEPROM_SPI_CS             1
		#define DIO_PIN_INDEX_for_EEPROM_SPI_SERI           2
		#define DIO_PIN_INDEX_for_EEPROM_SPI_SERO           3

		// FPGA 통신용 I2C PIN 매핑
		#define DIO_PIN_INDEX_for_I2C_SCL                   0
		#define DIO_PIN_INDEX_for_I2C_SDA                   1

		// 배터리 전압 체크용 PIN 매핑
		#define DIO_PIN_INDEX_for_devidedBatteryLevel       5

		// 버튼 입력용 PIN 매핑
		#define DIO_PIN_INDEX_for_Button_A                  29

		// LED 출력 핀  매핑
		#define DIO_PIN_INDEX_for_LED_color_R              22
		#define DIO_PIN_INDEX_for_LED_color_G              23
		#define DIO_PIN_INDEX_for_LED_color_B              24


		// CFX UART
		#define	DIO_PIN_INDEX_forUART_TX					24
		#define	DIO_PIN_INDEX_forUART_RX					29


////////////////////////////////////////////////////////////////////////////////////////////
// CM3에서 사용되는 핀
/////////////



////////////////////////////////////////////////////////////////////////////////////////////
// 버튼으로 사용되는  DIO핀의 설정
/////////////
#define     DIO_PIN_CFG_FOR_INPUT_Button	DIO_PIN_CFG_FOR_GPIO_INPUT_NOPULLUP

////////////////////////////////////////////////////////////////////////////////////////////
// LED로 사용되는 DIO핀의 설정
/////////////





#endif
