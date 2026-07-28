
#include <hw.h>
#include <stdbool.h>
#include <stdint.h>
#include <tdc_ble_map_stim.h>
#include <tdc_ble_mapping.h>
#include <tdc_sys_error.h>
#include <board.h>
#include <internalStimulationChip.h>
#include <tdc_shm.h>
#include <tdc_hal_spi.h>
#include <tdc_stim_definitions.h>
#include <tdc_isd_map_live.h>
#include <tdc_isd_map_specific_stim.h>
#include <tdc_isd.h>
#include <tdc_isd_map_data.h>
#include <tdc_printf.h>

// 패킷 인덱스 0 은 헤더(명령)이며 호출자가 이미 읽었다.
// 따라서 각 파싱 함수는 인덱스 1 부터 시작한다.
#define df_payloadStartIndex 1

void tdc_ble_map_stim_specific(const uint8_t *Rx_dataPacket)  // 0x65
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    int  index = df_payloadStartIndex;
    int  value;
    bool dataRangeError = false;

    p_mappingPacket->tdc_isd_map_specific_stim_step.usableElectrodeNum           = Rx_dataPacket[index++];
    p_mappingPacket->tdc_isd_map_specific_stim_step.pulseWidth                   = Rx_dataPacket[index++];
    p_mappingPacket->tdc_isd_map_specific_stim_step.firstPulsePhase              = Rx_dataPacket[index++];
    p_mappingPacket->tdc_isd_map_specific_stim_step.stimulatonMode               = Rx_dataPacket[index++];
    p_mappingPacket->tdc_isd_map_specific_stim_step.stimulationElectrodeNum      = Rx_dataPacket[index++];
    p_mappingPacket->tdc_isd_map_specific_stim_step.bipolarReferenceElectrodeNum = Rx_dataPacket[index++];

    value                                                     = Rx_dataPacket[index++] << 8;
    value                                                     = value | Rx_dataPacket[index++];
    p_mappingPacket->tdc_isd_map_specific_stim_step.stimulationLevel_uA     = value;
    p_mappingPacket->tdc_isd_map_specific_stim_step.stimulationTime_100msec = Rx_dataPacket[index++];

    // 데이터 범위 검사
    dataRangeError = false;

    // 사용 가능한 전극 수
    if ((p_mappingPacket->tdc_isd_map_specific_stim_step.usableElectrodeNum < 1)       // 1 미만
        || (32 < p_mappingPacket->tdc_isd_map_specific_stim_step.usableElectrodeNum))  // 32 초과 시 에러
    {
        dataRangeError = true;
    }

    // 펄스 위상 폭
    if ((p_mappingPacket->tdc_isd_map_specific_stim_step.pulseWidth < 13)       // 13 미만
        || (255 < p_mappingPacket->tdc_isd_map_specific_stim_step.pulseWidth))  // 255 초과 시 에러
    {
        dataRangeError = true;
    }

    // 선행 펄스 위상
    if ((p_mappingPacket->tdc_isd_map_specific_stim_step.firstPulsePhase < 0)      // 0 미만
        || (1 < p_mappingPacket->tdc_isd_map_specific_stim_step.firstPulsePhase))  // 1 초과 시 에러
    {
        dataRangeError = true;
    }

    // 자극 모드
    if ((p_mappingPacket->tdc_isd_map_specific_stim_step.stimulatonMode < 1)      // 1 미만
        || (6 < p_mappingPacket->tdc_isd_map_specific_stim_step.stimulatonMode))  // 6 초과 시 에러
    {
        dataRangeError = true;
    }

    // 자극 전극 번호
    if ((p_mappingPacket->tdc_isd_map_specific_stim_step.stimulationElectrodeNum < 1)       // 1 미만
        || (32 < p_mappingPacket->tdc_isd_map_specific_stim_step.stimulationElectrodeNum))  // 32 초과 시 에러
    {
        dataRangeError = true;
    }

    // 바이폴라 모드일 때, 기준전극 번호
    if ((p_mappingPacket->tdc_isd_map_specific_stim_step.bipolarReferenceElectrodeNum < 1)       // 1 미만
        || (32 < p_mappingPacket->tdc_isd_map_specific_stim_step.bipolarReferenceElectrodeNum))  //  32 초과 시 에러
    {
        if (p_mappingPacket->tdc_isd_map_specific_stim_step.bipolarReferenceElectrodeNum != 99)  // 99인 경우 예외 (for 모노폴라)
        {
            dataRangeError = true;
        }
    }

    // 자극 크기 uA
    if ((p_mappingPacket->tdc_isd_map_specific_stim_step.stimulationLevel_uA < 0 /*1*/)   // 0 미만
        || (1800 < p_mappingPacket->tdc_isd_map_specific_stim_step.stimulationLevel_uA))  // 1800 초과 시 에러
    {
        dataRangeError = true;
    }

    // 자극 유지 시간 100msec
    if ((p_mappingPacket->tdc_isd_map_specific_stim_step.stimulationTime_100msec < 1)        // 1 미만
        || (255 < p_mappingPacket->tdc_isd_map_specific_stim_step.stimulationTime_100msec))  // 255 초과 시 에러
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
        p_mappingPacket->fetched_command = en__mapping_specific_stimulation;
    }
}

void tdc_ble_map_stim_live(const uint8_t *Rx_dataPacket)  // 0x66
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    int  index = df_payloadStartIndex;
    int  i;
    int  value, tempA, tempB;
    int  liveSubCommand;
    int  subCommandData_Num_index;
    bool dataRangeError = false;

    // 알림 자극(하위 명령 5)의 자극 크기 상·하한. 맵 데이터를 훑어 갱신하며
    // 호출 간에 값을 유지해야 하므로 static 이다(원본 fetch_packet 과 동일).
    static int max_C_uA = 0, min_T_uA = 1800;

    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int     buffer_tx_index = 0;

    ST__CFX_CM3_SharedMemory_mapData *p_mapDataSharedMemory;

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
        return;
    }
    else
    {
        p_mappingPacket->fetched_command = en__mapping_live_stimulation;
    }

    switch (liveSubCommand)  // 하위 명령 1~9
    {
        case en__allParameter:  // 하위 명령 1
        {
            p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;

            subCommandData_Num_index = Rx_dataPacket[index++];  // index++ : 2 → 3

            if (subCommandData_Num_index == 1)
            {
                tdc_ble_mapping_set_seq_index(0);
            }

            if (subCommandData_Num_index != tdc_ble_mapping_get_seq_index() + 1)
            {
                // 데이터가 순차적으로 들어와야된다. 순차적으로 들어 오지 않으면 에러 전송
                tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__DATA_Order, __LINE__);  // 데이터 범위 벗어남
                tdc_ble_mapping_set_seq_index(0);
            }
            else
            {
                tdc_ble_mapping_set_seq_index(subCommandData_Num_index);

                // 라이브 모드:0x66 -> 하위 명령 1: 전체 파라미터 전달 -> 데이터 인덱스 1~15에 대한 switch
                switch (subCommandData_Num_index)
                {
                    case 1:  // 헤더 0x66 실시간 자극 -> 하위 명령 1 -> 데이터 인덱스 1
                    {
                        p_mappingPacket->tdc_isd_map_live_step.stimulVolume                     = Rx_dataPacket[index++];          // index++ : 3 → 4
                        p_mappingPacket->tdc_isd_map_live_step.audioVolume                      = Rx_dataPacket[index++];          // index++ : 4 → 5
                        p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorChannelNum   = Rx_dataPacket[index++];          // index++ : 5 → 6
                        value                                                          = Rx_dataPacket[index++] << 8;     // index++ : 6 → 7
                        value                                                          = value | Rx_dataPacket[index++];  // index++ : 7 → 8
                        p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorAmplitude_uA = value;

                        p_mappingPacket->tdc_isd_map_live_step.stimulationStrategy        = Rx_dataPacket[index++];  // index++ : 8 → 9
                        p_mappingPacket->tdc_isd_map_live_step.stimulationMode            = Rx_dataPacket[index++];  // index++ : 9 → 10
                        p_mappingPacket->tdc_isd_map_live_step.firstPulsePhase            = Rx_dataPacket[index++];  // index++ : 10 → 11
                        p_mappingPacket->tdc_isd_map_live_step.stimulationPulsePhaseWidth = Rx_dataPacket[index++];  // index++ : 11 → 12
                        p_mappingPacket->tdc_isd_map_live_step.numFrequencyBand           = Rx_dataPacket[index++];  // index++ : 12 → 13

                        // 자극 볼륨 (p_mappingPacket->tdc_isd_map_live_step.stimulVolume : 1~4)
                        if ((p_mappingPacket->tdc_isd_map_live_step.stimulVolume < 1)      // 1 미만
                            || (4 < p_mappingPacket->tdc_isd_map_live_step.stimulVolume))  // 4 초과 시 에러
                        {
                            dataRangeError = true;
                        }

                        // 오디오 볼륨 (p_mappingPacket->tdc_isd_map_live_step.audioVolume : 1~10)
                        if ((p_mappingPacket->tdc_isd_map_live_step.audioVolume < 1)       // 1 미만
                            || (10 < p_mappingPacket->tdc_isd_map_live_step.audioVolume))  // 10 초과 시 에러
                        {
                            dataRangeError = true;
                        }

                        // 알림용 자극 채널 번호 (p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorChannelNum : 1~32)
                        if ((p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorChannelNum < 1)       // 1 미만
                            || (32 < p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorChannelNum))  // 32 초과 시 에러
                        {
                            dataRangeError = true;
                        }

                        // 알림용 자극 크기 uA (p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorAmplitude_uA : 1~1800)
                        if ((p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorAmplitude_uA < 0 /*1*/)   // 0 미만
                            || (1800 < p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorAmplitude_uA))  // 1800 초과 시 에러
                        {
                            dataRangeError = true;
                        }

                        // 자극 기법 (p_mappingPacket->tdc_isd_map_live_step.stimulationStrategy : 1~3)
                        if ((p_mappingPacket->tdc_isd_map_live_step.stimulationStrategy < 1)      // 1 미만
                            || (3 < p_mappingPacket->tdc_isd_map_live_step.stimulationStrategy))  // 3 초과 시 에러
                        {
                            dataRangeError = true;
                        }

                        // 자극 모드 (p_mappingPacket->tdc_isd_map_live_step.stimulationMode : 1~6)
                        if ((p_mappingPacket->tdc_isd_map_live_step.stimulationMode < 1)      // 1 미만
                            || (6 < p_mappingPacket->tdc_isd_map_live_step.stimulationMode))  // 6 초과 시 에러
                        {
                            dataRangeError = true;
                        }

                        // 선행 펄스 위상 (p_mappingPacket->tdc_isd_map_live_step.firstPulsePhase : 0~1)
                        if ((p_mappingPacket->tdc_isd_map_live_step.firstPulsePhase < 0)      // 0 미만
                            || (1 < p_mappingPacket->tdc_isd_map_live_step.firstPulsePhase))  // 1 초과 시 에러
                        {
                            dataRangeError = true;
                        }

                        // 펄스 위상 폭 (p_mappingPacket->tdc_isd_map_live_step.stimulationPulsePhaseWidth : 13~255)
                        if ((p_mappingPacket->tdc_isd_map_live_step.stimulationPulsePhaseWidth < 13)       // 13 미만
                            || (255 < p_mappingPacket->tdc_isd_map_live_step.stimulationPulsePhaseWidth))  // 255 초과 시 에러
                        {
                            dataRangeError = true;
                        }

                        // 주파수 밴드 (p_mappingPacket->tdc_isd_map_live_step.numFrequencyBand : 1~32)
                        if ((p_mappingPacket->tdc_isd_map_live_step.numFrequencyBand < 1)       // 1 미만
                            || (32 < p_mappingPacket->tdc_isd_map_live_step.numFrequencyBand))  // 32 초과 시 에러
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
                            p_mappingPacket->tdc_isd_map_live_step.usableStimulationElectrodIndex[i] = Rx_dataPacket[index++];

                            if ((p_mappingPacket->tdc_isd_map_live_step.usableStimulationElectrodIndex[i] < 1)        // 1 미만
                                || (100 < p_mappingPacket->tdc_isd_map_live_step.usableStimulationElectrodIndex[i]))  // 100 초과 시 에러
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
                            p_mappingPacket->tdc_isd_map_live_step.usableStimulationElectrodIndex[i] = Rx_dataPacket[index++];

                            if ((p_mappingPacket->tdc_isd_map_live_step.usableStimulationElectrodIndex[i] < 1)        // 1 미만
                                || (100 < p_mappingPacket->tdc_isd_map_live_step.usableStimulationElectrodIndex[i]))  // 100 초과 시 에러
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
                            p_mappingPacket->tdc_isd_map_live_step.usableReferenceElectrodIndex[i] = Rx_dataPacket[index++];

                            if ((p_mappingPacket->tdc_isd_map_live_step.usableReferenceElectrodIndex[i] < 1)        // 1 미만
                                || (100 < p_mappingPacket->tdc_isd_map_live_step.usableReferenceElectrodIndex[i]))  // 100 초과 시 에러
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
                            p_mappingPacket->tdc_isd_map_live_step.usableReferenceElectrodIndex[i] = Rx_dataPacket[index++];

                            if ((p_mappingPacket->tdc_isd_map_live_step.usableReferenceElectrodIndex[i] < 1)        // 1 미만
                                || (100 < p_mappingPacket->tdc_isd_map_live_step.usableReferenceElectrodIndex[i]))  // 100 초과 시 에러
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
                            p_mappingPacket->tdc_isd_map_live_step.CIS_FreqBandOrder[i] = Rx_dataPacket[index++];

                            if ((p_mappingPacket->tdc_isd_map_live_step.CIS_FreqBandOrder[i] < 1)        // 1 미만
                                || (100 < p_mappingPacket->tdc_isd_map_live_step.CIS_FreqBandOrder[i]))  // 100 초과 시 에러
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
                            p_mappingPacket->tdc_isd_map_live_step.CIS_FreqBandOrder[i] = Rx_dataPacket[index++];

                            if ((p_mappingPacket->tdc_isd_map_live_step.CIS_FreqBandOrder[i] < 1)        // 1 미만
                                || (100 < p_mappingPacket->tdc_isd_map_live_step.CIS_FreqBandOrder[i]))  // 100 초과 시 에러
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
                            p_mappingPacket->tdc_isd_map_live_step.T_level_uA[i] = value;

                            if ((p_mappingPacket->tdc_isd_map_live_step.T_level_uA[i] < 0)         // 0 미만
                                || (1800 < p_mappingPacket->tdc_isd_map_live_step.T_level_uA[i]))  // 1800 초과 시 에러
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
                            p_mappingPacket->tdc_isd_map_live_step.T_level_uA[i] = value;

                            if ((p_mappingPacket->tdc_isd_map_live_step.T_level_uA[i] < 0)         // 0 미만
                                || (1800 < p_mappingPacket->tdc_isd_map_live_step.T_level_uA[i]))  // 1800 초과 시 에러
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
                            p_mappingPacket->tdc_isd_map_live_step.T_level_uA[i] = value;

                            if ((p_mappingPacket->tdc_isd_map_live_step.T_level_uA[i] < 0)         // 0 미만
                                || (1800 < p_mappingPacket->tdc_isd_map_live_step.T_level_uA[i]))  // 1800 초과 시 에러
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
                            p_mappingPacket->tdc_isd_map_live_step.T_level_uA[i] = value;

                            if ((p_mappingPacket->tdc_isd_map_live_step.T_level_uA[i] < 0)         // 0 미만
                                || (1800 < p_mappingPacket->tdc_isd_map_live_step.T_level_uA[i]))  // 1800 초과 시 에러
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
                            p_mappingPacket->tdc_isd_map_live_step.C_level_uA[i] = value;

                            if ((p_mappingPacket->tdc_isd_map_live_step.C_level_uA[i] < 0)         // 0 미만
                                || (1800 < p_mappingPacket->tdc_isd_map_live_step.C_level_uA[i]))  // 1800 초과 시 에러
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
                            p_mappingPacket->tdc_isd_map_live_step.C_level_uA[i] = value;

                            if ((p_mappingPacket->tdc_isd_map_live_step.C_level_uA[i] < 0)         // 0 미만
                                || (1800 < p_mappingPacket->tdc_isd_map_live_step.C_level_uA[i]))  // 1800 초과 시 에러
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
                            p_mappingPacket->tdc_isd_map_live_step.C_level_uA[i] = value;

                            if ((p_mappingPacket->tdc_isd_map_live_step.C_level_uA[i] < 0)         // 0 미만
                                || (1800 < p_mappingPacket->tdc_isd_map_live_step.C_level_uA[i]))  // 1800 초과 시 에러
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
                            p_mappingPacket->tdc_isd_map_live_step.C_level_uA[i] = value;

                            if ((p_mappingPacket->tdc_isd_map_live_step.C_level_uA[i] < 0)         // 0 미만
                                || (1800 < p_mappingPacket->tdc_isd_map_live_step.C_level_uA[i]))  // 1800 초과 시 에러
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
                    tdc_ble_mapping_set_seq_index(0);
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
                        tdc_ble_mapping_set_seq_index(0);

                        // 현재는 매핑프로그램에서 받지는 않지만, 펌웨어는 이 값을 사용하여 자극 범위를 계산하고
                        // 있다. 매핑 프로그램의 기능이 확장되면 이 값을 넣을 지도 고려..

                        // x- min
                        for (i = 0; i < 32; i++)
                        {
                            p_mappingPacket->tdc_isd_map_live_step.audio_input_x_mim[i] = df_minAudioForLogarithm;
                        }

                        // x- max
                        for (i = 0; i < 32; i++)
                        {
                            p_mappingPacket->tdc_isd_map_live_step.audio_input_x_max[i] = df_maxAudioForLogarithm;
                        }

                        p_mappingPacket->tdc_isd_map_live_step.subCommand = en__allParameter;

                        TDC_PRINTF_I("[LIVE] LIVE ALL PARAMETER PAYLOAD NUM : NOW, SUB COMMAND SET TO EN__ALL_PARAMETER \r\n");
                    }
                    else
                    {
                        p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
                    }
                }
            }
        }
        break;

        case en__Start:  // 하위 명령 2 (실시간 자극 시작)
        {
            p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Start;
        }
        break;

        case en__StimulationVolumeAdjust:  // 하위 명령 3 (자극 볼륨 조절)
        {
            if (p_mappingPacket->tdc_isd_map_live_step.subCommand == en__HoldOn)
            {
                value = Rx_dataPacket[index++];

                if ((1 <= value)                                 // 1 이상
                    && (value <= df_maxStimulationVloumeLevel))  // 4 이하
                {
                    p_mappingPacket->tdc_isd_map_live_step.stimulVolume = value;
                    p_mappingPacket->tdc_isd_map_live_step.subCommand   = en__StimulationVolumeAdjust;
                }
                else
                {
                    // 에러 전송
                    tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
                    p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
                }
            }
            else
            {
                tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__Command_Order,
                               __LINE__);  // 라이브 자극 중에만 컨트롤 되는 명령어

                p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
            }
        }
        break;

        case en__MicSensitivityAdjust:  // 하위 명령 4 (마이크 감도 조절)
        {
            if (p_mappingPacket->tdc_isd_map_live_step.subCommand == en__HoldOn)
            {
                value = Rx_dataPacket[index++];
                if ((1 <= value)                         // 1 이상
                    && (value <= df_maxMicVloumeLevel))  // 10 이하
                {
                    p_mappingPacket->tdc_isd_map_live_step.audioVolume = value;
                    p_mappingPacket->tdc_isd_map_live_step.subCommand  = en__MicSensitivityAdjust;
                }
                else
                {
                    // 에러 전송
                    tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
                    p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
                }
            }
            else
            {
                tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__Command_Order, __LINE__);  // 라이브 자극 중에만 컨트롤 되는 명령어
                p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
            }
        }
        break;

        case en__mapping_Stimul_indicator:  // 하위 명령 5 (알림 자극 출력)
        {
            if (p_mappingPacket->tdc_isd_map_live_step.subCommand == en__HoldOn)
            {
                p_mapDataSharedMemory = tdc_shm_get_pointer_current_map_data();

                value = Rx_dataPacket[index++];  // 알림용 자극 채널 번호

                if ((value > p_mapDataSharedMemory->numFrequencyBand)  // 알림용 자극 채널 번호가 주파수 밴드 수 보다 크거나
                    || (value < 1))                                    // 1 미만이면 에러
                {
                    // 에러 전송
                    tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
                    p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
                }
                else
                {
                    p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorChannelNum = value;

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
                        p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
                    }
                    else
                    {
                        p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorAmplitude_uA = value;
                        p_mappingPacket->tdc_isd_map_live_step.subCommand                       = en__mapping_Stimul_indicator;
                    }
                }
            }
            else
            {
                tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__Command_Order, __LINE__);  // 라이브 자극 중에만 컨트롤 되는 명령어

                p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
            }
        }
        break;

        case en__readEqualizer:  // 하위 명령 6 (자극 출력 값 읽기: 이퀄라이저)
        {
            if (p_mappingPacket->tdc_isd_map_live_step.subCommand == en__HoldOn)
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
                    p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
                }
                else
                {
                    if ((tempB - tempA) > 8)
                    {
                        // 에러 전송
                        tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);  // 데이터 범위 벗어남
                        p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
                    }
                    else
                    {
                        p_mappingPacket->tdc_isd_map_live_step.equlizer_ReadStart_index = tempA;
                        p_mappingPacket->tdc_isd_map_live_step.equlizer_ReadEnd_index   = tempB;

                        p_mappingPacket->tdc_isd_map_live_step.subCommand = en__readEqualizer;
                    }
                }
            }
            else
            {
                tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__BLE_PROTOCOL_ERROR, en__Command_Order,
                               __LINE__);  // 라이브 자극 중에만 컨트롤 되는 명령어

                p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
            }
        }
        break;

        case en__readDeviceStatus:  // 하위 명령 7 (장치 상태 읽기)
        {
            p_mappingPacket->tdc_isd_map_live_step.subCommand = en__readDeviceStatus;
        }
        break;

        case en__Stop:  // 하위 명령 8 (실시간 자극 종료)
        {
            p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Stop;
        }
        break;

        default:
        {
            p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
        }
        break;
    }
}
