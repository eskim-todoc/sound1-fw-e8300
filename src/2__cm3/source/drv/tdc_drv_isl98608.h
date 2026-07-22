#include "board.h"

#if defined(Board_is_OTE_VER_1_3)
#ifndef __tdc_drv_isl98608_h__
#define __tdc_drv_isl98608_h__

	#include <stdbool.h>


	#define TDC_DRV_ISL98608_SLAVE_ADDR	0x29

	#define TDC_DRV_ISL98608_REG_FAULTSTATUS		0x04
	#define TDC_DRV_ISL98608_REG_ENABLE			0x05
	#define TDC_DRV_ISL98608_REG_VBST_VOLTAGE		0x06
	#define TDC_DRV_ISL98608_REG_VN_VOLTAGE		0x08
	#define TDC_DRV_ISL98608_REG_VP_VOLTAGE		0x09


	#define TDC_DRV_ISL98608_VAL_ONLYVP_VALUE


	// VP, VN   base 전압 : 5V
	//			최대 전압 : 7V
	// VBST		base 전압  : 5.4V
	/// 			최대 전압  : 7.3V


	// VP, VN, VBST 전압 계산식 	Out_V = base_V + digit*50mV

#if 0
	//#define TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE	38		// 6.9 V = 5V + 38*50mV
	#define TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE	3		// 7 V = 5V + 40*50mV
	//#define TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE	14		// 5.7 V = 5V + 14*50mV
	#define TDC_DRV_PMIC_MIN_VOLTAGE_CONTROL_VALUE	1		// 5.5 V = 5V + 3*50mV
#else

	//#define TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE	38		// 6.9 V = 5V + 38*50mV
	#define TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE	40		// 7 V = 5V + 40*50mV
	//#define TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE	14		// 5.7 V = 5V + 14*50mV
	#define TDC_DRV_PMIC_MIN_VOLTAGE_CONTROL_VALUE	3		// 5.5 V = 5V + 3*50mV

#endif
	#define	TDC_DRV_PMIC_VOLTAGE_CONTROL_STEP	1

	#define TDC_DRV_PMIC_MIN_TX_POWER_VALUE	(TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE-1)				// 6.95 V = 5V + 39*50mV  == 동작, 불안정
	//#define TDC_DRV_PMIC_MIN_TX_POWER_VALUE	(TDC_DRV_PMIC_MIN_VOLTAGE_CONTROL_VALUE+32)				// 6.5 V = 5V + 30*50mV  == 동작, 불안정
	//#define TDC_DRV_PMIC_MIN_TX_POWER_VALUE	(TDC_DRV_PMIC_MIN_VOLTAGE_CONTROL_VALUE+17)				// 6 V = 5V + 20*50mV ==> 동작 안됨


	#define TDC_DRV_PMIC_RESET_VOLTAGE_SET_VALUE	TDC_DRV_PMIC_MIN_VOLTAGE_CONTROL_VALUE



	// 2023.02.28 현재 PMIC의 읽기 에러가 있어서.. 읽는 기능은 사용하지 않게 했다.
	#define	 TDC_DRV_ISL98608_ERR_READ_BYTE



	bool tdc_drv_isl98608_write_register(int registerAddr, int value);
	bool tdc_drv_isl98608_read_register(int registerAddr, int *read_value);
	bool tdc_drv_isl98608_reset(void);
	bool tdc_drv_isl98608_reset_disable_output(void);
	bool tdc_drv_isl98608_enable_vp(void);
	bool tdc_drv_isl98608_disable(void);






#endif

#endif
