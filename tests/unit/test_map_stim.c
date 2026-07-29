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

// ---------------------------------------------------------------------------
// 0x66 하위 1 (전체 파라미터) - 데이터 인덱스 1~15
//
// 인덱스 1 은 스칼라 9필드, 2~15 는 배열 청크다. 청크는 배열·구간·폭·범위만
// 다르고 구조가 같아 아래 표 하나로 다룬다.
//
// 값은 원소마다 다르게(base + i*step) 넣는다. 전부 같은 값이면 첨자를 하나
// 밀려 적재해도 테스트가 통과해 버린다 - 이 단계에서 가장 틀리기 쉬운 것이
// 바로 청크의 시작 첨자와 개수다.
// ---------------------------------------------------------------------------

#define CHUNK_COUNT 14  // 데이터 인덱스 2~15

static const struct
{
    int dataIndex;  // 데이터 인덱스
    int count;      // 원소 수
    int srcWidth;   // 패킷에서 읽는 폭 (1 또는 2)
    int base;       // 첫 원소 값
    int step;       // 원소 간 증가
    int arrStart;   // 대상 배열 내 시작 첨자
} k_chunk[CHUNK_COUNT] = {
    //             cnt  w  base step  start
    {  2, 17, 1,   1,  1,  0 },  // 자극 전극 [0..16]
    {  3, 15, 1,  18,  1, 17 },  // 자극 전극 [17..31]
    {  4, 17, 1,  33,  1,  0 },  // 기준 전극 [0..16]
    {  5, 15, 1,  50,  1, 17 },  // 기준 전극 [17..31]
    {  6, 17, 1,  65,  1,  0 },  // 밴드 출력 순서 [0..16]
    {  7, 15, 1,  82,  1, 17 },  // 밴드 출력 순서 [17..31]
    {  8,  8, 2, 100, 10,  0 },  // T 레벨 [0..7]
    {  9,  8, 2, 200, 10,  8 },  // T 레벨 [8..15]
    { 10,  8, 2, 300, 10, 16 },  // T 레벨 [16..23]
    { 11,  8, 2, 400, 10, 24 },  // T 레벨 [24..31]
    { 12,  8, 2, 500, 10,  0 },  // C 레벨 [0..7]
    { 13,  8, 2, 600, 10,  8 },  // C 레벨 [8..15]
    { 14,  8, 2, 700, 10, 16 },  // C 레벨 [16..23]
    { 15,  8, 2, 800, 10, 24 },  // C 레벨 [24..31]
};

// 데이터 인덱스 1 - 스칼라 9필드 전부 유효값.
static void make_all_param_idx1(uint8_t *pkt)
{
    make_live(pkt, en__allParameter);
    pkt[2]  = 1;     // 데이터 인덱스
    pkt[3]  = 2;     // stimulVolume                1~4
    pkt[4]  = 5;     // audioVolume                 1~10
    pkt[5]  = 8;     // stimulationIndicatorChannel 1~32
    pkt[6]  = 0x01;  // indicatorAmplitude 상위
    pkt[7]  = 0x2C;  // 하위 -> 300                 0~1800
    pkt[8]  = 2;     // stimulationStrategy         1~3
    pkt[9]  = 4;     // stimulationMode             1~6
    pkt[10] = 1;     // firstPulsePhase             0~1
    pkt[11] = 30;    // stimulationPulsePhaseWidth  13~255
    pkt[12] = 16;    // numFrequencyBand            1~32
}

// k_chunk[slot] 사양대로 데이터 인덱스 2~15 패킷을 만든다.
static void make_all_param_chunk(uint8_t *pkt, int slot)
{
    int e;

    make_live(pkt, en__allParameter);
    pkt[2] = (uint8_t) k_chunk[slot].dataIndex;

    for (e = 0; e < k_chunk[slot].count; e++)
    {
        int v = k_chunk[slot].base + (e * k_chunk[slot].step);

        if (k_chunk[slot].srcWidth == 2)
        {
            pkt[3 + (e * 2)]     = (uint8_t) ((v >> 8) & 0xFF);
            pkt[3 + (e * 2) + 1] = (uint8_t) (v & 0xFF);
        }
        else
        {
            pkt[3 + e] = (uint8_t) v;
        }
    }
}

// 데이터 인덱스가 어느 배열로 가는지. 테스트가 독립적으로 기대하는 대응이다.
static const int *chunk_target(const ST__MAPPING_PACKET *p, int dataIndex)
{
    switch (dataIndex)
    {
        case 2:
        case 3:
            return p->tdc_isd_map_live_step.usableStimulationElectrodIndex;
        case 4:
        case 5:
            return p->tdc_isd_map_live_step.usableReferenceElectrodIndex;
        case 6:
        case 7:
            return p->tdc_isd_map_live_step.CIS_FreqBandOrder;
        case 8:
        case 9:
        case 10:
        case 11:
            return p->tdc_isd_map_live_step.T_level_uA;
        default:
            return p->tdc_isd_map_live_step.C_level_uA;
    }
}

// 청크의 마지막 원소 값.
static int chunk_last_value(int slot)
{
    return k_chunk[slot].base + ((k_chunk[slot].count - 1) * k_chunk[slot].step);
}

// 데이터 인덱스 1 부터 lastIndex 까지 유효값으로 순차 주입한다.
// 인덱스는 1씩 증가해야만 수락되므로 중간을 건너뛸 수 없다.
static void feed_upto(uint8_t *pkt, int lastIndex)
{
    int n;

    make_all_param_idx1(pkt);
    tdc_ble_map_stim_live(pkt);

    for (n = 2; n <= lastIndex; n++)
    {
        make_all_param_chunk(pkt, n - 2);
        tdc_ble_map_stim_live(pkt);
    }
}

int main(void)
{
    uint8_t             pkt[BLE_DataPacketSize + 1];
    ST__MAPPING_PACKET *p;
    int                 i;
    char                name[80];
    uint8_t             expected_tx[4];

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

    // ==================================================================
    // 여기부터 데이터 인덱스 2~15.
    //
    // 단계_3(패킷 디스크립터 테이블) 전환 대상이며, 그 전까지 검증 케이스가
    // 하나도 없던 영역이다. 전환 "전" 코드로 먼저 통과시켜 두어야
    // "전환 후에도 통과 = 동작 보존"이 성립한다.
    // ==================================================================

    // ------------------------------------------------------------------
    TEST_GROUP("0x66 하위 1 - 데이터 인덱스 2~15 적재 위치");

    stub_reset();
    make_all_param_idx1(pkt);
    tdc_ble_map_stim_live(pkt);

    for (i = 0; i < CHUNK_COUNT; i++)
    {
        const int *target;
        int        first = k_chunk[i].arrStart;
        int        last  = k_chunk[i].arrStart + k_chunk[i].count - 1;

        make_all_param_chunk(pkt, i);
        tdc_ble_map_stim_live(pkt);

        p      = tdc_ble_mapping_get_packet();
        target = chunk_target(p, k_chunk[i].dataIndex);

        sprintf(name, "인덱스 %2d 첫 원소 -> [%d]", k_chunk[i].dataIndex, first);
        CHECK_EQ(name, target[first], k_chunk[i].base);

        sprintf(name, "인덱스 %2d 끝 원소 -> [%d]", k_chunk[i].dataIndex, last);
        CHECK_EQ(name, target[last], chunk_last_value(i));

        // 청크 밖 오염 확인. 뒤 원소가 배열 안에 있으면 아직 0 이어야 하고,
        // 청크가 배열 끝까지 닿았으면 앞 청크의 마지막 값이 남아 있어야 한다.
        if ((last + 1) < df_MaxNumOfElectrode)
        {
            sprintf(name, "인덱스 %2d 뒤 [%d] 미오염", k_chunk[i].dataIndex, last + 1);
            CHECK_EQ(name, target[last + 1], 0);
        }
        else
        {
            sprintf(name, "인덱스 %2d 앞 [%d] 유지", k_chunk[i].dataIndex, first - 1);
            CHECK_EQ(name, target[first - 1], chunk_last_value(i - 1));
        }
    }

    CHECK_EQ("인덱스 2~15 전 구간 에러 없음", stub_error_count(), 0);

    // ------------------------------------------------------------------
    TEST_GROUP("0x66 하위 1 - 16비트 조립 (인덱스 8)");

    stub_reset();
    feed_upto(pkt, 7);

    make_live(pkt, en__allParameter);
    pkt[2]  = 8;
    pkt[3]  = 0x07;  pkt[4]  = 0x08;  // 1800 - 상한
    pkt[5]  = 0x00;  pkt[6]  = 0x00;  // 0    - 하한
    pkt[7]  = 0x00;  pkt[8]  = 0xFF;  // 255  - 하위 바이트만
    pkt[9]  = 0x01;  pkt[10] = 0x00;  // 256  - 상위 바이트만
    pkt[11] = 0x03;  pkt[12] = 0xE8;  // 1000
    pkt[13] = 0x00;  pkt[14] = 0x01;  // 1
    pkt[15] = 0x02;  pkt[16] = 0x2B;  // 555
    pkt[17] = 0x07;  pkt[18] = 0x08;  // 1800
    tdc_ble_map_stim_live(pkt);
    p = tdc_ble_mapping_get_packet();

    CHECK_EQ("0x0708 -> 1800", p->tdc_isd_map_live_step.T_level_uA[0], 1800);
    CHECK_EQ("0x0000 -> 0", p->tdc_isd_map_live_step.T_level_uA[1], 0);
    CHECK_EQ("0x00FF -> 255 (하위만)", p->tdc_isd_map_live_step.T_level_uA[2], 255);
    CHECK_EQ("0x0100 -> 256 (상위만)", p->tdc_isd_map_live_step.T_level_uA[3], 256);
    CHECK_EQ("0x03E8 -> 1000", p->tdc_isd_map_live_step.T_level_uA[4], 1000);
    CHECK_EQ("0x0001 -> 1", p->tdc_isd_map_live_step.T_level_uA[5], 1);
    CHECK_EQ("0x022B -> 555", p->tdc_isd_map_live_step.T_level_uA[6], 555);
    CHECK_EQ("0x0708 -> 1800 (끝)", p->tdc_isd_map_live_step.T_level_uA[7], 1800);
    CHECK_EQ("에러 없음", stub_error_count(), 0);

    // ------------------------------------------------------------------
    TEST_GROUP("0x66 하위 1 - 인덱스 2 범위 경계 (1~100)");

    stub_reset();
    feed_upto(pkt, 1);
    make_all_param_chunk(pkt, 0);
    pkt[3] = 100;  // 상한
    tdc_ble_map_stim_live(pkt);
    CHECK_EQ("상한 100 통과", stub_error_count(), 0);
    CHECK_EQ("100 적재", tdc_ble_mapping_get_packet()->tdc_isd_map_live_step.usableStimulationElectrodIndex[0], 100);

    stub_reset();
    feed_upto(pkt, 1);
    make_all_param_chunk(pkt, 0);
    pkt[3] = 101;  // 초과
    tdc_ble_map_stim_live(pkt);
    CHECK_EQ("101 은 거부", stub_error_count(), 1);
    CHECK_EQ("minor = DATA_Order", stub_error_last_minor(), en__DATA_Order);

    stub_reset();
    feed_upto(pkt, 1);
    make_all_param_chunk(pkt, 0);
    pkt[3] = 0;  // 하한 미만
    tdc_ble_map_stim_live(pkt);
    CHECK_EQ("0 은 거부", stub_error_count(), 1);

    stub_reset();
    feed_upto(pkt, 1);
    make_all_param_chunk(pkt, 0);
    pkt[19] = 0;  // 청크 마지막(17번째) 원소만 위반
    tdc_ble_map_stim_live(pkt);
    CHECK_EQ("마지막 원소 위반도 잡는다", stub_error_count(), 1);

    // ------------------------------------------------------------------
    TEST_GROUP("0x66 하위 1 - 인덱스 8 범위 경계 (0~1800)");

    stub_reset();
    feed_upto(pkt, 7);
    make_all_param_chunk(pkt, 6);
    pkt[3] = 0x07;  pkt[4] = 0x08;  // 1800 - 상한
    tdc_ble_map_stim_live(pkt);
    CHECK_EQ("상한 1800 통과", stub_error_count(), 0);
    CHECK_EQ("1800 적재", tdc_ble_mapping_get_packet()->tdc_isd_map_live_step.T_level_uA[0], 1800);

    stub_reset();
    feed_upto(pkt, 7);
    make_all_param_chunk(pkt, 6);
    pkt[3] = 0x07;  pkt[4] = 0x09;  // 1801 - 초과
    tdc_ble_map_stim_live(pkt);
    CHECK_EQ("1801 은 거부", stub_error_count(), 1);

    // ------------------------------------------------------------------
    TEST_GROUP("0x66 하위 1 - 15단계 순차 완주");

    stub_reset();
    feed_upto(pkt, 14);
    p = tdc_ble_mapping_get_packet();
    CHECK_EQ("14 단계까지는 아직 Standby", p->tdc_isd_map_live_step.subCommand, en__Standby);
    CHECK_EQ("seq = 14", tdc_ble_mapping_get_seq_index(), 14);

    stub_reset();
    feed_upto(pkt, 15);
    p = tdc_ble_mapping_get_packet();

    CHECK_EQ("15 단계 전 구간 에러 없음", stub_error_count(), 0);
    CHECK_EQ("완료 시 subCommand = allParameter", p->tdc_isd_map_live_step.subCommand, en__allParameter);
    CHECK_EQ("완료 후 seq 리셋", tdc_ble_mapping_get_seq_index(), 0);
    CHECK_EQ("audio_input_x_mim[0]", p->tdc_isd_map_live_step.audio_input_x_mim[0], df_minAudioForLogarithm);
    CHECK_EQ("audio_input_x_mim[31]", p->tdc_isd_map_live_step.audio_input_x_mim[31], df_minAudioForLogarithm);
    CHECK_EQ("audio_input_x_max[0]", p->tdc_isd_map_live_step.audio_input_x_max[0], df_maxAudioForLogarithm);
    CHECK_EQ("audio_input_x_max[31]", p->tdc_isd_map_live_step.audio_input_x_max[31], df_maxAudioForLogarithm);
    CHECK_EQ("송신 15회", stub_tx_count(), 15);
    CHECK_EQ("마지막 응답 3바이트", stub_tx_len(), 3);

    expected_tx[0] = (uint8_t) en__mapping_live_stimulation;
    expected_tx[1] = (uint8_t) en__allParameter;
    expected_tx[2] = 15;
    CHECK_BYTES("마지막 응답 = [명령, 하위1, 인덱스15]", stub_tx_buffer(), expected_tx, 3);

    // 15 단계를 마친 뒤에도 전 구간 값이 남아 있는지 (마지막 청크가 앞을 덮지 않았는지)
    CHECK_EQ("자극 전극 [0] 유지", p->tdc_isd_map_live_step.usableStimulationElectrodIndex[0], 1);
    CHECK_EQ("자극 전극 [31] 유지", p->tdc_isd_map_live_step.usableStimulationElectrodIndex[31], 32);
    CHECK_EQ("기준 전극 [0] 유지", p->tdc_isd_map_live_step.usableReferenceElectrodIndex[0], 33);
    CHECK_EQ("기준 전극 [31] 유지", p->tdc_isd_map_live_step.usableReferenceElectrodIndex[31], 64);
    CHECK_EQ("밴드 순서 [0] 유지", p->tdc_isd_map_live_step.CIS_FreqBandOrder[0], 65);
    CHECK_EQ("밴드 순서 [31] 유지", p->tdc_isd_map_live_step.CIS_FreqBandOrder[31], 96);
    CHECK_EQ("T 레벨 [0] 유지", p->tdc_isd_map_live_step.T_level_uA[0], 100);
    CHECK_EQ("T 레벨 [31] 유지", p->tdc_isd_map_live_step.T_level_uA[31], 470);
    CHECK_EQ("C 레벨 [0] 유지", p->tdc_isd_map_live_step.C_level_uA[0], 500);
    CHECK_EQ("C 레벨 [31] 유지", p->tdc_isd_map_live_step.C_level_uA[31], 870);
    CHECK_EQ("인덱스 1 스칼라 유지", p->tdc_isd_map_live_step.numFrequencyBand, 16);

    // ------------------------------------------------------------------
    TEST_GROUP("0x66 하위 1 - 데이터 인덱스 상한 방어");

    // 데이터 인덱스 카운터는 map_flash 와 공유된다(접근자 1쌍).
    // flash 는 인덱스를 34 까지 쓰므로 카운터가 15 이상인 상태가 실재하고,
    // 그때 0x66 하위 1 이 도착하면 16 이상이 파싱부에 도달한다.
    stub_reset();
    tdc_ble_mapping_set_seq_index(15);
    make_live(pkt, en__allParameter);
    pkt[2] = 16;
    tdc_ble_map_stim_live(pkt);
    CHECK_EQ("인덱스 16 은 거부", stub_error_count(), 1);
    CHECK_EQ("minor = DATA_Order", stub_error_last_minor(), en__DATA_Order);
    CHECK_EQ("거부 후 seq 리셋", tdc_ble_mapping_get_seq_index(), 0);

    stub_reset();
    tdc_ble_mapping_set_seq_index(34);  // flash 의 최대 데이터 인덱스
    make_live(pkt, en__allParameter);
    pkt[2] = 35;
    tdc_ble_map_stim_live(pkt);
    CHECK_EQ("인덱스 35 도 거부", stub_error_count(), 1);
    CHECK_EQ("거부 후 seq 리셋", tdc_ble_mapping_get_seq_index(), 0);

    // ------------------------------------------------------------------
    TEST_GROUP("공통");

    CHECK_EQ("저장소 오버플로 없음", stub_repository_overflow(), 0);

    TEST_SUMMARY();
}
