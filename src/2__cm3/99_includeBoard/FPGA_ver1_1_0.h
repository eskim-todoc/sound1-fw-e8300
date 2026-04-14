
#ifndef FPGA_ver1_1_1_H___
#define FPGA_ver1_1_1_H___


        #define df_I2C_Addr_ATMEGA                                         0x33
        #define df_I2C_ADDR_FPGA_BackTel                                   0x30
        #define df_I2C_ADDR_FPGA_ProgramVersion                            0x20
        #define df_I2C_ADDR_FPGA_System_State   	                       0x21
        #define df_I2C_ADDR_FPGA_Error_Flag		                           0x22
        #define df_I2C_ADDR_FPGA_Register_StimulusPulsePhaseWidth          0x23
		#define df_I2C_ADDR_FPGA_FIFO_counter					           0x24

		#define df_I2C_ADDR_FPGA_System_State_2   	                       0x22


		#define	df_FPGA_FIFO_buffSize								100


		//#define FPGA_Status_Reset									0xA0
		#define FPGA_Status_Reset									0xA0 // FIFO가 비어 있고 PCM 에러 상태
		#define FPGA_Status_OK_FIFO_empty							0x80 // FPGA 정상상태, FIFO가  비어 있는 상태
		#define FPGA_Status_OK_FIFO_notEmpty						0x00 // FPGA 정상상태, FIFO에 데이터 있는 상태


		#define FPGA_BitPosition_FIFO_empty								7
		#define FPGA_BitPosition_clearBuffer							6
		#define FPGA_BitPosition_SystemOK								5
		#define FPGA_BitPosition_TxPower								2
		#define FPGA_BitPosition_TxEnable								1

		#define	FPGA_TxPower_Max										7
		#define	FPGA_TxPower_Min										0
		#define FPGA_TxPower_initValue									FPGA_TxPower_Max	//0~3


		// 데이터 전송 관련
		#define FPGA_oneChannelDataTokenTime								41 //  하나의 채널 데이터를  전송가능한 최대로 할당된 시간 단위  41.66usec ... 1ms 동안 24채널을
		#define FPGA_electrodAndStimulLevelTokenTime						10 // 펄스폭을 제외한 자극파라미터(전극번호+자극크기)를 에러가 발생하지 않고 전송가능한 시간 단위 usec
		#define FPGA_interphaseGapTokenTime									4  // 펄스폭을 최소로 했을 때 에러가 발생하지 않고 전송가능한 시간 단위 usec
		#define FPGA_pulsePhaseWidth_minimum									13 // 최소 펄스폭 크기.



		#define	FIPGA_FIFO_index_0											0
		#define	FIPGA_FIFO_index_1											1

#endif
