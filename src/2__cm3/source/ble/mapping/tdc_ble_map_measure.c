
#include <stdint.h>
#include <tdc_ble_map_measure.h>
#include <tdc_ble_mapping.h>
#include <tdc_sys_error.h>

// 패킷 인덱스 0 은 헤더(명령)이며 호출자가 이미 읽었다.
// 따라서 각 파싱 함수는 인덱스 1 부터 시작한다.
#define df_payloadStartIndex 1

void tdc_ble_map_measure_impedance_check(const uint8_t *Rx_dataPacket)
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    int index = df_payloadStartIndex;
    int value;

    p_mappingPacket->impedanceCheck.iterationNum          = Rx_dataPacket[index++];  // index 1 → 2
    p_mappingPacket->impedanceCheck.channel               = Rx_dataPacket[index++];  // index 2 → 3
    p_mappingPacket->impedanceCheck.pulseWidth_start_usec = Rx_dataPacket[index++];  // index 3 → 4
    p_mappingPacket->impedanceCheck.pulseWidth_end_usec   = Rx_dataPacket[index++];  // index 4 → 5

    value                                              = Rx_dataPacket[index++] << 8;     // 자극 uA단위 상위 바이트 // index 5 → 6
    p_mappingPacket->impedanceCheck.stimulationLevel_uA = value | Rx_dataPacket[index++];  // 자극 uA단위 하위 바이트 // index 6 → 7

    // 측정 반복 횟수 범위 검사
    if ((1 <= p_mappingPacket->impedanceCheck.iterationNum)        // 1 이상
        && (p_mappingPacket->impedanceCheck.iterationNum <= 255))  // 255 이하
    {
        // 측정 채널 선택 범위 검사
        if (((1 <= p_mappingPacket->impedanceCheck.channel)       // 1 이상
             && (p_mappingPacket->impedanceCheck.channel <= 32))  // 32 이하
            || (p_mappingPacket->impedanceCheck.channel == 255))  // 또는 255 (전채널)
        {
            // 좁은 펄스 위상 폭 usec (시작) 범위 검사
            if ((13 <= p_mappingPacket->impedanceCheck.pulseWidth_start_usec)       // 13 이상
                && (p_mappingPacket->impedanceCheck.pulseWidth_start_usec <= 255))  // 255 이하
            {
                // 넓은 펄스 위상 폭 (최종) 범위 검사
                if ((13 <= p_mappingPacket->impedanceCheck.pulseWidth_end_usec)       // 13 이상
                    && (p_mappingPacket->impedanceCheck.pulseWidth_end_usec <= 255))  // 255 이하
                {
                    // 측정용 자극 크기 uA 범위 검사
                    if ((1 <= p_mappingPacket->impedanceCheck.stimulationLevel_uA)         // 1 이상
                        && (p_mappingPacket->impedanceCheck.stimulationLevel_uA <= 1024))  // 1024 이하
                    {
                        p_mappingPacket->fetched_command = en__mapping_impedanceChekck;
                        return;
                    }
                }
            }
        }
    }

    // 데이터 범위를 벗어남
    tdc_sys_error_send_to_app(en__mapping_impedanceChekck, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
}

void tdc_ble_map_measure_ecap_masking(const uint8_t *Rx_dataPacket)
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    int index = df_payloadStartIndex;
    int value;

    p_mappingPacket->eCapMeasurement.iterationNum = Rx_dataPacket[index++];
    p_mappingPacket->eCapMeasurement.pulseWidth   = Rx_dataPacket[index++];

    p_mappingPacket->eCapMeasurement.firstPulsePhase              = Rx_dataPacket[index++];
    p_mappingPacket->eCapMeasurement.stimulatonMode               = Rx_dataPacket[index++];
    p_mappingPacket->eCapMeasurement.stimulationElectrodeNum      = Rx_dataPacket[index++];
    p_mappingPacket->eCapMeasurement.bipolarReferenceElectrodeNum = Rx_dataPacket[index++];
    p_mappingPacket->eCapMeasurement.measurementElectrodeNum      = Rx_dataPacket[index++];

    value                                                      = Rx_dataPacket[index++] << 8;
    value                                                      = value | Rx_dataPacket[index++];
    p_mappingPacket->eCapMeasurement.stimulationLevel_uA_masker = value;

    value                                                     = Rx_dataPacket[index++] << 8;
    value                                                     = value | Rx_dataPacket[index++];
    p_mappingPacket->eCapMeasurement.stimulationLevel_uA_probe = value;

    p_mappingPacket->eCapMeasurement.maskerProbeInterval_numFrame = Rx_dataPacket[index++];

    p_mappingPacket->eCapMeasurement.adcPreampGain        = Rx_dataPacket[index++];
    p_mappingPacket->eCapMeasurement.adcSamplingFreq      = Rx_dataPacket[index++];
    p_mappingPacket->eCapMeasurement.adcMeasurementDelay  = Rx_dataPacket[index++];
    p_mappingPacket->eCapMeasurement.measurementSampleNum = Rx_dataPacket[index++];

    p_mappingPacket->fetched_command = en__mapping_eCAP_Measurement_masking;
}

void tdc_ble_map_measure_ecap_alternative(const uint8_t *Rx_dataPacket)
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    int index = df_payloadStartIndex;
    int value;

    p_mappingPacket->eCapMeasurement.iterationNum = Rx_dataPacket[index++];
    p_mappingPacket->eCapMeasurement.pulseWidth   = Rx_dataPacket[index++];

    p_mappingPacket->eCapMeasurement.stimulatonMode               = Rx_dataPacket[index++];
    p_mappingPacket->eCapMeasurement.stimulationElectrodeNum      = Rx_dataPacket[index++];
    p_mappingPacket->eCapMeasurement.bipolarReferenceElectrodeNum = Rx_dataPacket[index++];
    p_mappingPacket->eCapMeasurement.measurementElectrodeNum      = Rx_dataPacket[index++];

    value                                                      = Rx_dataPacket[index++] << 8;
    value                                                      = value | Rx_dataPacket[index++];
    p_mappingPacket->eCapMeasurement.stimulationLevel_uA_masker = value;

    p_mappingPacket->fetched_command = en__mapping_eCAP_Measurement_alternative;
}
