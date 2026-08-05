// 매핑 프로토콜 0x62~0x64 (임피던스 · eCAP 측정) 파싱 테스트.
//
// 검증 기준은 코드에서 역추출한 범위 검사다. 이 테스트의 목적은
// 사양 준수 확인이 아니라 현재 동작을 고정하는 것(회귀 안전망)이다.
// 설계안 단계_3(패킷 디스크립터 테이블)에서 파싱을 테이블 주도로
// 바꿀 때 결과가 같은지 확인하는 데 쓴다.

#include <string.h>
#include <stdint.h>

#include <tdc_ble_mapping.h>
#include <tdc_sys_error.h>
#include <tdc_ble_map_measure.h>

#include "tdc_test.h"
#include "stub_ble.h"

// 패킷을 0 으로 채우고 헤더만 세운다.
// 인덱스 0 은 명령 헤더이며 호출자가 이미 읽은 것으로 간주된다.
static void make_packet(uint8_t *pkt, int command)
{
    stub_reset();
    memset(pkt, 0, BLE_DataPacketSize + 1);
    pkt[0] = (uint8_t) command;
}

// 0x62 임피던스: 유효한 기본 패킷을 만든다.
// iteration 10 · channel 5 · pw_start 20 · pw_end 30 · level 300
static void make_impedance_ok(uint8_t *pkt)
{
    make_packet(pkt, en__mapping_impedanceChekck);
    pkt[1] = 10;
    pkt[2] = 5;
    pkt[3] = 20;
    pkt[4] = 30;
    pkt[5] = 0x01;
    pkt[6] = 0x2C;  // 0x012C = 300
}

// 0x63 eCAP 마스킹: 유효한 기본 패킷을 만든다.
// 값은 각 필드의 유효 범위 안에서 서로 구분되게 골랐다.
static void make_ecap_masking_ok(uint8_t *pkt)
{
    make_packet(pkt, en__mapping_eCAP_Measurement_masking);
    pkt[1]  = 4;    // iterationNum                  (1~255)
    pkt[2]  = 25;   // pulseWidth                    (13~255)
    pkt[3]  = 1;    // firstPulsePhase               (0~1)
    pkt[4]  = 2;    // stimulatonMode                (1~6)
    pkt[5]  = 7;    // stimulationElectrodeNum       (1~32)
    pkt[6]  = 8;    // bipolarReferenceElectrodeNum  (1~32 또는 99)
    pkt[7]  = 9;    // measurementElectrodeNum       (1~32)
    pkt[8]  = 0x02; // masker 상위
    pkt[9]  = 0x58; // masker 하위 -> 600
    pkt[10] = 0x01; // probe 상위
    pkt[11] = 0xF4; // probe 하위 -> 500
    pkt[12] = 3;    // maskerProbeInterval_numFrame  (검사 없음 - isd 에서 검사)
    pkt[13] = 11;   // adcPreampGain                 (검사 없음 - 비트폭 미확정)
    pkt[14] = 5;    // adcSamplingFreq               (0~7)
    pkt[15] = 13;   // adcMeasurementDelay           (0~15)
    pkt[16] = 14;   // measurementSampleNum          (검사 없음)
}

// 0x64 eCAP 교대: 유효한 기본 패킷을 만든다.
// 0x63 과 달리 firstPulsePhase 가 없어 이후 필드가 한 칸씩 당겨진다.
static void make_ecap_alternative_ok(uint8_t *pkt)
{
    make_packet(pkt, en__mapping_eCAP_Measurement_alternative);
    pkt[1] = 6;    // iterationNum                  (1~255)
    pkt[2] = 26;   // pulseWidth                    (13~255)
    pkt[3] = 3;    // stimulatonMode                (1~6)
    pkt[4] = 15;   // stimulationElectrodeNum       (1~32)
    pkt[5] = 16;   // bipolarReferenceElectrodeNum  (1~32 또는 99)
    pkt[6] = 17;   // measurementElectrodeNum       (1~32)
    pkt[7] = 0x03; // masker 상위
    pkt[8] = 0xE8; // masker 하위 -> 1000
}

int main(void)
{
    uint8_t             pkt[BLE_DataPacketSize + 1];
    ST__MAPPING_PACKET *p;

    // ------------------------------------------------------------------
    TEST_GROUP("0x62 임피던스 - 정상");

    make_impedance_ok(pkt);
    tdc_ble_map_measure_impedance_check(pkt);
    p = tdc_ble_mapping_get_packet();

    CHECK_EQ("iterationNum", p->impedanceCheck.iterationNum, 10);
    CHECK_EQ("channel", p->impedanceCheck.channel, 5);
    CHECK_EQ("pulseWidth_start_usec", p->impedanceCheck.pulseWidth_start_usec, 20);
    CHECK_EQ("pulseWidth_end_usec", p->impedanceCheck.pulseWidth_end_usec, 30);
    CHECK_EQ("stimulationLevel_uA (16비트 조립)", p->impedanceCheck.stimulationLevel_uA, 300);
    CHECK_EQ("fetched_command", p->fetched_command, en__mapping_impedanceChekck);
    CHECK_EQ("에러 없음", stub_error_count(), 0);
    CHECK_EQ("송신 없음", stub_tx_count(), 0);

    // ------------------------------------------------------------------
    TEST_GROUP("0x62 임피던스 - iterationNum 경계 (1~255)");

    make_impedance_ok(pkt);
    pkt[1] = 1;
    tdc_ble_map_measure_impedance_check(pkt);
    CHECK_EQ("하한 1 통과", stub_error_count(), 0);

    make_impedance_ok(pkt);
    pkt[1] = 0;  // 하한 미만
    tdc_ble_map_measure_impedance_check(pkt);
    CHECK_EQ("0 은 거부", stub_error_count(), 1);
    CHECK_EQ("minor = OutOfDataRange", stub_error_last_minor(), en__OutOfDataRange);

    make_impedance_ok(pkt);
    pkt[1] = 255;
    tdc_ble_map_measure_impedance_check(pkt);
    CHECK_EQ("상한 255 통과", stub_error_count(), 0);

    // ------------------------------------------------------------------
    TEST_GROUP("0x62 임피던스 - channel 경계 (1~32 또는 255)");

    make_impedance_ok(pkt);
    pkt[2] = 1;
    tdc_ble_map_measure_impedance_check(pkt);
    CHECK_EQ("하한 1 통과", stub_error_count(), 0);

    make_impedance_ok(pkt);
    pkt[2] = 32;
    tdc_ble_map_measure_impedance_check(pkt);
    CHECK_EQ("상한 32 통과", stub_error_count(), 0);

    make_impedance_ok(pkt);
    pkt[2] = 33;
    tdc_ble_map_measure_impedance_check(pkt);
    CHECK_EQ("33 은 거부", stub_error_count(), 1);

    make_impedance_ok(pkt);
    pkt[2] = 255;  // 전채널 예외값
    tdc_ble_map_measure_impedance_check(pkt);
    CHECK_EQ("255(전채널) 통과", stub_error_count(), 0);

    make_impedance_ok(pkt);
    pkt[2] = 0;
    tdc_ble_map_measure_impedance_check(pkt);
    CHECK_EQ("0 은 거부", stub_error_count(), 1);

    // ------------------------------------------------------------------
    TEST_GROUP("0x62 임피던스 - 펄스 위상 폭 경계 (13~255)");

    make_impedance_ok(pkt);
    pkt[3] = 13;
    tdc_ble_map_measure_impedance_check(pkt);
    CHECK_EQ("start 하한 13 통과", stub_error_count(), 0);

    make_impedance_ok(pkt);
    pkt[3] = 12;
    tdc_ble_map_measure_impedance_check(pkt);
    CHECK_EQ("start 12 는 거부", stub_error_count(), 1);

    make_impedance_ok(pkt);
    pkt[4] = 13;
    tdc_ble_map_measure_impedance_check(pkt);
    CHECK_EQ("end 하한 13 통과", stub_error_count(), 0);

    make_impedance_ok(pkt);
    pkt[4] = 12;
    tdc_ble_map_measure_impedance_check(pkt);
    CHECK_EQ("end 12 는 거부", stub_error_count(), 1);

    // ------------------------------------------------------------------
    TEST_GROUP("0x62 임피던스 - 자극 크기 경계 (1~1024)");

    make_impedance_ok(pkt);
    pkt[5] = 0x00;
    pkt[6] = 0x01;  // 1
    tdc_ble_map_measure_impedance_check(pkt);
    CHECK_EQ("하한 1 통과", stub_error_count(), 0);

    make_impedance_ok(pkt);
    pkt[5] = 0x00;
    pkt[6] = 0x00;  // 0
    tdc_ble_map_measure_impedance_check(pkt);
    CHECK_EQ("0 은 거부", stub_error_count(), 1);

    make_impedance_ok(pkt);
    pkt[5] = 0x04;
    pkt[6] = 0x00;  // 1024
    tdc_ble_map_measure_impedance_check(pkt);
    CHECK_EQ("상한 1024 통과", stub_error_count(), 0);

    make_impedance_ok(pkt);
    pkt[5] = 0x04;
    pkt[6] = 0x01;  // 1025
    tdc_ble_map_measure_impedance_check(pkt);
    CHECK_EQ("1025 는 거부", stub_error_count(), 1);

    // ------------------------------------------------------------------
    TEST_GROUP("0x63 eCAP 마스킹 - 전 필드 적재");

    make_ecap_masking_ok(pkt);
    tdc_ble_map_measure_ecap_masking(pkt);
    p = tdc_ble_mapping_get_packet();

    CHECK_EQ("iterationNum", p->eCapMeasurement.iterationNum, 4);
    CHECK_EQ("pulseWidth", p->eCapMeasurement.pulseWidth, 25);
    CHECK_EQ("firstPulsePhase", p->eCapMeasurement.firstPulsePhase, 1);
    CHECK_EQ("stimulatonMode", p->eCapMeasurement.stimulatonMode, 2);
    CHECK_EQ("stimulationElectrodeNum", p->eCapMeasurement.stimulationElectrodeNum, 7);
    CHECK_EQ("bipolarReferenceElectrodeNum", p->eCapMeasurement.bipolarReferenceElectrodeNum, 8);
    CHECK_EQ("measurementElectrodeNum", p->eCapMeasurement.measurementElectrodeNum, 9);
    CHECK_EQ("masker 600 (16비트)", p->eCapMeasurement.stimulationLevel_uA_masker, 600);
    CHECK_EQ("probe 500 (16비트)", p->eCapMeasurement.stimulationLevel_uA_probe, 500);
    CHECK_EQ("maskerProbeInterval_numFrame", p->eCapMeasurement.maskerProbeInterval_numFrame, 3);
    CHECK_EQ("adcPreampGain", p->eCapMeasurement.adcPreampGain, 11);
    CHECK_EQ("adcSamplingFreq", p->eCapMeasurement.adcSamplingFreq, 5);
    CHECK_EQ("adcMeasurementDelay", p->eCapMeasurement.adcMeasurementDelay, 13);
    CHECK_EQ("measurementSampleNum", p->eCapMeasurement.measurementSampleNum, 14);
    CHECK_EQ("fetched_command", p->fetched_command, en__mapping_eCAP_Measurement_masking);
    CHECK_EQ("정상 입력은 에러 없음", stub_error_count(), 0);
    CHECK_EQ("송신 없음", stub_tx_count(), 0);

    // ------------------------------------------------------------------
    // 아래는 2026-08-05 신설 (상시 점검 대장 위험_1).
    // 전극 번호는 isd/tdc_isd_map_ecap.c 에서 electrodeMap[32] 인덱스로 쓰이므로
    // 파싱에서 막지 않으면 배열 범위 밖을 읽는다.
    // ------------------------------------------------------------------
    TEST_GROUP("0x63 eCAP 마스킹 - iterationNum 경계 (1~255)");

    make_ecap_masking_ok(pkt);
    pkt[1] = 1;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("하한 1 통과", stub_error_count(), 0);

    make_ecap_masking_ok(pkt);
    pkt[1] = 0;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("0 은 거부", stub_error_count(), 1);
    CHECK_EQ("minor = OutOfDataRange", stub_error_last_minor(), en__OutOfDataRange);

    make_ecap_masking_ok(pkt);
    pkt[1] = 255;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("상한 255 통과", stub_error_count(), 0);

    // ------------------------------------------------------------------
    TEST_GROUP("0x63 eCAP 마스킹 - pulseWidth 경계 (13~255)");

    make_ecap_masking_ok(pkt);
    pkt[2] = 13;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("하한 13 통과", stub_error_count(), 0);

    make_ecap_masking_ok(pkt);
    pkt[2] = 12;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("12 는 거부 (FPGA 최소 펄스폭 미만)", stub_error_count(), 1);

    make_ecap_masking_ok(pkt);
    pkt[2] = 255;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("상한 255 통과", stub_error_count(), 0);

    // ------------------------------------------------------------------
    TEST_GROUP("0x63 eCAP 마스킹 - firstPulsePhase 경계 (0~1)");

    make_ecap_masking_ok(pkt);
    pkt[3] = 0;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("0 통과", stub_error_count(), 0);

    make_ecap_masking_ok(pkt);
    pkt[3] = 1;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("1 통과", stub_error_count(), 0);

    make_ecap_masking_ok(pkt);
    pkt[3] = 2;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("2 는 거부", stub_error_count(), 1);

    // ------------------------------------------------------------------
    TEST_GROUP("0x63 eCAP 마스킹 - stimulatonMode 경계 (1~6)");

    make_ecap_masking_ok(pkt);
    pkt[4] = 1;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("하한 1 통과", stub_error_count(), 0);

    make_ecap_masking_ok(pkt);
    pkt[4] = 0;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("0 (en__referenceNA) 은 거부", stub_error_count(), 1);

    make_ecap_masking_ok(pkt);
    pkt[4] = 6;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("상한 6 통과", stub_error_count(), 0);

    make_ecap_masking_ok(pkt);
    pkt[4] = 7;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("7 은 거부", stub_error_count(), 1);

    // ------------------------------------------------------------------
    TEST_GROUP("0x63 eCAP 마스킹 - stimulationElectrodeNum 경계 (1~32)");

    make_ecap_masking_ok(pkt);
    pkt[5] = 1;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("하한 1 통과", stub_error_count(), 0);

    make_ecap_masking_ok(pkt);
    pkt[5] = 0;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("0 은 거부 (electrodeMap[-1] 차단)", stub_error_count(), 1);

    make_ecap_masking_ok(pkt);
    pkt[5] = 32;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("상한 32 통과", stub_error_count(), 0);

    make_ecap_masking_ok(pkt);
    pkt[5] = 33;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("33 은 거부", stub_error_count(), 1);

    make_ecap_masking_ok(pkt);
    pkt[5] = 255;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("255 는 거부 (electrodeMap[254] 차단)", stub_error_count(), 1);
    CHECK_EQ("거부 시 fetched_command 미설정", tdc_ble_mapping_get_packet()->fetched_command, en__mapping_IDLE);

    // ------------------------------------------------------------------
    TEST_GROUP("0x63 eCAP 마스킹 - bipolarReferenceElectrodeNum 경계 (1~32 또는 99)");

    make_ecap_masking_ok(pkt);
    pkt[6] = 1;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("하한 1 통과", stub_error_count(), 0);

    make_ecap_masking_ok(pkt);
    pkt[6] = 0;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("0 은 거부", stub_error_count(), 1);

    make_ecap_masking_ok(pkt);
    pkt[6] = 32;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("상한 32 통과", stub_error_count(), 0);

    make_ecap_masking_ok(pkt);
    pkt[6] = 33;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("33 은 거부", stub_error_count(), 1);

    make_ecap_masking_ok(pkt);
    pkt[6] = 98;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("98 은 거부", stub_error_count(), 1);

    make_ecap_masking_ok(pkt);
    pkt[6] = 99;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("99 는 예외로 통과 (0x65 와 동일, for 모노폴라)", stub_error_count(), 0);

    // ------------------------------------------------------------------
    TEST_GROUP("0x63 eCAP 마스킹 - measurementElectrodeNum 경계 (1~32)");

    make_ecap_masking_ok(pkt);
    pkt[7] = 1;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("하한 1 통과", stub_error_count(), 0);

    make_ecap_masking_ok(pkt);
    pkt[7] = 0;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("0 은 거부 (electrodeMap[-1] 차단)", stub_error_count(), 1);

    make_ecap_masking_ok(pkt);
    pkt[7] = 32;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("상한 32 통과", stub_error_count(), 0);

    make_ecap_masking_ok(pkt);
    pkt[7] = 33;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("33 은 거부", stub_error_count(), 1);

    make_ecap_masking_ok(pkt);
    pkt[7] = 255;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("255 는 거부 (electrodeMap[254] 차단)", stub_error_count(), 1);
    CHECK_EQ("거부 시 fetched_command 미설정", tdc_ble_mapping_get_packet()->fetched_command, en__mapping_IDLE);

    // ------------------------------------------------------------------
    TEST_GROUP("0x63 eCAP 마스킹 - adcSamplingFreq 경계 (0~7, 레지스터 3비트)");

    make_ecap_masking_ok(pkt);
    pkt[14] = 0;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("0 통과", stub_error_count(), 0);

    make_ecap_masking_ok(pkt);
    pkt[14] = 7;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("상한 7 통과", stub_error_count(), 0);

    make_ecap_masking_ok(pkt);
    pkt[14] = 8;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("8 은 거부 (3비트 초과)", stub_error_count(), 1);

    // ------------------------------------------------------------------
    TEST_GROUP("0x63 eCAP 마스킹 - adcMeasurementDelay 경계 (0~15, 레지스터 4비트)");

    make_ecap_masking_ok(pkt);
    pkt[15] = 0;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("0 통과", stub_error_count(), 0);

    make_ecap_masking_ok(pkt);
    pkt[15] = 15;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("상한 15 통과", stub_error_count(), 0);

    make_ecap_masking_ok(pkt);
    pkt[15] = 16;
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("16 은 거부 (4비트 초과)", stub_error_count(), 1);

    // ------------------------------------------------------------------
    TEST_GROUP("0x63 eCAP 마스킹 - 검사하지 않는 필드는 통과");

    make_ecap_masking_ok(pkt);
    pkt[12] = 255;  // maskerProbeInterval_numFrame - isd 에서 검사한다
    pkt[13] = 255;  // adcPreampGain                - 비트폭 미확정이라 미검사
    pkt[16] = 255;  // measurementSampleNum         - 하한 근거 없음
    tdc_ble_map_measure_ecap_masking(pkt);
    CHECK_EQ("미검사 필드는 255 도 통과", stub_error_count(), 0);
    CHECK_EQ("fetched_command 설정됨", tdc_ble_mapping_get_packet()->fetched_command, en__mapping_eCAP_Measurement_masking);

    // ------------------------------------------------------------------
    TEST_GROUP("0x64 eCAP 교대 - 필드 적재와 오프셋");

    make_ecap_alternative_ok(pkt);
    tdc_ble_map_measure_ecap_alternative(pkt);
    p = tdc_ble_mapping_get_packet();

    CHECK_EQ("iterationNum", p->eCapMeasurement.iterationNum, 6);
    CHECK_EQ("pulseWidth", p->eCapMeasurement.pulseWidth, 26);
    CHECK_EQ("stimulatonMode (오프셋 3)", p->eCapMeasurement.stimulatonMode, 3);
    CHECK_EQ("stimulationElectrodeNum", p->eCapMeasurement.stimulationElectrodeNum, 15);
    CHECK_EQ("bipolarReferenceElectrodeNum", p->eCapMeasurement.bipolarReferenceElectrodeNum, 16);
    CHECK_EQ("measurementElectrodeNum", p->eCapMeasurement.measurementElectrodeNum, 17);
    CHECK_EQ("masker 1000 (16비트)", p->eCapMeasurement.stimulationLevel_uA_masker, 1000);
    CHECK_EQ("fetched_command", p->fetched_command, en__mapping_eCAP_Measurement_alternative);

    // 0x64 는 firstPulsePhase · probe · adc* 를 건드리지 않는다.
    // 직전 0x63 의 값이 남는 것이 현재 동작이며 이를 고정한다.
    CHECK_EQ("firstPulsePhase 는 손대지 않음", p->eCapMeasurement.firstPulsePhase, 0);
    CHECK_EQ("probe 는 손대지 않음", p->eCapMeasurement.stimulationLevel_uA_probe, 0);
    CHECK_EQ("정상 입력은 에러 없음", stub_error_count(), 0);

    // ------------------------------------------------------------------
    // 아래는 2026-08-05 신설 (상시 점검 대장 위험_2).
    // 0x64 는 실행부가 주석 처리돼 있으나 0x63 과 같은 구조체를 쓰므로
    // 되살리는 순간 같은 범위 밖 읽기가 발생한다. 선제 차단한다.
    // ------------------------------------------------------------------
    TEST_GROUP("0x64 eCAP 교대 - iterationNum 경계 (1~255)");

    make_ecap_alternative_ok(pkt);
    pkt[1] = 1;
    tdc_ble_map_measure_ecap_alternative(pkt);
    CHECK_EQ("하한 1 통과", stub_error_count(), 0);

    make_ecap_alternative_ok(pkt);
    pkt[1] = 0;
    tdc_ble_map_measure_ecap_alternative(pkt);
    CHECK_EQ("0 은 거부", stub_error_count(), 1);
    CHECK_EQ("minor = OutOfDataRange", stub_error_last_minor(), en__OutOfDataRange);

    make_ecap_alternative_ok(pkt);
    pkt[1] = 255;
    tdc_ble_map_measure_ecap_alternative(pkt);
    CHECK_EQ("상한 255 통과", stub_error_count(), 0);

    // ------------------------------------------------------------------
    TEST_GROUP("0x64 eCAP 교대 - pulseWidth 경계 (13~255)");

    make_ecap_alternative_ok(pkt);
    pkt[2] = 13;
    tdc_ble_map_measure_ecap_alternative(pkt);
    CHECK_EQ("하한 13 통과", stub_error_count(), 0);

    make_ecap_alternative_ok(pkt);
    pkt[2] = 12;
    tdc_ble_map_measure_ecap_alternative(pkt);
    CHECK_EQ("12 는 거부 (FPGA 최소 펄스폭 미만)", stub_error_count(), 1);

    make_ecap_alternative_ok(pkt);
    pkt[2] = 255;
    tdc_ble_map_measure_ecap_alternative(pkt);
    CHECK_EQ("상한 255 통과", stub_error_count(), 0);

    // ------------------------------------------------------------------
    TEST_GROUP("0x64 eCAP 교대 - stimulatonMode 경계 (1~6)");

    make_ecap_alternative_ok(pkt);
    pkt[3] = 1;
    tdc_ble_map_measure_ecap_alternative(pkt);
    CHECK_EQ("하한 1 통과", stub_error_count(), 0);

    make_ecap_alternative_ok(pkt);
    pkt[3] = 0;
    tdc_ble_map_measure_ecap_alternative(pkt);
    CHECK_EQ("0 (en__referenceNA) 은 거부", stub_error_count(), 1);

    make_ecap_alternative_ok(pkt);
    pkt[3] = 6;
    tdc_ble_map_measure_ecap_alternative(pkt);
    CHECK_EQ("상한 6 통과", stub_error_count(), 0);

    make_ecap_alternative_ok(pkt);
    pkt[3] = 7;
    tdc_ble_map_measure_ecap_alternative(pkt);
    CHECK_EQ("7 은 거부", stub_error_count(), 1);

    // ------------------------------------------------------------------
    TEST_GROUP("0x64 eCAP 교대 - stimulationElectrodeNum 경계 (1~32)");

    make_ecap_alternative_ok(pkt);
    pkt[4] = 1;
    tdc_ble_map_measure_ecap_alternative(pkt);
    CHECK_EQ("하한 1 통과", stub_error_count(), 0);

    make_ecap_alternative_ok(pkt);
    pkt[4] = 0;
    tdc_ble_map_measure_ecap_alternative(pkt);
    CHECK_EQ("0 은 거부 (electrodeMap[-1] 차단)", stub_error_count(), 1);

    make_ecap_alternative_ok(pkt);
    pkt[4] = 32;
    tdc_ble_map_measure_ecap_alternative(pkt);
    CHECK_EQ("상한 32 통과", stub_error_count(), 0);

    make_ecap_alternative_ok(pkt);
    pkt[4] = 33;
    tdc_ble_map_measure_ecap_alternative(pkt);
    CHECK_EQ("33 은 거부", stub_error_count(), 1);

    make_ecap_alternative_ok(pkt);
    pkt[4] = 255;
    tdc_ble_map_measure_ecap_alternative(pkt);
    CHECK_EQ("255 는 거부 (electrodeMap[254] 차단)", stub_error_count(), 1);
    CHECK_EQ("거부 시 fetched_command 미설정", tdc_ble_mapping_get_packet()->fetched_command, en__mapping_IDLE);

    // ------------------------------------------------------------------
    TEST_GROUP("0x64 eCAP 교대 - bipolarReferenceElectrodeNum 경계 (1~32 또는 99)");

    make_ecap_alternative_ok(pkt);
    pkt[5] = 32;
    tdc_ble_map_measure_ecap_alternative(pkt);
    CHECK_EQ("상한 32 통과", stub_error_count(), 0);

    make_ecap_alternative_ok(pkt);
    pkt[5] = 33;
    tdc_ble_map_measure_ecap_alternative(pkt);
    CHECK_EQ("33 은 거부", stub_error_count(), 1);

    make_ecap_alternative_ok(pkt);
    pkt[5] = 99;
    tdc_ble_map_measure_ecap_alternative(pkt);
    CHECK_EQ("99 는 예외로 통과 (0x65 와 동일)", stub_error_count(), 0);

    // ------------------------------------------------------------------
    TEST_GROUP("0x64 eCAP 교대 - measurementElectrodeNum 경계 (1~32)");

    make_ecap_alternative_ok(pkt);
    pkt[6] = 1;
    tdc_ble_map_measure_ecap_alternative(pkt);
    CHECK_EQ("하한 1 통과", stub_error_count(), 0);

    make_ecap_alternative_ok(pkt);
    pkt[6] = 0;
    tdc_ble_map_measure_ecap_alternative(pkt);
    CHECK_EQ("0 은 거부 (electrodeMap[-1] 차단)", stub_error_count(), 1);

    make_ecap_alternative_ok(pkt);
    pkt[6] = 32;
    tdc_ble_map_measure_ecap_alternative(pkt);
    CHECK_EQ("상한 32 통과", stub_error_count(), 0);

    make_ecap_alternative_ok(pkt);
    pkt[6] = 255;
    tdc_ble_map_measure_ecap_alternative(pkt);
    CHECK_EQ("255 는 거부 (electrodeMap[254] 차단)", stub_error_count(), 1);
    CHECK_EQ("거부 시 fetched_command 미설정", tdc_ble_mapping_get_packet()->fetched_command, en__mapping_IDLE);

    // ------------------------------------------------------------------
    TEST_GROUP("0x64 eCAP 교대 - masker 는 검사하지 않는다");

    make_ecap_alternative_ok(pkt);
    pkt[7] = 0xFF;
    pkt[8] = 0xFF;  // masker 65535
    tdc_ble_map_measure_ecap_alternative(pkt);
    CHECK_EQ("masker 는 상한 근거가 없어 미검사", stub_error_count(), 0);
    CHECK_EQ("masker 65535 적재", tdc_ble_mapping_get_packet()->eCapMeasurement.stimulationLevel_uA_masker, 65535);

    // ------------------------------------------------------------------
    TEST_GROUP("공통");

    CHECK_EQ("저장소 오버플로 없음", stub_repository_overflow(), 0);

    TEST_SUMMARY();
}
