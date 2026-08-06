#include <tdc_ble_map_field_range.h>

#include <tdc_stim_definitions.h>  // df_MaxNumOfElectrode

/*
  아래 경계값의 1차 정의 위치.

    펄스 위상 폭 하한 13 = FPGA_pulsePhaseWidth_minimum (board/FPGA_ver2_7_0.h:133)
    자극 모드 1~6        = EN___STIMULATION_MODE 에서 en__referenceNA(0) 를 뺀 실제 모드 범위
                           (cfx_link/tdc_shm.h:67~76)
    밴드 수 · 알람 채널 상한 = df_MaxNumOfElectrode (stim/tdc_stim_definitions.h:68)
    전극번호 상한 100    = 미사용 센티널 99 를 통과시키기 위한 여유다.
                           변환 단계에서 99 를 숨기면 하류 범위 검사가 이상을 잡지 못한다.
                           최종 1~32 검사는 isd/tdc_isd_stim_para_setting.c:492 가 한다.
    자극 크기 상한 1800  = 매핑 프로토콜 상한. 0x65 특정자극 파싱과 같은 값이다.

  df_MaxNumOfElectrode 외에는 값을 여기 두되 출처를 남긴다.
  이 파일이 순수 함수만 갖도록 유지해야 호스트 테스트가 단독으로 링크된다.
*/
#define TDC_BLE_MAP_STRATEGY_MIN    1
#define TDC_BLE_MAP_STRATEGY_MAX    3
#define TDC_BLE_MAP_FIRST_PHASE_MIN 0
#define TDC_BLE_MAP_FIRST_PHASE_MAX 1
#define TDC_BLE_MAP_STIM_MODE_MIN   1
#define TDC_BLE_MAP_STIM_MODE_MAX   6
#define TDC_BLE_MAP_PULSE_WIDTH_MIN 13
#define TDC_BLE_MAP_PULSE_WIDTH_MAX 255
#define TDC_BLE_MAP_ELECTRODE_MIN   1
#define TDC_BLE_MAP_ELECTRODE_MAX   100
#define TDC_BLE_MAP_LEVEL_UA_MIN    0
#define TDC_BLE_MAP_LEVEL_UA_MAX    1800

bool tdc_ble_map_field_stimul_para_in_range(int field_index, int value)
{
    bool in_range = true;

    switch (field_index)
    {
        case TDC_BLE_MAP_FIELD_STRATEGY:
        {
            in_range = ((TDC_BLE_MAP_STRATEGY_MIN <= value) && (value <= TDC_BLE_MAP_STRATEGY_MAX));
            break;
        }

        case TDC_BLE_MAP_FIELD_FIRST_PHASE:
        {
            in_range = ((TDC_BLE_MAP_FIRST_PHASE_MIN <= value) && (value <= TDC_BLE_MAP_FIRST_PHASE_MAX));
            break;
        }

        case TDC_BLE_MAP_FIELD_STIM_MODE:
        {
            in_range = ((TDC_BLE_MAP_STIM_MODE_MIN <= value) && (value <= TDC_BLE_MAP_STIM_MODE_MAX));
            break;
        }

        case TDC_BLE_MAP_FIELD_PULSE_WIDTH:
        {
            in_range = ((TDC_BLE_MAP_PULSE_WIDTH_MIN <= value) && (value <= TDC_BLE_MAP_PULSE_WIDTH_MAX));
            break;
        }

        case TDC_BLE_MAP_FIELD_NUM_BAND:
        {
            in_range = ((1 <= value) && (value <= df_MaxNumOfElectrode));
            break;
        }

        case TDC_BLE_MAP_FIELD_ALARM_CH:
        {
            in_range = ((1 <= value) && (value <= df_MaxNumOfElectrode));
            break;
        }

        default:
        {
            // 매핑 일자 6필드는 검사하지 않는다.
            in_range = true;
            break;
        }
    }

    return in_range;
}

bool tdc_ble_map_field_electrode_in_range(int value)
{
    return ((TDC_BLE_MAP_ELECTRODE_MIN <= value) && (value <= TDC_BLE_MAP_ELECTRODE_MAX));
}

bool tdc_ble_map_field_level_uA_in_range(int value)
{
    return ((TDC_BLE_MAP_LEVEL_UA_MIN <= value) && (value <= TDC_BLE_MAP_LEVEL_UA_MAX));
}
