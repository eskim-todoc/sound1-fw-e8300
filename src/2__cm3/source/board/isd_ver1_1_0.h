
#ifndef isd_ver1_1_0_H__
#define isd_ver1_1_0_H__





	#define	ISD_writeRegister		1
	#define	ISD_readRegister		0


	#define	NumByte_ISD_chip_ID						4

	#define	ISD_registerAddr_chip_ID								0x00
	#define	ISD_registerAddr_ciper_data								0x01
	#define	ISD_registerAddr_PowerCheck								0x02
	#define	ISD_registerAddr_cipherDataStatus						0x02
	#define	ISD_registerAddr_forwardPath_check						0x03
	#define	ISD_registerAddr_en__bipolar_referenceElectroldIndex		0x04
	#define	ISD_registerAddr_DAC_offsetValue						0x05
	#define	ISD_registerAddr_StimulationConfig						0x06
	#define	ISD_registerAddr_eCAP_smaplingSize						0x07
	#define	ISD_registerAddr_adc_samplingChannel					0x08
	#define	ISD_registerAddr_adc_measurement						0x09
	#define	ISD_registerAddr_adc_measurementDealy					0x0A
	#define	ISD_registerAddr_adc_preAmpGain_lowerChannel			0x0B
	#define	ISD_registerAddr_adc_preAmpGain_higherChannel			0x0C
	#define	ISD_registerAddr_LSK_Clk_Config							0x0D
	#define	ISD_registerAddr_PPSK_Config							0x0E
	#define	ISD_registerAddr_SystemClkReset							0x0F
	#define	ISD_registerAddr_IO_Config								0x10
	#define	ISD_registerAddr_Stimul_10V								0x11


	#define bitLength_isd_PowerState			2
	#define bitPosition_isd_PowerState			0
	#define bitPosition_VTG_LOCK				4





#define	df_encryptionKey	0xF0E1C2B3
#define	df_forwardPathCheck_arbitraryValue	0x83

#define	df_duplicateZeroValue	32 // 백텔 안들어옴 (32, 39, 59, 119, 120, 121	0x83


#define	unusedReferenceElectrode_DummyNum		31		// 바이폴라 자극 시, 사용하지 않는 기준전극의 dummy 전극 번호



// 자극 출력 chip DAC 특성


	typedef enum {
		Offset_DAC_A=0,
		Offset_DAC_B
	}EN_Offset_DAC_Slope;




	typedef enum {
		Stimulation_DAC_A=0,
		Stimulation_DAC_B,
		Stimulation_DAC_C,
		Stimulation_DAC_D
	} EN_Stimulation_DAC_Slope;
	// bit




	#define	offsetDAC_A_Slope				3.29
	#define offsetDAC_A_SaturationLevel			150
	#define	offsetDAC_B_Slope				6.62
	#define offsetDAC_B_SaturationLevel			150

	#define offsetDAC_A_Saturation_uA		493//(offsetDAC_A_Slope*offsetDAC_A_SaturationLevel)= 3.29*150
	#define offsetDAC_B_Saturation_uA		993//(offsetDAC_B_Slope*offsetDAC_B_SaturationLevel)= 6.62*150


	#define	DAC_A_Slope						2.86
	#define DAC_A_Slope_SaturationLevel			180
	#define	DAC_B_Slope						5.83
	#define DAC_B_Slope_SaturationLevel			180
	#define	DAC_C_Slope						8.65
	#define DAC_C_Slope_SaturationLevel			180
	#define	DAC_D_Slope						11.74
	#define DAC_D_Slope_SaturationLevel			180


	#define stimulDAC_A_only_Saturation_uA		514//(DAC_A_Slope*DAC_A_Slope_SaturationLevel) = 2.86 *180
	#define stimulDAC_B_only_Saturation_uA		1049//(DAC_B_Slope*DAC_B_Slope_SaturationLevel) = 5.83 *180
	#define stimulDAC_C_only_Saturation_uA		1557//(DAC_C_Slope*DAC_C_Slope_SaturationLevel) = 8.65 *180
	#define stimulDAC_D_only_Saturation_uA		2113//(DAC_D_Slope*DAC_D_Slope_SaturationLevel) = 11.74 *180







// 자극출력 DAC 관련



	#define stimulDAC_A_dynamicRange_uA		stimulDAC_A_only_Saturation_uA//510
	#define stimulDAC_B_dynamicRange_uA		stimulDAC_B_only_Saturation_uA//1020
	#define stimulDAC_C_dynamicRange_uA		stimulDAC_C_only_Saturation_uA//1530
	#define stimulDAC_D_dynamicRange_uA		stimulDAC_D_only_Saturation_uA//




	#define offsetDAC_A_Slope_QI5F12		13475		// 3.29*2^12
	#define offsetDAC_B_Slope_QI5F12		27115		// 6.62*4096
	#define DAC_A_Slope_QI5F12				11714		// 2.86*4096
	#define DAC_B_Slope_QI5F12				23879		// 5.83*4096
	#define DAC_C_Slope_QI5F12				35553		// 8.65*4096
	#define DAC_D_Slope_QI5F12				48087		// 11.74*4096



#define offsetDAC_A_Slope_QI4F4		52		// 3.29*2^4
#define offsetDAC_B_Slope_QI4F4		105		// 6.62*16
#define DAC_A_Slope_QI4F4				45		// 2.86*16
#define DAC_B_Slope_QI4F4				93		// 5.83*16
#define DAC_C_Slope_QI4F4				138		// 8.65*16
#define DAC_D_Slope_QI4F4				176		// 11.74*16



	#define reciprocal_dividing_QI1F15_Stimulation_DAC_A								11457// (int)((1/DAC_A_Slope)*2^15)
	#define reciprocal_dividing_QI1F15_Stimulation_DAC_B								5620 // (int)((1/DAC_B_Slope)*32768)
	#define reciprocal_dividing_QI1F15_Stimulation_DAC_C								3788 //(int)((1/DAC_C_Slope)*32768)
	#define reciprocal_dividing_QI1F15_Stimulation_DAC_D								2791//(int)((1/DAC_D_Slope)*32768)


	#define reciprocal_dividing_QI1F15_Offset_DAC_A										9959// (int)((1/offsetDAC_A_Slope)*32768)
	#define reciprocal_dividing_QI1F15_Offset_DAC_B										4949//(int)((1/offsetDAC_B_Slope)*32768)



	//#define stimulDAC_Slope_4uA_shiftDivider								2
	//#define stimulDAC_Slope_6uA_divisionInverse_QI1F23						1398101
	//#define stimulDAC_Slope_8uA_shiftDivider								3

// 측정용 ADC 관련 - 0x09


	#define	adc_measurementMode_impedance									0
	#define	adc_measurementMode_eCAP										1
	#define	mesurementStart_on_configureADCregister							1	//ADC_Start_EN // 이 레지스터의 설정이후, ADC를 설정하는 명령이 수신되면,이전(ADC_Start_EN 설정 이후)에 전달된 자극출력 파라미터의 설정값이  출력되고 측정이 시작된다.
	#define	mesurementStart_on_stiulationOut								2	//ADC_Start_EN // 이 레지스터의 설정이후, 자극 출력 명령이 수신되면, 이전(ADC_Start_EN 설정 이후)에 전달된 자극출력 파라미터의 설정값이  출력되고 측정이 시작된다.
	#define	mesurementStart_Disable											0
	#define	adcSamplingRate40kHz											0
	#define	adcSamplingRate20kHz											1



// 자극 출력 파라미터
	#define	negativePulseFirst												0
	#define	positivePulseFirst												1







#endif


