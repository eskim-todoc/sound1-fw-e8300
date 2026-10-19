// 리모콘 프로토콜 0x40~0x59 파싱·처리 테스트.
//
// 목적은 사양 준수 확인이 아니라 현재 동작을 고정하는 것이다(회귀 안전망).
// 이월_2(tdc_ble_remote_fetch_packet / tdc_ble_remote_step 분해)에서
// 결과가 같은지 확인하는 데 쓴다.
//
// 착수 시 전제가 반전됐다 - 이월_2 는 "fetch_packet 약 1,340줄" 로 적혀 있었으나
// 실제로는 두 함수다.
//   tdc_ble_remote_fetch_packet()  :51~:620   570줄  파싱만
//   tdc_ble_remote_step()          :639~:1390 752줄  실행·응답
// 파서와 실행이 이미 갈려 있으므로 테스트도 두 층으로 나눈다.
//   층_1 파싱   - remoteDataPacket(전역)을 직접 단언
//   층_2 실행   - step() 후 SPI TX 바이트와 공유메모리 값을 단언
//
// step() 은 tdc_ble_remote_is_passkey_match() 게이트(:770) 안쪽에서 명령을
// 분기하므로, 테스트는 set_passkey_match() 로 문을 열고 시작한다.

#include <string.h>
#include <stdint.h>

#include <tdc_ble_remote.h>
#include <tdc_ble_protocol.h>
#include <tdc_stim_definitions.h>
#include <tdc_sys_error.h>

#include "tdc_test.h"
#include "stub_ble.h"
#include "stub_remote.h"

// 파싱 결과가 담기는 전역 (tdc_ble_remote.c:24)
extern ST__REMOTECONTROL_PACKET remoteDataPacket;

// 패킷을 0 으로 채우고 헤더만 세운다.
//
// tdc_ble_remote_clear_command() 를 반드시 부른다. remoteDataPacket 은
// tdc_ble_remote.c 의 전역이라 stub_reset() 이 건드리지 않으며, 직전 명령이
// IDLE 이 아니면 fetch_packet:74 의 "이전 명령 미완료" 가드가 새 명령을
// 통째로 버린다(아래 전용 테스트 그룹에서 그 동작 자체를 고정한다).
static void make_packet(uint8_t *pkt, int command)
{
    stub_reset();
    stub_remote_reset();
    tdc_ble_remote_clear_passkey_match();
    tdc_ble_remote_clear_command();
    memset(pkt, 0, BLE_DataPacketSize + 1);
    pkt[0] = (uint8_t) command;
}

// 패스키 문을 열고 명령을 실행한다. 리모콘 명령의 표준 경로.
static void fetch_and_step(const uint8_t *pkt)
{
    tdc_ble_remote_set_passkey_match();
    tdc_ble_remote_fetch_packet(pkt);
    tdc_ble_remote_step(true);
}

int main(void)
{
    uint8_t pkt[BLE_DataPacketSize + 1];

    // ==================================================================
    // 층_1 - fetch_packet 파싱
    // ==================================================================
    TEST_GROUP("파싱 - 일반 명령은 헤더 + 페이로드 19바이트");

    make_packet(pkt, en__remoteControl_adjustStimulationVolume);
    for (int i = 1; i <= todoc_PayloadSize; i++)
    {
        pkt[i] = (uint8_t) (i * 2);
    }
    tdc_ble_remote_fetch_packet(pkt);

    CHECK_EQ("command 적재", remoteDataPacket.command, en__remoteControl_adjustStimulationVolume);
    CHECK_EQ("data[0] (패킷 인덱스 1)", remoteDataPacket.data[0], 2);
    CHECK_EQ("data[1]", remoteDataPacket.data[1], 4);
    CHECK_EQ("data[17]", remoteDataPacket.data[17], 36);
    CHECK_EQ("data[18] (마지막)", remoteDataPacket.data[18], 38);
    CHECK_EQ("파싱만으로는 송신 없음", stub_tx_count(), 0);
    CHECK_EQ("파싱만으로는 에러 없음", stub_error_count(), 0);

    // ------------------------------------------------------------------
    TEST_GROUP("파싱 - 명령 헤더가 그대로 보존된다");

    make_packet(pkt, en__remoteControl_OnOffLED);
    pkt[1] = en__PAYLOAD_ON;
    tdc_ble_remote_fetch_packet(pkt);
    CHECK_EQ("0x49 command", remoteDataPacket.command, en__remoteControl_OnOffLED);
    CHECK_EQ("data[0] = ON", remoteDataPacket.data[0], en__PAYLOAD_ON);

    // ==================================================================
    // 층_2 - step 실행 (0x45 자극 볼륨)
    // ==================================================================
    TEST_GROUP("0x45 자극 볼륨 - 증가");

    make_packet(pkt, en__remoteControl_adjustStimulationVolume);
    pkt[1] = en__PAYLOAD_INCREASE;
    stub_remote_set_stimul_volume(2);
    fetch_and_step(pkt);

    CHECK_EQ("볼륨 2 -> 3", stub_remote_get_stimul_volume(), 3);
    CHECK_EQ("송신 1회", stub_tx_count(), 1);
    CHECK_EQ("응답 길이 2", stub_tx_len(), 2);
    CHECK_EQ("응답[0] = 명령 루프백", stub_tx_buffer()[0], en__remoteControl_adjustStimulationVolume);
    CHECK_EQ("응답[1] = 바뀐 볼륨", stub_tx_buffer()[1], 3);
    CHECK_EQ("에러 없음", stub_error_count(), 0);

    // ------------------------------------------------------------------
    TEST_GROUP("0x45 자극 볼륨 - 상한에서 더 오르지 않는다");

    make_packet(pkt, en__remoteControl_adjustStimulationVolume);
    pkt[1] = en__PAYLOAD_INCREASE;
    stub_remote_set_stimul_volume(df_maxStimulationVloumeLevel);
    fetch_and_step(pkt);

    CHECK_EQ("상한 유지", stub_remote_get_stimul_volume(), df_maxStimulationVloumeLevel);
    CHECK_EQ("상한이어도 응답은 나간다", stub_tx_count(), 1);
    CHECK_EQ("응답[1] = 상한", stub_tx_buffer()[1], df_maxStimulationVloumeLevel);

    // ------------------------------------------------------------------
    TEST_GROUP("0x45 자극 볼륨 - 감소");

    make_packet(pkt, en__remoteControl_adjustStimulationVolume);
    pkt[1] = en__PAYLOAD_DECREASE;
    stub_remote_set_stimul_volume(3);
    fetch_and_step(pkt);

    CHECK_EQ("볼륨 3 -> 2", stub_remote_get_stimul_volume(), 2);
    CHECK_EQ("응답[1]", stub_tx_buffer()[1], 2);

    // ------------------------------------------------------------------
    TEST_GROUP("0x45 자극 볼륨 - 하한 1 에서 더 내려가지 않는다");

    make_packet(pkt, en__remoteControl_adjustStimulationVolume);
    pkt[1] = en__PAYLOAD_DECREASE;
    stub_remote_set_stimul_volume(1);
    fetch_and_step(pkt);

    CHECK_EQ("하한 유지", stub_remote_get_stimul_volume(), 1);

    // ------------------------------------------------------------------
    TEST_GROUP("0x45 자극 볼륨 - 범위 밖 옵션은 거부");

    make_packet(pkt, en__remoteControl_adjustStimulationVolume);
    pkt[1] = 0;  // INCREASE(1) · DECREASE(2) 가 아니다
    stub_remote_set_stimul_volume(2);
    fetch_and_step(pkt);

    CHECK_EQ("볼륨 불변", stub_remote_get_stimul_volume(), 2);
    CHECK_EQ("에러 1건", stub_error_count(), 1);
    CHECK_EQ("minor = OutOfDataRange", stub_error_last_minor(), en__OutOfDataRange);
    CHECK_EQ("정상 응답 없음", stub_tx_count(), 0);

    make_packet(pkt, en__remoteControl_adjustStimulationVolume);
    pkt[1] = 3;
    stub_remote_set_stimul_volume(2);
    fetch_and_step(pkt);
    CHECK_EQ("3 도 거부", stub_error_count(), 1);

    // ==================================================================
    // 층_2 - 0x46 마이크 볼륨
    // ==================================================================
    TEST_GROUP("0x46 마이크 볼륨 - 증가·감소·범위 밖");

    make_packet(pkt, en__remoteControl_adjustMicVolume);
    pkt[1] = en__PAYLOAD_INCREASE;
    stub_remote_set_audio_volume(1);
    fetch_and_step(pkt);
    CHECK_EQ("증가 반영", stub_remote_get_audio_volume(), 2);
    CHECK_EQ("응답[0] = 0x46", stub_tx_buffer()[0], en__remoteControl_adjustMicVolume);
    CHECK_EQ("응답[1] = 바뀐 값", stub_tx_buffer()[1], 2);

    make_packet(pkt, en__remoteControl_adjustMicVolume);
    pkt[1] = en__PAYLOAD_DECREASE;
    stub_remote_set_audio_volume(3);
    fetch_and_step(pkt);
    CHECK_EQ("감소 반영", stub_remote_get_audio_volume(), 2);

    make_packet(pkt, en__remoteControl_adjustMicVolume);
    pkt[1] = 7;
    stub_remote_set_audio_volume(3);
    fetch_and_step(pkt);
    CHECK_EQ("범위 밖 거부", stub_error_count(), 1);
    CHECK_EQ("값 불변", stub_remote_get_audio_volume(), 3);

    // ==================================================================
    // 층_2 - On/Off 3종 (0x47 · 0x48 · 0x49)
    // ==================================================================
    TEST_GROUP("0x47 텔레코일 On/Off");

    make_packet(pkt, en__remoteControl_OnOffTelecoil);
    pkt[1] = en__PAYLOAD_ON;
    fetch_and_step(pkt);
    CHECK_EQ("ON 반영", stub_remote_get_tele_coil_on_off(), en__PAYLOAD_ON);
    CHECK_EQ("응답[0] = 0x47", stub_tx_buffer()[0], en__remoteControl_OnOffTelecoil);
    CHECK_EQ("에러 없음", stub_error_count(), 0);

    make_packet(pkt, en__remoteControl_OnOffTelecoil);
    pkt[1] = en__PAYLOAD_OFF;
    fetch_and_step(pkt);
    CHECK_EQ("OFF 반영", stub_remote_get_tele_coil_on_off(), en__PAYLOAD_OFF);

    make_packet(pkt, en__remoteControl_OnOffTelecoil);
    pkt[1] = 5;
    fetch_and_step(pkt);
    CHECK_EQ("범위 밖 거부", stub_error_count(), 1);

    // ------------------------------------------------------------------
    TEST_GROUP("0x48 자극 인디케이터 On/Off");

    make_packet(pkt, en__remoteControl_OnOffStimulationIndicator);
    pkt[1] = en__PAYLOAD_ON;
    fetch_and_step(pkt);
    CHECK_EQ("ON 반영", stub_remote_get_stimul_indicator_on_off(), en__PAYLOAD_ON);
    CHECK_EQ("응답[0] = 0x48", stub_tx_buffer()[0], en__remoteControl_OnOffStimulationIndicator);

    make_packet(pkt, en__remoteControl_OnOffStimulationIndicator);
    pkt[1] = 0;
    fetch_and_step(pkt);
    CHECK_EQ("범위 밖 거부", stub_error_count(), 1);

    // ------------------------------------------------------------------
    TEST_GROUP("0x49 LED On/Off");

    make_packet(pkt, en__remoteControl_OnOffLED);
    pkt[1] = en__PAYLOAD_OFF;
    fetch_and_step(pkt);
    CHECK_EQ("OFF 반영", stub_remote_get_led_indicator_on_off(), en__PAYLOAD_OFF);
    CHECK_EQ("응답[0] = 0x49", stub_tx_buffer()[0], en__remoteControl_OnOffLED);

    make_packet(pkt, en__remoteControl_OnOffLED);
    pkt[1] = 99;
    fetch_and_step(pkt);
    CHECK_EQ("범위 밖 거부", stub_error_count(), 1);

    // ==================================================================
    // 층_2 - 패스키 게이트
    // ==================================================================
    TEST_GROUP("패스키 미인증 상태에서는 명령이 실행되지 않는다");

    make_packet(pkt, en__remoteControl_adjustStimulationVolume);
    pkt[1] = en__PAYLOAD_INCREASE;
    stub_remote_set_stimul_volume(2);
    tdc_ble_remote_clear_passkey_match();  // 문을 닫는다
    tdc_ble_remote_fetch_packet(pkt);
    tdc_ble_remote_step(true);

    CHECK_EQ("볼륨 불변", stub_remote_get_stimul_volume(), 2);
    CHECK_EQ("송신 없음", stub_tx_count(), 0);
    CHECK_EQ("보안 에러 1건", stub_error_count(), 1);
    CHECK_EQ("minor = NO_SECURITY", stub_error_last_minor(), en__NO_SECURITY);
    CHECK_EQ("명령이 해제된다", remoteDataPacket.command, en__remoteControl_IDLE);

    // ==================================================================
    // 층_1 - 이전 명령 미완료 가드 (fetch_packet:74)
    // ==================================================================
    TEST_GROUP("직전 명령이 IDLE 이 아니면 새 명령을 버린다");

    make_packet(pkt, en__remoteControl_adjustStimulationVolume);
    pkt[1] = en__PAYLOAD_INCREASE;
    tdc_ble_remote_fetch_packet(pkt);  // step 을 부르지 않아 명령이 IDLE 로 돌아가지 않는다
    CHECK_EQ("첫 명령은 적재된다", remoteDataPacket.command, en__remoteControl_adjustStimulationVolume);

    uint8_t pkt2[BLE_DataPacketSize + 1];
    memset(pkt2, 0, sizeof(pkt2));
    pkt2[0] = (uint8_t) en__remoteControl_OnOffLED;
    pkt2[1] = en__PAYLOAD_ON;
    tdc_ble_remote_fetch_packet(pkt2);  // 직전 명령이 살아 있는 채로 새 명령 수신

    // 주의 - :77 주석은 "현재 받은 명령을 수행하지 않는다" 이지만, tempCommand 를
    // IDLE 로 바꾼 뒤 switch 의 default 로 떨어지므로 remoteDataPacket.command 에
    // IDLE 이 대입된다. 즉 새 명령을 버리는 데 그치지 않고 **직전 명령까지 지운다**.
    // 게다가 data[] 에는 새 패킷의 페이로드가 그대로 복사된다.
    // 분해 시 놓치기 쉬운 동작이라 명시적으로 고정한다.
    CHECK_EQ("새 명령도, 직전 명령도 아닌 IDLE 이 된다", remoteDataPacket.command, en__remoteControl_IDLE);
    CHECK_EQ("그래도 페이로드는 새 패킷 것으로 덮인다", remoteDataPacket.data[0], en__PAYLOAD_ON);
    CHECK_EQ("에러 1건", stub_error_count(), 1);
    CHECK_EQ("minor = PreviouCommnadIsNotCompleted", stub_error_last_minor(), en__PreviouCommnadIsNotCompleted);
    CHECK_EQ("에러의 command 는 버려진 새 명령", stub_error_last_command(), en__remoteControl_OnOffLED);

    // ------------------------------------------------------------------
    TEST_GROUP("0x40 패스키 확인은 게이트 밖에서 처리된다");

    make_packet(pkt, en__remoteControl_check_isd_passKey);
    tdc_ble_remote_clear_passkey_match();
    tdc_ble_remote_fetch_packet(pkt);
    tdc_ble_remote_step(true);

    CHECK_EQ("미인증이어도 응답한다", stub_tx_count(), 1);
    CHECK_EQ("응답[0] = 0x40", stub_tx_buffer()[0], en__remoteControl_check_isd_passKey);
    CHECK_EQ("이벤트 로그 1건", stub_remote_event_log_write_count(), 1);

    TEST_SUMMARY();
}
