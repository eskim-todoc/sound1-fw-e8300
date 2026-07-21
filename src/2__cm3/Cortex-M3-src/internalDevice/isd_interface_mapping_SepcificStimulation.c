

#include <hw.h>
#include <isdExecution/driver_PCM.h>
#include <stdbool.h>
#include "tdc_sys_error.h"
#include "FPGA.h"
#include "isd_interface_FPGA.h"
#include "internalStimulationChip.h"
#include "isd_interface.h"
#if 0
#include "tdc_hal_i2c_cfx.h"
#else
#include "tdc_hal_i2c_isd.h"
#endif
#include "definitionsForAlgorithm.h"

#include "isd_interface.h"
#include "isd_interface_init_FPGA.h"
#include "cfx_cm3_sharedMemory.h"
#include "tdc_ble_mapping.h"
#include "tdc_hal_spi.h"
#include "stimulationParaCal.h"

#include "electrodeMapping.h"

// int tempBuff[24];

void specificStimulation(bool startFlag)
{
    int last_fpga_settingValue;
    int w_FPGA_registerValue;
    int r_FPGA_registerValue;
    int w_isd_registerValue;

    static int stimulationTime_msec;
    static int flowCounter = 0;

    int pcm_index = 0;

    int i, k;

    // 계산되는 자극 파라미터
    static int numFramePerChannel;
    static int TransferabelChannelNum = df_MaxNumTransferableChannel;

    int deliveryCharge_pico;

    static int stimulDAC_slope;
    static int stimulDAC_offsetResolution;
    static int stimulDAC_offsetValue;
    static int stimulLevel_255;

    int stimulDAC_offsetValue_uA;
    int stimulLevel_uA;

    static int bipolarFIFO_index = 0;
    static int stimulationHoldTime_msec;
    static int cofigureDoneCounter = 100;

    static int pulseDurationNop_idex = 1;

    static int channel_index = 0;

    int bufferForSPI_tx[BLE_DataPacketSize];
    int buffer_tx_index = 0;

    static ST__MAPPING_PACKET const *mappingPacket;

    static int bipolarReferenceElectrodeNum[df_MaxNumOfElectrode];  // (2025.12.17 바이폴라 기능)

    int tempInt;

    int specific_command_idle_loop_i;         // by 김은수
    int specific_command_idle_check_counter;  // by 김은수

    if (startFlag)
    {
        flowCounter          = 0;
        stimulationTime_msec = 0;
        channel_index        = 0;
    }

    switch (flowCounter)
    {
        case 0:
        {
            // 이전에 전송된 자극 파라미터 값이 있을 수 있기 때문에 출력을 내보내기 위하여 설정 파라미터들 전송한다.
            changePcmOutputMode(PcmBitStream_Mode_NopStandby);

            // 펄스폭에 따른 자극 프레임 갯수 계산
            mappingPacket      = tdc_ble_mapping_get_packet();
            numFramePerChannel = calculationNumFramePerOneChannle(mappingPacket->specificStimulation.pulseWidth);

            // 사용가능한 채널 수는 24채널 보다 적고 펄스폭은 길게 설정할 경우... 가능하다면 1msec에 1회의 출력이 나올 수 있도록...
            TransferabelChannelNum = calculationTransferableChanneNum(numFramePerChannel, mappingPacket->specificStimulation.usableElectrodeNum);

            // 최대 전하량 계산

            // usec단위... (time*10^-6}*{level*10^-6) //마스커 자극값을 기준으로 계산한다.
            deliveryCharge_pico = (mappingPacket->specificStimulation.pulseWidth) * mappingPacket->specificStimulation.stimulationLevel_uA;

            if (deliveryCharge_pico > df_MaxDeliveryCharge_pC)  // 전하량 초과
            {
                tdc_sys_error_send_to_app(en__mapping_specific_stimulation, en__EN__ISD_ERROR, en__MaxChargeOver, __LINE__);  // 에러 전송
                tdc_ble_mapping_clear_command();                                                                            // 커맨드 리셋;
            }

            // 자극 슬로프 및 자극 데이터. 계산 stimulLevel_uA stimulLevel_uA stimulDAC_offsetValue_uA
            // 마스커에 대한 것
            if (mappingPacket->specificStimulation.stimulationLevel_uA < (stimulDAC_A_only_Saturation_uA + offsetDAC_B_Saturation_uA))  // 2uA 기울기로 전달 가능한 범위내. // 1530 보다 작은 경우
            {
                // 자극 DAC 기울기 2uA
                stimulDAC_slope = Stimulation_DAC_A;

                if (mappingPacket->specificStimulation.stimulationLevel_uA > (stimulDAC_A_only_Saturation_uA + offsetDAC_A_Saturation_uA))  // 1020~1530  - > 1020 오프셋을 적용해야 되는 경우
                {
                    stimulDAC_offsetResolution = Offset_DAC_B;
                    stimulDAC_offsetValue_uA   = offsetDAC_B_Saturation_uA;
                    tempInt                    = stimulDAC_offsetValue_uA * reciprocal_dividing_QI1F15_Offset_DAC_B;
                    stimulDAC_offsetValue      = tempInt >> 15;

                    stimulLevel_uA  = mappingPacket->specificStimulation.stimulationLevel_uA - stimulDAC_offsetValue_uA;
                    tempInt         = stimulLevel_uA * reciprocal_dividing_QI1F15_Stimulation_DAC_A;
                    stimulLevel_255 = tempInt >> 15;
                }
                else  // 1020보다 작은 경우
                {
                    //
                    if (mappingPacket->specificStimulation.stimulationLevel_uA > stimulDAC_A_only_Saturation_uA)  // 510 ~ 1020 -> 510 오프셋을 적용한다.
                    {
                        stimulDAC_offsetResolution = Offset_DAC_A;
                        stimulDAC_offsetValue_uA   = offsetDAC_A_Saturation_uA;  //
                        tempInt                    = stimulDAC_offsetValue_uA * reciprocal_dividing_QI1F15_Offset_DAC_A;
                        stimulDAC_offsetValue      = tempInt >> 15;

                        stimulLevel_uA  = mappingPacket->specificStimulation.stimulationLevel_uA - stimulDAC_offsetValue_uA;
                        tempInt         = stimulLevel_uA * reciprocal_dividing_QI1F15_Stimulation_DAC_A;
                        stimulLevel_255 = tempInt >> 15;
                    }
                    else  // 오프셋 적용을 안해도 되는 경우.
                    {
                        stimulDAC_offsetResolution = Offset_DAC_A;
                        stimulDAC_offsetValue_uA   = 0;  //
                        stimulDAC_offsetValue      = 0;

                        stimulLevel_uA  = mappingPacket->specificStimulation.stimulationLevel_uA - stimulDAC_offsetValue_uA;
                        tempInt         = stimulLevel_uA * reciprocal_dividing_QI1F15_Stimulation_DAC_A;
                        stimulLevel_255 = tempInt >> 15;
                    }
                }
            }
            else  // 6uA로  자극
            {
                // 자극 DAC 기울기 6uA
                stimulDAC_slope = Stimulation_DAC_C;

                stimulDAC_offsetResolution = Offset_DAC_A;
                stimulDAC_offsetValue_uA   = offsetDAC_A_Saturation_uA;
                tempInt                    = stimulDAC_offsetValue_uA * reciprocal_dividing_QI1F15_Offset_DAC_A;
                stimulDAC_offsetValue      = tempInt >> 15;

                stimulLevel_uA  = mappingPacket->specificStimulation.stimulationLevel_uA - stimulDAC_offsetValue_uA;
                tempInt         = stimulLevel_uA * reciprocal_dividing_QI1F15_Stimulation_DAC_C;
                stimulLevel_255 = tempInt >> 15;
            }

            if (stimulLevel_255 >= 255)
            {
                stimulLevel_255 = 255;
            }

            stimulationHoldTime_msec = mappingPacket->specificStimulation.stimulationTime_100msec * 100;
        }
        break;

        case 1:
        {
            // 펄스 폭 0으로 설정
#if 0
                w_FPGA_registerValue=0;  // 펄스폭 0
                w_FPGA_registerValue=(w_FPGA_registerValue|pcm_Mold_PulsePhaseWidth);
                fillSepcificCommndBuffer(pcm_index++,w_FPGA_registerValue);
#else
            chang_PulseWidth_minimum(pcm_index++);
            upadte_fpga_pulsePhaseWidth_written_Value(FPGA_pulsePhaseWidth_minimum);
#endif

#if 0
                    // ISD path 확인용 임의의 값
                    w_isd_registerValue = ISD_registerAddr_forwardPath_check;
                    w_isd_registerValue = w_isd_registerValue << 1;
                    w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
                    w_isd_registerValue = w_isd_registerValue << 8;

                    w_isd_registerValue = w_isd_registerValue | df_forwardPathCheck_arbitraryValue;
                    w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;
                    fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
#else
            change_8BitBacktel_mode(pcm_index++);        // 8비트 백텔 설정 (2025.12.17 바이폴라 기능)
            fill_pcmBuff_lastSimulationOut(&pcm_index);  // 이전 자극 파라미터를 출력하기 위한 임의의 값 출력
#endif
            for (; pcm_index < df_MaxNumTransferableChannel;)
            {
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            }

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand

            if (mappingPacket->specificStimulation.stimulatonMode != en__bipolar)
            {
                flowCounter = flowCounter + 6;  // 바이폴라 기준전극 설정 값 전송에 6msec 필요, 해당 루틴 생략 (2025.12.17 바이폴라 기능)

                // 1 + 6 = 7; case 7로 강제 설정한 뒤,
                // 마지막에 flowCounter++; 수행하여 다음 진입 시 flowCounter가 8인 상태로 만든다.
                // 즉, 바이폴라 설정 과정을 생략할 때 적용되는 것이다.
            }
        }
        break;

        case 3:  // (2025.12.17 바이폴라 기능)
        {
            for (i = 0; i < df_MaxNumOfElectrode; i++)
            {
                bipolarReferenceElectrodeNum[i] = unusedReferenceElectrode_DummyNum;
            }

            bipolarReferenceElectrodeNum[electrodeMap[mappingPacket->specificStimulation.stimulationElectrodeNum - 1]] =  // 코드 길어서 line wrapping
                electrodeMap[mappingPacket->specificStimulation.bipolarReferenceElectrodeNum - 1];

            // Bipolar 기준전극 FIFO 지우기
            w_isd_registerValue = ISD_registerAddr_cipherDataStatus;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
            w_isd_registerValue = w_isd_registerValue << 8;

            w_isd_registerValue = w_isd_registerValue | 0x80;  // CHIP_ID_FIFO_RDDATA_INDEX에 아무값이나 쓰면 FIFO가 지워진다.
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

            for (; pcm_index < df_MaxNumTransferableChannel;)
            {
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            }

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        case 5:  // (2025.12.17 바이폴라 기능)
        {
            // 기준 전극 번호.
            for (i = 0; i < df_MaxNumTransferableChannel; i++)  // 1~24번 자극 전극에 대응하는 기준 전극 번호
            {
                w_isd_registerValue = ISD_registerAddr_en__bipolar_referenceElectroldIndex;
                w_isd_registerValue = w_isd_registerValue << 1;
                w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
                w_isd_registerValue = w_isd_registerValue << 8;

                w_isd_registerValue = w_isd_registerValue | bipolarReferenceElectrodeNum[i];
                w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

                fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
            }

            for (; pcm_index < df_MaxNumTransferableChannel;)
            {
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            }

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        case 7:  // (2025.12.17 바이폴라 기능)
        {
            // 기준 전극 번호.
            for (i = 24; i < 32; i++)  // 24~32번 자극 전극에 대응하는 기준 전극 번호
            {
                w_isd_registerValue = ISD_registerAddr_en__bipolar_referenceElectroldIndex;
                w_isd_registerValue = w_isd_registerValue << 1;
                w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
                w_isd_registerValue = w_isd_registerValue << 8;

                w_isd_registerValue = w_isd_registerValue | bipolarReferenceElectrodeNum[i];
                w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

                fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
            }

            for (; pcm_index < df_MaxNumTransferableChannel;)
            {
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            }

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        case 9:  // 자극 파라미터.
        {
            // 자극 출력 DAC 설정
            // ISD 0x5 - DAC Offset "쓰기"
            w_isd_registerValue = ISD_registerAddr_DAC_offsetValue;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
            w_isd_registerValue = w_isd_registerValue << 8;

            w_isd_registerValue = w_isd_registerValue | stimulDAC_offsetValue;  // 마스커 인덱스 값
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);  //      | 0x50000

            // ISD 0x6 - 자극 파라미터 설정 "쓰기"
            w_isd_registerValue = ISD_registerAddr_StimulationConfig;

            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;

            // 0, 7번비트
            w_isd_registerValue = w_isd_registerValue << 1;

            // offsetResolution 6번 비트
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | stimulDAC_offsetResolution;

            // DAC slope 4,5번 비트
            w_isd_registerValue = w_isd_registerValue << 2;
            w_isd_registerValue = w_isd_registerValue | stimulDAC_slope;

            // 모노폴라 출력 모드에서 기준전극 모드 2,3번 비트
            w_isd_registerValue = w_isd_registerValue << 2;
            switch (mappingPacket->specificStimulation.stimulatonMode)  // 리셋값이 3이며, 바이폴라, 공통접지, 동시 자극의 경우는 접지를 끊는다.
            {
                case en__monopolr_body:
                    w_isd_registerValue = w_isd_registerValue | en__monopolr_body;
                    break;
                case en__monopolr_rod:
                    w_isd_registerValue = w_isd_registerValue | en__monopolr_rod;
                    break;
                case en__monopolr_BothRodBody:
                    w_isd_registerValue = w_isd_registerValue | en__monopolr_BothRodBody;
                    break;
                default:
                    w_isd_registerValue = w_isd_registerValue | en__referenceNA;
                    break;
            }

            // 자극 출력 모드 0,1번 비트
            w_isd_registerValue = w_isd_registerValue << 2;
            switch (mappingPacket->specificStimulation.stimulatonMode)
            {
                case en__monopolr_body:
                case en__monopolr_rod:
                case en__monopolr_BothRodBody:
                    w_isd_registerValue = w_isd_registerValue | 0;
                    break;
                case en__bipolar:
                    w_isd_registerValue = w_isd_registerValue | 1;
                    break;
                case en__commonground:
                    w_isd_registerValue = w_isd_registerValue | 2;
                    break;
                case en__semi_simultaneously:
                    w_isd_registerValue = w_isd_registerValue | 3;
                    break;
            }

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;  //  | 0x50000

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

            // 펄스 폭 조정
#if 0
            w_FPGA_registerValue=(mappingPacket->specificStimulation.pulseWidth-FPGA_pulsePhaseWidth_minimum);
            w_FPGA_registerValue=(w_FPGA_registerValue|pcm_Mold_PulsePhaseWidth);
            fillSepcificCommndBuffer(pcm_index++, w_FPGA_registerValue);
#else
            chang_PulseWidth(pcm_index++, mappingPacket->specificStimulation.pulseWidth);
            upadte_fpga_pulsePhaseWidth_written_Value(FPGA_pulsePhaseWidth_minimum);
#endif

            // 나머지 버퍼는 NOP-Standby
            for (; pcm_index < df_MaxNumTransferableChannel;)
            {
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            }

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand

            // 임시 디버깅 용
            bipolarFIFO_index = 1;
        }
        break;

        case 14:
        {
            if (read_FPGA_PulseWidth(&r_FPGA_registerValue))
            {
                if (r_FPGA_registerValue == mappingPacket->specificStimulation.pulseWidth)
                {
                    cofigureDoneCounter = flowCounter;
                }
                else
                {
                    tdc_sys_error_send_to_app(en__mapping_specific_stimulation, en__FPGA_CONFIGUARATION_ERROR, en_PulseWidthDifferent, __LINE__);
                    tdc_ble_mapping_clear_command();  // 커맨드 리셋;

                    if (read_FPGA_systemError_Flag(&r_FPGA_registerValue))
                    {
                        if ((0x0F & r_FPGA_registerValue) != 0)
                        {
                            change_isd_state(en__isdStatus_PowerIC_OK);
                        }
                    }
                }
            }
        }
        break;

#if 1
        // 어떻게 처리되는 건지 디버그 메시지 출력을 위한 별도의 case 문
        case 20:
        {
            TDC_PRINTF_I("[SPEC] USABLE ELEC NUM : %d \r\n", mappingPacket->specificStimulation.usableElectrodeNum);
            TDC_PRINTF_I("[SPEC] PULSE WIDTH : %d \r\n", mappingPacket->specificStimulation.pulseWidth);
            TDC_PRINTF_I("[SPEC] FRAME NUM PER 1 CH : %d \r\n", numFramePerChannel);
            TDC_PRINTF_I("[SPEC] TRANSFERABLE CH NUM PER 1 MS : %d \r\n", TransferabelChannelNum);
            TDC_PRINTF_I("[SPEC] STIM LEVEL (UA) : %d \r\n", mappingPacket->specificStimulation.stimulationLevel_uA);
            TDC_PRINTF_I("[SPEC] DELIVERY CHARGE PICO : %d \r\n",  //
                      (mappingPacket->specificStimulation.pulseWidth) * mappingPacket->specificStimulation.stimulationLevel_uA);
            TDC_PRINTF_I("[SPEC] STIM DAC SLOPE : %d \r\n", stimulDAC_slope);
            TDC_PRINTF_I("[SPEC] STIM DAC VALUE : %d \r\n", stimulLevel_255);
            TDC_PRINTF_I("[SPEC] OFFSET DAC RESOLUTION : %d \r\n", stimulDAC_offsetResolution);
            TDC_PRINTF_I("[SPEC] OFFSET DAC VALUE : %d \r\n", stimulDAC_offsetValue);
            TDC_PRINTF_I("[SPEC] STIM HOLD TIME (MS)  : %d \r\n", stimulationHoldTime_msec);
            TDC_PRINTF_I("[SPEC] REF CH MODE : %s \r\n",  //
                      mappingPacket->specificStimulation.stimulatonMode == en__monopolr_body          ? "MP-B"
                      : mappingPacket->specificStimulation.stimulatonMode == en__monopolr_rod         ? "MP-R"
                      : mappingPacket->specificStimulation.stimulatonMode == en__monopolr_BothRodBody ? "MP-R&B"
                                                                                                      : "BP, CG, ETC...");
            TDC_PRINTF_I("[SPEC] STIM MODE : %s \r\n",  //
                      mappingPacket->specificStimulation.stimulatonMode == en__monopolr_body          ? "MP-B"
                      : mappingPacket->specificStimulation.stimulatonMode == en__monopolr_rod         ? "MP-R"
                      : mappingPacket->specificStimulation.stimulatonMode == en__monopolr_BothRodBody ? "MP-R&B"
                      : mappingPacket->specificStimulation.stimulatonMode == en__bipolar              ? "BP"
                      : mappingPacket->specificStimulation.stimulatonMode == en__commonground         ? "CG"
                      : mappingPacket->specificStimulation.stimulatonMode == en__semi_simultaneously  ? "SIMULTANEOUSLY"
                                                                                                      : "INVALID");
            if (mappingPacket->specificStimulation.stimulatonMode == en__bipolar)
            {
                TDC_PRINTF_I("[SPEC] BIPOLAR REF CH NUM : %d (PCB : %d) \r\n",  //
                          mappingPacket->specificStimulation.bipolarReferenceElectrodeNum,
                          bipolarReferenceElectrodeNum[electrodeMap[(mappingPacket->specificStimulation.stimulationElectrodeNum - 1)]]);
            }
            TDC_PRINTF_I("[SPEC] STIM CH NUM : %d (PCB : %d ) \r\n",  //
                      mappingPacket->specificStimulation.stimulationElectrodeNum,
                      electrodeMap[(mappingPacket->specificStimulation.stimulationElectrodeNum - 1)]);
        }
        break;
#endif

        default:
            break;
    }

    if (flowCounter > cofigureDoneCounter)  // 자극 설정이 완료되었으며 자극출력을 시작한다.
    {
        if (stimulationTime_msec >= stimulationHoldTime_msec)  // 자극 유지시간 도래.
        {
            // 마지막 자극을 위하여 자극 출력
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);

            chang_PulseWidth_minimum(pcm_index++);
            upadte_fpga_pulsePhaseWidth_written_Value(FPGA_pulsePhaseWidth_minimum);

            // 이전 자극 파라미터를 출력하기 위한 임의의 값 출력
            fill_pcmBuff_lastSimulationOut(&pcm_index);

            for (; pcm_index < df_MaxNumTransferableChannel;)
            {
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            }

            // 출력 완료.

            // 송신 데이터 준비
            // command loop-back
            bufferForSPI_tx[buffer_tx_index++] = mappingPacket->command;

            // pay-load 준비
            bufferForSPI_tx[buffer_tx_index++] = 1;  // 자극 출력 성공

            // 송신 데이터 SPI TX버퍼에 복사

            tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

            // 커맨드 리셋;
            tdc_ble_mapping_clear_command();

            TDC_PRINTF_I("[MAPPING] STIMULATION HOLD TIME FINISHED \r\n");
        }
        else
        {
            // SpecificCommand 체크 카운터가 1 이상이면, CFX가 SpecificCommand를 읽는 중임
            for (specific_command_idle_loop_i = 0; specific_command_idle_loop_i < 50; specific_command_idle_loop_i++)
            {
                specific_command_idle_check_counter = 0;
                Sys_Delay(SystemCoreClock / 1000000);  // 1usec
                specific_command_idle_check_counter += cfx_cm3_sharedMemoryAll.is_pcm_specific_command_reading;
                Sys_Delay(SystemCoreClock / 1000000);  // 1usec
                specific_command_idle_check_counter += cfx_cm3_sharedMemoryAll.is_pcm_specific_command_reading;

                if (specific_command_idle_check_counter == 0)
                {
                    // CFX가 SpecificCommand를 읽고 있지 않음
                    break;
                }

                TDC_PRINTF_I("[MAPPING] NOW, CFX IS READING SPECIFIC COMMAND (IDLE LOOP CNT: %d) \r\n", specific_command_idle_loop_i);
            }

            if (50 <= specific_command_idle_loop_i)
            {
                TDC_PRINTF_E("[MAPPING] SPECIFIC COMMAND TIMING ISSUE OCCURRED \r\n");
            }

            for (i = 0; i < TransferabelChannelNum; i++)  //
            {
                if (channel_index == 0)  // 해당자극 출력
                {
                    w_isd_registerValue = mappingPacket->specificStimulation.firstPulsePhase << firstPulsePhasePositionAtPCM_Mold;                                                    // 선행 펄스
                    w_isd_registerValue = w_isd_registerValue | (electrodeMap[(mappingPacket->specificStimulation.stimulationElectrodeNum - 1)] << electrodIndexPositionAtPCM_Mold);  // 자극 전극 번호
                    w_isd_registerValue = w_isd_registerValue | (stimulLevel_255 << stimulationPositionAtPCM_Mold);                                                                   // 자극 출력 크기
                    w_isd_registerValue = w_isd_registerValue | pcm_Mold_Stimulation;

                    // tempBuff[pcm_index]=w_isd_registerValue;
                    fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

                    bipolarFIFO_index++;  // 임시 디버깅용
                }
                else  //
                {
                    fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);  // 다른 채널 자극 시점..
                }

                // 펄스 폭에 맞추어 NOP
                for (pulseDurationNop_idex = 1; pulseDurationNop_idex < numFramePerChannel; pulseDurationNop_idex++)
                {
                    fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
                }

                channel_index++;

#if 1
                if (mappingPacket->specificStimulation.usableElectrodeNum > TransferabelChannelNum)
                {

                    if (channel_index >= mappingPacket->specificStimulation.usableElectrodeNum)
                    {
                        channel_index = 0;
                    }
                }
                else
                {
                    if (channel_index >= TransferabelChannelNum)
                    {
                        channel_index = 0;
                    }
                }
#else

                if (channel_index >= mappingPacket->specificStimulation.usableElectrodeNum)
                    channel_index = 0;

#endif
            }

            //
            for(;pcm_index < df_MaxNumTransferableChannel;)
            {
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            }

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // 다음 출력 모드 : NopStandby
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }

        stimulationTime_msec++;
    }

    flowCounter++;
}
