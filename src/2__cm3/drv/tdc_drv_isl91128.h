

#include "board.h"
#if defined(Board_is_OTE_VER_1_2)
		#ifndef __tdc_drv_isl91128_h__
		#define __tdc_drv_isl91128_h__


		#define TDC_DRV_ISL91128_SLAVE_ADDR	0x1C

		#define TDC_DRV_ISL91128_REG_VOLTAGECONTROL	0x00
		#define TDC_DRV_ISL91128_REG_MODECONTROL		0x01




		//#define TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE	0x1F	// 3.4V
		//#define TDC_DRV_PMIC_MIN_VOLTAGE_CONTROL_VALUE	0x01	// 1.9V
		#define TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE	0x3F		//5.0V
		#define TDC_DRV_PMIC_MIN_VOLTAGE_CONTROL_VALUE	0x2B		// 4.0V
		#define TDC_DRV_PMIC_VOLTAGE_CONTROL_STEP	2

		#define TDC_DRV_PMIC_MIN_TX_POWER_VALUE 0x3E

		#define TDC_DRV_PMIC_RESET_VOLTAGE_CONTROL_VALUE	0x01	// 1.9V


		typedef enum
		{
			TDC_DRV_ISL91128_VOLTAGECONTROL_BITPOSITION=0,
			TDC_DRV_ISL91128_ULTRASONICMODE_BITPOSITION=6,
			TDC_DRV_ISL91128_ENALBE_I2C_CONTROL_BITPOSITION=7

		}tdc_drv_isl91128_volt_rg_bit_position_t;


		typedef enum
		{
			TDC_DRV_ISL91128_VOLTAGECONTROL_BITLENGT=5

		}tdc_drv_isl91128_volt_rg_bit_length_t;


		typedef enum
		{
			TDC_DRV_ISL91128_SLEWRATE_BITPOSITION=0,
			TDC_DRV_ISL91128_MODE_BITPOSITION=3,
			TDC_DRV_ISL91128_DISCHARGE_BITPOSITION=4,
			TDC_DRV_ISL91128_BYPAS_BITPOSITION=5,

		}tdc_drv_isl91128_mode_rg_bit_position_t;



		bool tdc_drv_isl91128_write_register(int registerAddr, int value);
		bool tdc_drv_isl91128_read_register(int registerAddr, int *read_value);
		bool tdc_drv_isl91128_reset(void);


		#endif

#endif
