#ifndef isd_ver1_0_0_H__
#define isd_ver1_0_0_H__


				#define     df_ISD_dataHederForConfigDoubleBiphasic             4   //  b100   , 3번째 비트: 제어용데이터(b1), 2번째 비트 : double 데이터(b0), 1번째 비트 : biphasic 모드(b0)
                #define     df_ISD_dataHederForStimulDoubleBiphasic             0   //  b000   , 3번째 비트: 자극데이터(b0), 2번째 비트 : double 데이터(b0), 1번째 비트 : biphasic 모드(b0)
                #define     df_ISD_dataHederForStimulSingleBiphasic             2   //  b010   , 3번째 비트: 자극데이터(b0), 2번째 비트 : single 데이터(b1), 1번째 비트 : biphasic 모드(b0)

                #define     df_ISD_dataHederForConfigDoubleTriphasic            5   //  b101   , 3번째 비트: 제어용데이터(b1), 2번째 비트 : double 데이터(b0), 1번째 비트 : triphasic 모드(b1)
                #define     df_ISD_dataHederForStimulDoubleTriphasic            1   //  b001   , 3번째 비트: 자극데이터(b0), 2번째 비트 : double 데이터(b0), 1번째 비트 : triphasic 모드(b1)
                #define     df_ISD_dataHederForStimulSingleTriphasic            3   //  b011   , 3번째 비트: 자극데이터(b0), 2번째 비트 : single 데이터(b1), 1번째 비트 : triphasic 모드(b1)

                #define     df_ISD_dataHederBitFieldPosition_PacketSize         1
                #define     df_ISD_dataHederBitFieldPosition_PulsePhasic        0


           // PCM 데이터 상위 20bit, 하위 20bit 중에서 kernel Data는 상위 16bit 하위 17bit가 사용된다.
            // 상위  16bit Kernel Data에서  ISD의 데이터 header 3bit가 사용되기 때문에
            //
                    #define     df_ISD_dataHeaderPosition                                   13// 모든 패킷 공통
            //      ////////////////
            //      double paket일 때,
            //      1. 제어 패킷구성은 다음과 같다.
            //        - 상위 16 bit
            //              [isd header ] [ register address ] | [reserved]
            //                  3bit     |        8bit         |     5bit
            //        - 하위 17 bit
            //               [  r/w  ] [ control Data ]
            //                  1bit  |     16bit
                        // 상위 16bit
                        #define     df_ISD_registerAddrPosition_forDoublePK                     5
                        // 하위 17 bit
                        #define     df_ISD_readWriteCommandPosition_forDoublePK                 16

            //      2. 자극  패킷구성은 다음과 같다.
            //        - 상위 16 bit
            //              [isd header ] [ electrode index A ] | [ electrode index B ] | [ stimuls Ampiltude A(상위 3bit/10bit) ]
            //                  3bit     |         5bit         |        5bit           |                3bit
            //        - 하위 17 bit
            //              [ stimuls Ampiltude A(하위  7bit/10bit) ] | [ stimuls Ampiltude B]
            //                            7bit                      |          10bit
                    // 상위 16it
                        #define     df_ISD_ElectrodeAPosition_forDoublePK                           8
                        #define     df_ISD_ElectrodeBPosition_forDoublePK                           3
                        #define     df_ISD_StimulsRoundOffBitLength_forDoublePK                     7        // ISD Kennel 데이터 상위 16비트 기준에서 포함되지 않고 잘려서 하위 17bit에 포함되는 데이터 길이
                    // 하위 17bit
                        #define     df_ISD_StimulationLevelMainLSB_Position_forDoublePK                 10

            //
            //      ///////////////////////////
            //      sigle paket일 때,
            //      3. 제어 패킷구성은 다음과 같다.
            //        - 상위 16 bit
            //              [isd header ] [ register address ] | [  r/w  ] | [ control data (상위 7bit/8bit) ]
            //                  3bit     |        5bit         |    1bit   |               7bit
            //        - 하위 17 bit
            //              [ control data (하위 1bit/8bit) ] | [ Zero Padding ]
            //                           1bit               |     16bit
                        // 상위 16bit
                        #define     df_ISD_registerAddrPosition_forSinglePK                     8
                        #define     df_ISD_readWriteCommandPosition_forSinglePK                 7
                        #define     df_ISD_controlDataRoundOffBitLength_forSinglePK             1        // ISD Kennel 데이터 상위 16비트 기준에서 포함되지 않고 잘려서 하위 17bit에 포함되는 데이터 길이
                        // 하위 17 bit
                        #define     df_ISD_controlDataLSBPosition_forSinglePK                   16

            //      4. 자극  패킷구성은 다음과 같다.
            //        - 상위 16 bit
            //              [isd header ] [ electrode index  ] | [ stimuls Ampiltude ( 상위 8bit/9bit) ]
            //                  3bit     |         5bit        |               8bit
            //        - 하위 17 bit
            //              [ stimuls Ampiltude (하위  1bit/9bit) ] | [ Zero Padding ]
            //                            1bit                   |          16bit
                    // 상위 16it
                        #define     df_ISD_ElectrodePosition_forSinglePK                            8
                        #define     df_ISD_StimulsRoundOffBitLength_forSinglePK                     1        // ISD Kennel 데이터 상위 16비트 기준에서 포함되지 않고 잘려서 하위 17bit에 포함되는 데이터 길이
                    // 하위 17bit
                        #define     df_ISD_StimulationLevelLSB_Position_forSinglePK                 16


            ///////////////////////////////////////////////////
            // 레지스터 피트 필드

                // 전원 설정 레지스터

                // 전원 상태 레지스터
                // 전원 상태 레지스터
                    #define          df_ISD_powerStateRegisterBitFieldPosition_DCPowerOutput             2
                    #define          df_ISD_powerStateRegisterBitFieldPosition_RectifierOutput           0

                    #define df_bitClear_ISD_powerStateRegister_DCPowerOutput                 (~(0x3 << df_ISD_powerStateRegisterBitFieldPosition_DCPowerOutput))
                    #define df_bitClear_ISD_powerStateRegister_RectifierOutput               (~(0x3 << df_ISD_powerStateRegisterBitFieldPosition_RectifierOutput))
                    #define df_bitSet_ISD_powerStateRegister_DCPowerOutput                 ((0x3 << df_ISD_powerStateRegisterBitFieldPosition_DCPowerOutput))
                    #define df_bitSet_ISD_powerStateRegister_RectifierOutput               ((0x3 << df_ISD_powerStateRegisterBitFieldPosition_RectifierOutput))

                        #define          df_ISD_powerStateRegister_DCPowerOutput_Ok_18V                  2
                        #define          df_ISD_powerStateRegister_DCPowerOutput_Low_9V                  1
                        #define          df_ISD_powerStateRegister_DCPowerOutput_Low_5V                  0

                        #define          df_ISD_powerStateRegister_RectifierOutput_Ok_3to4V              0
                        #define          df_ISD_powerStateRegister_RectifierOutput_Low_2to3V             1
                        #define          df_ISD_powerStateRegister_RectifierOutput_Lower_2V              2
                        #define          df_ISD_powerStateRegister_RectifierOutput_High_4V               3


                        #define  df_ISD_registerValue_PowerOk                         0x0000
                        #define  df_ISD_registerValue_PowerIsLow                      0x0002
                        #define  df_ISD_registerValue_PowerIsHigh                     0x0003
                // 클럭 및 리셋 설정 레지스터

                // ADC 설정 레지스터

                // 프리엠프(for ADC) 설정 레지스터

                // 자극 설정 레지스터
                    #define          df_ISD_stimulusConfigRegisterBitFieldPosition_Stabilization                    8
                    #define          df_ISD_stimulusConfigRegisterBitFieldPosition_DacInterval                      4
                    #define          df_ISD_stimulusConfigRegisterBitFieldPosition_ReferencElectrodeConnection      3
                    #define          df_ISD_stimulusConfigRegisterBitFieldPosition_VirtuarChannel                   2
                    #define          df_ISD_stimulusConfigRegisterBitFieldPosition_Polarity                         1
                    #define          df_ISD_stimulusConfigRegisterBitFieldPosition_firstPhase                       0






            // nop로 동작 시킬 때 사용할 전극
                    #define         df_StimulusElectrodeAsNop       0


#endif
