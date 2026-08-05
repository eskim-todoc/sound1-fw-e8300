
#include <stdbool.h>
#include <stdint.h>
#include <tdc_ble_map_measure.h>
#include <tdc_ble_mapping.h>
#include <tdc_sys_error.h>

// 패킷 인덱스 0 은 헤더(명령)이며 호출자가 이미 읽었다.
// 따라서 각 파싱 함수는 인덱스 1 부터 시작한다.
#define df_payloadStartIndex 1

void tdc_ble_cmd_0x62_impedance_check(const uint8_t *Rx_dataPacket)
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

void tdc_ble_cmd_0x63_ecap_masking(const uint8_t *Rx_dataPacket)
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    int  index = df_payloadStartIndex;
    int  value;
    bool dataRangeError = false;

    p_mappingPacket->eCapMeasurement.iterationNum = Rx_dataPacket[index++];
    p_mappingPacket->eCapMeasurement.pulseWidth   = Rx_dataPacket[index++];

    p_mappingPacket->eCapMeasurement.firstPulsePhase              = Rx_dataPacket[index++];
    p_mappingPacket->eCapMeasurement.stimulationMode               = Rx_dataPacket[index++];
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

    // 데이터 범위 검사.
    // 전극 번호는 isd/tdc_isd_map_ecap.c 에서 electrodeMap[32] 의 인덱스로 쓰이므로
    // 여기서 막지 않으면 배열 범위 밖을 읽는다 (board/electrodeMapping.c:3).
    // adc 계열은 레지스터 비트폭을 넘으면 인접 필드를 오염시킨다.

    // 측정 반복 횟수
    if ((p_mappingPacket->eCapMeasurement.iterationNum < 1)       // 1 미만
        || (255 < p_mappingPacket->eCapMeasurement.iterationNum))  // 255 초과 시 에러
    {
        dataRangeError = true;
    }

    // 펄스 위상 폭 (13 은 FPGA 최소 펄스폭. board/FPGA_ver2_7_0.h 의 FPGA_pulsePhaseWidth_minimum
    // 과 같은 값이며, 미만이면 tdc_isd_map_ecap.c 의 뺄셈이 언더플로한다)
    if ((p_mappingPacket->eCapMeasurement.pulseWidth < 13)       // 13 미만
        || (255 < p_mappingPacket->eCapMeasurement.pulseWidth))  // 255 초과 시 에러
    {
        dataRangeError = true;
    }

    // 선행 펄스 위상
    if ((p_mappingPacket->eCapMeasurement.firstPulsePhase < 0)      // 0 미만
        || (1 < p_mappingPacket->eCapMeasurement.firstPulsePhase))  // 1 초과 시 에러
    {
        dataRangeError = true;
    }

    // 자극 모드 (EN___STIMULATION_MODE 에서 en__referenceNA(0) 를 뺀 실제 모드 범위)
    if ((p_mappingPacket->eCapMeasurement.stimulationMode < 1)      // 1 미만
        || (6 < p_mappingPacket->eCapMeasurement.stimulationMode))  // 6 초과 시 에러
    {
        dataRangeError = true;
    }

    // 자극 전극 번호
    if ((p_mappingPacket->eCapMeasurement.stimulationElectrodeNum < 1)       // 1 미만
        || (32 < p_mappingPacket->eCapMeasurement.stimulationElectrodeNum))  // 32 초과 시 에러
    {
        dataRangeError = true;
    }

    // 바이폴라 모드일 때, 기준전극 번호
    if ((p_mappingPacket->eCapMeasurement.bipolarReferenceElectrodeNum < 1)       // 1 미만
        || (32 < p_mappingPacket->eCapMeasurement.bipolarReferenceElectrodeNum))  // 32 초과 시 에러
    {
        if (p_mappingPacket->eCapMeasurement.bipolarReferenceElectrodeNum != 99)  // 99인 경우 예외 (for 모노폴라)
        {
            dataRangeError = true;
        }
    }

    // 측정 전극 번호
    if ((p_mappingPacket->eCapMeasurement.measurementElectrodeNum < 1)       // 1 미만
        || (32 < p_mappingPacket->eCapMeasurement.measurementElectrodeNum))  // 32 초과 시 에러
    {
        dataRangeError = true;
    }

    // adc 샘플링 주파수 (레지스터에서 3비트만 차지한다)
    if ((p_mappingPacket->eCapMeasurement.adcSamplingFreq < 0)      // 0 미만
        || (7 < p_mappingPacket->eCapMeasurement.adcSamplingFreq))  // 7 초과 시 에러
    {
        dataRangeError = true;
    }

    // 프로브 출력 후 측정 딜레이 (레지스터에서 4비트만 차지한다)
    if ((p_mappingPacket->eCapMeasurement.adcMeasurementDelay < 0)       // 0 미만
        || (15 < p_mappingPacket->eCapMeasurement.adcMeasurementDelay))  // 15 초과 시 에러
    {
        dataRangeError = true;
    }

    if (dataRangeError)
    {
        // 데이터 범위를 벗어남
        tdc_sys_error_send_to_app(en__mapping_eCAP_Measurement_masking, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
    }
    else
    {
        p_mappingPacket->fetched_command = en__mapping_eCAP_Measurement_masking;
    }
}

void tdc_ble_cmd_0x64_ecap_alternative(const uint8_t *Rx_dataPacket)
{
    ST__MAPPING_PACKET *p_mappingPacket = tdc_ble_mapping_get_packet();

    int  index = df_payloadStartIndex;
    int  value;
    bool dataRangeError = false;

    p_mappingPacket->eCapMeasurement.iterationNum = Rx_dataPacket[index++];
    p_mappingPacket->eCapMeasurement.pulseWidth   = Rx_dataPacket[index++];

    p_mappingPacket->eCapMeasurement.stimulationMode               = Rx_dataPacket[index++];
    p_mappingPacket->eCapMeasurement.stimulationElectrodeNum      = Rx_dataPacket[index++];
    p_mappingPacket->eCapMeasurement.bipolarReferenceElectrodeNum = Rx_dataPacket[index++];
    p_mappingPacket->eCapMeasurement.measurementElectrodeNum      = Rx_dataPacket[index++];

    value                                                      = Rx_dataPacket[index++] << 8;
    value                                                      = value | Rx_dataPacket[index++];
    p_mappingPacket->eCapMeasurement.stimulationLevel_uA_masker = value;

    // 데이터 범위 검사.
    // 0x64 는 현재 실행부가 주석 처리돼 있으나(tdc_ble_mapping.c:403) 0x63 과 같은
    // eCapMeasurement 구조체를 쓰므로, 되살리는 순간 같은 범위 밖 읽기가 발생한다.
    // 검사는 0x64 가 실제로 파싱하는 필드에만 넣는다.

    // 측정 반복 횟수
    if ((p_mappingPacket->eCapMeasurement.iterationNum < 1)        // 1 미만
        || (255 < p_mappingPacket->eCapMeasurement.iterationNum))  // 255 초과 시 에러
    {
        dataRangeError = true;
    }

    // 펄스 위상 폭 (13 은 FPGA 최소 펄스폭)
    if ((p_mappingPacket->eCapMeasurement.pulseWidth < 13)       // 13 미만
        || (255 < p_mappingPacket->eCapMeasurement.pulseWidth))  // 255 초과 시 에러
    {
        dataRangeError = true;
    }

    // 자극 모드 (EN___STIMULATION_MODE 에서 en__referenceNA(0) 를 뺀 실제 모드 범위)
    if ((p_mappingPacket->eCapMeasurement.stimulationMode < 1)      // 1 미만
        || (6 < p_mappingPacket->eCapMeasurement.stimulationMode))  // 6 초과 시 에러
    {
        dataRangeError = true;
    }

    // 자극 전극 번호
    if ((p_mappingPacket->eCapMeasurement.stimulationElectrodeNum < 1)       // 1 미만
        || (32 < p_mappingPacket->eCapMeasurement.stimulationElectrodeNum))  // 32 초과 시 에러
    {
        dataRangeError = true;
    }

    // 바이폴라 모드일 때, 기준전극 번호
    if ((p_mappingPacket->eCapMeasurement.bipolarReferenceElectrodeNum < 1)       // 1 미만
        || (32 < p_mappingPacket->eCapMeasurement.bipolarReferenceElectrodeNum))  // 32 초과 시 에러
    {
        if (p_mappingPacket->eCapMeasurement.bipolarReferenceElectrodeNum != 99)  // 99인 경우 예외 (for 모노폴라)
        {
            dataRangeError = true;
        }
    }

    // 측정 전극 번호
    if ((p_mappingPacket->eCapMeasurement.measurementElectrodeNum < 1)       // 1 미만
        || (32 < p_mappingPacket->eCapMeasurement.measurementElectrodeNum))  // 32 초과 시 에러
    {
        dataRangeError = true;
    }

    if (dataRangeError)
    {
        // 데이터 범위를 벗어남
        tdc_sys_error_send_to_app(en__mapping_eCAP_Measurement_alternative, en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange, __LINE__);
    }
    else
    {
        p_mappingPacket->fetched_command = en__mapping_eCAP_Measurement_alternative;
    }
}
