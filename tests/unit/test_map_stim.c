// 매핑 프로토콜 0x65~0x66 (특정 자극 · 라이브 자극) 파싱 테스트.
//
// 0x66 의 하위 명령 8종은 static 이라 직접 호출할 수 없다(단계_1b-3b).
// 진입점 tdc_ble_map_stim_live() 로 간접 테스트한다 - pkt[1] 에 하위
// 명령 번호를 넣으면 원하는 분기에 도달한다. 실사용 경로가 어차피
// 진입점 경유이므로 이쪽이 더 실제에 가깝다.
//
// 하위 3(볼륨)·4(마이크)는 선행 상태 en__HoldOn 을 요구한다.
// 이는 설계안이 근인_2 로 지목한 "라이브 상태 4층 분산"의 단면이며,
// 단계_4(라이브 상태 계약 명시)에서 손댈 부분이라 지금 고정해 둔다.

#include <string.h>
#include <stdint.h>

#include <tdc_ble_mapping.h>
#include <tdc_sys_error.h>
#include <tdc_isd_map_live.h>  // EN__LIVE_STIMULATION_SUB_COMMAND (en__HoldOn 등)
#include <tdc_ble_map_stim.h>

#include "tdc_test.h"
#include "stub_ble.h"

static void make_packet(uint8_t *pkt, int command)
{
    memset(pkt, 0, BLE_DataPacketSize + 1);
    pkt[0] = (uint8_t) command;
}

// 0x66 라이브 패킷. pkt[1] 이 하위 명령 번호다.
static void make_live(uint8_t *pkt, int subCommand)
{
    make_packet(pkt, en__mapping_live_stimulation);
    pkt[1] = (uint8_t) subCommand;
}

// 0x65 유효 기본 패킷
static void make_specific_ok(uint8_t *pkt)
{
    make_packet(pkt, en__mapping_specific_stimulation);
    pkt[1] = 16;    // usableElectrodeNum          1~32
    pkt[2] = 25;    // pulseWidth                  13~255
    pkt[3] = 0;     // firstPulsePhase             0~1
    pkt[4] = 3;     // stimulatonMode              1~6
    pkt[5] = 10;    // stimulationElectrodeNum     1~32
    pkt[6] = 11;    // bipolarReferenceElectrodeNum 1~32 또는 99
    pkt[7] = 0x01;  // level 상위
    pkt[8] = 0x2C;  // level 하위 -> 300
    pkt[9] = 5;     // stimulationTime_100msec
}

// 선행 상태를 HoldOn 으로 만든다(볼륨·마이크 조절의 전제).
static void set_hold_on(void)
{
    tdc_ble_mapping_get_packet()->tdc_isd_map_live_step.subCommand = en__HoldOn;
}

int main(void)
{
    uint8_t             pkt[BLE_DataPacketSize + 1];
    ST__MAPPING_PACKET *p;

    // ------------------------------------------------------------------
    TEST_GROUP("0x65 특정 자극 - 정상");

    stub_reset();
    make_specific_ok(pkt);
    tdc_ble_map_stim_specific(pkt);
    p = tdc_ble_mapping_get_packet();

    CHECK_EQ("usableElectrodeNum", p->tdc_isd_map_specific_stim_step.usableElectrodeNum, 16);
    CHECK_EQ("pulseWidth", p->tdc_isd_map_specific_stim_step.pulseWidth, 25);
    CHECK_EQ("stimulatonMode", p->tdc_isd_map_specific_stim_step.stimulatonMode, 3);
    CHECK_EQ("stimulationElectrodeNum", p->tdc_isd_map_specific_stim_step.stimulationElectrodeNum, 10);
    CHECK_EQ("bipolarReferenceElectrodeNum", p->tdc_isd_map_specific_stim_step.bipolarReferenceElectrodeNum, 11);
    CHECK_EQ("stimulationLevel_uA (16비트)", p->tdc_isd_map_specific_stim_step.stimulationLevel_uA, 300);
    CHECK_EQ("stimulationTime_100msec", p->tdc_isd_map_specific_stim_step.stimulationTime_100msec, 5);
    CHECK_EQ("에러 없음", stub_error_count(), 0);

    // ------------------------------------------------------------------
    TEST_GROUP("0x65 - 전극 수 경계 (1~32)");

    stub_reset();
    make_specific_ok(pkt);
    pkt[1] = 1;
    tdc_ble_map_stim_specific(pkt);
    CHECK_EQ("하한 1 통과", stub_error_count(), 0);

    stub_reset();
    make_specific_ok(pkt);
    pkt[1] = 32;
    tdc_ble_map_stim_specific(pkt);
    CHECK_EQ("상한 32 통과", stub_error_count(), 0);

    stub_reset();
    make_specific_ok(pkt);
    pkt[1] = 33;
    tdc_ble_map_stim_specific(pkt);
    CHECK_EQ("33 은 거부", stub_error_count(), 1);

    stub_reset();
    make_specific_ok(pkt);
    pkt[1] = 0;
    tdc_ble_map_stim_specific(pkt);
    CHECK_EQ("0 은 거부", stub_error_count(), 1);

    // ------------------------------------------------------------------
    TEST_GROUP("0x65 - 펄스 위상 폭 경계 (13~255)");

    stub_reset();
    make_specific_ok(pkt);
    pkt[2] = 13;
    tdc_ble_map_stim_specific(pkt);
    CHECK_EQ("하한 13 통과", stub_error_count(), 0);

    stub_reset();
    make_specific_ok(pkt);
    pkt[2] = 12;
    tdc_ble_map_stim_specific(pkt);
    CHECK_EQ("12 는 거부", stub_error_count(), 1);

    // ------------------------------------------------------------------
    TEST_GROUP("0x65 - 자극 모드 경계 (1~6)");

    stub_reset();
    make_specific_ok(pkt);
    pkt[4] = 6;
    tdc_ble_map_stim_specific(pkt);
    CHECK_EQ("상한 6 통과", stub_error_count(), 0);

    stub_reset();
    make_specific_ok(pkt);
    pkt[4] = 7;
    tdc_ble_map_stim_specific(pkt);
    CHECK_EQ("7 은 거부", stub_error_count(), 1);

    // ------------------------------------------------------------------
    TEST_GROUP("0x65 - 바이폴라 기준전극 99 예외");

    // 99 는 모노폴라를 뜻하는 예외값이라 1~32 밖이어도 통과한다.
    stub_reset();
    make_specific_ok(pkt);
    pkt[6] = 99;
    tdc_ble_map_stim_specific(pkt);
    CHECK_EQ("99 는 예외로 통과", stub_error_count(), 0);

    stub_reset();
    make_specific_ok(pkt);
    pkt[6] = 33;
    tdc_ble_map_stim_specific(pkt);
    CHECK_EQ("33 은 거부", stub_error_count(), 1);

    // ------------------------------------------------------------------
    TEST_GROUP("0x66 라이브 - 하위 명령 번호 범위 (1~9)");

    stub_reset();
    make_live(pkt, 0);
    tdc_ble_map_stim_live(pkt);
    CHECK_EQ("0 은 거부", stub_error_count(), 1);
    CHECK_EQ("minor = OutOfDataRange", stub_error_last_minor(), en__OutOfDataRange);

    stub_reset();
    make_live(pkt, 10);
    tdc_ble_map_stim_live(pkt);
    CHECK_EQ("10 은 거부", stub_error_count(), 1);

    stub_reset();
    make_live(pkt, en__Start);
    tdc_ble_map_stim_live(pkt);
    p = tdc_ble_mapping_get_packet();
    CHECK_EQ("유효 하위 명령은 fetched_command 설정", p->fetched_command, en__mapping_live_stimulation);

    // ------------------------------------------------------------------
    TEST_GROUP("0x66 하위 2 / 7 / 8 - 상태 설정");

    stub_reset();
    make_live(pkt, en__Start);
    tdc_ble_map_stim_live(pkt);
    p = tdc_ble_mapping_get_packet();
    CHECK_EQ("Start -> subCommand", p->tdc_isd_map_live_step.subCommand, en__Start);
    CHECK_EQ("에러 없음", stub_error_count(), 0);

    stub_reset();
    make_live(pkt, en__readDeviceStatus);
    tdc_ble_map_stim_live(pkt);
    p = tdc_ble_mapping_get_packet();
    CHECK_EQ("readDeviceStatus -> subCommand", p->tdc_isd_map_live_step.subCommand, en__readDeviceStatus);

    stub_reset();
    make_live(pkt, en__Stop);
    tdc_ble_map_stim_live(pkt);
    p = tdc_ble_mapping_get_packet();
    CHECK_EQ("Stop -> subCommand", p->tdc_isd_map_live_step.subCommand, en__Stop);

    // ------------------------------------------------------------------
    TEST_GROUP("0x66 하위 3 자극 볼륨 - 선행 상태 HoldOn 요구");

    // HoldOn 이 아니면 명령 순서 에러
    stub_reset();
    make_live(pkt, en__StimulationVolumeAdjust);
    pkt[2] = 2;
    tdc_ble_map_stim_live(pkt);
    p = tdc_ble_mapping_get_packet();
    CHECK_EQ("HoldOn 아니면 에러", stub_error_count(), 1);
    CHECK_EQ("minor = Command_Order", stub_error_last_minor(), en__Command_Order);
    CHECK_EQ("상태는 Standby 로", p->tdc_isd_map_live_step.subCommand, en__Standby);

    // HoldOn 이면 정상 처리
    stub_reset();
    set_hold_on();
    make_live(pkt, en__StimulationVolumeAdjust);
    pkt[2] = 3;
    tdc_ble_map_stim_live(pkt);
    p = tdc_ble_mapping_get_packet();
    CHECK_EQ("HoldOn 이면 통과", stub_error_count(), 0);
    CHECK_EQ("stimulVolume 적재", p->tdc_isd_map_live_step.stimulVolume, 3);
    CHECK_EQ("subCommand 전이", p->tdc_isd_map_live_step.subCommand, en__StimulationVolumeAdjust);

    // 범위 경계 (1~4)
    stub_reset();
    set_hold_on();
    make_live(pkt, en__StimulationVolumeAdjust);
    pkt[2] = 4;
    tdc_ble_map_stim_live(pkt);
    CHECK_EQ("상한 4 통과", stub_error_count(), 0);

    stub_reset();
    set_hold_on();
    make_live(pkt, en__StimulationVolumeAdjust);
    pkt[2] = 5;
    tdc_ble_map_stim_live(pkt);
    p = tdc_ble_mapping_get_packet();
    CHECK_EQ("5 는 거부", stub_error_count(), 1);
    CHECK_EQ("minor = OutOfDataRange", stub_error_last_minor(), en__OutOfDataRange);
    CHECK_EQ("거부 시 Standby 로", p->tdc_isd_map_live_step.subCommand, en__Standby);

    stub_reset();
    set_hold_on();
    make_live(pkt, en__StimulationVolumeAdjust);
    pkt[2] = 0;
    tdc_ble_map_stim_live(pkt);
    CHECK_EQ("0 은 거부", stub_error_count(), 1);

    // ------------------------------------------------------------------
    TEST_GROUP("0x66 하위 4 마이크 감도 - 선행 상태와 범위 (1~10)");

    stub_reset();
    make_live(pkt, en__MicSensitivityAdjust);
    pkt[2] = 5;
    tdc_ble_map_stim_live(pkt);
    CHECK_EQ("HoldOn 아니면 에러", stub_error_count(), 1);
    CHECK_EQ("minor = Command_Order", stub_error_last_minor(), en__Command_Order);

    stub_reset();
    set_hold_on();
    make_live(pkt, en__MicSensitivityAdjust);
    pkt[2] = 10;
    tdc_ble_map_stim_live(pkt);
    p = tdc_ble_mapping_get_packet();
    CHECK_EQ("상한 10 통과", stub_error_count(), 0);
    CHECK_EQ("audioVolume 적재", p->tdc_isd_map_live_step.audioVolume, 10);

    stub_reset();
    set_hold_on();
    make_live(pkt, en__MicSensitivityAdjust);
    pkt[2] = 11;
    tdc_ble_map_stim_live(pkt);
    CHECK_EQ("11 은 거부", stub_error_count(), 1);

    // ------------------------------------------------------------------
    TEST_GROUP("0x66 하위 1 전체 파라미터 - 데이터 인덱스 1");

    stub_reset();
    make_live(pkt, en__allParameter);
    pkt[2]  = 1;     // 데이터 인덱스
    pkt[3]  = 2;     // stimulVolume                 1~4
    pkt[4]  = 5;     // audioVolume                  1~10
    pkt[5]  = 8;     // stimulationIndicatorChannel  1~32
    pkt[6]  = 0x01;  // indicatorAmplitude 상위
    pkt[7]  = 0x2C;  // 하위 -> 300                  0~1800
    pkt[8]  = 2;     // stimulationStrategy          1~3
    pkt[9]  = 4;     // stimulationMode              1~6
    pkt[10] = 1;     // firstPulsePhase              0~1
    pkt[11] = 30;    // stimulationPulsePhaseWidth
    pkt[12] = 16;    // numFrequencyBand
    tdc_ble_map_stim_live(pkt);
    p = tdc_ble_mapping_get_packet();

    CHECK_EQ("에러 없음", stub_error_count(), 0);
    CHECK_EQ("seq_index = 1", tdc_ble_mapping_get_seq_index(), 1);
    CHECK_EQ("stimulVolume", p->tdc_isd_map_live_step.stimulVolume, 2);
    CHECK_EQ("audioVolume", p->tdc_isd_map_live_step.audioVolume, 5);
    CHECK_EQ("indicatorChannel", p->tdc_isd_map_live_step.stimulationIndicatorChannelNum, 8);
    CHECK_EQ("indicatorAmplitude 16비트", p->tdc_isd_map_live_step.stimulationIndicatorAmplitude_uA, 300);
    CHECK_EQ("stimulationStrategy", p->tdc_isd_map_live_step.stimulationStrategy, 2);
    CHECK_EQ("stimulationMode", p->tdc_isd_map_live_step.stimulationMode, 4);
    CHECK_EQ("firstPulsePhase", p->tdc_isd_map_live_step.firstPulsePhase, 1);
    CHECK_EQ("진입 시 subCommand 는 Standby", p->tdc_isd_map_live_step.subCommand, en__Standby);

    // ------------------------------------------------------------------
    TEST_GROUP("0x66 하위 1 - 데이터 인덱스 순서 위반");

    stub_reset();
    make_live(pkt, en__allParameter);
    pkt[2] = 3;  // 1 을 건너뜀
    tdc_ble_map_stim_live(pkt);
    CHECK_EQ("에러 1건", stub_error_count(), 1);
    CHECK_EQ("minor = DATA_Order", stub_error_last_minor(), en__DATA_Order);
    CHECK_EQ("seq 리셋", tdc_ble_mapping_get_seq_index(), 0);

    // ------------------------------------------------------------------
    TEST_GROUP("0x66 하위 1 - 범위 검사");

    stub_reset();
    make_live(pkt, en__allParameter);
    pkt[2] = 1;
    pkt[3] = 5;   // stimulVolume 1~4 초과
    pkt[4] = 5;
    pkt[5] = 8;
    pkt[8] = 2;
    pkt[9] = 4;
    tdc_ble_map_stim_live(pkt);
    CHECK_EQ("stimulVolume 5 는 거부", stub_error_count(), 1);

    stub_reset();
    make_live(pkt, en__allParameter);
    pkt[2] = 1;
    pkt[3] = 2;
    pkt[4] = 11;  // audioVolume 1~10 초과
    pkt[5] = 8;
    pkt[8] = 2;
    pkt[9] = 4;
    tdc_ble_map_stim_live(pkt);
    CHECK_EQ("audioVolume 11 은 거부", stub_error_count(), 1);

    stub_reset();
    make_live(pkt, en__allParameter);
    pkt[2] = 1;
    pkt[3] = 2;
    pkt[4] = 5;
    pkt[5] = 33;  // indicatorChannel 1~32 초과
    pkt[8] = 2;
    pkt[9] = 4;
    tdc_ble_map_stim_live(pkt);
    CHECK_EQ("indicatorChannel 33 은 거부", stub_error_count(), 1);

    // ------------------------------------------------------------------
    TEST_GROUP("공통");

    CHECK_EQ("저장소 오버플로 없음", stub_repository_overflow(), 0);

    TEST_SUMMARY();
}
