
#include <hw.h>
#include <stdbool.h>
#include <stdint.h>
#include <tdc_ble_cmd_0x65_specific.h>
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
#include <tdc_ble_reply.h>

// 패킷 인덱스 0 은 헤더(명령)이며 호출자가 이미 읽었다.
// 따라서 각 파싱 함수는 인덱스 1 부터 시작한다.
#define df_payloadStartIndex 1

void tdc_ble_cmd_0x65_specific_stim(const uint8_t *Rx_dataPacket)  // 0x65
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

