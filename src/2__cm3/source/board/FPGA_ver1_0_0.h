

#ifndef FPGA_ver1_0_0_H___
#define FPGA_ver1_0_0_H___

        #define df_I2C_Addr_ATMEGA                                         0x33
        #define df_I2C_ADDR_FPGA_BackTel                                   0x30
        #define df_I2C_ADDR_FPGA_ProgramVersion                            0x20
        #define df_I2C_ADDR_FPGA_System_State   	                       0x21
        #define df_I2C_ADDR_FPGA_Error_Flag		                           0x22
        #define df_I2C_ADDR_FPGA_Register_StimulusPulseDuration            0x23


//FPGA를 통해서 전달할 수 있는 자극 채널 수
    #define TransferableChanne32KKK //1msec 동안 32개 채널을 자극하기 위한 데이터 전송이 가능할 경우


// FPGA 상태


        #define df_FPGA_State_BackTel_FIFO_ReadWait_Warnning           -4
        #define df_FPGA_State_BackTel_FIFO_Full_Warnning               -3
        #define df_FPGA_State_BackTel_AbortWarnning                    -2
        #define df_FPGA_State_BackTel_NumOfDataDifferentWarnning       -1
        #define df_FPGA_State_OK                                       0
        #define df_FPGA_State_I2C_communicationERROR                   1   // FPGA와 I2C 통신에 에러가 발생(케이블이 빠졌든지...)
        #define df_FPGA_State_FPGA_ConfigurationERROR                  2   // FPGA 레지스터 설정에 에러가 발생(설정한 값과 읽어들인 값이 다른경우)
        #define df_FPGA_State_PCM_SyncLostERROR                        3   // FPGA과 통신에 에러가 발생(Synclost)
        #define df_FPGA_State_PCM_RateERROR                            4   // FPGA과 CM통신에 에러가 발생(Synclost)
        #define df_FPGA_State_Reset_Error                              5   // 리셋값 에러
        #define df_FPGA_State_Unknown_Error                            6   //
        #define df_FPGA_State_Default_Error                            7  // 초기화 전, 내부 상태가 확인되지 않은 상태

    // 헤드셋 상태
        #define df_HeadsetDetached                                     0   // 헤드셋이 머리에 부착되지 않은 상태
        #define df_HeadsetAttached                                  1   // 헤드셋이 머리에 부착된 상태


	/////////////////////////
	// FPGA  기능

		#define HeadsetAttachementCheckFunction_EnabledKKK
		#define BackTelFunction_EnabledKKK




    // 내부기 전원 상태
        #define df_ISD_internalPower_NotOK                             0   //
        #define df_ISD_nternalPower_IsLow                              1   //
        #define df_ISD_internalPower_OK                                2   //
        #define df_ISD_internalPower_High                              3   //


    // 내부기 자극 출력 설정 상태
        #define df_ISD_StimulationCofiguration_NotOK                   0    //
        #define df_ISD_StimulationCofiguration_Error                   1    //
        #define df_ISD_StimulationCofiguration_OK                      2    //

    // 내부기 전극 임피던스 측정 설정 상태
        #define df_ISD_ImpedanceMeasurementConfiguration_NotOK          0    //
        #define df_ISD_ImpedanceMeasurementConfiguration_Error          1    //
        #define df_ISD_ImpedanceMeasurementConfiguration_OK             2    //

    // 내부기 eCAP 측정 설정 상태
        #define df_ISD_eCapMeasurementConfiguration_NotOK                   0    //
        #define df_ISD_eCapMeasurementConfiguration_Error                   1    //
        #define df_ISD_eCapMeasurementConfiguration_OK                      2    //

    // 내부기 특정 명령 설정 상태
        #define df_ISD_SpecificConfiguration_NotOK                   0    //
        #define df_ISD_SpecificConfiguration_Error                   1    //
        #define df_ISD_SpecificConfiguration_OK                      2    //


#endif

