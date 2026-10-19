

#include "board.h"
#if defined(Board_is_OTE_VER_1_2)
		#ifndef REN_ISL91128_H__
		#define REN_ISL91128_H__


		#define REN_ISL91128_SlaveAddr	0x1C

		#define REN_ISL91128_registerAddr_voltageControl	0x00
		#define REN_ISL91128_registerAddr_ModeControl		0x01




		//#define MaxVoltageControlValue	0x1F	// 3.4V
		//#define MinVoltageControlValue	0x01	// 1.9V
		#define MaxVoltageControlValue	0x3F		//5.0V
		#define MinVoltageControlValue	0x2B		// 4.0V
		#define VoltageControlStep	2

		#define MinTxPowerValue 0x3E

		#define ResetVoltageControlValue	0x01	// 1.9V


		typedef enum
		{
			en__voltageControl_BitPosition=0,
			en__UltraSonicMode_BitPosition=6,
			en__enalbe_I2C_Control_BitPosition=7

		}REN_ISL91128_VOLT_RG_BIT_POSITION;


		typedef enum
		{
			en__voltageControl_BitLengt=5

		}EN__REN_ISL91128_VOLT_RG_BIT_Length;


		typedef enum
		{
			en__SlewRate_BitPosition=0,
			en__Mode_BitPosition=3,
			en__Discharge_BitPosition=4,
			en__Bypas_BitPosition=5,

		}EN__REN_ISL91128_MODE_RG_BIT_POSITION;



		bool write_REN_ISL91128_register_byCM3_I2C(int registerAddr, int value);
		bool read_REN_ISL91128_register_byCM3_I2C(int registerAddr, int *read_value);
		bool Reset_REN_ISL91128(void);


		#endif

#endif
