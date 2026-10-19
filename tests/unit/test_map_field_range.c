// 맵 데이터 자극 파라미터 필드 범위 검사 테스트 (위험_4).
//
// 매핑 앱(0x4B 계열)과 리모콘(0x4D)이 같은 저장소에 맵을 쓰는데
// 2026-08-06 이전에는 매핑 앱 경로에만 검사가 있었다. 검사를
// tdc_ble_map_field_range.c 로 모아 양쪽이 같은 것을 쓰게 했고,
// 이 파일이 그 경계값을 고정한다.
//
// 이 모듈은 순수 함수만 갖는다. isd/ 의 의존 사슬(shm · i2c · FPGA)에
// 닿지 않으므로 스텁 없이 단독으로 링크된다.

#include <tdc_ble_map_field_range.h>

#include "tdc_test.h"

int main(void)
{
    // ------------------------------------------------------------------
    TEST_GROUP("자극 기법 (필드 6) - 1~3");

    CHECK("0 거부", !tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_STRATEGY, 0));
    CHECK("1 허용", tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_STRATEGY, 1));
    CHECK("3 허용", tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_STRATEGY, 3));
    CHECK("4 거부", !tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_STRATEGY, 4));

    // ------------------------------------------------------------------
    TEST_GROUP("선행 펄스 위상 (필드 7) - 0~1");

    CHECK("-1 거부", !tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_FIRST_PHASE, -1));
    CHECK("0 허용", tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_FIRST_PHASE, 0));
    CHECK("1 허용", tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_FIRST_PHASE, 1));
    CHECK("2 거부", !tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_FIRST_PHASE, 2));

    // ------------------------------------------------------------------
    TEST_GROUP("자극 모드 (필드 8) - 1~6, en__referenceNA(0) 제외");

    CHECK("0 (referenceNA) 거부", !tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_STIM_MODE, 0));
    CHECK("1 (monopolr_body) 허용", tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_STIM_MODE, 1));
    CHECK("6 (semi_simultaneously) 허용", tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_STIM_MODE, 6));
    CHECK("7 거부", !tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_STIM_MODE, 7));

    // ------------------------------------------------------------------
    TEST_GROUP("펄스 위상 폭 (필드 9) - 13~255");

    CHECK("12 거부 (FPGA 최소 미만)", !tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_PULSE_WIDTH, 12));
    CHECK("13 허용 (FPGA_pulsePhaseWidth_minimum)", tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_PULSE_WIDTH, 13));
    CHECK("255 허용", tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_PULSE_WIDTH, 255));
    CHECK("256 거부", !tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_PULSE_WIDTH, 256));

    // ------------------------------------------------------------------
    TEST_GROUP("주파수 밴드 수 (필드 10) - 1~32  [위험_4 핵심]");

    CHECK("0 거부", !tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_NUM_BAND, 0));
    CHECK("1 허용", tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_NUM_BAND, 1));
    CHECK("32 허용 (df_MaxNumOfElectrode)", tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_NUM_BAND, 32));
    CHECK("33 거부", !tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_NUM_BAND, 33));
    CHECK("99 거부", !tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_NUM_BAND, 99));
    CHECK("255 거부 (바이트 최대)", !tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_NUM_BAND, 255));

    // ------------------------------------------------------------------
    TEST_GROUP("알람용 자극 채널 번호 (필드 11) - 1~32");

    CHECK("0 거부", !tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_ALARM_CH, 0));
    CHECK("1 허용", tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_ALARM_CH, 1));
    CHECK("32 허용", tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_ALARM_CH, 32));
    CHECK("33 거부", !tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_ALARM_CH, 33));

    // ------------------------------------------------------------------
    TEST_GROUP("매핑 일자 (필드 0~5) - 검사하지 않는다");

    CHECK("필드 0 극단값 통과", tdc_ble_map_field_stimul_para_in_range(0, 9999));
    CHECK("필드 1 통과", tdc_ble_map_field_stimul_para_in_range(1, 0));
    CHECK("필드 2 통과", tdc_ble_map_field_stimul_para_in_range(2, -5));
    CHECK("필드 3 통과", tdc_ble_map_field_stimul_para_in_range(3, 255));
    CHECK("필드 4 통과", tdc_ble_map_field_stimul_para_in_range(4, 60));
    CHECK("필드 5 통과", tdc_ble_map_field_stimul_para_in_range(5, 60));
    CHECK("범위 밖 인덱스도 통과", tdc_ble_map_field_stimul_para_in_range(12, 12345));

    // ------------------------------------------------------------------
    TEST_GROUP("전극번호 계열 - 1~100 (99 는 미사용 센티널이라 통과)");

    CHECK("0 거부", !tdc_ble_map_field_electrode_in_range(0));
    CHECK("1 허용", tdc_ble_map_field_electrode_in_range(1));
    CHECK("32 허용", tdc_ble_map_field_electrode_in_range(32));
    CHECK("99 허용 (미사용 센티널)", tdc_ble_map_field_electrode_in_range(99));
    CHECK("100 허용", tdc_ble_map_field_electrode_in_range(100));
    CHECK("101 거부", !tdc_ble_map_field_electrode_in_range(101));
    CHECK("255 거부", !tdc_ble_map_field_electrode_in_range(255));

    // ------------------------------------------------------------------
    TEST_GROUP("T · C 레벨과 자극 알람 크기 - 0~1800 uA");

    CHECK("-1 거부", !tdc_ble_map_field_level_uA_in_range(-1));
    CHECK("0 허용 (하한 0 은 26.02.25 수정분)", tdc_ble_map_field_level_uA_in_range(0));
    CHECK("900 허용", tdc_ble_map_field_level_uA_in_range(900));
    CHECK("1800 허용", tdc_ble_map_field_level_uA_in_range(1800));
    CHECK("1801 거부", !tdc_ble_map_field_level_uA_in_range(1801));
    CHECK("65535 거부 (2바이트 최대)", !tdc_ble_map_field_level_uA_in_range(65535));

    // ------------------------------------------------------------------
    TEST_GROUP("위험_4 회귀 - 리모콘이 보내던 무검증 값들");

    // 2026-08-06 이전 리모콘 0x4D 는 아래 값들을 그대로 통과시켜
    // 플래시 맵에 기록했고, numFrequencyBand 는 루프 상한이라
    // isd/tdc_isd_stim_para_setting.c 가 32칸 배열을 넘어 읽었다.
    CHECK("밴드 수 33 이 이제 막힌다", !tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_NUM_BAND, 33));
    CHECK("밴드 수 224 (구조체 밖) 이 막힌다", !tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_NUM_BAND, 224));
    CHECK("자극 모드 0 이 막힌다", !tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_STIM_MODE, 0));
    CHECK("펄스 폭 0 이 막힌다", !tdc_ble_map_field_stimul_para_in_range(TDC_BLE_MAP_FIELD_PULSE_WIDTH, 0));
    CHECK("전극번호 101 이 막힌다", !tdc_ble_map_field_electrode_in_range(101));
    CHECK("레벨 65535 가 막힌다", !tdc_ble_map_field_level_uA_in_range(65535));

    TEST_SUMMARY();
}
