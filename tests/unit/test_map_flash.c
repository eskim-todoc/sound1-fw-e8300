// 매핑 프로토콜 0x68~0x71 (슬롯·맵 데이터 읽기/쓰기/삭제/복구) 파싱 테스트.
//
// 초점은 멀티 패킷 명령(0x68 · 0x6C · 0x6D)의 데이터 인덱스 연속성이다.
// 이 경로는 tdc_ble_mapping_get/set_seq_index() 접근자를 거치며,
// 단계_1b-2 에서 함수 지역 static 을 파일 스코프로 올린 부분이라
// 회귀 위험이 가장 큰 곳이다.

#include <string.h>
#include <stdint.h>

#include <tdc_ble_mapping.h>
#include <tdc_sys_error.h>
#include <tdc_isd_map_data.h>
#include <tdc_ble_map_flash.h>

#include "tdc_test.h"
#include "stub_ble.h"

static void make_packet(uint8_t *pkt, int command)
{
    memset(pkt, 0, BLE_DataPacketSize + 1);
    pkt[0] = (uint8_t) command;
}

// 멀티 패킷 명령의 한 조각을 만든다. pkt[1] 이 데이터 인덱스다.
static void make_chunk(uint8_t *pkt, int command, int dataIndex)
{
    make_packet(pkt, command);
    pkt[1] = (uint8_t) dataIndex;
}

int main(void)
{
    uint8_t             pkt[BLE_DataPacketSize + 1];
    ST__MAPPING_PACKET *p;
    int                *repo;

    // ------------------------------------------------------------------
    TEST_GROUP("0x69 원래 내부기 정보 읽기 - 헤더 only");

    stub_reset();
    make_packet(pkt, en__mapping_read_original_ISD_N_USER);
    tdc_ble_cmd_0x69_read_original_isd_user();
    p = tdc_ble_mapping_get_packet();

    CHECK_EQ("fetched_command", p->fetched_command, en__mapping_read_original_ISD_N_USER);
    CHECK_EQ("에러 없음", stub_error_count(), 0);
    CHECK_EQ("송신 없음", stub_tx_count(), 0);

    // ------------------------------------------------------------------
    TEST_GROUP("0x6A 슬롯 데이터 읽기 - 슬롯 범위 (1~4)");

    stub_reset();
    make_packet(pkt, en__mapping_read_SlotData_ISD_N_USER);
    pkt[1] = 1;
    tdc_ble_cmd_0x6A_read_slot_data(pkt);
    p = tdc_ble_mapping_get_packet();
    CHECK_EQ("슬롯 1 통과", stub_error_count(), 0);
    CHECK_EQ("slot_index 적재", p->ReadWriteMapData_Flash.slot_index, 1);
    CHECK_EQ("map_index 는 0 으로 초기화", p->ReadWriteMapData_Flash.map_index, 0);
    CHECK_EQ("fetched_command", p->fetched_command, en__mapping_read_SlotData_ISD_N_USER);

    stub_reset();
    make_packet(pkt, en__mapping_read_SlotData_ISD_N_USER);
    pkt[1] = MaxNumUser;
    tdc_ble_cmd_0x6A_read_slot_data(pkt);
    CHECK_EQ("슬롯 4 통과", stub_error_count(), 0);

    stub_reset();
    make_packet(pkt, en__mapping_read_SlotData_ISD_N_USER);
    pkt[1] = MaxNumUser + 1;
    tdc_ble_cmd_0x6A_read_slot_data(pkt);
    CHECK_EQ("슬롯 5 는 거부", stub_error_count(), 1);
    CHECK_EQ("minor = OutOfDataRange", stub_error_last_minor(), en__OutOfDataRange);

    stub_reset();
    make_packet(pkt, en__mapping_read_SlotData_ISD_N_USER);
    pkt[1] = 0;
    tdc_ble_cmd_0x6A_read_slot_data(pkt);
    CHECK_EQ("슬롯 0 은 거부", stub_error_count(), 1);

    // ------------------------------------------------------------------
    TEST_GROUP("0x6B 맵 데이터 읽기 - 슬롯·맵 범위");

    stub_reset();
    make_packet(pkt, en__mapping_read_Mapdata_STIMUL_PARA);
    pkt[1] = 2;  // slot
    pkt[2] = 3;  // map
    tdc_ble_cmd_0x6B_read_map_data(pkt);
    p = tdc_ble_mapping_get_packet();
    CHECK_EQ("정상 통과", stub_error_count(), 0);
    CHECK_EQ("slot_index", p->ReadWriteMapData_Flash.slot_index, 2);
    CHECK_EQ("map_index", p->ReadWriteMapData_Flash.map_index, 3);

    stub_reset();
    make_packet(pkt, en__mapping_read_Mapdata_STIMUL_PARA);
    pkt[1] = 2;
    pkt[2] = MaxNumMap + 1;
    tdc_ble_cmd_0x6B_read_map_data(pkt);
    CHECK_EQ("맵 5 는 거부", stub_error_count(), 1);

    stub_reset();
    make_packet(pkt, en__mapping_read_Mapdata_STIMUL_PARA);
    pkt[1] = 0;
    pkt[2] = 1;
    tdc_ble_cmd_0x6B_read_map_data(pkt);
    CHECK_EQ("슬롯 0 은 거부", stub_error_count(), 1);

    // ------------------------------------------------------------------
    TEST_GROUP("0x68 원래 내부기 정보 쓰기 - 데이터 인덱스 연속성");

    // 인덱스 1 -> 2 순서로 오면 정상이다.
    stub_reset();
    make_chunk(pkt, en__mapping_write_original_ISD_N_USER, 1);
    pkt[2] = 0x25;  // 제조번호 년
    pkt[3] = 0x31;  // 월+모델
    pkt[4] = 0x01;  // 시리얼 상위
    pkt[5] = 0xF4;  // 시리얼 하위 -> 500
    pkt[6] = 1;     // 수술 위치 (1~2 만 유효)
    tdc_ble_cmd_0x68_write_original_isd_user(pkt);

    CHECK_EQ("인덱스 1 에러 없음", stub_error_count(), 0);
    CHECK_EQ("seq_index = 1", tdc_ble_mapping_get_seq_index(), 1);
    CHECK_EQ("중간 패킷은 루프백 송신", stub_tx_count(), 1);
    CHECK_EQ("루프백 길이 2", stub_tx_len(), 2);
    {
        const uint8_t expect[2] = { en__mapping_write_original_ISD_N_USER, 1 };
        CHECK_BYTES("루프백 [명령, 인덱스]", stub_tx_buffer(), expect, 2);
    }

    repo = stub_isd_info_repository();
    CHECK_EQ("제조번호 년 적재", repo[0], 0x25);
    CHECK_EQ("시리얼 16비트 조립", repo[2], 500);
    p = tdc_ble_mapping_get_packet();
    CHECK_EQ("수술 위치", p->rx_orignal_ISD_info.location, 1);

    // 이어서 인덱스 2 (마지막 패킷) -> 명령 확정
    make_chunk(pkt, en__mapping_write_original_ISD_N_USER, 2);
    tdc_ble_cmd_0x68_write_original_isd_user(pkt);

    CHECK_EQ("인덱스 2 에러 없음", stub_error_count(), 0);
    CHECK_EQ("마지막 패킷은 seq 리셋", tdc_ble_mapping_get_seq_index(), 0);
    CHECK_EQ("마지막 패킷은 루프백 없음", stub_tx_count(), 1);
    p = tdc_ble_mapping_get_packet();
    CHECK_EQ("command 확정", p->command, en__mapping_write_original_ISD_N_USER);

    // ------------------------------------------------------------------
    TEST_GROUP("0x68 - 데이터 인덱스 순서 위반");

    stub_reset();
    make_chunk(pkt, en__mapping_write_original_ISD_N_USER, 2);  // 1 을 건너뛰고 2
    tdc_ble_cmd_0x68_write_original_isd_user(pkt);

    CHECK_EQ("에러 1건", stub_error_count(), 1);
    CHECK_EQ("minor = DATA_Order", stub_error_last_minor(), en__DATA_Order);
    CHECK_EQ("major = BLE_PROTOCOL_ERROR", stub_error_last_major(), en__EN__BLE_PROTOCOL_ERROR);
    CHECK_EQ("seq 리셋", tdc_ble_mapping_get_seq_index(), 0);

    // 인덱스 1 은 언제나 시퀀스를 새로 연다
    stub_reset();
    tdc_ble_mapping_set_seq_index(7);  // 엉뚱한 상태
    make_chunk(pkt, en__mapping_write_original_ISD_N_USER, 1);
    pkt[6] = 1;
    tdc_ble_cmd_0x68_write_original_isd_user(pkt);
    CHECK_EQ("인덱스 1 은 상태를 무시하고 시작", stub_error_count(), 0);
    CHECK_EQ("seq_index = 1", tdc_ble_mapping_get_seq_index(), 1);

    // ------------------------------------------------------------------
    TEST_GROUP("0x68 - 수술 위치 범위 (1~2)");

    stub_reset();
    make_chunk(pkt, en__mapping_write_original_ISD_N_USER, 1);
    pkt[6] = 2;
    tdc_ble_cmd_0x68_write_original_isd_user(pkt);
    CHECK_EQ("위치 2 통과", stub_error_count(), 0);

    stub_reset();
    make_chunk(pkt, en__mapping_write_original_ISD_N_USER, 1);
    pkt[6] = 3;
    tdc_ble_cmd_0x68_write_original_isd_user(pkt);
    CHECK_EQ("위치 3 은 거부", stub_error_count(), 1);
    CHECK_EQ("minor = OutOfDataRange", stub_error_last_minor(), en__OutOfDataRange);

    stub_reset();
    make_chunk(pkt, en__mapping_write_original_ISD_N_USER, 1);
    pkt[6] = 0;
    tdc_ble_cmd_0x68_write_original_isd_user(pkt);
    CHECK_EQ("위치 0 은 거부", stub_error_count(), 1);

    // ------------------------------------------------------------------
    TEST_GROUP("0x6C 슬롯 데이터 쓰기 - 슬롯 범위와 연속성");

    stub_reset();
    make_chunk(pkt, en__mapping_write_SlotData_ISD_N_USER, 1);
    pkt[2] = 3;  // 슬롯 번호
    pkt[7] = 1;  // 수술 위치
    tdc_ble_cmd_0x6C_write_slot_data(pkt);
    p = tdc_ble_mapping_get_packet();
    CHECK_EQ("에러 없음", stub_error_count(), 0);
    CHECK_EQ("slot_index", p->ReadWriteMapData_Flash.slot_index, 3);
    CHECK_EQ("루프백 송신", stub_tx_count(), 1);

    stub_reset();
    make_chunk(pkt, en__mapping_write_SlotData_ISD_N_USER, 1);
    pkt[2] = MaxNumUser + 1;
    tdc_ble_cmd_0x6C_write_slot_data(pkt);
    CHECK_EQ("슬롯 5 는 거부", stub_error_count(), 1);
    CHECK_EQ("minor = OutOfDataRange", stub_error_last_minor(), en__OutOfDataRange);

    // 3패킷 시퀀스를 끝까지 진행하면 명령이 확정된다
    stub_reset();
    make_chunk(pkt, en__mapping_write_SlotData_ISD_N_USER, 1);
    pkt[2] = 1;
    pkt[7] = 1;
    tdc_ble_cmd_0x6C_write_slot_data(pkt);

    make_chunk(pkt, en__mapping_write_SlotData_ISD_N_USER, 2);
    pkt[15] = '1';  // 패스키 4자리 (인덱스 30~33 -> repo[29..32])
    pkt[16] = '2';
    pkt[17] = 'a';
    pkt[18] = 'Z';
    pkt[19] = 1;    // 맵 번호 (1~4)
    tdc_ble_cmd_0x6C_write_slot_data(pkt);
    CHECK_EQ("인덱스 2 에러 없음", stub_error_count(), 0);

    make_chunk(pkt, en__mapping_write_SlotData_ISD_N_USER, 3);
    pkt[2] = 2;  // 자극 볼륨
    pkt[3] = 1;  // 마이크 감도
    pkt[4] = 1;  // LED
    pkt[5] = 1;  // 자극 알림
    pkt[6] = 0;  // 텔레코일 (잠수함 패치로 0 허용)
    tdc_ble_cmd_0x6C_write_slot_data(pkt);

    CHECK_EQ("3패킷 완주 에러 없음", stub_error_count(), 0);
    CHECK_EQ("완주 후 seq 리셋", tdc_ble_mapping_get_seq_index(), 0);
    p = tdc_ble_mapping_get_packet();
    CHECK_EQ("command 확정", p->command, en__mapping_write_SlotData_ISD_N_USER);

    repo = stub_isd_info_repository();
    CHECK_EQ("텔레코일은 항상 2 로 강제", repo[38], 2);
    CHECK_EQ("BLE On/Off 는 항상 1", repo[39], 1);

    // ------------------------------------------------------------------
    TEST_GROUP("0x6C - 패스키 문자 범위");

    stub_reset();
    make_chunk(pkt, en__mapping_write_SlotData_ISD_N_USER, 1);
    pkt[2] = 1;
    pkt[7] = 1;
    tdc_ble_cmd_0x6C_write_slot_data(pkt);

    make_chunk(pkt, en__mapping_write_SlotData_ISD_N_USER, 2);
    pkt[15] = '!';  // 영숫자가 아님
    pkt[16] = '2';
    pkt[17] = '3';
    pkt[18] = '4';
    pkt[19] = 1;
    tdc_ble_cmd_0x6C_write_slot_data(pkt);
    CHECK_EQ("패스키에 기호는 거부", stub_error_count(), 1);

    // ------------------------------------------------------------------
    TEST_GROUP("0x6D 맵 데이터 쓰기 - 자극 파라미터 범위");

    stub_reset();
    make_chunk(pkt, en__mapping_write_Mapdata_STIMUL_PARA, 1);
    pkt[2]  = 1;   // 슬롯
    pkt[3]  = 1;   // 맵
    pkt[10] = 2;   // i=6 자극 기법 (1~3)
    pkt[11] = 0;   // i=7 선행 펄스 위상 (0~1)
    pkt[12] = 3;   // i=8 자극 모드 (1~6)
    pkt[13] = 20;  // i=9 펄스 위상 폭 (13~255)
    pkt[14] = 16;  // i=10 주파수 밴드 수 (1~32)
    pkt[15] = 8;   // i=11 알람 채널 (1~32)
    pkt[16] = 0x01;
    pkt[17] = 0x2C;  // 알람 크기 300 (0~1800)
    tdc_ble_cmd_0x6D_write_map_data(pkt);

    CHECK_EQ("에러 없음", stub_error_count(), 0);
    CHECK_EQ("seq_index = 1", tdc_ble_mapping_get_seq_index(), 1);
    p = tdc_ble_mapping_get_packet();
    CHECK_EQ("slot_index", p->ReadWriteMapData_Flash.slot_index, 1);
    CHECK_EQ("map_index", p->ReadWriteMapData_Flash.map_index, 1);

    repo = stub_stimul_para_repository();
    CHECK_EQ("자극 기법 적재", repo[6], 2);
    CHECK_EQ("펄스 위상 폭 적재", repo[9], 20);
    CHECK_EQ("알람 크기 16비트 조립", repo[12], 300);

    // 자극 기법 범위 초과
    stub_reset();
    make_chunk(pkt, en__mapping_write_Mapdata_STIMUL_PARA, 1);
    pkt[2]  = 1;
    pkt[3]  = 1;
    pkt[10] = 4;   // 1~3 초과
    pkt[11] = 0;
    pkt[12] = 3;
    pkt[13] = 20;
    pkt[14] = 16;
    pkt[15] = 8;
    tdc_ble_cmd_0x6D_write_map_data(pkt);
    CHECK_EQ("자극 기법 4 는 거부", stub_error_count(), 1);

    // 펄스 위상 폭 하한 미만
    stub_reset();
    make_chunk(pkt, en__mapping_write_Mapdata_STIMUL_PARA, 1);
    pkt[2]  = 1;
    pkt[3]  = 1;
    pkt[10] = 2;
    pkt[11] = 0;
    pkt[12] = 3;
    pkt[13] = 12;  // 13 미만
    pkt[14] = 16;
    pkt[15] = 8;
    tdc_ble_cmd_0x6D_write_map_data(pkt);
    CHECK_EQ("펄스 폭 12 는 거부", stub_error_count(), 1);

    // 데이터 인덱스 2 - 전극 번호 18개 (1~100)
    stub_reset();
    make_chunk(pkt, en__mapping_write_Mapdata_STIMUL_PARA, 1);
    pkt[2]  = 1;
    pkt[3]  = 1;
    pkt[10] = 2;
    pkt[11] = 0;
    pkt[12] = 3;
    pkt[13] = 20;
    pkt[14] = 16;
    pkt[15] = 8;
    tdc_ble_cmd_0x6D_write_map_data(pkt);

    make_chunk(pkt, en__mapping_write_Mapdata_STIMUL_PARA, 2);
    {
        int i;
        for (i = 0; i < 18; i++)
        {
            pkt[2 + i] = (uint8_t) (i + 1);  // 1~18, 전부 유효
        }
    }
    tdc_ble_cmd_0x6D_write_map_data(pkt);
    CHECK_EQ("전극 18개 에러 없음", stub_error_count(), 0);
    repo = stub_stimul_para_repository();
    CHECK_EQ("전극 첫 값", repo[13], 1);
    CHECK_EQ("전극 마지막 값", repo[30], 18);

    // ------------------------------------------------------------------
    TEST_GROUP("0x6E 슬롯 삭제 - 슬롯 범위");

    stub_reset();
    make_packet(pkt, en__mapping_erase_SlotData_manufacture);
    pkt[1] = 2;
    tdc_ble_cmd_0x6E_erase_slot(en__mapping_erase_SlotData_manufacture, pkt);
    p = tdc_ble_mapping_get_packet();
    CHECK_EQ("슬롯 2 통과", stub_error_count(), 0);
    CHECK_EQ("fetched_command", p->fetched_command, en__mapping_erase_SlotData_manufacture);

    stub_reset();
    make_packet(pkt, en__mapping_erase_SlotData_manufacture);
    pkt[1] = 5;
    tdc_ble_cmd_0x6E_erase_slot(en__mapping_erase_SlotData_manufacture, pkt);
    p = tdc_ble_mapping_get_packet();
    CHECK_EQ("슬롯 5 는 거부", stub_error_count(), 1);
    CHECK_EQ("거부 시 fetched_command 를 IDLE 로 되돌림", p->fetched_command, en__mapping_IDLE);

    // ------------------------------------------------------------------
    TEST_GROUP("0x6F 맵 삭제 - 슬롯·맵 각각 검사");

    stub_reset();
    make_packet(pkt, en__mapping_erase_mapData_STIMUL_PARA);
    pkt[1] = 1;
    pkt[2] = 4;
    tdc_ble_cmd_0x6F_erase_map(en__mapping_erase_mapData_STIMUL_PARA, pkt);
    CHECK_EQ("슬롯 1 맵 4 통과", stub_error_count(), 0);

    stub_reset();
    make_packet(pkt, en__mapping_erase_mapData_STIMUL_PARA);
    pkt[1] = 5;  // 슬롯 위반
    pkt[2] = 5;  // 맵 위반
    tdc_ble_cmd_0x6F_erase_map(en__mapping_erase_mapData_STIMUL_PARA, pkt);
    CHECK_EQ("둘 다 위반이면 에러 2건", stub_error_count(), 2);

    // ------------------------------------------------------------------
    TEST_GROUP("0x70 / 0x71 복구");

    stub_reset();
    make_packet(pkt, en__mapping_recover_mppingData_exceptSlot_1);
    pkt[1] = 2;
    pkt[2] = 3;
    tdc_ble_cmd_0x70_recover_except_slot1(en__mapping_recover_mppingData_exceptSlot_1, pkt);
    p = tdc_ble_mapping_get_packet();
    CHECK_EQ("0x70 fetched_command", p->fetched_command, en__mapping_recover_mppingData_exceptSlot_1);
    CHECK_EQ("0x70 slot_index", p->ReadWriteMapData_Flash.slot_index, 2);
    CHECK_EQ("0x70 map_index", p->ReadWriteMapData_Flash.map_index, 3);
    CHECK_EQ("0x70 은 범위 검사가 없다", stub_error_count(), 0);

    stub_reset();
    tdc_ble_cmd_0x71_recover_all(en__mapping_recover_ALL_SlotData_ManufactureData);
    p = tdc_ble_mapping_get_packet();
    CHECK_EQ("0x71 fetched_command", p->fetched_command, en__mapping_recover_ALL_SlotData_ManufactureData);
    CHECK_EQ("0x71 에러 없음", stub_error_count(), 0);

    // ------------------------------------------------------------------
    TEST_GROUP("공통");

    CHECK_EQ("저장소 오버플로 없음", stub_repository_overflow(), 0);

    TEST_SUMMARY();
}
