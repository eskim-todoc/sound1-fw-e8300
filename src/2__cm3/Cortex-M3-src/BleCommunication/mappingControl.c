
#include <hw.h>
#include <stdbool.h>
#include "error.h"
#include "mappingControl.h"
#include "board.h"
#include "internalStimulationChip.h"
#include "cfx_cm3_sharedMemory.h"
#include "mappingControl.h"
#include "driver_SPI.h"
#include "definitionsForAlgorithm.h"
#include "isd_interface_mapping_impedanceMeasurement.h"
#include "isd_interface_mapping_eCAP_Measurement.h"
#include "isd_interface_mapping_SepcificStimulation.h"
#include "isd_interface_mapping_Live.h"
#include "isd_interface.h"
#include "isd_interface_mapping_testStimulation.h"
#include "isd_interface_mapping_readWrtieMapData.h"
#include "isd_interface_mapping_Live.h"
#include "isd_interface_init_ISD.h"
#include "FPGA.h"
#include "systemControl.h"
#include "remoteControl.h"

static ST__MAPPING_PACKET mappingPacket;

void clear_mappingCommand()
{
    mappingPacket.command                    = en__mapping_IDLE;
    mappingPacket.liveStimulation.subCommand = en__Standby;

    changePcmOutputMode(PcmBitStream_Mode_NopStandby);
}

void changeMappingCommandBleDisconneted(void)
{
    mappingPacket.command = en__mapping_ble_disconneted;
}

void changeMappingCommandWaitingForBleOff(void)
{
    mappingPacket.command = en__mapping_waiting_for_BleOff;
}

EN__MAPPING_COMMAND getMappingCommand(void)
{
    return mappingPacket.command;
}

// const ST__MAPPING_PACKET *getMappingPacket(void)
ST__MAPPING_PACKET *getMappingPacket(void)
{
    return &mappingPacket;
}

static int writingStartSlot_index = 0;

void fetch_mappingControlPacket(const int *Rx_dataPacket)  // spi 통신에서 호출 됨
{
    int        i, k, index;
    int        tempCommand;
    int        value, tempA, tempB;
    int        liveSubCommand;
    static int prev_subCommandData_Num_index = 0;
    int        subCommandData_Num_index;

    static int max_C_uA = 0, min_T_uA = 1800;

    int bufferForSPI_tx[BLE_DataPacketSize];
    int buffer_tx_index;

    ST__CFX_CM3_SharedMemory_mapData *p_mapDataSharedMemory;
    int                              *p_RepositoryFor_ISD_info;

    int *p_RepositoryFor_stimulPara;

    int        intFromByte;
    static int stimulPara_index = 0;

    bool exceptionCase = false;
    int  tempValue;
    int  trashValue;
    bool dataRangeError = false;

    index           = 0;
    buffer_tx_index = 0;
    tempCommand     = Rx_dataPacket[index++];  // index 0 → 1

    // mappingPacket은 static 전역 구조체이다.
    // 그러므로 초기 값으로 mappingPacket.command는 en__mapping_IDLE로 시작한다.

    // 이전 명령이 완료되지 않아
    // mappingPacket.command가 en__mapping_IDLE가 아닌 상태에서
    // 새 명령이 입력되면
    // 아래와 같이 특정 예외 상황이 아니면 에러 응답을 송신한다.
    if (mappingPacket.command != en__mapping_IDLE)
    {
        if (tempCommand == en__mapping_live_stimulation)  // header 0x66 // 예외 사항
        {
            // 프로토콜 문서 상 0x66 명령어의 subCommand 1~8이 아닌,
            // 9: en__HoldOn, 0: en__Standby 인 경우 예외로 처리한다.
            if ((mappingPacket.liveStimulation.subCommand == en__HoldOn) || (mappingPacket.liveStimulation.subCommand == en__Standby))
            {
                exceptionCase = true;
            }
        }

        if (tempCommand == en__mapping_disconnect)  // header 0x61
        {
            exceptionCase = true;
        }

        if (!exceptionCase)
        {
            // 현재 받은 명령을 수행하지 않는다.
            sendErrorToApp(tempCommand, en__EN__BLE_PROTOCOL_ERROR, en__PreviouCommnadIsNotCompleted, __LINE__);
            tempCommand = en__mapping_IDLE;
        }
    }

    switch (tempCommand)
    {
        case en__mapping_connect:  // header 0x60
        {
            mappingPacket.fetched_command = tempCommand;
            prev_subCommandData_Num_index = 0;
        }
        break;

        case en__mapping_disconnect:  // header 0x61
        {
            mappingPacket.fetched_command = tempCommand;
        }
        break;

        case en__mapping_impedanceChekck:  // 0x62
        {
            mappingPacket.impedanceCheck.iterationNum          = Rx_dataPacket[index++];  // index 1 → 2
            mappingPacket.impedanceCheck.channel               = Rx_dataPacket[index++];  // index 2 → 3
            mappingPacket.impedanceCheck.pulseWidth_start_usec = Rx_dataPacket[index++];  // index 3 → 4
            mappingPacket.impedanceCheck.pulseWidth_end_usec   = Rx_dataPacket[index++];  // index 4 → 5

            value                                            = Rx_dataPacket[index++] << 8;     // 자극 uA단위 상위 바이트 // index 5 → 6
            mappingPacket.impedanceCheck.stimulationLevel_uA = value | Rx_dataPacket[index++];  // 자극 uA단위 하위 바이트 // index 6 → 7

            // 측정 반복 횟수 범위 검사
            if ((1 <= mappingPacket.impedanceCheck.iterationNum)        // 1 이상
                && (mappingPacket.impedanceCheck.iterationNum <= 255))  // 255 이하
            {
                // 측정 채널 선택 범위 검사
                if (((1 <= mappingPacket.impedanceCheck.channel)       // 1 이상
                     && (mappingPacket.impedanceCheck.channel <= 32))  // 32 이하
                    || (mappingPacket.impedanceCheck.channel == 255))  // 또는 255 (전채널)
                {
                    // 좁은 펄스 위상 폭 usec (시작) 범위 검사
                    if ((13 <= mappingPacket.impedanceCheck.pulseWidth_start_usec)       // 13 이상
                        && (mappingPacket.impedanceCheck.pulseWidth_start_usec <= 255))  // 255 이하
                    {
                        // 넓은 펄스 위상 폭 (최종) 범위 검사
                        if ((13 <= mappingPacket.impedanceCheck.pulseWidth_end_usec)       // 13 이상
                            && (mappingPacket.impedanceCheck.pulseWidth_end_usec <= 255))  // 255 이하
                        {
                            // 측정용 자극 크기 uA 범위 검사
                            if ((1 <= mappingPacket.impedanceCheck.stimulationLevel_uA)         // 1 이상
                                && (mappingPacket.impedanceCheck.stimulationLevel_uA <= 1024))  // 1024 이하
                            {
                                mappingPacket.fetched_command = en__mapping_impedanceChekck;
                                break;
                            }
                        }
                    }
                }
            }

            // 데이터 범위를 벗어남
            sendErrorToApp(en__mapping_impedanceChekck, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
        }
        break;

        case en__mapping_eCAP_Measurement_masking:  // 헤더 0x063
        {
            mappingPacket.eCapMeasurement.iterationNum = Rx_dataPacket[index++];
            mappingPacket.eCapMeasurement.pulseWidth   = Rx_dataPacket[index++];

            mappingPacket.eCapMeasurement.firstPulsePhase              = Rx_dataPacket[index++];
            mappingPacket.eCapMeasurement.stimulatonMode               = Rx_dataPacket[index++];
            mappingPacket.eCapMeasurement.stimulationElectrodeNum      = Rx_dataPacket[index++];
            mappingPacket.eCapMeasurement.bipolarReferenceElectrodeNum = Rx_dataPacket[index++];
            mappingPacket.eCapMeasurement.measurementElectrodeNum      = Rx_dataPacket[index++];

            value                                                    = Rx_dataPacket[index++] << 8;
            value                                                    = value | Rx_dataPacket[index++];
            mappingPacket.eCapMeasurement.stimulationLevel_uA_masker = value;

            value                                                   = Rx_dataPacket[index++] << 8;
            value                                                   = value | Rx_dataPacket[index++];
            mappingPacket.eCapMeasurement.stimulationLevel_uA_probe = value;

            mappingPacket.eCapMeasurement.maskerProbeInterval_numFrame = Rx_dataPacket[index++];

            mappingPacket.eCapMeasurement.adcPreampGain        = Rx_dataPacket[index++];
            mappingPacket.eCapMeasurement.adcSamplingFreq      = Rx_dataPacket[index++];
            mappingPacket.eCapMeasurement.adcMeasurementDelay  = Rx_dataPacket[index++];
            mappingPacket.eCapMeasurement.measurementSampleNum = Rx_dataPacket[index++];

            mappingPacket.fetched_command = en__mapping_eCAP_Measurement_masking;
        }
        break;

        case en__mapping_eCAP_Measurement_alternative:  // 헤더 0x64
        {
            mappingPacket.eCapMeasurement.iterationNum = Rx_dataPacket[index++];
            mappingPacket.eCapMeasurement.pulseWidth   = Rx_dataPacket[index++];

            mappingPacket.eCapMeasurement.stimulatonMode               = Rx_dataPacket[index++];
            mappingPacket.eCapMeasurement.stimulationElectrodeNum      = Rx_dataPacket[index++];
            mappingPacket.eCapMeasurement.bipolarReferenceElectrodeNum = Rx_dataPacket[index++];
            mappingPacket.eCapMeasurement.measurementElectrodeNum      = Rx_dataPacket[index++];

            value                                                    = Rx_dataPacket[index++] << 8;
            value                                                    = value | Rx_dataPacket[index++];
            mappingPacket.eCapMeasurement.stimulationLevel_uA_masker = value;

            mappingPacket.fetched_command = en__mapping_eCAP_Measurement_alternative;
        }
        break;

        case en__mapping_specific_stimulation:  // 헤더 0x65
        {
            mappingPacket.specificStimulation.usableElectrodeNum           = Rx_dataPacket[index++];
            mappingPacket.specificStimulation.pulseWidth                   = Rx_dataPacket[index++];
            mappingPacket.specificStimulation.firstPulsePhase              = Rx_dataPacket[index++];
            mappingPacket.specificStimulation.stimulatonMode               = Rx_dataPacket[index++];
            mappingPacket.specificStimulation.stimulationElectrodeNum      = Rx_dataPacket[index++];
            mappingPacket.specificStimulation.bipolarReferenceElectrodeNum = Rx_dataPacket[index++];

            value                                                     = Rx_dataPacket[index++] << 8;
            value                                                     = value | Rx_dataPacket[index++];
            mappingPacket.specificStimulation.stimulationLevel_uA     = value;
            mappingPacket.specificStimulation.stimulationTime_100msec = Rx_dataPacket[index++];

            // 데이터 범위 검사
            dataRangeError = false;

            // 사용 가능한 전극 수
            if ((mappingPacket.specificStimulation.usableElectrodeNum < 1)       // 1 미만
                || (32 < mappingPacket.specificStimulation.usableElectrodeNum))  // 32 초과 시 에러
            {
                dataRangeError = true;
            }

            // 펄스 위상 폭
            if ((mappingPacket.specificStimulation.pulseWidth < 13)       // 13 미만
                || (255 < mappingPacket.specificStimulation.pulseWidth))  // 255 초과 시 에러
            {
                dataRangeError = true;
            }

            // 선행 펄스 위상
            if ((mappingPacket.specificStimulation.firstPulsePhase < 0)      // 0 미만
                || (1 < mappingPacket.specificStimulation.firstPulsePhase))  // 1 초과 시 에러
            {
                dataRangeError = true;
            }

            // 자극 모드
            if ((mappingPacket.specificStimulation.stimulatonMode < 1)      // 1 미만
                || (6 < mappingPacket.specificStimulation.stimulatonMode))  // 6 초과 시 에러
            {
                dataRangeError = true;
            }

            // 자극 전극 번호
            if ((mappingPacket.specificStimulation.stimulationElectrodeNum < 1)       // 1 미만
                || (32 < mappingPacket.specificStimulation.stimulationElectrodeNum))  // 32 초과 시 에러
            {
                dataRangeError = true;
            }

            // 바이폴라 모드일 때, 기준전극 번호
            if ((mappingPacket.specificStimulation.bipolarReferenceElectrodeNum < 1)       // 1 미만
                || (32 < mappingPacket.specificStimulation.bipolarReferenceElectrodeNum))  //  32 초과 시 에러
            {
                if (mappingPacket.specificStimulation.bipolarReferenceElectrodeNum != 99)  // 99인 경우 예외 (for 모노폴라)
                {
                    dataRangeError = true;
                }
            }

            // 자극 크기 uA
            if ((mappingPacket.specificStimulation.stimulationLevel_uA < 0 /*1*/)   // 0 미만
                || (1800 < mappingPacket.specificStimulation.stimulationLevel_uA))  // 1800 초과 시 에러
            {
                dataRangeError = true;
            }

            // 자극 유지 시간 100msec
            if ((mappingPacket.specificStimulation.stimulationTime_100msec < 1)        // 1 미만
                || (255 < mappingPacket.specificStimulation.stimulationTime_100msec))  // 255 초과 시 에러
            {
                dataRangeError = true;
            }

            if (dataRangeError)
            {
                // 데이터 범위를 벗어남
                sendErrorToApp(en__mapping_specific_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
            }
            else
            {
                mappingPacket.fetched_command = en__mapping_specific_stimulation;
            }
        }
        break;

        case en__mapping_live_stimulation:  // 헤더 0x66
        {
            // index 1은 하위 명령 1~9에 대한 인덱스
            liveSubCommand = Rx_dataPacket[index++];  // index = 1, index++ = 2

            // liveSubCommand
            // 1: 전체 파라미터 전달             : en__allParameter
            // 2: 라이브 모드 시작               : en__Start
            // 3: 자극 볼륨 조절                 : en__StimulationVolumeAdjust
            // 4: 마이크 감도 조절               : en__MicSensitivityAdjust
            // 5: 알림 자극 출력                 : en__mapping_Stimul_indicator
            // 6: 자극 출력 값 읽기 (이퀄라이저) : en__readEqualizer
            // 7: 장치 상태 읽기                 : en__readDeviceStatus
            // 8: 라이브 모드 종료               : en__Stop

            // 프로토콜과 별개로 사용되는 상태 값
            // 0: en__Standby
            // 9: en__HoldOn

            if ((liveSubCommand < 1) || (9 < liveSubCommand))  // 하위 명령 1~9 가능
            {
                // 데이터 범위를 벗어남
                sendErrorToApp(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
                break;
            }
            else
            {
                mappingPacket.fetched_command = en__mapping_live_stimulation;
            }

            switch (liveSubCommand)  // 하위 명령 1~9
            {
                case en__allParameter:  // 하위 명령 1
                {
                    mappingPacket.liveStimulation.subCommand = en__Standby;

                    subCommandData_Num_index = Rx_dataPacket[index++];  // index++ : 2 → 3

                    if (subCommandData_Num_index == 1)
                    {
                        prev_subCommandData_Num_index = 0;
                    }

                    if (subCommandData_Num_index != prev_subCommandData_Num_index + 1)
                    {
                        // 데이터가 순차적으로 들어와야된다. 순차적으로 들어 오지 않으면 에러 전송
                        sendErrorToApp(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__DATA_Order, __LINE__);  // 데이터 범위 벗어남
                        prev_subCommandData_Num_index = 0;
                    }
                    else
                    {
                        prev_subCommandData_Num_index = subCommandData_Num_index;

                        // 라이브 모드:0x66 -> 하위 명령 1: 전체 파라미터 전달 -> 데이터 인덱스 1~15에 대한 switch
                        switch (subCommandData_Num_index)
                        {
                            case 1:  // 헤더 0x66 실시간 자극 -> 하위 명령 1 -> 데이터 인덱스 1
                            {
                                mappingPacket.liveStimulation.stimulVolume                     = Rx_dataPacket[index++];          // index++ : 3 → 4
                                mappingPacket.liveStimulation.audioVolume                      = Rx_dataPacket[index++];          // index++ : 4 → 5
                                mappingPacket.liveStimulation.stimulationIndicatorChannelNum   = Rx_dataPacket[index++];          // index++ : 5 → 6
                                value                                                          = Rx_dataPacket[index++] << 8;     // index++ : 6 → 7
                                value                                                          = value | Rx_dataPacket[index++];  // index++ : 7 → 8
                                mappingPacket.liveStimulation.stimulationIndicatorAmplitude_uA = value;

                                mappingPacket.liveStimulation.stimulationStrategy        = Rx_dataPacket[index++];  // index++ : 8 → 9
                                mappingPacket.liveStimulation.stimulationMode            = Rx_dataPacket[index++];  // index++ : 9 → 10
                                mappingPacket.liveStimulation.firstPulsePhase            = Rx_dataPacket[index++];  // index++ : 10 → 11
                                mappingPacket.liveStimulation.stimulationPulsePhaseWidth = Rx_dataPacket[index++];  // index++ : 11 → 12
                                mappingPacket.liveStimulation.numFrequencyBand           = Rx_dataPacket[index++];  // index++ : 12 → 13

                                // 자극 볼륨 (mappingPacket.liveStimulation.stimulVolume : 1~4)
                                if ((mappingPacket.liveStimulation.stimulVolume < 1)      // 1 미만
                                    || (4 < mappingPacket.liveStimulation.stimulVolume))  // 4 초과 시 에러
                                {
                                    dataRangeError = true;
                                }

                                // 오디오 볼륨 (mappingPacket.liveStimulation.audioVolume : 1~10)
                                if ((mappingPacket.liveStimulation.audioVolume < 1)       // 1 미만
                                    || (10 < mappingPacket.liveStimulation.audioVolume))  // 10 초과 시 에러
                                {
                                    dataRangeError = true;
                                }

                                // 알림용 자극 채널 번호 (mappingPacket.liveStimulation.stimulationIndicatorChannelNum : 1~32)
                                if ((mappingPacket.liveStimulation.stimulationIndicatorChannelNum < 1)       // 1 미만
                                    || (32 < mappingPacket.liveStimulation.stimulationIndicatorChannelNum))  // 32 초과 시 에러
                                {
                                    dataRangeError = true;
                                }

                                // 알림용 자극 크기 uA (mappingPacket.liveStimulation.stimulationIndicatorAmplitude_uA : 1~1800)
                                if ((mappingPacket.liveStimulation.stimulationIndicatorAmplitude_uA < 0 /*1*/)   // 0 미만
                                    || (1800 < mappingPacket.liveStimulation.stimulationIndicatorAmplitude_uA))  // 1800 초과 시 에러
                                {
                                    dataRangeError = true;
                                }

                                // 자극 기법 (mappingPacket.liveStimulation.stimulationStrategy : 1~3)
                                if ((mappingPacket.liveStimulation.stimulationStrategy < 1)      // 1 미만
                                    || (3 < mappingPacket.liveStimulation.stimulationStrategy))  // 3 초과 시 에러
                                {
                                    dataRangeError = true;
                                }

                                // 자극 모드 (mappingPacket.liveStimulation.stimulationMode : 1~6)
                                if ((mappingPacket.liveStimulation.stimulationMode < 1)      // 1 미만
                                    || (6 < mappingPacket.liveStimulation.stimulationMode))  // 6 초과 시 에러
                                {
                                    dataRangeError = true;
                                }

                                // 선행 펄스 위상 (mappingPacket.liveStimulation.firstPulsePhase : 0~1)
                                if ((mappingPacket.liveStimulation.firstPulsePhase < 0)      // 0 미만
                                    || (1 < mappingPacket.liveStimulation.firstPulsePhase))  // 1 초과 시 에러
                                {
                                    dataRangeError = true;
                                }

                                // 펄스 위상 폭 (mappingPacket.liveStimulation.stimulationPulsePhaseWidth : 13~255)
                                if ((mappingPacket.liveStimulation.stimulationPulsePhaseWidth < 13)       // 13 미만
                                    || (255 < mappingPacket.liveStimulation.stimulationPulsePhaseWidth))  // 255 초과 시 에러
                                {
                                    dataRangeError = true;
                                }

                                // 주파수 밴드 (mappingPacket.liveStimulation.numFrequencyBand : 1~32)
                                if ((mappingPacket.liveStimulation.numFrequencyBand < 1)       // 1 미만
                                    || (32 < mappingPacket.liveStimulation.numFrequencyBand))  // 32 초과 시 에러
                                {
                                    dataRangeError = true;
                                }
                            }
                            break;

                            // 헤더 0x66 실시간 자극 -> 하위 명령 1 -> 데이터 인덱스 2
                            case 2:  // 자극 전극 번호
                            {
                                for (i = 0; i < 17; i++)
                                {
                                    mappingPacket.liveStimulation.usableStimulationElectrodIndex[i] = Rx_dataPacket[index++];

                                    if ((mappingPacket.liveStimulation.usableStimulationElectrodIndex[i] < 1)        // 1 미만
                                        || (100 < mappingPacket.liveStimulation.usableStimulationElectrodIndex[i]))  // 100 초과 시 에러
                                    {
                                        dataRangeError = true;
                                    }
                                }
                            }
                            break;

                            // 헤더 0x66 실시간 자극 -> 하위 명령 1 -> 데이터 인덱스 3
                            case 3:  // 자극 전극 번호
                            {
                                for (i = 17; i < 32; i++)
                                {
                                    mappingPacket.liveStimulation.usableStimulationElectrodIndex[i] = Rx_dataPacket[index++];

                                    if ((mappingPacket.liveStimulation.usableStimulationElectrodIndex[i] < 1)        // 1 미만
                                        || (100 < mappingPacket.liveStimulation.usableStimulationElectrodIndex[i]))  // 100 초과 시 에러
                                    {
                                        dataRangeError = true;
                                    }
                                }
                            }
                            break;

                            // 헤더 0x66 실시간 자극 -> 하위 명령 1 -> 데이터 인덱스 4
                            case 4:  // 기준 전극 번호
                            {
                                for (i = 0; i < 17; i++)
                                {
                                    mappingPacket.liveStimulation.usableReferenceElectrodIndex[i] = Rx_dataPacket[index++];

                                    if ((mappingPacket.liveStimulation.usableReferenceElectrodIndex[i] < 1)        // 1 미만
                                        || (100 < mappingPacket.liveStimulation.usableReferenceElectrodIndex[i]))  // 100 초과 시 에러
                                    {
                                        dataRangeError = true;
                                    }
                                }
                            }
                            break;

                            // 헤더 0x66 실시간 자극 -> 하위 명령 1 -> 데이터 인덱스 5
                            case 5:  // 기즌 전극 번호
                            {
                                for (i = 17; i < 32; i++)
                                {
                                    mappingPacket.liveStimulation.usableReferenceElectrodIndex[i] = Rx_dataPacket[index++];

                                    if ((mappingPacket.liveStimulation.usableReferenceElectrodIndex[i] < 1)        // 1 미만
                                        || (100 < mappingPacket.liveStimulation.usableReferenceElectrodIndex[i]))  // 100 초과 시 에러
                                    {
                                        dataRangeError = true;
                                    }
                                }
                            }
                            break;

                            // 헤더 0x66 실시간 자극 -> 하위 명령 1 -> 데이터 인덱스 6
                            case 6:  // 주파수 밴드 출력 순서
                            {
                                for (i = 0; i < 17; i++)
                                {
                                    mappingPacket.liveStimulation.CIS_FreqBandOrder[i] = Rx_dataPacket[index++];

                                    if ((mappingPacket.liveStimulation.CIS_FreqBandOrder[i] < 1)        // 1 미만
                                        || (100 < mappingPacket.liveStimulation.CIS_FreqBandOrder[i]))  // 100 초과 시 에러
                                    {
                                        dataRangeError = true;
                                    }
                                }
                            }
                            break;

                            // 헤더 0x66 실시간 자극 -> 하위 명령 1 -> 데이터 인덱스 7
                            case 7:  // 주파수 밴드 출력 순서
                            {
                                for (i = 17; i < 32; i++)
                                {
                                    mappingPacket.liveStimulation.CIS_FreqBandOrder[i] = Rx_dataPacket[index++];

                                    if ((mappingPacket.liveStimulation.CIS_FreqBandOrder[i] < 1)        // 1 미만
                                        || (100 < mappingPacket.liveStimulation.CIS_FreqBandOrder[i]))  // 100 초과 시 에러
                                    {
                                        dataRangeError = true;
                                    }
                                }
                            }
                            break;

                            // 헤더 0x66 실시간 자극 -> 하위 명령 1 -> 데이터 인덱스 8
                            case 8:  // T 레벨
                            {
                                for (i = 0; i < 8; i++)
                                {
                                    value                                       = Rx_dataPacket[index++] << 8;
                                    value                                       = value | Rx_dataPacket[index++];
                                    mappingPacket.liveStimulation.T_level_uA[i] = value;

                                    if ((mappingPacket.liveStimulation.T_level_uA[i] < 0)         // 0 미만
                                        || (1800 < mappingPacket.liveStimulation.T_level_uA[i]))  // 1800 초과 시 에러
                                    {
                                        dataRangeError = true;
                                    }
                                }
                            }
                            break;

                            // 헤더 0x66 실시간 자극 -> 하위 명령 1 -> 데이터 인덱스 9
                            case 9:  // T 레벨
                            {
                                for (i = 8; i < 16; i++)
                                {
                                    value                                       = Rx_dataPacket[index++] << 8;
                                    value                                       = value | Rx_dataPacket[index++];
                                    mappingPacket.liveStimulation.T_level_uA[i] = value;

                                    if ((mappingPacket.liveStimulation.T_level_uA[i] < 0)         // 0 미만
                                        || (1800 < mappingPacket.liveStimulation.T_level_uA[i]))  // 1800 초과 시 에러
                                    {
                                        dataRangeError = true;
                                    }
                                }
                            }
                            break;

                            // 헤더 0x66 실시간 자극 -> 하위 명령 1 -> 데이터 인덱스 10
                            case 10:  // T 레벨
                            {
                                for (i = 16; i < 24; i++)
                                {
                                    value                                       = Rx_dataPacket[index++] << 8;
                                    value                                       = value | Rx_dataPacket[index++];
                                    mappingPacket.liveStimulation.T_level_uA[i] = value;

                                    if ((mappingPacket.liveStimulation.T_level_uA[i] < 0)         // 0 미만
                                        || (1800 < mappingPacket.liveStimulation.T_level_uA[i]))  // 1800 초과 시 에러
                                    {
                                        dataRangeError = true;
                                    }
                                }
                            }
                            break;

                            // 헤더 0x66 실시간 자극 -> 하위 명령 1 -> 데이터 인덱스 11
                            case 11:  // T 레벨
                            {
                                for (i = 24; i < 32; i++)
                                {
                                    value                                       = Rx_dataPacket[index++] << 8;
                                    value                                       = value | Rx_dataPacket[index++];
                                    mappingPacket.liveStimulation.T_level_uA[i] = value;

                                    if ((mappingPacket.liveStimulation.T_level_uA[i] < 0)         // 0 미만
                                        || (1800 < mappingPacket.liveStimulation.T_level_uA[i]))  // 1800 초과 시 에러
                                    {
                                        dataRangeError = true;
                                    }
                                }
                            }
                            break;

                            // 헤더 0x66 실시간 자극 -> 하위 명령 1 -> 데이터 인덱스 12
                            case 12:  // C 레벨
                            {
                                for (i = 0; i < 8; i++)
                                {
                                    value                                       = Rx_dataPacket[index++] << 8;
                                    value                                       = value | Rx_dataPacket[index++];
                                    mappingPacket.liveStimulation.C_level_uA[i] = value;

                                    if ((mappingPacket.liveStimulation.C_level_uA[i] < 0)         // 0 미만
                                        || (1800 < mappingPacket.liveStimulation.C_level_uA[i]))  // 1800 초과 시 에러
                                    {
                                        dataRangeError = true;
                                    }
                                }
                            }
                            break;

                            // 헤더 0x66 실시간 자극 -> 하위 명령 1 -> 데이터 인덱스 13
                            case 13:  // C 레벨
                            {
                                for (i = 8; i < 16; i++)
                                {
                                    value                                       = Rx_dataPacket[index++] << 8;
                                    value                                       = value | Rx_dataPacket[index++];
                                    mappingPacket.liveStimulation.C_level_uA[i] = value;

                                    if ((mappingPacket.liveStimulation.C_level_uA[i] < 0)         // 0 미만
                                        || (1800 < mappingPacket.liveStimulation.C_level_uA[i]))  // 1800 초과 시 에러
                                    {
                                        dataRangeError = true;
                                    }
                                }
                            }
                            break;

                            // 헤더 0x66 실시간 자극 -> 하위 명령 1 -> 데이터 인덱스 14
                            case 14:  // C 레벨
                            {
                                for (i = 16; i < 24; i++)
                                {
                                    value                                       = Rx_dataPacket[index++] << 8;
                                    value                                       = value | Rx_dataPacket[index++];
                                    mappingPacket.liveStimulation.C_level_uA[i] = value;

                                    if ((mappingPacket.liveStimulation.C_level_uA[i] < 0)         // 0 미만
                                        || (1800 < mappingPacket.liveStimulation.C_level_uA[i]))  // 1800 초과 시 에러
                                    {
                                        dataRangeError = true;
                                    }
                                }
                            }
                            break;

                            // 헤더 0x66 실시간 자극 -> 하위 명령 1 -> 데이터 인덱스 15
                            case 15:  // C 레벨
                            {
                                for (i = 24; i < 32; i++)
                                {
                                    value                                       = Rx_dataPacket[index++] << 8;
                                    value                                       = value | Rx_dataPacket[index++];
                                    mappingPacket.liveStimulation.C_level_uA[i] = value;

                                    if ((mappingPacket.liveStimulation.C_level_uA[i] < 0)         // 0 미만
                                        || (1800 < mappingPacket.liveStimulation.C_level_uA[i]))  // 1800 초과 시 에러
                                    {
                                        dataRangeError = true;
                                    }
                                }
                            }
                            break;

                            default:
                            {
                                dataRangeError = true;
                            }
                            break;
                        }

                        if (dataRangeError)
                        {
                            sendErrorToApp(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__DATA_Order, __LINE__);  // 데이터 범위 벗어남
                            prev_subCommandData_Num_index = 0;
                        }
                        else
                        {
                            // command loop-back
                            bufferForSPI_tx[buffer_tx_index++] = en__mapping_live_stimulation;

                            //  sub-command loop-back
                            bufferForSPI_tx[buffer_tx_index++] = en__allParameter;

                            bufferForSPI_tx[buffer_tx_index++] = subCommandData_Num_index;  // payload num 전송

                            // 송신 데이터 SPI TX버퍼에 복사
                            writeDataToSpiTxBuff(bufferForSPI_tx, buffer_tx_index);

                            if (subCommandData_Num_index == Live_AllParameter_payloadNum)  // 모든 파라미터 수신 완료.
                            {
                                prev_subCommandData_Num_index = 0;

                                // 현재는 매핑프로그램에서 받지는 않지만, 펌웨어는 이 값을 사용하여 자극 범위를 계산하고
                                // 있다. 매핑 프로그램의 기능이 확장되면 이 값을 넣을 지도 고려..

                                // x- min
                                for (i = 0; i < 32; i++)
                                {
                                    mappingPacket.liveStimulation.audio_input_x_mim[i] = df_minAudioForLogarithm;
                                }

                                // x- max
                                for (i = 0; i < 32; i++)
                                {
                                    mappingPacket.liveStimulation.audio_input_x_max[i] = df_maxAudioForLogarithm;
                                }

                                mappingPacket.liveStimulation.subCommand = en__allParameter;

                                TDC_PRINTF_I("[LIVE] LIVE ALL PARAMETER PAYLOAD NUM : NOW, SUB COMMAND SET TO EN__ALL_PARAMETER \r\n");
                            }
                            else
                            {
                                mappingPacket.liveStimulation.subCommand = en__Standby;
                            }
                        }
                    }
                }
                break;

                case en__Start:  // 하위 명령 2 (실시간 자극 시작)
                {
                    mappingPacket.liveStimulation.subCommand = en__Start;
                }
                break;

                case en__StimulationVolumeAdjust:  // 하위 명령 3 (자극 볼륨 조절)
                {
                    if (mappingPacket.liveStimulation.subCommand == en__HoldOn)
                    {
                        value = Rx_dataPacket[index++];

                        if ((1 <= value)                                 // 1 이상
                            && (value <= df_maxStimulationVloumeLevel))  // 4 이하
                        {
                            mappingPacket.liveStimulation.stimulVolume = value;
                            mappingPacket.liveStimulation.subCommand   = en__StimulationVolumeAdjust;
                        }
                        else
                        {
                            // 에러 전송
                            sendErrorToApp(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
                            mappingPacket.liveStimulation.subCommand = en__Standby;
                        }
                    }
                    else
                    {
                        sendErrorToApp(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__Command_Order,
                                       __LINE__);  // 라이브 자극 중에만 컨트롤 되는 명령어

                        mappingPacket.liveStimulation.subCommand = en__Standby;
                    }
                }
                break;

                case en__MicSensitivityAdjust:  // 하위 명령 4 (마이크 감도 조절)
                {
                    if (mappingPacket.liveStimulation.subCommand == en__HoldOn)
                    {
                        value = Rx_dataPacket[index++];
                        if ((1 <= value)                         // 1 이상
                            && (value <= df_maxMicVloumeLevel))  // 10 이하
                        {
                            mappingPacket.liveStimulation.audioVolume = value;
                            mappingPacket.liveStimulation.subCommand  = en__MicSensitivityAdjust;
                        }
                        else
                        {
                            // 에러 전송
                            sendErrorToApp(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
                            mappingPacket.liveStimulation.subCommand = en__Standby;
                        }
                    }
                    else
                    {
                        sendErrorToApp(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__Command_Order, __LINE__);  // 라이브 자극 중에만 컨트롤 되는 명령어
                        mappingPacket.liveStimulation.subCommand = en__Standby;
                    }
                }
                break;

                case en__mapping_Stimul_indicator:  // 하위 명령 5 (알림 자극 출력)
                {
                    if (mappingPacket.liveStimulation.subCommand == en__HoldOn)
                    {
                        p_mapDataSharedMemory = getPointerCurrentMapData();

                        value = Rx_dataPacket[index++];  // 알림용 자극 채널 번호

                        if ((value > p_mapDataSharedMemory->numFrequencyBand)  // 알림용 자극 채널 번호가 주파수 밴드 수 보다 크거나
                            || (value < 1))                                    // 1 미만이면 에러
                        {
                            // 에러 전송
                            sendErrorToApp(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
                            mappingPacket.liveStimulation.subCommand = en__Standby;
                        }
                        else
                        {
                            mappingPacket.liveStimulation.stimulationIndicatorChannelNum = value;

                            value = Rx_dataPacket[index++] << 8;     // 알림용 자극 크기 uA (upper)
                            value = value | Rx_dataPacket[index++];  // 알림용 자극 크기 uA (lower)

                            // 자극 알림 출력값의 데이터 범위 검사를 위한 자극 범위 계산
                            for (i = 0; i < df_MaxNumOfElectrode; i++)
                            {
                                if (p_mapDataSharedMemory->C_level_uA[i] > max_C_uA)
                                {
                                    max_C_uA = p_mapDataSharedMemory->C_level_uA[i];
                                }

#if 1  // 1세대에서 사용되던 코드 (min_T_uA가 1800으로 유지되는 버그 있음)
                                if (p_mapDataSharedMemory->T_level_uA[i] != 0)
                                {
                                    if (p_mapDataSharedMemory->T_level_uA[i] < min_T_uA)
                                    {
                                        min_T_uA = p_mapDataSharedMemory->T_level_uA[i];
                                    }
                                }
#endif
                            }

#if 1
                            // 0x66 라이브모드의 하위 명령 1, 데이터 인덱스 1에서 주파수 밴드 수를 입력 받았다.
                            // 또한, T, C 정보는 사용가능한 전극 번호의 값과는 상관 없이
                            // 매핑 앱의 첫번째 채널에서부터 활성화된 수만큼 앞으로 패딩되어 전달된다.
                            // 즉, 실제 사용 가능한 밴드 수 만큼만 T레벨을 확인하면
                            // 활성화된 밴드들 중에서의 가장 작은 T레벨을 찾을 수 있다.
                            // 그게 비로 0 값이라 하더라도, 해당 밴드는 활성화된 밴드이므로 0이 올바른 T레벨 값일 것이다.
                            for (int li = 0; li < p_mapDataSharedMemory->numFrequencyBand; li++)
                            {
                                if (p_mapDataSharedMemory->T_level_uA[li] < min_T_uA)
                                {
                                    min_T_uA = p_mapDataSharedMemory->T_level_uA[li];
                                }
                            }
#endif

                            if ((value > max_C_uA) || (value < min_T_uA))
                            {
                                TDC_PRINTF_E("[MAPPING] VALUE=%d, max_C_uA=%d, min_T_uA=%d \r\n", value, max_C_uA, min_T_uA);
                                // 에러 전송
                                sendErrorToApp(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
                                mappingPacket.liveStimulation.subCommand = en__Standby;
                            }
                            else
                            {
                                mappingPacket.liveStimulation.stimulationIndicatorAmplitude_uA = value;
                                mappingPacket.liveStimulation.subCommand                       = en__mapping_Stimul_indicator;
                            }
                        }
                    }
                    else
                    {
                        sendErrorToApp(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__Command_Order, __LINE__);  // 라이브 자극 중에만 컨트롤 되는 명령어

                        mappingPacket.liveStimulation.subCommand = en__Standby;
                    }
                }
                break;

                case en__readEqualizer:  // 하위 명령 6 (자극 출력 값 읽기: 이퀄라이저)
                {
                    if (mappingPacket.liveStimulation.subCommand == en__HoldOn)
                    {
                        tempA = Rx_dataPacket[index++];  // start index
                        tempB = Rx_dataPacket[index++];  // end index

                        if ((tempA < 1)       // 1 미만
                            || (32 < tempA))  // 32 초과 시 에러
                        {
                            dataRangeError = true;
                        }

                        if ((tempB < 1)       // 1 미만
                            || (32 < tempB))  // 32 초과 시 에러
                        {
                            dataRangeError = true;
                        }

                        if (dataRangeError)
                        {
                            sendErrorToApp(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
                            mappingPacket.liveStimulation.subCommand = en__Standby;
                        }
                        else
                        {
                            if ((tempB - tempA) > 8)
                            {
                                // 에러 전송
                                sendErrorToApp(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
                                mappingPacket.liveStimulation.subCommand = en__Standby;
                            }
                            else
                            {
                                mappingPacket.liveStimulation.equlizer_ReadStart_index = tempA;
                                mappingPacket.liveStimulation.equlizer_ReadEnd_index   = tempB;

                                mappingPacket.liveStimulation.subCommand = en__readEqualizer;
                            }
                        }
                    }
                    else
                    {
                        sendErrorToApp(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__Command_Order,
                                       __LINE__);  // 라이브 자극 중에만 컨트롤 되는 명령어

                        mappingPacket.liveStimulation.subCommand = en__Standby;
                    }
                }
                break;

                case en__readDeviceStatus:  // 하위 명령 7 (장치 상태 읽기)
                {
                    mappingPacket.liveStimulation.subCommand = en__readDeviceStatus;
                }
                break;

                case en__Stop:  // 하위 명령 8 (실시간 자극 종료)
                {
                    mappingPacket.liveStimulation.subCommand = en__Stop;
                }
                break;

                default:
                {
                    mappingPacket.liveStimulation.subCommand = en__Standby;
                }
                break;
            }
        }
        break;

        case en__mapping_deviceStatus:  // 헤더 0x67 (외부기 상태)
        {
            mappingPacket.fetched_command = en__mapping_deviceStatus;
        }
        break;

        case en__mapping_read_original_ISD_N_USER:  // 헤더 0x69 (외부기 원래 내부기 정보 읽어 오기)
        {
            mappingPacket.fetched_command = en__mapping_read_original_ISD_N_USER;
        }
        break;

        case en__mapping_write_original_ISD_N_USER:  // 헤더 0x68 (외부기 최초연결 사용자 이름 등록)
        {
            p_RepositoryFor_ISD_info = getPointerRepositoryForReadWriteMapData_isd_info();

            subCommandData_Num_index = Rx_dataPacket[index++];

            if (subCommandData_Num_index == 1)
            {
                prev_subCommandData_Num_index = 0;
            }

            if (subCommandData_Num_index != prev_subCommandData_Num_index + 1)
            {
                // 데이터가 순차적으로 들어와야된다. 순차적으로 들어 오지 않으면 에러 전송
                sendErrorToApp(en__mapping_write_original_ISD_N_USER, en__EN__BLE_PROTOCOL_ERROR, en__DATA_Order, __LINE__);

                prev_subCommandData_Num_index = 0;
                stimulPara_index              = 0;
            }
            else
            {
                prev_subCommandData_Num_index = subCommandData_Num_index;

                switch (subCommandData_Num_index)
                {
                    case 1:  // 데이터 인덱스 1
                    {
                        for (i = 0; i < 18; i++)
                        {
                            switch (i)
                            {
                                case 0:  // 내부기 제조번호  년 8bit
                                    p_RepositoryFor_ISD_info[i] = Rx_dataPacket[index++];

                                    mappingPacket.rx_orignal_ISD_info.ISD_id = p_RepositoryFor_ISD_info[i];
                                    mappingPacket.rx_orignal_ISD_info.ISD_id = ((mappingPacket.rx_orignal_ISD_info.ISD_id) << 8);
                                    break;

                                case 1:                                                    // 내부기 제조번호  월 4bit + 모델번호 4bit
                                    p_RepositoryFor_ISD_info[i] = Rx_dataPacket[index++];  // 내부기 제조번호 (년, 월_모델번호)

                                    // 실제 내부기 ID 확인용..
                                    mappingPacket.rx_orignal_ISD_info.ISD_id = mappingPacket.rx_orignal_ISD_info.ISD_id | p_RepositoryFor_ISD_info[i];
                                    mappingPacket.rx_orignal_ISD_info.ISD_id = ((mappingPacket.rx_orignal_ISD_info.ISD_id) << 16);

                                    tempValue = sizeof(mappingPacket.rx_orignal_ISD_info.ISD_id);
                                    break;

                                case 2:                                  // 시리얼 번호 상위 8bit
                                    tempValue = Rx_dataPacket[index++];  // 시리얼 번호 상위 8bit
                                    tempValue = tempValue << 8;
                                    break;

                                case 3:  // 시리얼 번호 하위 8bit
                                    // 상위 8bit와 하위 8bit를 합쳐서
                                    // 시리얼 번호가 16bit가 전달되면서 인덱스 감소 [i-1]
                                    p_RepositoryFor_ISD_info[i - 1] = tempValue | Rx_dataPacket[index++];  // 시리얼 번호 상위 8bit

                                    // 실제 내부기 ID 확인용..
                                    mappingPacket.rx_orignal_ISD_info.ISD_id = mappingPacket.rx_orignal_ISD_info.ISD_id | p_RepositoryFor_ISD_info[i - 1];
                                    break;

                                case 4:  // 수술 위치
                                    p_RepositoryFor_ISD_info[i - 1] = Rx_dataPacket[index++];

                                    // 실제 내부기 ID 확인용..
                                    mappingPacket.rx_orignal_ISD_info.location = p_RepositoryFor_ISD_info[i - 1];

                                    if ((mappingPacket.rx_orignal_ISD_info.location < 1) || (2 < mappingPacket.rx_orignal_ISD_info.location))
                                    {
                                        dataRangeError = true;
                                    }

                                    k = 0;
                                    break;

                                default:  // 사용자 이니셜
                                    p_RepositoryFor_ISD_info[i - 1] = Rx_dataPacket[index++];

                                    // 실제 내부기 ID 확인용..
                                    mappingPacket.rx_orignal_ISD_info.userName[k] = p_RepositoryFor_ISD_info[i - 1];
                                    // 시리얼 번호가 16bit가 전달되면서 인덱스 감소 [i-1]
                                    k++;
                                    break;
                            }
                        }
                    }
                    break;

                    case 2:  // 데이터 인덱스 2
                    {
                        for (i = 18, k = 13; i < 30; k++, i++)
                        {
                            p_RepositoryFor_ISD_info[i - 1] = Rx_dataPacket[index++];  //  사용자 이름 나머지

                            // 실제 내부기 ID 확인용..
                            mappingPacket.rx_orignal_ISD_info.userName[k] = p_RepositoryFor_ISD_info[i - 1];
                        }

                        mappingPacket.rx_orignal_ISD_info.id_check_is_completed = false;
                        mappingPacket.rx_orignal_ISD_info.id_match              = false;

                        //                                                              // 시리얼 번호가 16bit가
                        //                                                              전달되면서 인덱스 감소 [i-1]

                        // 나머지 데이터는 초기값으로 설정한다.
                        // 패스키
                        p_RepositoryFor_ISD_info[(i++) - 1] = 0x31;
                        p_RepositoryFor_ISD_info[(i++) - 1] = 0x31;
                        p_RepositoryFor_ISD_info[(i++) - 1] = 0x31;
                        p_RepositoryFor_ISD_info[(i++) - 1] = 0x31;

                        // 맵번호
                        p_RepositoryFor_ISD_info[(i++) - 1] = 1;  // 1~4
                        // 자극볼륨
                        p_RepositoryFor_ISD_info[(i++) - 1] = 4;  // 1~4
                        // 마이크감도
                        p_RepositoryFor_ISD_info[(i++) - 1] = 1;  // 1~10
                        // LED
                        p_RepositoryFor_ISD_info[(i++) - 1] = 1;  // 1 : 켬, 2 : 끔
                        // 자극알림
                        p_RepositoryFor_ISD_info[(i++) - 1] = 1;  // 1 : 켬, 2 : 끔
                        // 텔레코일
                        p_RepositoryFor_ISD_info[(i++) - 1] = 2;  // 1 : 켬, 2 : 끔

                        // Ble On/Off
                        p_RepositoryFor_ISD_info[(i++) - 1] = 1;  // 1 : 켬, 2 : 끔

                        // 맵 스템프
                        p_RepositoryFor_ISD_info[(i++) - 1] = 0;
                        p_RepositoryFor_ISD_info[(i++) - 1] = 0;
                        p_RepositoryFor_ISD_info[(i++) - 1] = 0;
                        p_RepositoryFor_ISD_info[(i++) - 1] = 0;
                        p_RepositoryFor_ISD_info[(i++) - 1] = 0;
                        p_RepositoryFor_ISD_info[(i++) - 1] = 0;
                    }
                    break;

                    default:
                    {
                    }
                    break;
                }

                if (!dataRangeError)
                {
                    if (subCommandData_Num_index != numPacket_writeMapData_Original_ISDnSetting)
                    {
                        bufferForSPI_tx[buffer_tx_index++] = en__mapping_write_original_ISD_N_USER;  // command loop-back
                        bufferForSPI_tx[buffer_tx_index++] = subCommandData_Num_index;               // payload num 전송

                        writeDataToSpiTxBuff(bufferForSPI_tx, buffer_tx_index);  // 송신 데이터 SPI TX버퍼에 복사
                    }
                    else  // 모든 데이터를 받은 시점에 Flash에 쓰기를 시작한다.
                    {
                        prev_subCommandData_Num_index = 0;

#if 0  //  매핑 명령 실행 하는 곳에서 최종 응답을 주게 변경하였다.
       // 플레쉬에 저장하면서 NRF 광고 이름을 바꾸기 위해서 NRF를 껏다가 켠다. 이러한 이유로 NRF를 끄기전에 수신된 명령을 루프백한다.
                        bufferForSPI_tx[buffer_tx_index++] = en__mapping_write_original_ISD_N_USER;  // command loop-back
                        bufferForSPI_tx[buffer_tx_index++] = subCommandData_Num_index;               // payload num 전송

                        writeDataToSpiTxBuff(bufferForSPI_tx, buffer_tx_index);  // 송신 데이터 SPI TX버퍼에 복사
#endif

                        mappingPacket.command = en__mapping_write_original_ISD_N_USER;

                        // 플래쉬에 저장 후, ble 응답 전송됨..해당 함수 참조
                    }
                }
                else
                {
                    // 에러 전송, 데이터 범위를 벗어남
                    sendErrorToApp(en__mapping_write_original_ISD_N_USER, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
                }
            }
        }
        break;

        case en__mapping_read_SlotData_ISD_N_USER:  // 헤더 0x6A (맵 프로그램 관리: 읽기 - 내부기 ID 및 사용자)
        {
            tempValue = Rx_dataPacket[index++];
            if ((tempValue >= 1)               // 1 이상
                && (tempValue <= MaxNumUser))  // 4 이하만 유효
            {
                mappingPacket.ReadWriteMapData_Flash.slot_index = tempValue;
                mappingPacket.ReadWriteMapData_Flash.map_index  = 0;

                mappingPacket.fetched_command = en__mapping_read_SlotData_ISD_N_USER;
            }
            else
            {
                // 에러 전송
                // 데이터 범위를 벗어남
                sendErrorToApp(en__mapping_read_SlotData_ISD_N_USER, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
            }
        }
        break;

        case en__mapping_write_SlotData_ISD_N_USER:  // 헤더 0x6C (맵 프로그램 관리: 쓰기 - 내부기 ID 및 사용자)
        {
            p_RepositoryFor_ISD_info = getPointerRepositoryForReadWriteMapData_isd_info();

            subCommandData_Num_index = Rx_dataPacket[index++];

            if (subCommandData_Num_index == 1)
            {
                prev_subCommandData_Num_index = 0;
            }

            if (subCommandData_Num_index != prev_subCommandData_Num_index + 1)
            {
                // 데이터가 순차적으로 들어와야된다. 순차적으로 들어 오지 않으면 에러 전송
                sendErrorToApp(en__mapping_write_SlotData_ISD_N_USER, en__EN__BLE_PROTOCOL_ERROR, en__DATA_Order, __LINE__);

                prev_subCommandData_Num_index = 0;
                stimulPara_index              = 0;
            }
            else
            {
                prev_subCommandData_Num_index = subCommandData_Num_index;

                switch (subCommandData_Num_index)
                {
                    case 1:  // 데이터 인덱스 1
                    {
                        // 슬롯 번호 범위가 맞으면 진행
                        tempValue = Rx_dataPacket[index++];
                        if ((tempValue >= 0) && (tempValue <= MaxNumUser))
                        {
                            mappingPacket.ReadWriteMapData_Flash.slot_index = tempValue;  // 내부기 Num

                            for (i = 0; i < 17; i++)
                            {
                                switch (i)
                                {
                                    case 0:
                                    case 1:
                                        p_RepositoryFor_ISD_info[i] = Rx_dataPacket[index++];  // 내부기 제조번호 (년, 월_모델번호)
                                        break;

                                    case 2:                                  // 시리얼 번호 상위 8bit
                                        tempValue = Rx_dataPacket[index++];  // 시리얼 번호 상위 8bit
                                        tempValue = tempValue << 8;
                                        break;

                                    case 3:                                                                    // 시리얼 번호 하위 8bit
                                        p_RepositoryFor_ISD_info[i - 1] = tempValue | Rx_dataPacket[index++];  // 시리얼 번호 상위 8bit
                                        // 시리얼 번호가 16bit가 전달되면서 인덱스 감소 [i-1]
                                        break;

                                    case 4:  // 수술 위치
                                        p_RepositoryFor_ISD_info[i - 1] = Rx_dataPacket[index++];
                                        // 시리얼 번호가 16bit가 전달되면서 인덱스 감소 [i-1]
                                        if ((p_RepositoryFor_ISD_info[i - 1] < 1) || (2 < p_RepositoryFor_ISD_info[i - 1]))
                                        {
                                            dataRangeError = true;
                                        }
                                        break;

                                    default:
                                        p_RepositoryFor_ISD_info[i - 1] = Rx_dataPacket[index++];
                                        // 시리얼 번호가 16bit가 전달되면서 인덱스 감소 [i-1]
                                        break;
                                }
                            }
                        }
                        else
                        {
                            dataRangeError = true;
                        }
                    }
                    break;

                    case 2:  // 데이터 인덱스 2
                    {
                        for (i = 17; i < 35; i++)
                        {
                            //  사용자 이름 나머지, 내부기 제어용 passkey, 맵번호, 시리얼
                            p_RepositoryFor_ISD_info[i - 1] = Rx_dataPacket[index++];  //  번호가 16bit가 전달되면서 인덱스 감소 [i-1]

                            switch (i)
                            {
                                case 30:  // 내부기 패스키
                                case 31:  // 내부기 패스키
                                case 32:  // 내부기 패스키
                                case 33:  // 내부기 패스키
                                    if (!((('0' <= p_RepositoryFor_ISD_info[i - 1]) && (p_RepositoryFor_ISD_info[i - 1] <= '9')) || (('a' <= p_RepositoryFor_ISD_info[i - 1]) && (p_RepositoryFor_ISD_info[i - 1] <= 'z')) || (('A' <= p_RepositoryFor_ISD_info[i - 1]) && (p_RepositoryFor_ISD_info[i - 1] <= 'Z'))))
                                    {
                                        dataRangeError = true;
                                    }
                                    break;

                                case 34:  // 맵 번호
                                    if (!((1 <= p_RepositoryFor_ISD_info[i - 1]) && (p_RepositoryFor_ISD_info[i - 1] <= 4)))
                                    {
                                        dataRangeError = true;
                                    }
                                    break;
                            }
                        }
                    }
                    break;

                    case 3:  // 데이터 인덱스 3
                    {
                        i = 35;

                        // 자극 볼륨
                        tempValue = Rx_dataPacket[index++];  // i=35

                        if ((1 <= tempValue) && (tempValue <= df_maxStimulationVloumeLevel))
                        {
                            p_RepositoryFor_ISD_info[i - 1] = tempValue;
                        }
                        else
                        {
                            dataRangeError = true;
                        }

                        // 마이크 감도
                        i++;  // i=36
                        tempValue = Rx_dataPacket[index++];

                        if ((1 <= tempValue) && (tempValue <= df_maxMicVloumeLevel))
                        {
                            p_RepositoryFor_ISD_info[i - 1] = tempValue;
                        }
                        else
                        {
                            dataRangeError = true;
                        }

                        // LED 설정
                        i++;  // i=37
                        tempValue = Rx_dataPacket[index++];

                        if ((1 <= tempValue) && (tempValue <= 2))
                        {
                            p_RepositoryFor_ISD_info[i - 1] = tempValue;
                        }
                        else
                        {
                            dataRangeError = true;
                        }

                        // 자극 알림 설정
                        i++;  // i=38
                        tempValue = Rx_dataPacket[index++];

                        if ((1 <= tempValue) && (tempValue <= 2))
                        {
                            p_RepositoryFor_ISD_info[i - 1] = tempValue;
                        }
                        else
                        {
                            dataRangeError = true;
                        }

                        // telecoil 설정
                        i++;  // i=39
                        tempValue = Rx_dataPacket[index++];

                        // 1세대 적용된 마지막 매핑 프로토콜 v1.0.5 기준으로는
                        // 텔레코일 값 범위가 1에서 2로 되어 있지만,
                        // 실제 운용되는 리모콘 및 매핑 앱에서는 텔레코일을 사용하지 않기 때문에
                        // 0 값을 전달한다. 이로 인해 데이터 범위 에러가 발생한다.
                        // 0에서 2까지 범위를 허용하도록 잠수함 패치한다. (2026.02.23 by 김은수)
                        if ((0 /*1*/ <= tempValue) && (tempValue <= 2))
                        {
                            p_RepositoryFor_ISD_info[i - 1] = tempValue;
                        }
                        else
                        {
                            dataRangeError = true;
                        }
#if 0
                        p_RepositoryFor_ISD_info[i - 1] = tempValue;
#else  // Telecoil은 현재 버전에서 비활성화 시킨다.
                        p_RepositoryFor_ISD_info[i - 1] = 2;
#endif
                        //  자극 볼륨, 마이크 감도, LED 설정, 자극 알림 설정, 텔레 코일 설정,

                        // BLE On/OFF 옵션
                        i++;                                  // i=40
                        p_RepositoryFor_ISD_info[i - 1] = 1;  // BLE On/OFF 옵션 (패킷 프로토콜에는 없음. 항상 1로 설정시키게 함)

                        for (i = 41; i < 47; i++)
                        {
                            p_RepositoryFor_ISD_info[i - 1] = Rx_dataPacket[index++];  //   맵 스템프
                        }

                        // 시리얼 번호가 16bit가 전달되면서 인덱스 감소 [i-1]
                    }
                    break;

                    default:
                    {
                    }
                    break;
                }

                if (!dataRangeError)
                {
                    if (subCommandData_Num_index != numPacket_writeMapData_ISDnSetting)
                    {
                        bufferForSPI_tx[buffer_tx_index++] = en__mapping_write_SlotData_ISD_N_USER;  // command loop-back
                        bufferForSPI_tx[buffer_tx_index++] = subCommandData_Num_index;               // payload num 전송

                        writeDataToSpiTxBuff(bufferForSPI_tx, buffer_tx_index);  // 송신 데이터 SPI TX버퍼에 복사
                    }
                    else  // 모든 데이터를 받은 시점에 Flash에 쓰기를 시작한다.
                    {
                        prev_subCommandData_Num_index = 0;
                        mappingPacket.command         = en__mapping_write_SlotData_ISD_N_USER;

                        // 플래쉬에 저장 후, ble 응답 전송됨..해당 함수 참조
                    }
                }
                else
                {
                    // 에러 전송
                    // 데이터 범위를 벗어남
                    sendErrorToApp(en__mapping_write_SlotData_ISD_N_USER, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
                }
            }
        }
        break;

        case en__mapping_read_Mapdata_STIMUL_PARA:  // 헤더 0x6B (맵 프로그램 관리: 읽기 - 맵 데이터)
        {
            tempValue = Rx_dataPacket[index++];

            if ((tempValue >= 0) && (tempValue <= MaxNumUser))
            {
                mappingPacket.ReadWriteMapData_Flash.slot_index = tempValue;
                tempValue                                       = Rx_dataPacket[index++];

                if ((tempValue >= 1) && (tempValue <= MaxNumMap))
                {
                    mappingPacket.ReadWriteMapData_Flash.map_index = tempValue;
                    mappingPacket.fetched_command                  = en__mapping_read_Mapdata_STIMUL_PARA;
                }
                else
                {
                    // 에러 전송
                    // 데이터 범위를 벗어남
                    sendErrorToApp(en__mapping_read_Mapdata_STIMUL_PARA, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
                }
            }
            else
            {
                // 에러 전송
                // 데이터 범위를 벗어남
                sendErrorToApp(en__mapping_read_Mapdata_STIMUL_PARA, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
            }
        }
        break;

        case en__mapping_write_Mapdata_STIMUL_PARA:  // 헤더 0x6D (맵 프로그램 관리: 쓰기 - 맵 데이터)
        {
            p_RepositoryFor_stimulPara = getPointerRepositoryForReadWriteMapData_stimulPara();

            subCommandData_Num_index = Rx_dataPacket[index++];
            if (subCommandData_Num_index == 1)
            {
                prev_subCommandData_Num_index = 0;
            }

            if (subCommandData_Num_index != prev_subCommandData_Num_index + 1)
            {
                // 데이터가 순차적으로 들어와야된다. 순차적으로 들어 오지 않으면 에러 전송
                sendErrorToApp(en__mapping_write_Mapdata_STIMUL_PARA, en__EN__BLE_PROTOCOL_ERROR, en__DATA_Order, __LINE__);
                prev_subCommandData_Num_index = 0;
            }
            else
            {
                prev_subCommandData_Num_index = subCommandData_Num_index;

                switch (subCommandData_Num_index)
                {
                    case 1:  // 데이터 인덱스 1
                    {
                        // 슬롯 범위가 맞아야 진입
                        tempValue = Rx_dataPacket[index++];
                        if ((tempValue >= 0) && (tempValue <= MaxNumUser))
                        {
                            mappingPacket.ReadWriteMapData_Flash.slot_index = tempValue;  // 내부기 Num

                            tempValue = Rx_dataPacket[index++];

                            // 프로그램 범위가 맞아야 진입
                            if ((tempValue >= 1) && (tempValue <= MaxNumMap))
                            {
                                mappingPacket.ReadWriteMapData_Flash.map_index = tempValue;  // 프로그램 번호

                                stimulPara_index = 0;
                                for (i = 0; i < 12; i++)
                                {
                                    p_RepositoryFor_stimulPara[stimulPara_index] = Rx_dataPacket[index++];  // 매핑 일자, 자극 펄스 파라미터 들..

                                    switch (i)
                                    {
                                        case 6:  // 자극 기법
                                            if (!((1 <= p_RepositoryFor_stimulPara[stimulPara_index]) && (p_RepositoryFor_stimulPara[stimulPara_index] <= 3)))
                                            {
                                                dataRangeError = true;
                                            }
                                            break;

                                        case 7:  // 선행 펄스 위상
                                            if (!((0 <= p_RepositoryFor_stimulPara[stimulPara_index]) && (p_RepositoryFor_stimulPara[stimulPara_index] <= 1)))
                                            {
                                                dataRangeError = true;
                                            }
                                            break;

                                        case 8:  // 자극 모드
                                            if (!((1 <= p_RepositoryFor_stimulPara[stimulPara_index]) && (p_RepositoryFor_stimulPara[stimulPara_index] <= 6)))
                                            {
                                                dataRangeError = true;
                                            }
                                            break;

                                        case 9:  // 펄스 위상 폭
                                            if (!((13 <= p_RepositoryFor_stimulPara[stimulPara_index]) && (p_RepositoryFor_stimulPara[stimulPara_index] <= 255)))
                                            {
                                                dataRangeError = true;
                                            }
                                            break;

                                        case 10:  // 주파수 밴드 수
                                            if (!((1 <= p_RepositoryFor_stimulPara[stimulPara_index]) && (p_RepositoryFor_stimulPara[stimulPara_index] <= 32)))
                                            {
                                                dataRangeError = true;
                                            }
                                            break;

                                        case 11:  // 알람용 자극 채널 번호
                                            if (!((1 <= p_RepositoryFor_stimulPara[stimulPara_index]) && (p_RepositoryFor_stimulPara[stimulPara_index] <= 32)))
                                            {
                                                dataRangeError = true;
                                            }
                                            break;
                                    }

                                    stimulPara_index++;
                                }

                                intFromByte                                    = Rx_dataPacket[index++] << 8;           // 자극 알람 크기, 상위 바이트
                                intFromByte                                    = intFromByte | Rx_dataPacket[index++];  // 상위|하위바이트
                                p_RepositoryFor_stimulPara[stimulPara_index++] = intFromByte;
                                if (!((0 /*1*/ <= intFromByte) && (intFromByte <= 1800)))  // 범위 수정: 26.02.25 김은수
                                {
                                    dataRangeError = true;
                                }
                            }
                            else
                            {
                                // 데이터 범위 에러 발생
                                dataRangeError = true;
                            }
                        }
                        else
                        {
                            // 데이터 범위 에러 발생
                            dataRangeError = true;
                        }
                    }
                    break;

                    case 2:
                    {
                        for (i = 0; i < 18; i++)
                        {
                            p_RepositoryFor_stimulPara[stimulPara_index] = Rx_dataPacket[index++];  // 사용가능 전극 번호 18개
                            if (!((1 <= p_RepositoryFor_stimulPara[stimulPara_index]) && (p_RepositoryFor_stimulPara[stimulPara_index] <= 100)))
                            {
                                dataRangeError = true;
                            }
                            stimulPara_index++;
                        }
                    }
                    break;

                    case 3:
                    {
                        for (i = 0; i < 14; i++)
                        {
                            p_RepositoryFor_stimulPara[stimulPara_index] = Rx_dataPacket[index++];  // 사용가능 전극 번호 12개
                            if (!((1 <= p_RepositoryFor_stimulPara[stimulPara_index]) && (p_RepositoryFor_stimulPara[stimulPara_index] <= 100)))
                            {
                                dataRangeError = true;
                            }
                            stimulPara_index++;
                        }
                    }
                    break;

                    case 4:
                    {
                        for (i = 0; i < 18; i++)
                        {
                            p_RepositoryFor_stimulPara[stimulPara_index] = Rx_dataPacket[index++];  //  바이폴라 기준 전극 18개
                            if (!((1 <= p_RepositoryFor_stimulPara[stimulPara_index]) && (p_RepositoryFor_stimulPara[stimulPara_index] <= 100)))
                            {
                                dataRangeError = true;
                            }
                            stimulPara_index++;
                        }
                    }
                    break;

                    case 5:
                    {
                        for (i = 0; i < 14; i++)
                        {
                            p_RepositoryFor_stimulPara[stimulPara_index] = Rx_dataPacket[index++];  //  바이폴라 기준 전극 12개
                            if (!((1 <= p_RepositoryFor_stimulPara[stimulPara_index]) && (p_RepositoryFor_stimulPara[stimulPara_index] <= 100)))
                            {
                                dataRangeError = true;
                            }
                            stimulPara_index++;
                        }
                    }
                    break;

                    case 6:
                    {
                        for (i = 0; i < 18; i++)
                        {
                            p_RepositoryFor_stimulPara[stimulPara_index] = Rx_dataPacket[index++];  //  주파수 밴드 출력 순서 18개
                            if (!((1 <= p_RepositoryFor_stimulPara[stimulPara_index]) && (p_RepositoryFor_stimulPara[stimulPara_index] <= 100)))
                            {
                                dataRangeError = true;
                            }
                            stimulPara_index++;
                        }
                    }
                    break;

                    case 7:
                    {
                        for (i = 0; i < 14; i++)
                        {
                            p_RepositoryFor_stimulPara[stimulPara_index] = Rx_dataPacket[index++];  //  주파수 밴드 출력 순서 12개
                            if (!((1 <= p_RepositoryFor_stimulPara[stimulPara_index]) && (p_RepositoryFor_stimulPara[stimulPara_index] <= 100)))
                            {
                                dataRangeError = true;
                            }
                            stimulPara_index++;
                        }
                    }
                    break;

                    case 8:
                    case 9:
                    case 10:
                    case 11:
                    {
                        // T_uA 자극 크기
                        for (i = 0; i < 8; i++)
                        {
                            intFromByte                                    = Rx_dataPacket[index++] << 8;           // 상위 바이트
                            intFromByte                                    = intFromByte | Rx_dataPacket[index++];  // 상위|하위바이트
                            p_RepositoryFor_stimulPara[stimulPara_index++] = intFromByte;

                            if (!((0 <= intFromByte) && (intFromByte <= 1800)))
                            {
                                dataRangeError = true;
                            }
                        }
                    }
                    break;

                    case 12:
                    case 13:
                    case 14:
                    case 15:
                    {
                        // C_uA 자극 크기
                        for (i = 0; i < 8; i++)
                        {
                            intFromByte                                    = Rx_dataPacket[index++] << 8;           // 상위 바이트
                            intFromByte                                    = intFromByte | Rx_dataPacket[index++];  // 상위|하위바이트
                            p_RepositoryFor_stimulPara[stimulPara_index++] = intFromByte;

                            if (!((0 <= intFromByte) && (intFromByte <= 1800)))
                            {
                                dataRangeError = true;
                            }
                        }
                    }
                    break;

                    default:
                    {
                    }
                    break;
                }

                if (!dataRangeError)
                {
                    if (subCommandData_Num_index != numPacket_writeMapData_stimulPara)
                    {
                        // command loop-back
                        bufferForSPI_tx[buffer_tx_index++] = en__mapping_write_Mapdata_STIMUL_PARA;

                        // payload num 전송

                        bufferForSPI_tx[buffer_tx_index++] = subCommandData_Num_index;  // payload num 전송

                        // 송신 데이터 SPI TX버퍼에 복사
                        writeDataToSpiTxBuff(bufferForSPI_tx, buffer_tx_index);
                    }
                    else  // 모든 데이터를 받은 시점에 Flash에 쓰기를 시작한다.
                    {
                        // xmin
                        for (i = 0; i < df_MaxNumOfElectrode; i++)
                        {
                            p_RepositoryFor_stimulPara[stimulPara_index++] = df_minAudioForLogarithm;
                        }

                        // xmax
                        for (i = 0; i < df_MaxNumOfElectrode; i++)
                        {
                            p_RepositoryFor_stimulPara[stimulPara_index++] = df_maxAudioForLogarithm;
                        }

                        prev_subCommandData_Num_index = 0;
                        stimulPara_index              = 0;

                        mappingPacket.fetched_command = en__mapping_write_Mapdata_STIMUL_PARA;

                        // 플래쉬에 저장 후, ble 응답 전송됨..해당 함수 참조
                    }
                }
                else
                {
                    sendErrorToApp(en__mapping_write_Mapdata_STIMUL_PARA, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
                }
            }
        }
        break;

        case en__mapping_erase_SlotData_manufacture:  // 헤더 0x6E (선택한 슬롯의 모든 맵데이터 삭제)
        {
            mappingPacket.fetched_command                   = tempCommand;
            mappingPacket.ReadWriteMapData_Flash.slot_index = Rx_dataPacket[index++];

            if (!((1 <= mappingPacket.ReadWriteMapData_Flash.slot_index) && (mappingPacket.ReadWriteMapData_Flash.slot_index <= 4)))
            {
                sendErrorToApp(en__mapping_erase_SlotData_manufacture, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
                mappingPacket.fetched_command = en__mapping_IDLE;
            }
        }
        break;

        case en__mapping_erase_mapData_STIMUL_PARA:  // 헤더 0x6F (선택한 맵 삭제)
        {
            mappingPacket.fetched_command                   = tempCommand;
            mappingPacket.ReadWriteMapData_Flash.slot_index = Rx_dataPacket[index++];
            mappingPacket.ReadWriteMapData_Flash.map_index  = Rx_dataPacket[index++];

            if (!((1 <= mappingPacket.ReadWriteMapData_Flash.slot_index) && (mappingPacket.ReadWriteMapData_Flash.slot_index <= 4)))
            {
                sendErrorToApp(en__mapping_erase_mapData_STIMUL_PARA, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
                mappingPacket.fetched_command = en__mapping_IDLE;
            }

            if (!((1 <= mappingPacket.ReadWriteMapData_Flash.map_index) && (mappingPacket.ReadWriteMapData_Flash.map_index <= 4)))
            {
                sendErrorToApp(en__mapping_erase_mapData_STIMUL_PARA, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
                mappingPacket.fetched_command = en__mapping_IDLE;
            }
        }
        break;

        case en__mapping_recover_mppingData_exceptSlot_1:  // 0x70
        {
            writingStartSlot_index                          = 2;
            mappingPacket.fetched_command                   = tempCommand;
            mappingPacket.ReadWriteMapData_Flash.slot_index = Rx_dataPacket[index++];
            mappingPacket.ReadWriteMapData_Flash.map_index  = Rx_dataPacket[index++];
        }
        break;

        case en__mapping_recover_ALL_SlotData_ManufactureData:  // 0x71
        {
            writingStartSlot_index        = 1;
            mappingPacket.fetched_command = tempCommand;
        }
        break;

#ifndef RELEASE
        case en__mapping_testStimulation:
        {

            subCommandData_Num_index = Rx_dataPacket[index++];

            if (subCommandData_Num_index == 1)
                prev_subCommandData_Num_index = 0;

            if (subCommandData_Num_index != prev_subCommandData_Num_index + 1)
            {
                // 데이터가 순차적으로 들어와야된다. 순차적으로 들어 오지 않으면 에러 전송
                // 에러 전송
                sendErrorToApp(en__mapping_testStimulation, en__EN__BLE_PROTOCOL_ERROR, en__DATA_Order,
                               __LINE__);  // 데이터 범위 벗어남

                prev_subCommandData_Num_index = 0;
                stimulPara_index              = 0;
            }
            else
            {

                prev_subCommandData_Num_index = subCommandData_Num_index;
                switch (subCommandData_Num_index)
                {
                    case 1:  //
                    {
                        mappingPacket.testStimulation.stimulatonMode                = Rx_dataPacket[index++];
                        trashValue                                                  = Rx_dataPacket[index++];  // 오프셋 필요없음.
                        mappingPacket.testStimulation.firstPulsePhase               = Rx_dataPacket[index++];
                        mappingPacket.testStimulation.pulseWidth                    = Rx_dataPacket[index++];
                        mappingPacket.testStimulation.stimulationDacSlope           = Rx_dataPacket[index++];
                        mappingPacket.testStimulation.stimulationDacOffsetReslution = Rx_dataPacket[index++];
                        mappingPacket.testStimulation.stimulationDacOffset_255      = Rx_dataPacket[index++];
                        mappingPacket.testStimulation.stimulationLevel_255          = Rx_dataPacket[index++];
                        mappingPacket.testStimulation.stimulationTime_100msec       = Rx_dataPacket[index++];
                    }
                    break;
                    case 2:
                    {
                        mappingPacket.testStimulation.usableElectrodeNum = Rx_dataPacket[index++];
                        for (i = 0; i < 16; i++)
                            mappingPacket.testStimulation.stimulationElectrodeNum[i] = Rx_dataPacket[index++];
                    }
                    case 3:
                    {
                        for (i = 16; i < df_MaxNumOfElectrode; i++)
                            mappingPacket.testStimulation.stimulationElectrodeNum[i] = Rx_dataPacket[index++];
                    }
                    break;
                    case 4:

                    {
                        for (i = 0; i < 16; i++)
                            mappingPacket.testStimulation.bipolarReferenceElectrodeNum[i] = Rx_dataPacket[index++];
                    }
                    break;
                    case 5:
                    {
                        for (i = 16; i < df_MaxNumOfElectrode; i++)
                            mappingPacket.testStimulation.bipolarReferenceElectrodeNum[i] = Rx_dataPacket[index++];

                        mappingPacket.fetched_command = en__mapping_testStimulation;
                    }
                    break;

                    default:
                        break;
                }

                if (subCommandData_Num_index < 5)  // 마지막 데이이타 이전에는 데이터 수신 후 바로 응답을 보내고, 마지막
                                                   // 데이터는 자극 출력 후 응답을 보낸다.
                {

                    // command loop-back
                    bufferForSPI_tx[buffer_tx_index++] = en__mapping_testStimulation;

                    bufferForSPI_tx[buffer_tx_index++] = subCommandData_Num_index;  // payload num 전송

                    // 송신 데이터 SPI TX버퍼에 복사
                    writeDataToSpiTxBuff(bufferForSPI_tx, buffer_tx_index);
                }
            }
        }
        break;
#endif
        case en__mapping_read_Connected_ISD_id:
        {
            mappingPacket.fetched_command = tempCommand;
        }
        break;

        default:
        {
            // command loop-back
            bufferForSPI_tx[buffer_tx_index++] = tempCommand;

            bufferForSPI_tx[buffer_tx_index++] = en__UndefinedCommand;  //

            // 송신 데이터 SPI TX버퍼에 복사
            writeDataToSpiTxBuff(bufferForSPI_tx, buffer_tx_index);
        }
        break;
    }
}

ST__MAPPING_STATE mappingControl(ST__ISD_STATUS ISD_state)
{
    static EN__MAPPING_COMMAND prev_mppingCommand      = en__mapping_IDLE;
    static int                 connectionCheckCounter  = df_connectionCheckPeriod_ms;
    static int                 delayCounter            = 0;
    static bool                connectionCheckING_Flag = false;
    static bool                mappingProgramConnected = false;

    ST__MAPPING_STATE     mappingStatus     = {en__isdStatus_NA, false, false, false};
    EN__ISD_CONTROL_STATE isdControlCommand = en__isdStatus_NA;
    ST__ERROR_CODE        errorCode;

    int  bufferForSPI_tx[BLE_DataPacketSize];
    int  buffer_tx_index;
    int  i;
    int *p_variable;
    int  value;

    bool mappingCommandStartFlag = false;
    bool result;
    bool sitmulationIndicatorTrigger = false;
    bool BLE_Off_mapping             = false;

    // 매핑 명령이 수신되 시접에 내부기 연결 확인용 backtel 전송 명령이 실행 중일 경우에는 백텔 수신이 완료되고 명령을 실행 할 수 있도록 한다.
    if (mappingPacket.fetched_command != en__mapping_IDLE)
    {
        if (!isING_connectionCheckWithMapping())
        {
            mappingPacket.command         = mappingPacket.fetched_command;
            mappingPacket.fetched_command = en__mapping_IDLE;

            // TDC_PRINTF_V("\r\n[MAPPING] COMMAND <-- FETCHED COMMAND (0x%02X) \r\n", mappingPacket.command);
        }
        else
        {
            if (prev_mppingCommand != mappingPacket.command)
            {
                TDC_PRINTF_V("\r\n[MAPPING] FETCHED COMMAND REMAINED : 0x%02X \r\n", mappingPacket.fetched_command);
                TDC_PRINTF_V("[MAPPING] PREV COMMAND : 0x%02X, CURRENT COMMAND : 0x%02X \r\n", prev_mppingCommand, mappingPacket.command);
                TDC_PRINTF_V("[MAPPING] BUT, NOW WAITING FOR LINK CONNECTION CHECK \r\n");
            }
        }
    }

    if (prev_mppingCommand != mappingPacket.command)
    {
        mappingCommandStartFlag = true;
    }
    else
    {
        mappingCommandStartFlag = false;
    }

    prev_mppingCommand = mappingPacket.command;

    buffer_tx_index = 0;

    if (mappingPacket.command == en__mapping_connect)
    {
        mappingProgramConnected = true;
        connectionCheckCounter  = 10;  // 연결되고 10msec 이후에 연결 체크(백텔)를 진행하도록 카운터 설정

        // 송신 데이터 준비
        bufferForSPI_tx[buffer_tx_index++] = mappingPacket.command;  // command loop-back
        bufferForSPI_tx[buffer_tx_index++] = 1;                      // pay-load 준비
        writeDataToSpiTxBuff(bufferForSPI_tx, buffer_tx_index);      // 송신 데이터 SPI TX버퍼에 복사
        mappingPacket.command = en__mapping_IDLE;                    // 명령 종료
    }
    else
    {
        if (mappingProgramConnected)
        {
            switch (mappingPacket.command)
            {
                case en__mapping_disconnect:
                {
                    // 송신 데이터 준비
                    bufferForSPI_tx[buffer_tx_index++] = mappingPacket.command;  // command loop-back
                    bufferForSPI_tx[buffer_tx_index++] = 1;                      // pay-load 준비
                    writeDataToSpiTxBuff(bufferForSPI_tx, buffer_tx_index);      // 송신 데이터 SPI TX버퍼에 복사

#if 1
                    while (1)
                    {
                        // 매핑 앱에서 (블루투스가 아닌, 매핑 기능) 연결을 종료하는 경우,
                        // SPI TX 버퍼가 모두 전송되고 난 뒤 시스템을 재부팅 시킨다. (with 워치독 리셋)
                        if (isSpiTxBuffEmpty())
                        {
                            clear_mappingCommand();
                            changeSystemModeFlag(en__systemReset);

                            for (int i = 0; i < 100; i++)
                            {
                                SYS_WATCHDOG_REFRESH();
                                Sys_Delay((SystemCoreClock / 1000));  // 1ms
                            }

                            SYS_WATCHDOG_RESET();  // NOTE: 강제 리셋
                        }
                    }
#else
                    clear_mappingCommand();
                    mappingProgramConnected = false;
                    isdControlCommand       = en__isdStatus_PowerIC_Reset;

                    // update_isd_Link_is_Disconnected();
#endif
                }
                break;

                case en__mapping_ble_disconneted:
                {
                    clear_mappingCommand();
                    mappingProgramConnected = false;
                    isdControlCommand       = en__isdStatus_PowerIC_Reset;

                    // update_isd_Link_is_Disconnected();
                }
                break;

                case en__mapping_impedanceChekck:
                {
                    // 카운터를 df_connectionCheckPeriod_ms로 리셋하여 연결확인 진행하지 않게 한다.
                    connectionCheckCounter = df_connectionCheckPeriod_ms;

                    if (ISD_state.conneded_ISD)
                    {
                        impedanceMeasurement(mappingCommandStartFlag);
                    }
                    else
                    {
                        TDC_PRINTF_E("[MAPPING] CAN NOT CHECK IMPEDANCE, BECAUSE ISD NOT CONNECTED \r\n");
                        sendErrorToApp(en__mapping_impedanceChekck, en__EN__ISD_ERROR, en__ISD_notConnected, __LINE__);
                        clear_mappingCommand();
                    }
                }
                break;

                case en__mapping_eCAP_Measurement_masking:
                {
                    // 카운터를 df_connectionCheckPeriod_ms로 리셋하여 연결확인 진행하지 않게 한다.
                    connectionCheckCounter = df_connectionCheckPeriod_ms;
                    if (ISD_state.conneded_ISD)
                    {
                        eCapMeasurement_masking(mappingCommandStartFlag);
                    }
                    else
                    {
                        sendErrorToApp(en__mapping_eCAP_Measurement_masking, en__EN__ISD_ERROR, en__ISD_notConnected, __LINE__);
                        clear_mappingCommand();
                    }
                }
                break;

                case en__mapping_eCAP_Measurement_alternative:
                {
                    // 카운터를 df_connectionCheckPeriod_ms로 리셋하여 연결확인 진행하지 않게 한다.
                    connectionCheckCounter = df_connectionCheckPeriod_ms;
                    // eCapMeasurement_masking(mappingCommandStartFlag);
                }
                break;

                case en__mapping_specific_stimulation:
                {
                    // 카운터를 df_connectionCheckPeriod_ms로 리셋하여 연결확인 진행하지 않게 한다.
                    connectionCheckCounter = df_connectionCheckPeriod_ms;
                    if (ISD_state.conneded_ISD)
                    {
                        specificStimulation(mappingCommandStartFlag);
                    }
                    else
                    {
                        sendErrorToApp(en__mapping_specific_stimulation, en__EN__ISD_ERROR, en__ISD_notConnected, __LINE__);
                        clear_mappingCommand();
                    }
                }
                break;

                case en__mapping_live_stimulation:
                {
                    connectionCheckCounter      = df_connectionCheckPeriod_ms;
                    sitmulationIndicatorTrigger = liveStimulation(ISD_state);
                    // 카운터를 df_connectionCheckPeriod_ms로 리셋하여 연결확인 진행하지 않게 한다.
                }
                break;

                case en__mapping_deviceStatus:
                {
                    // command loop-back
                    bufferForSPI_tx[buffer_tx_index++] = en__mapping_deviceStatus;

                    // 내부기 연결 상태
                    if (ISD_state.conneded_ISD)
                    {
                        bufferForSPI_tx[buffer_tx_index++] = 1;  // 연결됨
                    }
                    else
                    {
                        bufferForSPI_tx[buffer_tx_index++] = 2;  // 끊어짐
                    }

                    // 배터리 레벨
                    bufferForSPI_tx[buffer_tx_index++] = readBatteryPercentage();

                    // ST__ERROR_CODE 전달
                    errorCode = readErrorCode();

                    bufferForSPI_tx[buffer_tx_index++] = (int) errorCode.ISD_ErrorFlag;

                    bufferForSPI_tx[buffer_tx_index++] = 0;

                    // NRF에 전달

                    // 송신 데이터 SPI TX버퍼에 복사
                    writeDataToSpiTxBuff(bufferForSPI_tx, buffer_tx_index);

                    clear_mappingCommand();
                }
                break;

                case en__mapping_read_original_ISD_N_USER:
                {
                    read_Original_isdInfo_N_userSetting_fromFlash(mappingCommandStartFlag, en__mapping_read_original_ISD_N_USER);
                }
                break;

                case en__mapping_write_original_ISD_N_USER:
                {
                    /* 플레쉬에 저장하면서 BLE 광고 이름을 바꾸기 위해서
                     * change_isd_state(en__isdStatus_ISD_Power_Ok);를 사용해 내부기 연결 해제를 유도하여
                     * BLE 연결 해제 후 광고이름 변경하여 진행하게 한다. */
                    write_Original_isdInfo_N_userSetting_atFlash(mappingCommandStartFlag, en__mapping_write_original_ISD_N_USER);
                }
                break;

                case en__mapping_read_SlotData_ISD_N_USER:
                {
                    read_isdInfo_N_userSetting_fromFlash(mappingCommandStartFlag, en__mapping_read_SlotData_ISD_N_USER, mappingPacket.ReadWriteMapData_Flash.slot_index);
                }
                break;

                case en__mapping_write_SlotData_ISD_N_USER:
                {
                    write_isdInfo_N_userSetting_atFlash(mappingCommandStartFlag, en__mapping_write_SlotData_ISD_N_USER, mappingPacket.ReadWriteMapData_Flash.slot_index);
                }
                break;

                case en__mapping_read_Mapdata_STIMUL_PARA:
                {
                    read_stimulPara_fromFlash(mappingCommandStartFlag, en__mapping_read_Mapdata_STIMUL_PARA, mappingPacket.ReadWriteMapData_Flash.slot_index, mappingPacket.ReadWriteMapData_Flash.map_index);
                }
                break;

                case en__mapping_write_Mapdata_STIMUL_PARA:
                {
                    write_stimulPara_atFlash(mappingCommandStartFlag, en__mapping_write_Mapdata_STIMUL_PARA, mappingPacket.ReadWriteMapData_Flash.slot_index, mappingPacket.ReadWriteMapData_Flash.map_index);
                }
                break;

                case en__mapping_erase_SlotData_manufacture:
                {
                    reset_NVM_Selected_ISD_allData(mappingCommandStartFlag, en__mapping_erase_SlotData_manufacture, mappingPacket.ReadWriteMapData_Flash.slot_index, flash_Command_Erase);
                }
                break;

                case en__mapping_erase_mapData_STIMUL_PARA:
                {
                    reset_NVM_MapData(mappingCommandStartFlag, en__mapping_erase_mapData_STIMUL_PARA, mappingPacket.ReadWriteMapData_Flash.slot_index, mappingPacket.ReadWriteMapData_Flash.map_index, flash_Command_Erase);
                }
                break;

                case en__mapping_recover_mppingData_exceptSlot_1:
                {
                    reset_NVM_2to4_ISD_allData(mappingCommandStartFlag, en__mapping_recover_mppingData_exceptSlot_1, flash_Command_Recover);
                }
                break;

                case en__mapping_recover_ALL_SlotData_ManufactureData:
                {
                    result = reset_NVM_All_ISD_allData(mappingCommandStartFlag, en__mapping_recover_ALL_SlotData_ManufactureData, flash_Command_Recover);

                    if (result)
                    {
                        while (1)
                        {
                            if (isSpiTxBuffEmpty())
                            {
                                if (en__mapping_recover_ALL_SlotData_ManufactureData > 0x60)
                                {
                                    clear_mappingCommand();
                                }
                                else
                                {
                                    clearRemoteColtrolCommand();
                                }

                                changeSystemModeFlag(en__systemReset);

                                // NOTE: 강제 리셋

                                for (int i = 0; i < 100; i++)
                                {
                                    SYS_WATCHDOG_REFRESH();
                                    Sys_Delay((SystemCoreClock / 1000));
                                }

                                SYS_WATCHDOG_RESET();
                            }
                        }
                    }
                }
                break;

                case en__mapping_waiting_for_BleOff:
                {
                    delayCounter++;

                    if (delayCounter == 150)  // 수신명령 응답 돤료 이후에 NRF끔
                    {
                        // update_isd_Link_is_Disconnected();
                        isdControlCommand = en__isdStatus_PowerIC_Reset;
                    }

                    if (delayCounter > 150)
                    {
                        // 내부기가 연결된 상태에서 BLE를 다시 켜야 광고이름이 제대로 나올 수 있음.
                        if (ISD_state.isd_controlState < en__isdStatus_ISD_pathOpen_Ok)
                        {
                            BLE_Off_mapping = true;
                        }
                        else
                        {
                            clear_mappingCommand();
                            delayCounter = 0;
                        }
                    }
                }
                break;

#ifndef RELEASE
                case en__mapping_testStimulation:
                {
                    connectionCheckCounter = df_connectionCheckPeriod_ms;  // 카운터를 df_connectionCheckPeriod_ms로
                                                                           // 리셋하여 연결확인 진행하지 않게 한다.

                    if (ISD_state.conneded_ISD)
                    {
                        testStimulation(mappingCommandStartFlag);
                    }
                    else
                    {
                        sendErrorToApp(en__mapping_testStimulation, en__EN__ISD_ERROR, en__ISD_notConnected, __LINE__);
                        clear_mappingCommand();
                    }
                }
                break;
#endif
                case en__mapping_read_Connected_ISD_id:
                {
                    // 송신 데이터 준비
                    // command loop-back
                    bufferForSPI_tx[buffer_tx_index++] = mappingPacket.command;

                    // pay-load 준비
                    value                              = read_Connected_ISD_id();
                    bufferForSPI_tx[buffer_tx_index++] = value >> 24;
                    bufferForSPI_tx[buffer_tx_index++] = 0xff & (value >> 16);
                    bufferForSPI_tx[buffer_tx_index++] = 0xff & (value >> 8);
                    bufferForSPI_tx[buffer_tx_index++] = 0xff & (value);

                    // 송신 데이터 SPI TX버퍼에 복사
                    writeDataToSpiTxBuff(bufferForSPI_tx, buffer_tx_index);

                    //  명령 종료
                    clear_mappingCommand();
                }
                break;

                case en__mapping_IDLE:
                default:
                {
                    if (ISD_state.isd_controlState >= en__isdStatus_stimul_10V_Ok)  // 내부기 초기화가 완로되어야 연결 확인이 가능하다.
                    {
#if 0
                        if (connectionCheckCounter == 0)
                        {
                            TDC_PRINTF_V("\r\n[MAPPING] IDLE, LINK CHECK \r\n");
                        }
#endif
                        update_isd_LinkConnection_byBacktel_withMapping(connectionCheckCounter);  // 체크가 완료되면 flag가 FLASE로 변경
                    }
                }
                break;
            }

            connectionCheckCounter--;

            if (connectionCheckCounter < 0)
            {
                connectionCheckCounter = df_connectionCheckPeriod_ms;
            }
        }
        else
        {
            if (mappingPacket.command > en__mapping_connect)
            {
                sendErrorToApp(mappingPacket.command, en__EN__BLE_PROTOCOL_ERROR, en__Command_Order, __LINE__);
                clear_mappingCommand();
            }
        }
    }

    mappingStatus.StimulationIndicatorTrigger = sitmulationIndicatorTrigger;
    mappingStatus.BLE_Off                     = BLE_Off_mapping;
    mappingStatus.mappingConnection           = mappingProgramConnected;
    mappingStatus.isdControlCommand           = isdControlCommand;

    return mappingStatus;
}

void updateMappingProgramConnection(bool connection)
{
    if (connection)
    {
        mappingPacket.fetched_command = en__mapping_connect;
    }
    else
    {
        mappingPacket.fetched_command = en__mapping_disconnect;
    }
}

#if 0




void importMappingConnectForDebug(void)
{
    //updateMappingProgramConnection(true);
    mappingPacket.command=en__mapping_IDLE;
}


void importMappingDisconnectForDebug(void)

{
    clear_mappingCommand();

    //updateMappingProgramConnection(false);


    //update_isd_Link_is_Disconnected();
}


void importImpedanceDataForDebug(void)
{


    mappingPacket.impedanceCheck.iterationNum=2;
    mappingPacket.impedanceCheck.pulseWidth_start_usec=80;
    mappingPacket.impedanceCheck.pulseWidth_end_usec=180;
    mappingPacket.impedanceCheck.stimulationLevel_uA=100;           // 현재 프로그램된 최대값은 1020

    mappingPacket.command=en__mapping_impedanceChekck;

}


void import_eCapDataForDebug(void)
{

    mappingPacket.eCapMeasurement.firstPulsePhase=negativePulseFirst;
    mappingPacket.eCapMeasurement.stimulatonMode=en__monopolr_body; //en__monopolr_body;    // 자극모드  en__bipolar

    mappingPacket.eCapMeasurement.stimulationElectrodeNum=4; //자극 전극 번호
    mappingPacket.eCapMeasurement.measurementElectrodeNum=2;

    mappingPacket.eCapMeasurement.bipolarReferenceElectrodeNum=4; // 바이폴라 자극 모드일때 기준전극 번호

    mappingPacket.eCapMeasurement.stimulationLevel_uA_masker=50;
    mappingPacket.eCapMeasurement.stimulationLevel_uA_probe=45;

    mappingPacket.eCapMeasurement.pulseWidth=FPGA_pulsePhaseWidth_minimum+0;//FPGA_pulsePhaseWidth_minimum~FPGA_pulsePhaseWidth_minimum+255
    mappingPacket.eCapMeasurement.maskerProbeInterval_numFrame=21;          // 4~255 //자극 펄스(펄스폭고려) 2개와 인터벌의 합이 2msec의 시간을 넘지 않아야 한다.
    mappingPacket.eCapMeasurement.iterationNum=1;
    mappingPacket.eCapMeasurement.adcPreampGain=7;
    mappingPacket.eCapMeasurement.adcSamplingFreq=1;    // 0 :40, 1:20, 2 :10 3: 5kHz.....
    mappingPacket.eCapMeasurement.adcMeasurementDelay=1;    // 단위는 약 30usec
    mappingPacket.eCapMeasurement.measurementSampleNum=32;






    mappingPacket.command=en__mapping_eCAP_Measurement_masking;

}


void import_SpecificStimulationDebug(void)
{

    mappingPacket.specificStimulation.usableElectrodeNum=10;                        //1~32
    mappingPacket.specificStimulation.firstPulsePhase=negativePulseFirst;
    mappingPacket.specificStimulation.stimulatonMode=en__monopolr_body;                 //en__monopolr_body;    // 자극모드  en__bipolar

    mappingPacket.specificStimulation.stimulationElectrodeNum=4;                    //자극 전극 번호


    mappingPacket.specificStimulation.bipolarReferenceElectrodeNum=2;               // 바이폴라 자극 모드일때 기준전극 번호

    mappingPacket.specificStimulation.stimulationLevel_uA=50;                       //

    mappingPacket.specificStimulation.pulseWidth=FPGA_pulsePhaseWidth_minimum+200;  //FPGA_pulsePhaseWidth_minimum~FPGA_pulsePhaseWidth_minimum+255
    mappingPacket.specificStimulation.stimulationTime_100msec=10;                   // 자극 출력 유지 시간.


    mappingPacket.command=en__mapping_specific_stimulation;                         //



}


void import_original_ISD(void)
{
    char tempData_A[]={/*0x68,0x01,*/0x17,0x11,0x00,0x4a,0x01,0x6b,0x6a,0x73,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    char tempData_B[]={/*0x68,0x02,*/0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

    int *p_RepositoryFor_ISD_info;
    int *p_RepositoryFor_userSetting;

    int tempValue;
    int i, index;

// ISD info
                            p_RepositoryFor_ISD_info= getPointerRepositoryForReadWriteMapData_isd_info();



                            for(i=0; i<18; i++)
                            {
                                switch(i)
                                {
                                    case 0 :
                                    case 1 :
                                        p_RepositoryFor_ISD_info[i]=tempData_A[index++]; // 내부기 제조번호 (년, 월_모델번호)
                                    break;
                                    case 2 : // 시리얼 번호 상위 8bit
                                        tempValue=tempData_A[index++]; // 시리얼 번호 상위 8bit
                                        tempValue=tempValue<<8;

                                    break;
                                    case 3 : // 시리얼 번호 하위 8bit
                                        p_RepositoryFor_ISD_info[i-1]=tempValue|tempData_A[index++]; // 시리얼 번호 상위 8bit
                                        // 시리얼 번호가 16bit가 전달되면서 인덱스 감소 [i-1]

                                    break;

                                    default :
                                        p_RepositoryFor_ISD_info[i-1]=tempData_A[index++];
                                        // 시리얼 번호가 16bit가 전달되면서 인덱스 감소 [i-1]
                                    break;
                                }
                            }



                            for(i=18; i<30; i++)
                                p_RepositoryFor_ISD_info[i-1]=tempData_B[index++];  //  사용자 이름 나머지
//                                                              // 시리얼 번호가 16bit가 전달되면서 인덱스 감소 [i-1]


                            // 나머지 데이터는 초기값으로 설정한다.
                                // 패스키
                                p_RepositoryFor_ISD_info[(i++)-1]=1;
                                p_RepositoryFor_ISD_info[(i++)-1]=1;
                                p_RepositoryFor_ISD_info[(i++)-1]=1;
                                p_RepositoryFor_ISD_info[(i++)-1]=1;

// user setting

                                p_RepositoryFor_userSetting=getPointerRepositoryForReadWriteMapData_userSetting();

                                i=0;
                                // 맵번호
                                p_RepositoryFor_userSetting[(i++)]=1;       //1
                                // 자극볼륨
                                p_RepositoryFor_userSetting[(i++)]=4;           //2
                                // 마이크감도
                                p_RepositoryFor_userSetting[(i++)]=1;           //3
                                // LED
                                p_RepositoryFor_userSetting[(i++)]=1;           //4
                                // 자극알림
                                p_RepositoryFor_userSetting[(i++)]=1;           //5
                                // 텔레코일
                                p_RepositoryFor_userSetting[(i++)]=2;           //6

                                //블루투스
                                p_RepositoryFor_userSetting[(i++)]=1;

                                // 맵 스템프

// map stamp
                                p_RepositoryFor_userSetting[(i++)]=0;
                                p_RepositoryFor_userSetting[(i++)]=0;
                                p_RepositoryFor_userSetting[(i++)]=0;
                                p_RepositoryFor_userSetting[(i++)]=0;
                                p_RepositoryFor_userSetting[(i++)]=0;
                                p_RepositoryFor_userSetting[(i++)]=0;

                                mappingPacket.command=en__mapping_write_original_ISD_N_USER;




}


void import_liveAllParaDebug(void)
{

    int i;


    mappingPacket.liveStimulation.subCommand=en__allParameter;  //1~32 // 시작, 중지, 자극 볼륨 조절, 마이크 감도 조절, 알림용 자극  설정
    mappingPacket.liveStimulation.stimulVolume=4;   //1~10
    mappingPacket.liveStimulation.audioVolume=1;    // 1~4
    mappingPacket.liveStimulation.stimulationIndicatorChannelNum=19;        // 1~32 주파수 밴스 인덱스로..받을  것
    mappingPacket.liveStimulation.stimulationIndicatorAmplitude_uA=1500;    // 0~1800


    mappingPacket.liveStimulation.stimulationStrategy=1;
    mappingPacket.liveStimulation.firstPulsePhase=negativePulseFirst;
    mappingPacket.liveStimulation.stimulationMode=en__monopolr_rod;
    mappingPacket.liveStimulation.stimulationPulsePhaseWidth=FPGA_pulsePhaseWidth_minimum;
    mappingPacket.liveStimulation.numFrequencyBand=32;

    for(i=0; i<32; i++)
        mappingPacket.liveStimulation.usableStimulationElectrodIndex[i]=i+1; //1~32

    for(i=0; i<31; i++)
        mappingPacket.liveStimulation.usableReferenceElectrodIndex[i]=i+2; //1~31

    mappingPacket.liveStimulation.usableReferenceElectrodIndex[i]=99; //32

    for(i=0; i<32; i++)
        mappingPacket.liveStimulation.CIS_FreqBandOrder[i]=i+1; //1~32

    for(i=0; i<32; i++)
        mappingPacket.liveStimulation.T_level_uA[i]=900;//0~1800
    for(i=0; i<32; i++)
        mappingPacket.liveStimulation.C_level_uA[i]=1500;

    for(i=0; i<32; i++)
        mappingPacket.liveStimulation.audio_input_x_mim[i]=df_minAudioForLogarithm;
    for(i=0; i<32; i++)
        mappingPacket.liveStimulation.audio_input_x_max[i]=df_maxAudioForLogarithm;





    mappingPacket.fetched_command=en__mapping_live_stimulation;

}


void import_liveStart(void)
{

    int i;


    mappingPacket.liveStimulation.subCommand=en__Start;


    mappingPacket.fetched_command=en__mapping_live_stimulation;

}






void import_liveStimulationVolumeAdjust(int volume)
{
    mappingPacket.liveStimulation.stimulVolume=volume;


    mappingPacket.liveStimulation.subCommand=en__StimulationVolumeAdjust;
    mappingPacket.fetched_command=en__mapping_live_stimulation;



}
void import_liveMicSensitivityAdjust(int volume)
{


    mappingPacket.liveStimulation.audioVolume=volume;

    mappingPacket.liveStimulation.subCommand=en__MicSensitivityAdjust;
    mappingPacket.fetched_command=en__mapping_live_stimulation;

}
void import_liveMapping_Stimul_indicator(int ch_index, int amplitude_uA)
{

    mappingPacket.liveStimulation.stimulationIndicatorChannelNum=ch_index;
    mappingPacket.liveStimulation.stimulationIndicatorAmplitude_uA=amplitude_uA;

    mappingPacket.liveStimulation.subCommand=en__mapping_Stimul_indicator;
    mappingPacket.fetched_command=en__mapping_live_stimulation;

}
void import_liveReadEqualizer(int start_index, int end_index)
{

    mappingPacket.liveStimulation.subCommand=en__readEqualizer;
    mappingPacket.fetched_command=en__mapping_live_stimulation;

    mappingPacket.liveStimulation.equlizer_ReadStart_index=start_index;
    mappingPacket.liveStimulation.equlizer_ReadEnd_index=end_index;

}
void import_liveReadDeviceStatus()
{

}

void import_liveStop()
{

    mappingPacket.liveStimulation.subCommand=en__Stop;
    mappingPacket.fetched_command=en__mapping_live_stimulation;

}



/*
90010201000D0602006405
9002200102030405060708090A0B0C0D0E0F10
90031112131415161718191A1B1C1D1E1F20
900402030405060708090A0B0C0D0E0F1011
900512131415161718191A1B1C1D1E1F20
*/
void import_testStimulation()
{
            int i, trashValue;


//
#if 0
            mappingPacket.testStimulation.stimulatonMode= en__monopolr_rod;
            trashValue=0; // 오프셋 필요없음.
            mappingPacket.testStimulation.firstPulsePhase=0 ;  // 0, 1
            mappingPacket.testStimulation.pulseWidth= 13;
            mappingPacket.testStimulation.stimulationDacSlope= 8;   // 2,4,6,8
            mappingPacket.testStimulation.stimulationDacOffsetReslution=2; //2,4;
            mappingPacket.testStimulation.stimulationDacOffset_255= 0;
            mappingPacket.testStimulation.stimulationLevel_255= 0xFF;
            mappingPacket.testStimulation.stimulationTime_100msec= 50;




            mappingPacket.testStimulation.usableElectrodeNum=32;
            for(i=0; i<df_MaxNumOfElectrode; i++)
                mappingPacket.testStimulation.stimulationElectrodeNum[i]=i+1;





            for(i=1; i<df_MaxNumOfElectrode; i++)
                mappingPacket.testStimulation.bipolarReferenceElectrodeNum[i-1]=i+1;



            mappingPacket.c=en__mapping_testStimulation;

#else


            mappingPacket.testStimulation.stimulatonMode= en__monopolr_rod;
            trashValue=0; // 오프셋 필요없음.
            mappingPacket.testStimulation.firstPulsePhase=0 ;  // 0, 1
            mappingPacket.testStimulation.pulseWidth= 0x0D;
            mappingPacket.testStimulation.stimulationDacSlope= 2;   // 2,4,6,8
            mappingPacket.testStimulation.stimulationDacOffsetReslution=2; //2,4;
            mappingPacket.testStimulation.stimulationDacOffset_255= 120;
            mappingPacket.testStimulation.stimulationLevel_255= 0;
            mappingPacket.testStimulation.stimulationTime_100msec= 50;




            mappingPacket.testStimulation.usableElectrodeNum=24;
            for(i=0; i<df_MaxNumOfElectrode; i++)
                mappingPacket.testStimulation.stimulationElectrodeNum[i]=i+1;





            for(i=1; i<df_MaxNumOfElectrode; i++)
                // mappingPacket.testStimulation.bipolarReferenceElectrodeNum[i-1]=i+1
                mappingPacket.testStimulation.bipolarReferenceElectrodeNum[i-1]=99;



            mappingPacket.command=en__mapping_testStimulation;


#endif


}



#endif




















