

#include <hw.h>
#include <isdExecution/dirver_PCM.h>
#include <stdbool.h>
#include "error.h"
#include "FPGA.h"
#include "isd_interface_init_FPGA.h"
#include "internalStimulationChip.h"
#include "isd_interface.h"
#if 0
#include "driver_cfx_i2c.h"
#else
#include "dirver_i2c_for_ISD.h"
#endif
#include "definitionsForAlgorithm.h"

#include "isd_interface.h"
#include "isd_interface_FPGA.h"
#include "cfx_cm3_sharedMemory.h"
#include "mappingControl.h"
#include "driver_SPI.h"
#include "stimulationParaCal.h"
#include "electrodeMapping.h"

#define TestBackTel

#ifndef RELEASE

bool testStimulation(bool startFlag)
{

    // 계산되는 자극 파라미터
    static int numFramePerChannel;
    static int TransferabelChannelNum = df_MaxNumTransferableChannel;
    static int stimulDAC_slope;
    static int stimulDAC_offsetResolution;

    int deliveryCharge_pico;

    int stimulDAC_offsetValue_uA;
    int stimulLevel_uA;

    ///

    int i;
    int w_FPGA_registerValue;
    int bitReverse;
    int r_FPGA_registerValue;
    int w_isd_registerValue;
    int r_isd_registerValue;
    int fifoCounter;
    int compare;
    int pcm_index = 0;
    int backtelBuff[64];
    int stimulElectrodeNum;

    int pulseDurationNop_idex;

    static int stimulationTime_msec;
    static int stimulationHoldTime_msec;
    static int stimulPCM_buff[df_MaxNumOfElectrode];
    static int trasnferChannel_Index  = 0;
    static int referencElectrod_index = 0;
    static int bipolarReferenceElectrodeNum[df_MaxNumOfElectrode];

    static int flowControlCounter  = 0;
    static int cofigureDoneCounter = 100;
    static int sent_stimulConfig;

    //

    static channel_index = 0;

    int  bufferForSPI_tx[BLE_DataPacketSize];
    int  buffer_tx_index;
    bool stimulationConfigError;
    bool isdSettingError;
    int  temp;
    bool FPGA_FIFO_empty;

    static ST__MAPPING_PACKET const *mappingPacket;

    if (startFlag)
    {

        flowControlCounter     = 0;
        referencElectrod_index = 0;

        trasnferChannel_Index = 0;
        stimulationTime_msec  = 0;
        buffer_tx_index       = 0;
        cofigureDoneCounter   = 1000;
    }

    pcm_index = 0;

    switch (flowControlCounter)
    {
        case 0:
        {

            // 이전에 전송된 자극 파라미터 값이 있을 수 있기 때문에 출력을 내보내기 위하여 설정 파라미터들 전송한다.

            changePcmOutputMode(PcmBitStream_Mode_NopStandby);

            // 펄스폭에 따른 자극 프레임 갯수 계산

            mappingPacket = getMappingPacket();

            numFramePerChannel = calculationNumFramePerOneChannle(mappingPacket->testStimulation.pulseWidth);

            // 사용가능한 채널 수는 24채널 보다 적고 펄스폭은 길게 설정할 경우... 가능하다면 1msec에 1회의 출력이 나올 수 있도록..

            TransferabelChannelNum = calculationTransferableChanneNum(numFramePerChannel, mappingPacket->testStimulation.usableElectrodeNum);

            // 자극 DAC 기울기
            switch (mappingPacket->testStimulation.stimulationDacSlope)
            {
                case 2:
                {
                    stimulDAC_slope = Stimulation_DAC_A;
                }
                break;
                case 4:
                {
                    stimulDAC_slope = Stimulation_DAC_B;
                }
                break;
                case 6:
                {
                    stimulDAC_slope = Stimulation_DAC_C;
                }
                break;
                case 8:
                {
                    stimulDAC_slope = Stimulation_DAC_D;
                }
                break;

                default:
                    // 에러 전송
                    sendErrorToApp(en__mapping_testStimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
                    // 커맨드 리셋;
                    clear_mappingCommand();
                    break;
            }

            //  자극 DAC Offset 해상도

            switch (mappingPacket->testStimulation.stimulationDacOffsetReslution)
            {
                case 2:
                {
                    stimulDAC_offsetResolution = Offset_DAC_A;
                }
                break;
                case 4:
                {
                    stimulDAC_offsetResolution = Offset_DAC_B;
                }
                break;

                default:
                    // 에러 전송
                    sendErrorToApp(en__mapping_testStimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
                    // 커맨드 리셋;
                    clear_mappingCommand();
                    break;
            }

            // 최대 전하량 계산
            stimulDAC_offsetValue_uA = stimulDAC_offsetResolution * mappingPacket->testStimulation.stimulationDacOffset_255;
            stimulLevel_uA           = stimulDAC_slope * mappingPacket->testStimulation.stimulationLevel_255;

            deliveryCharge_pico = (mappingPacket->testStimulation.pulseWidth) *
                                  (stimulLevel_uA + stimulDAC_offsetValue_uA); // usec단위.. (time*10^-6}*{level*10^-6) //마스커 자극값을 기준으로 계산한다.
            if (deliveryCharge_pico > df_MaxDeliveryCharge_pC)                 // 전하량 초과
            {
                // 에러 전송
                sendErrorToApp(en__mapping_testStimulation, en__EN__ISD_ERROR, en__MaxChargeOver, __LINE__);
                // 커맨드 리셋;
                clear_mappingCommand();
            }

            for (i = 0; i < df_MaxNumOfElectrode; i++)
                stimulPCM_buff[i] = pcm_Mold_NopStandby;

            for (i = 0; i < mappingPacket->testStimulation.usableElectrodeNum; i++)
            {

                w_isd_registerValue = mappingPacket->testStimulation.firstPulsePhase << firstPulsePhasePositionAtPCM_Mold; // 선행 펄스 ;
                w_isd_registerValue = w_isd_registerValue | (electrodeMap[(mappingPacket->testStimulation.stimulationElectrodeNum[i] - 1)]
                                                             << electrodIndexPositionAtPCM_Mold); // 자극 전극 번호
                w_isd_registerValue =
                    w_isd_registerValue | (mappingPacket->testStimulation.stimulationLevel_255 << stimulationPositionAtPCM_Mold); // 자극 출력 크기
                w_isd_registerValue = w_isd_registerValue | pcm_Mold_Stimulation;
                stimulPCM_buff[i]   = w_isd_registerValue;
            }

            // 자극 유지 시간
            stimulationHoldTime_msec = mappingPacket->testStimulation.stimulationTime_100msec * 100;

            // 바이폴라 기준 전극 버퍼 구성
            for (i = 0; i < df_MaxNumOfElectrode; i++)
                bipolarReferenceElectrodeNum[i] = unusedReferenceElectrode_DummyNum;

            for (i = 0; i < df_MaxNumOfElectrode; i++)
            {
                if (mappingPacket->testStimulation.stimulationElectrodeNum[i] != 99)
                {
                    stimulElectrodeNum = mappingPacket->testStimulation.stimulationElectrodeNum[i] - 1;
                    if (mappingPacket->testStimulation.bipolarReferenceElectrodeNum[i] != 99)
                        bipolarReferenceElectrodeNum[stimulElectrodeNum] = mappingPacket->testStimulation.bipolarReferenceElectrodeNum[i] - 1;
                }
            }

            if (write_FPGA_clear_FIFO())
            {

                // FPGA 상태를 읽어 본다.
                if (check_FPGA_FIFO_empty(&FPGA_FIFO_empty)) // 지워 졌는지 확인.
                {
                    if (!FPGA_FIFO_empty)
                    {

                        sendErrorToApp(en__mapping_testStimulation, en__FPGA_CONFIGUARATION_ERROR, en_FIFO_NotCleared, __LINE__);
                        // 커맨드 리셋;
                        clear_mappingCommand();

                        change_isd_state(en__isdStatus_PowerIC_OK); //
                    }
                }
                else
                {
                    sendErrorToApp(en__mapping_testStimulation, en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_WritingError, __LINE__);
                    // 커맨드 리셋;
                    clear_mappingCommand();

                    change_isd_state(en__isdStatus_PowerIC_OK); //
                }
            }
            else
            {
                sendErrorToApp(en__mapping_testStimulation, en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_WritingError, __LINE__);
                // 커맨드 리셋;
                clear_mappingCommand();

                change_isd_state(en__isdStatus_PowerIC_OK); //
            }
        }
        break;

        case 1:
        {
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);

            // 펄스 폭 0으로 설정
            chang_PulseWidth_minimum(pcm_index++);
            upadte_fpga_pulsePhaseWidth_written_Value(FPGA_pulsePhaseWidth_minimum);

            // 8bit backter 설정
            change_8BitBacktel_mode(pcm_index++);

            // 이전 자극 파라미터를 출력하기 위한 임의의 값 출력
            fill_pcmBuff_lastSimulationOut(&pcm_index);

            for (; pcm_index < df_MaxNumTransferableChannel;)
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);

            if (mappingPacket->testStimulation.stimulatonMode != en__bipolar)
            {
                flowControlCounter = 20; // 바이폴라 기준 전극 전송 및 확인 과정 생략
            }

            // NOP-Standby
            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);
        }
        break;

            ////// 바이폴라 모드일 경우, 바이폴라 기준 전극 번호 설정

        case 2:
        {
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);

            // Bipolar 기준전극  FIFO 지우기
            w_isd_registerValue = ISD_registerAddr_cipherDataStatus;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
            w_isd_registerValue = w_isd_registerValue << 8;

            w_isd_registerValue = w_isd_registerValue | 0x80; // CHIP_ID_FIFO_RDDATA_INDEXdp 아무값이나 쓰면 FIFO가 지워진다.
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

            for (; pcm_index < df_MaxNumTransferableChannel;)
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            // NOP-Standby
            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);
        }
        break;

        case 3:
        {

            // PCM 출력 모드 변경
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);

            for (i = 0; i < df_MaxNumTransferableChannel; i++) // 1~24번 자극채널에  대응하는 기준 전극 번호
            {
                w_isd_registerValue = ISD_registerAddr_en__bipolar_referenceElectroldIndex;
                w_isd_registerValue = w_isd_registerValue << 1;
                w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
                w_isd_registerValue = w_isd_registerValue << 8;

                w_isd_registerValue = w_isd_registerValue | electrodeMap[bipolarReferenceElectrodeNum[i]];
                w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

                fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
            }

            for (; pcm_index < df_MaxNumTransferableChannel;)
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);

            // NOP-Standby
            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);
        }
        break;

        case 4:
        {
            // PCM 출력 모드 변경
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);

            for (i = 24; i < 32; i++) // 24번~30 자극채널에  대응하는 기준 전극 번호
            {
                w_isd_registerValue = ISD_registerAddr_en__bipolar_referenceElectroldIndex;
                w_isd_registerValue = w_isd_registerValue << 1;
                w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
                w_isd_registerValue = w_isd_registerValue << 8;

                w_isd_registerValue = w_isd_registerValue | electrodeMap[bipolarReferenceElectrodeNum[i]];
                w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

                fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
            }

            for (; pcm_index < df_MaxNumTransferableChannel;)
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);

            // NOP-Standby
            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);
        }
        break;

#ifndef DisalbedBackTel

        case 10: // PCM 출력 - 바이폴라 기준 전극 값 확인 - FIFO 값 확인..0~5
        {

            // PCM 출력 모드 변경
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);

            w_isd_registerValue = ISD_registerAddr_en__bipolar_referenceElectroldIndex;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_readRegister;
            w_isd_registerValue = w_isd_registerValue << 8;

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            for (i = 0; i < 6; i++)
            {
                fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
            }

            for (; pcm_index < df_MaxNumTransferableChannel;)
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);

            // NOP-Standby
            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);
        }
        break;

        case 11: // PCM 출력 - 바이폴라 기준 전극 값 확인 - FIFO 값 확인..6~11
        {
            // PCM 출력 모드 변경
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);

            w_isd_registerValue = ISD_registerAddr_en__bipolar_referenceElectroldIndex;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_readRegister;
            w_isd_registerValue = w_isd_registerValue << 8;

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            for (i = 0; i < 6; i++)
            {
                fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
            }

            for (; pcm_index < df_MaxNumTransferableChannel;)
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);

            // NOP-Standby
            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);
        }
        break;

        case 12: // PCM 출력 - 바이폴라 기준 전극 값 확인 - FIFO 값 확인..12~17
        {
            // PCM 출력 모드 변경
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);

            w_isd_registerValue = ISD_registerAddr_en__bipolar_referenceElectroldIndex;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_readRegister;
            w_isd_registerValue = w_isd_registerValue << 8;

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            for (i = 0; i < 6; i++)
            {
                fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
            }

            for (; pcm_index < df_MaxNumTransferableChannel;)
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);

            // NOP-Standby
            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);
        }
        break;

        case 13: // PCM 출력 - 바이폴라 기준 전극 값 확인 - FIFO 값 확인..18~23
        {
            // PCM 출력 모드 변경
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);

            pcm_index = 0;

            w_isd_registerValue = ISD_registerAddr_en__bipolar_referenceElectroldIndex;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_readRegister;
            w_isd_registerValue = w_isd_registerValue << 8;

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            for (i = 0; i < 6; i++)
            {
                fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
            }

            for (; pcm_index < df_MaxNumTransferableChannel;)
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);

            // NOP-Standby
            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);
        }
        break;

        case 14: // PCM 출력 - 바이폴라 기준 전극 값 확인 - FIFO 값 확인..24~29
        {
            // PCM 출력 모드 변경
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);

            pcm_index = 0;

            w_isd_registerValue = ISD_registerAddr_en__bipolar_referenceElectroldIndex;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_readRegister;
            w_isd_registerValue = w_isd_registerValue << 8;

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            for (i = 0; i < 6; i++)
            {
                fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
            }

            for (; pcm_index < df_MaxNumTransferableChannel;)
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);

            // NOP-Standby
            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);
        }
        break;

        case 15: // PCM 출력 - 바이폴라 기준 전극 값 확인 - FIFO 값 확인..30~31
        {
            // PCM 출력 모드 변경
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);

            pcm_index = 0;

            w_isd_registerValue = ISD_registerAddr_en__bipolar_referenceElectroldIndex;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_readRegister;
            w_isd_registerValue = w_isd_registerValue << 8;

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            for (i = 0; i < 2; i++)
            {
                fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
            }

            for (; pcm_index < df_MaxNumTransferableChannel;)
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);

            // NOP-Standby
            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);
        }
        break;

        case 20: // I2C 읽기 - 바이폴라 기준 전극 값 확인 - FIFO 값 확인..
        {

            for (i = 0; i < df_MaxNumOfElectrode; i++)
                backtelBuff[i] = 0;

            // nop=(cfx_i2c_read(df_I2C_ADDR_FPGA_FIFO_counter, &r_FPGA_registerValue, 1));
            if (read_FPGA_FIFO_counter(&r_FPGA_registerValue)) // 0
            {

                if (r_FPGA_registerValue != 0)
                {

                    // if (cfx_i2c_read(df_I2C_ADDR_FPGA_BackTel, backtelBuff, df_MaxNumOfElectrode))
                    if (read_FPGA_backtel_FIFO(backtelBuff, df_MaxNumOfElectrode))
                    {

                        for (i = 0; i < df_MaxNumOfElectrode - 1; i++)
                        {
                            if (electrodeMap[bipolarReferenceElectrodeNum[i]] != ((0x1F) & (backtelBuff[i])))
                            {
                                sendErrorToApp(en__mapping_testStimulation, en__EN__ISD_ERROR, en__SettingError_BipolarElectrodeNum, __LINE__);
                                // 커맨드 리셋;
                                clear_mappingCommand();

                                change_isd_state(en__isdStatus_FPGA_Ok); // 내부기 전송 파워 설정 부터 다시.
                                stimulationConfigError = true;
                            }
                        }
                    }
                }
                else
                {
                    sendErrorToApp(en__mapping_testStimulation, en__EN__ISD_ERROR, en__BackterDataLengthError, __LINE__);
                    // 커맨드 리셋;
                    clear_mappingCommand();
                    // 백텔이 안들어 왔다.
                    change_isd_state(en__isdStatus_FPGA_Ok); // 내부기 전송 파워 설정 부터 다시.
                }
            }
            else
            {
                sendErrorToApp(en__mapping_testStimulation, en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_ReadingError, __LINE__);
                // 커맨드 리셋;
                clear_mappingCommand();

                change_isd_state(en__isdStatus_PowerIC_OK); // FPGA 리셋
            }
        }
        break;

#endif

            //////

        case 21: // 자극 파라미터.
        {

            // PCM 출력 모드 변경
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);

            // 자극 출력 DAC 설정
            // ISD 0x5   - DAC Offset 쓰기
            w_isd_registerValue = ISD_registerAddr_DAC_offsetValue;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
            w_isd_registerValue = w_isd_registerValue << 8;

            w_isd_registerValue = w_isd_registerValue | mappingPacket->testStimulation.stimulationDacOffset_255;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue); //      | 0x50000
            fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);

            // ISD 0x6    - 자극 파라미터 설정  쓰기
            w_isd_registerValue = ISD_registerAddr_StimulationConfig;

            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;

            // 0, 7번비트
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | 1;

            // offsetResolution 6번 비트
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | stimulDAC_offsetResolution;

            // DAC slope    4,5번 비트
            w_isd_registerValue = w_isd_registerValue << 2;
            w_isd_registerValue = w_isd_registerValue | stimulDAC_slope;

            // 모노폴라 출력 모드에서 기준전극 모드 2,3번 비트
            w_isd_registerValue = w_isd_registerValue << 2;
            switch (mappingPacket->testStimulation.stimulatonMode)
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
                    w_isd_registerValue = w_isd_registerValue | en__referenceNA; // // 리셋값이 3이며, 바이폴라, 공통접지, 동시 자극의 경우는 접지를 끊는다.
                    break;
            }

            // 자극 출력 모드 0,1번 비트
            w_isd_registerValue = w_isd_registerValue << 2;
            switch (mappingPacket->testStimulation.stimulatonMode)
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

            sent_stimulConfig   = w_isd_registerValue & 0xFF;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration; //  | 0x50000

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
            fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);

#ifndef DisalbedBackTel
            // ISD  - DAC Offset 읽기
            w_isd_registerValue = ISD_registerAddr_DAC_offsetValue;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_readRegister;
            w_isd_registerValue = w_isd_registerValue << 8;

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
            fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
            fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
            fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);

            // ISD  - 자극 파라미터 설정  읽기
            w_isd_registerValue = ISD_registerAddr_StimulationConfig;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_readRegister;
            w_isd_registerValue = w_isd_registerValue << 8;

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

            for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
                fillSepcificCommndBuffer(i, pcm_Mold_NopBacktel);

#else

            for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);

#endif

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);
        }
        break;

#ifndef DisalbedBackTel
        case 25: // 자극 파라미터 설정  확인
        {

            if (read_FPGA_FIFO_counter(&r_FPGA_registerValue))
            {

                if (r_FPGA_registerValue == 2)
                {

                    if (read_FPGA_backtel_FIFO(backtelBuff, 2))
                    {

                        // DAC offset 값 확인
                        // if(backtelBuff[0]!=(referencElectrod_index-1) )
                        if (backtelBuff[0] != mappingPacket->testStimulation.stimulationDacOffset_255)
                        {

                            sendErrorToApp(en__mapping_testStimulation, en__EN__ISD_ERROR, en__SettingError_OffsetDAC_Level, __LINE__);
                            // 커맨드 리셋;
                            clear_mappingCommand();
                            change_isd_state(en__isdStatus_FPGA_Ok); // 내부기 전송 파워 설정 부터 다시.
                            stimulationConfigError = true;
                        }
                        else if (backtelBuff[1] != sent_stimulConfig)
                        {

                            sendErrorToApp(en__mapping_testStimulation, en__EN__ISD_ERROR, en__SettingError_StimulatonPara, __LINE__);
                            // 커맨드 리셋;
                            clear_mappingCommand();
                            change_isd_state(en__isdStatus_FPGA_Ok); // 내부기 전송 파워 설정 부터 다시.
                            stimulationConfigError = true;
                        }

                        if (referencElectrod_index >= 256)
                        {
                            referencElectrod_index = 1000;
                        }
                    }
                    else
                    {
                        sendErrorToApp(en__mapping_testStimulation, en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_ReadingError, __LINE__);
                        // 커맨드 리셋;
                        clear_mappingCommand();
                        // 백텔이 안들어 왔다.
                        change_isd_state(en__isdStatus_PowerIC_OK); // FPGA 리셋
                    }
                }
                else
                {

                    read_FPGA_backtel_FIFO(backtelBuff, 2);

                    read_FPGA_systemError_Flag(&r_FPGA_registerValue);

                    read_FPGA_backtelError_Flag(&r_FPGA_registerValue);

                    sendErrorToApp(en__mapping_testStimulation, en__EN__ISD_ERROR, en__BackterDataLengthError, __LINE__);
                    // 커맨드 리셋;
                    clear_mappingCommand();
                    // 백텔이 안들어 왔다.
                    change_isd_state(en__isdStatus_FPGA_Ok); // 내부기 전송 파워 설정 부터 다시.
                }
            }
            else
            {
                sendErrorToApp(en__mapping_testStimulation, en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_ReadingError, __LINE__);
                // 커맨드 리셋;
                clear_mappingCommand();
                // 백텔이 안들어 왔다.
                change_isd_state(en__isdStatus_PowerIC_OK); // FPGA 리셋
            }
        }
        break;

#endif

        case 26: // 자극용 파라미터 설정 시작(펄스 폭 설정)
        {

            // PCM 출력 모드 변경
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);

            // 펄스 폭 조정
            chang_PulseWidth(pcm_index++, mappingPacket->testStimulation.pulseWidth);

            // 나머지 버퍼는  NOP-Standby
            for (; pcm_index < df_MaxNumTransferableChannel;)
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);

            // NOP-Standby
            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);

            // 펄스 폭 .. 설정 PCM 출력으로  FPGA에 전달

            // NOP-Standby
            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);
        }
        break;

        case 30: // 펄스폭 확인
        {

            if (read_FPGA_PulseWidth(&r_FPGA_registerValue))
            {
                if (r_FPGA_registerValue == mappingPacket->testStimulation.pulseWidth)
                {

                    cofigureDoneCounter = flowControlCounter;
                }
                else
                {

                    sendErrorToApp(en__mapping_testStimulation, en__FPGA_COMMUNICATION_ERROR, en_PulseWidthDifferent, __LINE__);
                    // 커맨드 리셋;
                    clear_mappingCommand();

                    stimulationConfigError = true;

                    if (read_FPGA_systemError_Flag(&r_FPGA_registerValue))
                    {
                        if ((0x0F & r_FPGA_registerValue) != 0)
                            change_isd_state(en__isdStatus_PowerIC_OK);
                    }
                }
            }
            else
            {
                sendErrorToApp(en__mapping_testStimulation, en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_ReadingError, __LINE__);
                // 커맨드 리셋;
                clear_mappingCommand();
                // 백텔이 안들어 왔다.
                change_isd_state(en__isdStatus_PowerIC_OK); // FPGA 리셋
            }
        }
        break;

        default:
            break;
    }

    if (flowControlCounter > cofigureDoneCounter) // 자극 설정이 완료되었으며 자극출력을 시작한다.
    {

        if (stimulationTime_msec >= stimulationHoldTime_msec) // 자극 유지시간 도래.
        // if(0)
        {

            // 마지막 자극을 위하여 자극 출력
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);

            chang_PulseWidth_minimum(pcm_index++);
            upadte_fpga_pulsePhaseWidth_written_Value(FPGA_pulsePhaseWidth_minimum);

            // 이전 자극 파라미터를 출력하기 위한 임의의 값 출력
            fill_pcmBuff_lastSimulationOut(&pcm_index);

            for (; pcm_index < df_MaxNumTransferableChannel;)
            {
                // tempBuff[pcm_index]=pcm_Mold_NopStandby;
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            }

            read_FPGA_FIFO_counter(&temp);

            // 출력 완료.

            // 송신 데이터 준비
            // command loop-back
            bufferForSPI_tx[buffer_tx_index++] = mappingPacket->command;

            // pay-load 준비
            bufferForSPI_tx[buffer_tx_index++] = 5; // 자극 출력 성공

            // 송신 데이터 SPI TX버퍼에 복사

            writeDataToSpiTxBuff(bufferForSPI_tx, buffer_tx_index);

            // 커맨드 리셋;
            clear_mappingCommand();
        }
        else // 자극 시간 동안 자극 출력
        {

            // PCM 출력 모드 변경
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);

            for (i = 0; i < TransferabelChannelNum; i++) // 1msec 동안 출력할 수 있는 채널 수
            {

                // tempBuff[pcm_index]=stimulPCM_buff[trasnferChannel_Index];
                fillSepcificCommndBuffer(pcm_index++, stimulPCM_buff[trasnferChannel_Index++]);

                // 펄스 폭에 맞추어 NOP
                for (pulseDurationNop_idex = 1; pulseDurationNop_idex < numFramePerChannel; pulseDurationNop_idex++)
                {
                    // tempBuff[pcm_index]=pcm_Mold_NopStandby;
                    fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
                }

                if (mappingPacket->testStimulation.usableElectrodeNum >
                    TransferabelChannelNum) // 1msec동안 출력할 수 있는 채널 수보다 사용가능한 채널 수 가 많을 경우
                {

                    if (trasnferChannel_Index >= mappingPacket->testStimulation.usableElectrodeNum)
                        trasnferChannel_Index = 0;
                }
                else
                {
                    if (trasnferChannel_Index >= TransferabelChannelNum)

                        trasnferChannel_Index = 0;
                }
            }

            for (; pcm_index < df_MaxNumTransferableChannel;)
            {
                // tempBuff[pcm_index]=pcm_Mold_NopStandby;
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            }

            stimulationTime_msec++;
        }
        // NOP-Standby
        changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);
    }

    flowControlCounter++;

    return stimulationConfigError;
}

#endif
