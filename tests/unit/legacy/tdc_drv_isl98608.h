/* Historical f9fa45ff driver; names adapted for host fixtures only. */
#include "board.h"

#if defined(Board_is_OTE_VER_1_3)
#ifndef REN_ISL98068_H__
#define REN_ISL98068_H__

	#include <stdbool.h>


	#define REN_ISL98608_SlaveAddr	0x29

	#define REN_ISL98608_registerAddr_FaultStatus		0x04
	#define REN_ISL98608_registerAddr_Enable			0x05
	#define REN_ISL98608_registerAddr_VBST_Voltage		0x06
	#define REN_ISL98608_registerAddr_VN_Voltage		0x08
	#define REN_ISL98608_registerAddr_VP_Voltage		0x09


	#define REN_ISL98608_registerValue_OnlyVP_Value


	// VP, VN   base 전압 : 5V
	//			최대 전압 : 7V
	// VBST		base 전압  : 5.4V
	/// 			최대 전압  : 7.3V


	// VP, VN, VBST 전압 계산식 	Out_V = base_V + digit*50mV

#if 0
	//#define TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE	38		// 6.9 V = 5V + 38*50mV
	#define TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE	3		// 7 V = 5V + 40*50mV
	//#define TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE	14		// 5.7 V = 5V + 14*50mV
	#define MinVoltageControlValue	1		// 5.5 V = 5V + 3*50mV
#else

	//#define TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE	38		// 6.9 V = 5V + 38*50mV
	#define TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE	40		// 7 V = 5V + 40*50mV
	//#define TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE	14		// 5.7 V = 5V + 14*50mV
	#define MinVoltageControlValue	3		// 5.5 V = 5V + 3*50mV

#endif
	#define	TDC_DRV_PMIC_VOLTAGE_CONTROL_STEP	1

	#define TDC_DRV_PMIC_MIN_TX_POWER_VALUE	(TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE-1)				// 6.95 V = 5V + 39*50mV  == 동작, 불안정
	//#define TDC_DRV_PMIC_MIN_TX_POWER_VALUE	(MinVoltageControlValue+32)				// 6.5 V = 5V + 30*50mV  == 동작, 불안정
	//#define TDC_DRV_PMIC_MIN_TX_POWER_VALUE	(MinVoltageControlValue+17)				// 6 V = 5V + 20*50mV ==> 동작 안됨


	#define TDC_DRV_PMIC_RESET_VOLTAGE_SET_VALUE	MinVoltageControlValue



	// 2023.02.28 현재 PMIC의 읽기 에러가 있어서.. 읽는 기능은 사용하지 않게 했다.
	#define	 TDC_DRV_ISL98608_ERR_READ_BYTE



	bool write_REN_ISL98608_register_byCM3_I2C(int registerAddr, int value);
	bool read_REN_ISL98608_register_byCM3_I2C(int registerAddr, int *read_value);
	bool Reset_REN_ISL98608(void);
	bool Reset_disableOutup_REN_ISL98608(void);
	bool enable_VP_REN_ISL98608(void);
	bool disable_REN_ISL98608(void);






#endif

#endif
