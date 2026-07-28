
#include <hw.h>
#include <stdbool.h>
#include <tdc_sys_error.h>
#include <tdc_ble_mapping.h>
#include <board.h>
#include <internalStimulationChip.h>
#include <tdc_shm.h>
#include <tdc_ble_mapping.h>
#include <tdc_hal_spi.h>
#include <tdc_stim_definitions.h>
#include <tdc_isd_map_impedance.h>
#include <tdc_isd_map_ecap.h>
#include <tdc_isd_map_specific_stim.h>
#include <tdc_isd_map_live.h>
#include <tdc_isd.h>
#include <tdc_isd_map_data.h>
#include <tdc_isd_map_live.h>
#include <tdc_isd_init.h>
#include <FPGA.h>
#include <tdc_sys_control.h>
#include <tdc_ble_remote.h>
#include <tdc_ble_map_measure.h>
#include <tdc_ble_map_flash.h>

static ST__MAPPING_PACKET mappingPacket;

// 데이터 인덱스 연속성 검사 카운터.
// 원래 fetch_packet 의 함수 지역 static 이었으나, 명령별 파싱 파일이 분리되면서
// 파일 경계를 넘게 되어 파일 스코프로 올리고 접근자로 노출한다.
static int prev_subCommandData_Num_index = 0;

int tdc_ble_mapping_get_seq_index(void)
{
    return prev_subCommandData_Num_index;
}

void tdc_ble_mapping_set_seq_index(int seqIndex)
{
    prev_subCommandData_Num_index = seqIndex;
}

void tdc_ble_mapping_clear_command()
{
    mappingPacket.command                    = en__mapping_IDLE;
    mappingPacket.tdc_isd_map_live_step.subCommand = en__Standby;

    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);
}

void tdc_ble_mapping_change_command_ble_disconnected(void)
{
    mappingPacket.command = en__mapping_ble_disconneted;
}

void tdc_ble_mapping_change_command_waiting_ble_off(void)
{
    mappingPacket.command = en__mapping_waiting_for_BleOff;
}

// const ST__MAPPING_PACKET *tdc_ble_mapping_get_packet(void)
ST__MAPPING_PACKET *tdc_ble_mapping_get_packet(void)
{
    return &mappingPacket;
}

void tdc_ble_mapping_fetch_packet(const uint8_t *Rx_dataPacket)  // spi 통신에서 호출 됨
{
    int        i, k, index;
    int        tempCommand;
    int        value, tempA, tempB;
    int        liveSubCommand;
    int        subCommandData_Num_index;

    static int max_C_uA = 0, min_T_uA = 1800;

    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int buffer_tx_index;

    ST__CFX_CM3_SharedMemory_mapData *p_mapDataSharedMemory;
    int                              *p_RepositoryFor_ISD_info;

    int intFromByte;

    bool exceptionCase = false;
    int  tempValue;
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
            if ((mappingPacket.tdc_isd_map_live_step.subCommand == en__HoldOn) || (mappingPacket.tdc_isd_map_live_step.subCommand == en__Standby))
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
            tdc_sys_error_send_to_app(tempCommand, en__EN__BLE_PROTOCOL_ERROR, en__PreviouCommnadIsNotCompleted, __LINE__);
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
            tdc_ble_map_measure_impedance_check(Rx_dataPacket);
        }
        break;

        case en__mapping_eCAP_Measurement_masking:  // 헤더 0x063
        {
            tdc_ble_map_measure_ecap_masking(Rx_dataPacket);
        }
        break;

        case en__mapping_eCAP_Measurement_alternative:  // 헤더 0x64
        {
            tdc_ble_map_measure_ecap_alternative(Rx_dataPacket);
        }
        break;

        case en__mapping_specific_stimulation:  // 헤더 0x65
        {
            mappingPacket.tdc_isd_map_specific_stim_step.usableElectrodeNum           = Rx_dataPacket[index++];
            mappingPacket.tdc_isd_map_specific_stim_step.pulseWidth                   = Rx_dataPacket[index++];
            mappingPacket.tdc_isd_map_specific_stim_step.firstPulsePhase              = Rx_dataPacket[index++];
            mappingPacket.tdc_isd_map_specific_stim_step.stimulatonMode               = Rx_dataPacket[index++];
            mappingPacket.tdc_isd_map_specific_stim_step.stimulationElectrodeNum      = Rx_dataPacket[index++];
            mappingPacket.tdc_isd_map_specific_stim_step.bipolarReferenceElectrodeNum = Rx_dataPacket[index++];

            value                                                     = Rx_dataPacket[index++] << 8;
            value                                                     = value | Rx_dataPacket[index++];
            mappingPacket.tdc_isd_map_specific_stim_step.stimulationLevel_uA     = value;
            mappingPacket.tdc_isd_map_specific_stim_step.stimulationTime_100msec = Rx_dataPacket[index++];

            // 데이터 범위 검사
            dataRangeError = false;

            // 사용 가능한 전극 수
            if ((mappingPacket.tdc_isd_map_specific_stim_step.usableElectrodeNum < 1)       // 1 미만
                || (32 < mappingPacket.tdc_isd_map_specific_stim_step.usableElectrodeNum))  // 32 초과 시 에러
            {
                dataRangeError = true;
            }

            // 펄스 위상 폭
            if ((mappingPacket.tdc_isd_map_specific_stim_step.pulseWidth < 13)       // 13 미만
                || (255 < mappingPacket.tdc_isd_map_specific_stim_step.pulseWidth))  // 255 초과 시 에러
            {
                dataRangeError = true;
            }

            // 선행 펄스 위상
            if ((mappingPacket.tdc_isd_map_specific_stim_step.firstPulsePhase < 0)      // 0 미만
                || (1 < mappingPacket.tdc_isd_map_specific_stim_step.firstPulsePhase))  // 1 초과 시 에러
            {
                dataRangeError = true;
            }

            // 자극 모드
            if ((mappingPacket.tdc_isd_map_specific_stim_step.stimulatonMode < 1)      // 1 미만
                || (6 < mappingPacket.tdc_isd_map_specific_stim_step.stimulatonMode))  // 6 초과 시 에러
            {
                dataRangeError = true;
            }

            // 자극 전극 번호
            if ((mappingPacket.tdc_isd_map_specific_stim_step.stimulationElectrodeNum < 1)       // 1 미만
                || (32 < mappingPacket.tdc_isd_map_specific_stim_step.stimulationElectrodeNum))  // 32 초과 시 에러
            {
                dataRangeError = true;
            }

            // 바이폴라 모드일 때, 기준전극 번호
            if ((mappingPacket.tdc_isd_map_specific_stim_step.bipolarReferenceElectrodeNum < 1)       // 1 미만
                || (32 < mappingPacket.tdc_isd_map_specific_stim_step.bipolarReferenceElectrodeNum))  //  32 초과 시 에러
            {
                if (mappingPacket.tdc_isd_map_specific_stim_step.bipolarReferenceElectrodeNum != 99)  // 99인 경우 예외 (for 모노폴라)
                {
                    dataRangeError = true;
                }
            }

            // 자극 크기 uA
            if ((mappingPacket.tdc_isd_map_specific_stim_step.stimulationLevel_uA < 0 /*1*/)   // 0 미만
                || (1800 < mappingPacket.tdc_isd_map_specific_stim_step.stimulationLevel_uA))  // 1800 초과 시 에러
            {
                dataRangeError = true;
            }

            // 자극 유지 시간 100msec
            if ((mappingPacket.tdc_isd_map_specific_stim_step.stimulationTime_100msec < 1)        // 1 미만
                || (255 < mappingPacket.tdc_isd_map_specific_stim_step.stimulationTime_100msec))  // 255 초과 시 에러
            {
                dataRangeError = true;
            }

            if (dataRangeError)
            {
                // 데이터 범위를 벗어남
                tdc_sys_error_send_to_app(en__mapping_specific_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
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
                tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
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
                    mappingPacket.tdc_isd_map_live_step.subCommand = en__Standby;

                    subCommandData_Num_index = Rx_dataPacket[index++];  // index++ : 2 → 3

                    if (subCommandData_Num_index == 1)
                    {
                        prev_subCommandData_Num_index = 0;
                    }

                    if (subCommandData_Num_index != prev_subCommandData_Num_index + 1)
                    {
                        // 데이터가 순차적으로 들어와야된다. 순차적으로 들어 오지 않으면 에러 전송
                        tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__DATA_Order, __LINE__);  // 데이터 범위 벗어남
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
                                mappingPacket.tdc_isd_map_live_step.stimulVolume                     = Rx_dataPacket[index++];          // index++ : 3 → 4
                                mappingPacket.tdc_isd_map_live_step.audioVolume                      = Rx_dataPacket[index++];          // index++ : 4 → 5
                                mappingPacket.tdc_isd_map_live_step.stimulationIndicatorChannelNum   = Rx_dataPacket[index++];          // index++ : 5 → 6
                                value                                                          = Rx_dataPacket[index++] << 8;     // index++ : 6 → 7
                                value                                                          = value | Rx_dataPacket[index++];  // index++ : 7 → 8
                                mappingPacket.tdc_isd_map_live_step.stimulationIndicatorAmplitude_uA = value;

                                mappingPacket.tdc_isd_map_live_step.stimulationStrategy        = Rx_dataPacket[index++];  // index++ : 8 → 9
                                mappingPacket.tdc_isd_map_live_step.stimulationMode            = Rx_dataPacket[index++];  // index++ : 9 → 10
                                mappingPacket.tdc_isd_map_live_step.firstPulsePhase            = Rx_dataPacket[index++];  // index++ : 10 → 11
                                mappingPacket.tdc_isd_map_live_step.stimulationPulsePhaseWidth = Rx_dataPacket[index++];  // index++ : 11 → 12
                                mappingPacket.tdc_isd_map_live_step.numFrequencyBand           = Rx_dataPacket[index++];  // index++ : 12 → 13

                                // 자극 볼륨 (mappingPacket.tdc_isd_map_live_step.stimulVolume : 1~4)
                                if ((mappingPacket.tdc_isd_map_live_step.stimulVolume < 1)      // 1 미만
                                    || (4 < mappingPacket.tdc_isd_map_live_step.stimulVolume))  // 4 초과 시 에러
                                {
                                    dataRangeError = true;
                                }

                                // 오디오 볼륨 (mappingPacket.tdc_isd_map_live_step.audioVolume : 1~10)
                                if ((mappingPacket.tdc_isd_map_live_step.audioVolume < 1)       // 1 미만
                                    || (10 < mappingPacket.tdc_isd_map_live_step.audioVolume))  // 10 초과 시 에러
                                {
                                    dataRangeError = true;
                                }

                                // 알림용 자극 채널 번호 (mappingPacket.tdc_isd_map_live_step.stimulationIndicatorChannelNum : 1~32)
                                if ((mappingPacket.tdc_isd_map_live_step.stimulationIndicatorChannelNum < 1)       // 1 미만
                                    || (32 < mappingPacket.tdc_isd_map_live_step.stimulationIndicatorChannelNum))  // 32 초과 시 에러
                                {
                                    dataRangeError = true;
                                }

                                // 알림용 자극 크기 uA (mappingPacket.tdc_isd_map_live_step.stimulationIndicatorAmplitude_uA : 1~1800)
                                if ((mappingPacket.tdc_isd_map_live_step.stimulationIndicatorAmplitude_uA < 0 /*1*/)   // 0 미만
                                    || (1800 < mappingPacket.tdc_isd_map_live_step.stimulationIndicatorAmplitude_uA))  // 1800 초과 시 에러
                                {
                                    dataRangeError = true;
                                }

                                // 자극 기법 (mappingPacket.tdc_isd_map_live_step.stimulationStrategy : 1~3)
                                if ((mappingPacket.tdc_isd_map_live_step.stimulationStrategy < 1)      // 1 미만
                                    || (3 < mappingPacket.tdc_isd_map_live_step.stimulationStrategy))  // 3 초과 시 에러
                                {
                                    dataRangeError = true;
                                }

                                // 자극 모드 (mappingPacket.tdc_isd_map_live_step.stimulationMode : 1~6)
                                if ((mappingPacket.tdc_isd_map_live_step.stimulationMode < 1)      // 1 미만
                                    || (6 < mappingPacket.tdc_isd_map_live_step.stimulationMode))  // 6 초과 시 에러
                                {
                                    dataRangeError = true;
                                }

                                // 선행 펄스 위상 (mappingPacket.tdc_isd_map_live_step.firstPulsePhase : 0~1)
                                if ((mappingPacket.tdc_isd_map_live_step.firstPulsePhase < 0)      // 0 미만
                                    || (1 < mappingPacket.tdc_isd_map_live_step.firstPulsePhase))  // 1 초과 시 에러
                                {
                                    dataRangeError = true;
                                }

                                // 펄스 위상 폭 (mappingPacket.tdc_isd_map_live_step.stimulationPulsePhaseWidth : 13~255)
                                if ((mappingPacket.tdc_isd_map_live_step.stimulationPulsePhaseWidth < 13)       // 13 미만
                                    || (255 < mappingPacket.tdc_isd_map_live_step.stimulationPulsePhaseWidth))  // 255 초과 시 에러
                                {
                                    dataRangeError = true;
                                }

                                // 주파수 밴드 (mappingPacket.tdc_isd_map_live_step.numFrequencyBand : 1~32)
                                if ((mappingPacket.tdc_isd_map_live_step.numFrequencyBand < 1)       // 1 미만
                                    || (32 < mappingPacket.tdc_isd_map_live_step.numFrequencyBand))  // 32 초과 시 에러
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
                                    mappingPacket.tdc_isd_map_live_step.usableStimulationElectrodIndex[i] = Rx_dataPacket[index++];

                                    if ((mappingPacket.tdc_isd_map_live_step.usableStimulationElectrodIndex[i] < 1)        // 1 미만
                                        || (100 < mappingPacket.tdc_isd_map_live_step.usableStimulationElectrodIndex[i]))  // 100 초과 시 에러
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
                                    mappingPacket.tdc_isd_map_live_step.usableStimulationElectrodIndex[i] = Rx_dataPacket[index++];

                                    if ((mappingPacket.tdc_isd_map_live_step.usableStimulationElectrodIndex[i] < 1)        // 1 미만
                                        || (100 < mappingPacket.tdc_isd_map_live_step.usableStimulationElectrodIndex[i]))  // 100 초과 시 에러
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
                                    mappingPacket.tdc_isd_map_live_step.usableReferenceElectrodIndex[i] = Rx_dataPacket[index++];

                                    if ((mappingPacket.tdc_isd_map_live_step.usableReferenceElectrodIndex[i] < 1)        // 1 미만
                                        || (100 < mappingPacket.tdc_isd_map_live_step.usableReferenceElectrodIndex[i]))  // 100 초과 시 에러
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
                                    mappingPacket.tdc_isd_map_live_step.usableReferenceElectrodIndex[i] = Rx_dataPacket[index++];

                                    if ((mappingPacket.tdc_isd_map_live_step.usableReferenceElectrodIndex[i] < 1)        // 1 미만
                                        || (100 < mappingPacket.tdc_isd_map_live_step.usableReferenceElectrodIndex[i]))  // 100 초과 시 에러
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
                                    mappingPacket.tdc_isd_map_live_step.CIS_FreqBandOrder[i] = Rx_dataPacket[index++];

                                    if ((mappingPacket.tdc_isd_map_live_step.CIS_FreqBandOrder[i] < 1)        // 1 미만
                                        || (100 < mappingPacket.tdc_isd_map_live_step.CIS_FreqBandOrder[i]))  // 100 초과 시 에러
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
                                    mappingPacket.tdc_isd_map_live_step.CIS_FreqBandOrder[i] = Rx_dataPacket[index++];

                                    if ((mappingPacket.tdc_isd_map_live_step.CIS_FreqBandOrder[i] < 1)        // 1 미만
                                        || (100 < mappingPacket.tdc_isd_map_live_step.CIS_FreqBandOrder[i]))  // 100 초과 시 에러
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
                                    mappingPacket.tdc_isd_map_live_step.T_level_uA[i] = value;

                                    if ((mappingPacket.tdc_isd_map_live_step.T_level_uA[i] < 0)         // 0 미만
                                        || (1800 < mappingPacket.tdc_isd_map_live_step.T_level_uA[i]))  // 1800 초과 시 에러
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
                                    mappingPacket.tdc_isd_map_live_step.T_level_uA[i] = value;

                                    if ((mappingPacket.tdc_isd_map_live_step.T_level_uA[i] < 0)         // 0 미만
                                        || (1800 < mappingPacket.tdc_isd_map_live_step.T_level_uA[i]))  // 1800 초과 시 에러
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
                                    mappingPacket.tdc_isd_map_live_step.T_level_uA[i] = value;

                                    if ((mappingPacket.tdc_isd_map_live_step.T_level_uA[i] < 0)         // 0 미만
                                        || (1800 < mappingPacket.tdc_isd_map_live_step.T_level_uA[i]))  // 1800 초과 시 에러
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
                                    mappingPacket.tdc_isd_map_live_step.T_level_uA[i] = value;

                                    if ((mappingPacket.tdc_isd_map_live_step.T_level_uA[i] < 0)         // 0 미만
                                        || (1800 < mappingPacket.tdc_isd_map_live_step.T_level_uA[i]))  // 1800 초과 시 에러
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
                                    mappingPacket.tdc_isd_map_live_step.C_level_uA[i] = value;

                                    if ((mappingPacket.tdc_isd_map_live_step.C_level_uA[i] < 0)         // 0 미만
                                        || (1800 < mappingPacket.tdc_isd_map_live_step.C_level_uA[i]))  // 1800 초과 시 에러
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
                                    mappingPacket.tdc_isd_map_live_step.C_level_uA[i] = value;

                                    if ((mappingPacket.tdc_isd_map_live_step.C_level_uA[i] < 0)         // 0 미만
                                        || (1800 < mappingPacket.tdc_isd_map_live_step.C_level_uA[i]))  // 1800 초과 시 에러
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
                                    mappingPacket.tdc_isd_map_live_step.C_level_uA[i] = value;

                                    if ((mappingPacket.tdc_isd_map_live_step.C_level_uA[i] < 0)         // 0 미만
                                        || (1800 < mappingPacket.tdc_isd_map_live_step.C_level_uA[i]))  // 1800 초과 시 에러
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
                                    mappingPacket.tdc_isd_map_live_step.C_level_uA[i] = value;

                                    if ((mappingPacket.tdc_isd_map_live_step.C_level_uA[i] < 0)         // 0 미만
                                        || (1800 < mappingPacket.tdc_isd_map_live_step.C_level_uA[i]))  // 1800 초과 시 에러
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
                            tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__DATA_Order, __LINE__);  // 데이터 범위 벗어남
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
                            tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

                            if (subCommandData_Num_index == Live_AllParameter_payloadNum)  // 모든 파라미터 수신 완료.
                            {
                                prev_subCommandData_Num_index = 0;

                                // 현재는 매핑프로그램에서 받지는 않지만, 펌웨어는 이 값을 사용하여 자극 범위를 계산하고
                                // 있다. 매핑 프로그램의 기능이 확장되면 이 값을 넣을 지도 고려..

                                // x- min
                                for (i = 0; i < 32; i++)
                                {
                                    mappingPacket.tdc_isd_map_live_step.audio_input_x_mim[i] = df_minAudioForLogarithm;
                                }

                                // x- max
                                for (i = 0; i < 32; i++)
                                {
                                    mappingPacket.tdc_isd_map_live_step.audio_input_x_max[i] = df_maxAudioForLogarithm;
                                }

                                mappingPacket.tdc_isd_map_live_step.subCommand = en__allParameter;

                                TDC_PRINTF_I("[LIVE] LIVE ALL PARAMETER PAYLOAD NUM : NOW, SUB COMMAND SET TO EN__ALL_PARAMETER \r\n");
                            }
                            else
                            {
                                mappingPacket.tdc_isd_map_live_step.subCommand = en__Standby;
                            }
                        }
                    }
                }
                break;

                case en__Start:  // 하위 명령 2 (실시간 자극 시작)
                {
                    mappingPacket.tdc_isd_map_live_step.subCommand = en__Start;
                }
                break;

                case en__StimulationVolumeAdjust:  // 하위 명령 3 (자극 볼륨 조절)
                {
                    if (mappingPacket.tdc_isd_map_live_step.subCommand == en__HoldOn)
                    {
                        value = Rx_dataPacket[index++];

                        if ((1 <= value)                                 // 1 이상
                            && (value <= df_maxStimulationVloumeLevel))  // 4 이하
                        {
                            mappingPacket.tdc_isd_map_live_step.stimulVolume = value;
                            mappingPacket.tdc_isd_map_live_step.subCommand   = en__StimulationVolumeAdjust;
                        }
                        else
                        {
                            // 에러 전송
                            tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
                            mappingPacket.tdc_isd_map_live_step.subCommand = en__Standby;
                        }
                    }
                    else
                    {
                        tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__Command_Order,
                                       __LINE__);  // 라이브 자극 중에만 컨트롤 되는 명령어

                        mappingPacket.tdc_isd_map_live_step.subCommand = en__Standby;
                    }
                }
                break;

                case en__MicSensitivityAdjust:  // 하위 명령 4 (마이크 감도 조절)
                {
                    if (mappingPacket.tdc_isd_map_live_step.subCommand == en__HoldOn)
                    {
                        value = Rx_dataPacket[index++];
                        if ((1 <= value)                         // 1 이상
                            && (value <= df_maxMicVloumeLevel))  // 10 이하
                        {
                            mappingPacket.tdc_isd_map_live_step.audioVolume = value;
                            mappingPacket.tdc_isd_map_live_step.subCommand  = en__MicSensitivityAdjust;
                        }
                        else
                        {
                            // 에러 전송
                            tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
                            mappingPacket.tdc_isd_map_live_step.subCommand = en__Standby;
                        }
                    }
                    else
                    {
                        tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__Command_Order, __LINE__);  // 라이브 자극 중에만 컨트롤 되는 명령어
                        mappingPacket.tdc_isd_map_live_step.subCommand = en__Standby;
                    }
                }
                break;

                case en__mapping_Stimul_indicator:  // 하위 명령 5 (알림 자극 출력)
                {
                    if (mappingPacket.tdc_isd_map_live_step.subCommand == en__HoldOn)
                    {
                        p_mapDataSharedMemory = tdc_shm_get_pointer_current_map_data();

                        value = Rx_dataPacket[index++];  // 알림용 자극 채널 번호

                        if ((value > p_mapDataSharedMemory->numFrequencyBand)  // 알림용 자극 채널 번호가 주파수 밴드 수 보다 크거나
                            || (value < 1))                                    // 1 미만이면 에러
                        {
                            // 에러 전송
                            tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
                            mappingPacket.tdc_isd_map_live_step.subCommand = en__Standby;
                        }
                        else
                        {
                            mappingPacket.tdc_isd_map_live_step.stimulationIndicatorChannelNum = value;

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
                                tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
                                mappingPacket.tdc_isd_map_live_step.subCommand = en__Standby;
                            }
                            else
                            {
                                mappingPacket.tdc_isd_map_live_step.stimulationIndicatorAmplitude_uA = value;
                                mappingPacket.tdc_isd_map_live_step.subCommand                       = en__mapping_Stimul_indicator;
                            }
                        }
                    }
                    else
                    {
                        tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__Command_Order, __LINE__);  // 라이브 자극 중에만 컨트롤 되는 명령어

                        mappingPacket.tdc_isd_map_live_step.subCommand = en__Standby;
                    }
                }
                break;

                case en__readEqualizer:  // 하위 명령 6 (자극 출력 값 읽기: 이퀄라이저)
                {
                    if (mappingPacket.tdc_isd_map_live_step.subCommand == en__HoldOn)
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
                            tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
                            mappingPacket.tdc_isd_map_live_step.subCommand = en__Standby;
                        }
                        else
                        {
                            if ((tempB - tempA) > 8)
                            {
                                // 에러 전송
                                tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
                                mappingPacket.tdc_isd_map_live_step.subCommand = en__Standby;
                            }
                            else
                            {
                                mappingPacket.tdc_isd_map_live_step.equlizer_ReadStart_index = tempA;
                                mappingPacket.tdc_isd_map_live_step.equlizer_ReadEnd_index   = tempB;

                                mappingPacket.tdc_isd_map_live_step.subCommand = en__readEqualizer;
                            }
                        }
                    }
                    else
                    {
                        tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__Command_Order,
                                       __LINE__);  // 라이브 자극 중에만 컨트롤 되는 명령어

                        mappingPacket.tdc_isd_map_live_step.subCommand = en__Standby;
                    }
                }
                break;

                case en__readDeviceStatus:  // 하위 명령 7 (장치 상태 읽기)
                {
                    mappingPacket.tdc_isd_map_live_step.subCommand = en__readDeviceStatus;
                }
                break;

                case en__Stop:  // 하위 명령 8 (실시간 자극 종료)
                {
                    mappingPacket.tdc_isd_map_live_step.subCommand = en__Stop;
                }
                break;

                default:
                {
                    mappingPacket.tdc_isd_map_live_step.subCommand = en__Standby;
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
            tdc_ble_map_flash_read_original_isd_user();
        }
        break;

        case en__mapping_write_original_ISD_N_USER:  // 헤더 0x68 (외부기 최초연결 사용자 이름 등록)
        {
            tdc_ble_map_flash_write_original_isd_user(Rx_dataPacket);
        }
        break;

        case en__mapping_read_SlotData_ISD_N_USER:  // 헤더 0x6A (맵 프로그램 관리: 읽기 - 내부기 ID 및 사용자)
        {
            tdc_ble_map_flash_read_slot_data(Rx_dataPacket);
        }
        break;

        case en__mapping_write_SlotData_ISD_N_USER:  // 헤더 0x6C (맵 프로그램 관리: 쓰기 - 내부기 ID 및 사용자)
        {
            tdc_ble_map_flash_write_slot_data(Rx_dataPacket);
        }
        break;

        case en__mapping_read_Mapdata_STIMUL_PARA:  // 헤더 0x6B (맵 프로그램 관리: 읽기 - 맵 데이터)
        {
            tdc_ble_map_flash_read_map_data(Rx_dataPacket);
        }
        break;

        case en__mapping_write_Mapdata_STIMUL_PARA:  // 헤더 0x6D (맵 프로그램 관리: 쓰기 - 맵 데이터)
        {
            tdc_ble_map_flash_write_map_data(Rx_dataPacket);
        }
        break;

        case en__mapping_erase_SlotData_manufacture:  // 헤더 0x6E (선택한 슬롯의 모든 맵데이터 삭제)
        {
            tdc_ble_map_flash_erase_slot(tempCommand, Rx_dataPacket);
        }
        break;

        case en__mapping_erase_mapData_STIMUL_PARA:  // 헤더 0x6F (선택한 맵 삭제)
        {
            tdc_ble_map_flash_erase_map(tempCommand, Rx_dataPacket);
        }
        break;

        case en__mapping_recover_mppingData_exceptSlot_1:  // 0x70
        {
            tdc_ble_map_flash_recover_except_slot1(tempCommand, Rx_dataPacket);
        }
        break;

        case en__mapping_recover_ALL_SlotData_ManufactureData:  // 0x71
        {
            tdc_ble_map_flash_recover_all(tempCommand);
        }
        break;

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
            tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);
        }
        break;
    }
}

ST__MAPPING_STATE tdc_ble_mapping_step(ST__ISD_STATUS ISD_state)
{
    static EN__MAPPING_COMMAND prev_mppingCommand      = en__mapping_IDLE;
    static int                 connectionCheckCounter  = df_connectionCheckPeriod_ms;
    static int                 delayCounter            = 0;
    static bool                mappingProgramConnected = false;

    ST__MAPPING_STATE     mappingStatus     = {en__isdStatus_NA, false, false, false};
    EN__ISD_CONTROL_STATE isdControlCommand = en__isdStatus_NA;
    tdc_sys_error_code_t        errorCode;

    uint8_t  bufferForSPI_tx[BLE_DataPacketSize];
    int  buffer_tx_index;
    int  i;
    int  value;

    bool mappingCommandStartFlag = false;
    bool result;
    bool sitmulationIndicatorTrigger = false;
    bool BLE_Off_mapping             = false;

    // 매핑 명령이 수신되 시접에 내부기 연결 확인용 backtel 전송 명령이 실행 중일 경우에는 백텔 수신이 완료되고 명령을 실행 할 수 있도록 한다.
    if (mappingPacket.fetched_command != en__mapping_IDLE)
    {
        if (!tdc_isd_is_connection_check_with_mapping())
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
        tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);      // 송신 데이터 SPI TX버퍼에 복사
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
                    tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);      // 송신 데이터 SPI TX버퍼에 복사

#if 1
                    while (1)
                    {
                        // 매핑 앱에서 (블루투스가 아닌, 매핑 기능) 연결을 종료하는 경우,
                        // SPI TX 버퍼가 모두 전송되고 난 뒤 시스템을 재부팅 시킨다. (with 워치독 리셋)
                        if (tdc_hal_spi_is_tx_buffer_empty())
                        {
                            tdc_ble_mapping_clear_command();
                            tdc_shm_change_system_mode_flag(en__systemReset);

                            for (int i = 0; i < 100; i++)
                            {
                                SYS_WATCHDOG_REFRESH();
                                Sys_Delay((SystemCoreClock / 1000));  // 1ms
                            }

                            SYS_WATCHDOG_RESET();  // NOTE: 강제 리셋
                        }
                    }
#else
                    tdc_ble_mapping_clear_command();
                    mappingProgramConnected = false;
                    isdControlCommand       = en__isdStatus_PowerIC_Reset;

                    // tdc_isd_update_link_disconnected();
#endif
                }
                break;

                case en__mapping_ble_disconneted:
                {
                    tdc_ble_mapping_clear_command();
                    mappingProgramConnected = false;
                    isdControlCommand       = en__isdStatus_PowerIC_Reset;

                    // tdc_isd_update_link_disconnected();
                }
                break;

                case en__mapping_impedanceChekck:
                {
                    // 카운터를 df_connectionCheckPeriod_ms로 리셋하여 연결확인 진행하지 않게 한다.
                    connectionCheckCounter = df_connectionCheckPeriod_ms;

                    if (ISD_state.conneded_ISD)
                    {
                        tdc_isd_map_impedance_step(mappingCommandStartFlag);
                    }
                    else
                    {
                        TDC_PRINTF_E("[MAPPING] CAN NOT CHECK IMPEDANCE, BECAUSE ISD NOT CONNECTED \r\n");
                        tdc_sys_error_send_to_app(en__mapping_impedanceChekck, en__EN__ISD_ERROR, en__ISD_notConnected, __LINE__);
                        tdc_ble_mapping_clear_command();
                    }
                }
                break;

                case en__mapping_eCAP_Measurement_masking:
                {
                    // 카운터를 df_connectionCheckPeriod_ms로 리셋하여 연결확인 진행하지 않게 한다.
                    connectionCheckCounter = df_connectionCheckPeriod_ms;
                    if (ISD_state.conneded_ISD)
                    {
                        tdc_isd_map_ecap_step(mappingCommandStartFlag);
                    }
                    else
                    {
                        tdc_sys_error_send_to_app(en__mapping_eCAP_Measurement_masking, en__EN__ISD_ERROR, en__ISD_notConnected, __LINE__);
                        tdc_ble_mapping_clear_command();
                    }
                }
                break;

                case en__mapping_eCAP_Measurement_alternative:
                {
                    // 카운터를 df_connectionCheckPeriod_ms로 리셋하여 연결확인 진행하지 않게 한다.
                    connectionCheckCounter = df_connectionCheckPeriod_ms;
                    // tdc_isd_map_ecap_step(mappingCommandStartFlag);
                }
                break;

                case en__mapping_specific_stimulation:
                {
                    // 카운터를 df_connectionCheckPeriod_ms로 리셋하여 연결확인 진행하지 않게 한다.
                    connectionCheckCounter = df_connectionCheckPeriod_ms;
                    if (ISD_state.conneded_ISD)
                    {
                        tdc_isd_map_specific_stim_step(mappingCommandStartFlag);
                    }
                    else
                    {
                        tdc_sys_error_send_to_app(en__mapping_specific_stimulation, en__EN__ISD_ERROR, en__ISD_notConnected, __LINE__);
                        tdc_ble_mapping_clear_command();
                    }
                }
                break;

                case en__mapping_live_stimulation:
                {
                    connectionCheckCounter      = df_connectionCheckPeriod_ms;
                    sitmulationIndicatorTrigger = tdc_isd_map_live_step(ISD_state);
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
                    bufferForSPI_tx[buffer_tx_index++] = tdc_pwr_battery_read_percentage();

                    // tdc_sys_error_code_t 전달
                    errorCode = tdc_sys_error_read();

                    bufferForSPI_tx[buffer_tx_index++] = (int) errorCode.ISD_ErrorFlag;

                    bufferForSPI_tx[buffer_tx_index++] = 0;

                    // NRF에 전달

                    // 송신 데이터 SPI TX버퍼에 복사
                    tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

                    tdc_ble_mapping_clear_command();
                }
                break;

                case en__mapping_read_original_ISD_N_USER:
                {
                    tdc_isd_map_read_original_info_setting(mappingCommandStartFlag, en__mapping_read_original_ISD_N_USER);
                }
                break;

                case en__mapping_write_original_ISD_N_USER:
                {
                    /* 플레쉬에 저장하면서 BLE 광고 이름을 바꾸기 위해서
                     * tdc_isd_change_state(en__isdStatus_ISD_Power_Ok);를 사용해 내부기 연결 해제를 유도하여
                     * BLE 연결 해제 후 광고이름 변경하여 진행하게 한다. */
                    tdc_isd_map_write_original_info_setting(mappingCommandStartFlag, en__mapping_write_original_ISD_N_USER);
                }
                break;

                case en__mapping_read_SlotData_ISD_N_USER:
                {
                    tdc_isd_map_read_info_setting(mappingCommandStartFlag, en__mapping_read_SlotData_ISD_N_USER, mappingPacket.ReadWriteMapData_Flash.slot_index);
                }
                break;

                case en__mapping_write_SlotData_ISD_N_USER:
                {
                    tdc_isd_map_write_info_setting(mappingCommandStartFlag, en__mapping_write_SlotData_ISD_N_USER, mappingPacket.ReadWriteMapData_Flash.slot_index);
                }
                break;

                case en__mapping_read_Mapdata_STIMUL_PARA:
                {
                    tdc_isd_map_read_stim_para(mappingCommandStartFlag, en__mapping_read_Mapdata_STIMUL_PARA, mappingPacket.ReadWriteMapData_Flash.slot_index, mappingPacket.ReadWriteMapData_Flash.map_index);
                }
                break;

                case en__mapping_write_Mapdata_STIMUL_PARA:
                {
                    tdc_isd_map_write_stim_para(mappingCommandStartFlag, en__mapping_write_Mapdata_STIMUL_PARA, mappingPacket.ReadWriteMapData_Flash.slot_index, mappingPacket.ReadWriteMapData_Flash.map_index);
                }
                break;

                case en__mapping_erase_SlotData_manufacture:
                {
                    tdc_isd_map_reset_nvm_selected(mappingCommandStartFlag, en__mapping_erase_SlotData_manufacture, mappingPacket.ReadWriteMapData_Flash.slot_index, flash_Command_Erase);
                }
                break;

                case en__mapping_erase_mapData_STIMUL_PARA:
                {
                    tdc_isd_map_reset_nvm_map_data(mappingCommandStartFlag, en__mapping_erase_mapData_STIMUL_PARA, mappingPacket.ReadWriteMapData_Flash.slot_index, mappingPacket.ReadWriteMapData_Flash.map_index, flash_Command_Erase);
                }
                break;

                case en__mapping_recover_mppingData_exceptSlot_1:
                {
                    tdc_isd_map_reset_nvm_2to4(mappingCommandStartFlag, en__mapping_recover_mppingData_exceptSlot_1, flash_Command_Recover);
                }
                break;

                case en__mapping_recover_ALL_SlotData_ManufactureData:
                {
                    result = tdc_isd_map_reset_nvm_all(mappingCommandStartFlag, en__mapping_recover_ALL_SlotData_ManufactureData, flash_Command_Recover);

                    if (result)
                    {
                        while (1)
                        {
                            if (tdc_hal_spi_is_tx_buffer_empty())
                            {
                                if (en__mapping_recover_ALL_SlotData_ManufactureData > 0x60)
                                {
                                    tdc_ble_mapping_clear_command();
                                }
                                else
                                {
                                    tdc_ble_remote_clear_command();
                                }

                                tdc_shm_change_system_mode_flag(en__systemReset);

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
                        // tdc_isd_update_link_disconnected();
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
                            tdc_ble_mapping_clear_command();
                            delayCounter = 0;
                        }
                    }
                }
                break;

                case en__mapping_read_Connected_ISD_id:
                {
                    // 송신 데이터 준비
                    // command loop-back
                    bufferForSPI_tx[buffer_tx_index++] = mappingPacket.command;

                    // pay-load 준비
                    value                              = tdc_isd_read_connected_id();
                    bufferForSPI_tx[buffer_tx_index++] = value >> 24;
                    bufferForSPI_tx[buffer_tx_index++] = 0xff & (value >> 16);
                    bufferForSPI_tx[buffer_tx_index++] = 0xff & (value >> 8);
                    bufferForSPI_tx[buffer_tx_index++] = 0xff & (value);

                    // 송신 데이터 SPI TX버퍼에 복사
                    tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

                    //  명령 종료
                    tdc_ble_mapping_clear_command();
                }
                break;

                case en__mapping_IDLE:
                default:
                {
                    if (ISD_state.isd_controlState >= en__isdStatus_stimul_10V_Ok)  // 내부기 초기화가 완로되어야 연결 확인이 가능하다.
                    {
                        /* 링크체크 진입 로그(connectionCheckCounter == 0 일 때 1회 출력)는
                         * 2차 리팩토링에서 제거했다(#if 0 사장). 단순 디버그 출력이었다. */
                        tdc_isd_update_link_by_backtel_mapping(connectionCheckCounter);  // 체크가 완료되면 flag가 FLASE로 변경
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
                tdc_sys_error_send_to_app(mappingPacket.command, en__EN__BLE_PROTOCOL_ERROR, en__Command_Order, __LINE__);
                tdc_ble_mapping_clear_command();
            }
        }
    }

    mappingStatus.StimulationIndicatorTrigger = sitmulationIndicatorTrigger;
    mappingStatus.BLE_Off                     = BLE_Off_mapping;
    mappingStatus.mappingConnection           = mappingProgramConnected;
    mappingStatus.isdControlCommand           = isdControlCommand;

    return mappingStatus;
}

/* import_*ForDebug / import_live* 계열 15개 제거(2026-07-22, G6).
 * mapping 프로토콜 수동 주입용 구 디버그 진입점. 정의는 #if 0 블록(396줄) 안에서
 * 죽어 있었고 헤더 선언 16개도 호출처가 0 이었다(import_livePause 는 선언만 있고
 * 정의조차 없는 고아였다). 상세: docs/tasks/cm3/20260720_cm3-full-refactor/게이트-노트/G6-ble-qcc.md */




















