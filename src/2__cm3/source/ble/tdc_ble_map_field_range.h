#ifndef TDC_BLE_MAP_FIELD_RANGE_H
#define TDC_BLE_MAP_FIELD_RANGE_H

#include <stdbool.h>

/*
  맵 데이터 자극 파라미터 필드의 범위 검사.

  매핑 앱(0x4B 계열)과 리모콘(0x4D)은 같은 저장소에 맵 데이터를 쓴다.
  저장소는 ST__CFX_CM3_SharedMemory_mapData 를 평평한 int 배열로 본 것이라
  (cfx_link/tdc_shm.c 의 tdc_shm_get_pointer_repository_for_read_write_map_data_stimul_para),
  저장소 인덱스가 곧 구조체 필드 순서다.

  두 경로가 같은 검사를 쓰도록 여기 모은다.

  2026-08-06 이전에는 매핑 앱 경로에만 검사가 있었다. 리모콘으로 맵을 쓰면
  무검증 값이 그대로 플래시에 들어갔고, 특히 주파수 밴드 수가 32 를 넘으면
  isd/tdc_isd_stim_para_setting.c 의 루프가 32칸 배열을 넘어 읽었다.
  상세는 docs/상시 점검 대장.md 의 위험_4 참조.
*/

// 하위명령 1 이 채우는 12필드. 값은 저장소 인덱스와 같다.
// [0..5] 는 매핑 일자(년 · 월 · 일 · 시 · 분 · 초)이며 검사하지 않는다.
#define TDC_BLE_MAP_FIELD_STRATEGY        6   // 자극 기법
#define TDC_BLE_MAP_FIELD_FIRST_PHASE     7   // 선행 펄스 위상
#define TDC_BLE_MAP_FIELD_STIM_MODE       8   // 자극 모드
#define TDC_BLE_MAP_FIELD_PULSE_WIDTH     9   // 펄스 위상 폭
#define TDC_BLE_MAP_FIELD_NUM_BAND        10  // 주파수 밴드 수
#define TDC_BLE_MAP_FIELD_ALARM_CH        11  // 알람용 자극 채널 번호
#define TDC_BLE_MAP_FIELD_STIMUL_PARA_NUM 12  // 하위명령 1 의 필드 개수

// 하위명령 1 의 12필드 중 하나를 검사한다. 검사 대상이 아닌 인덱스는 true 를 돌려준다.
bool tdc_ble_map_field_stimul_para_in_range(int field_index, int value);

// 사용가능 전극번호 · 바이폴라 기준전극 · 주파수 밴드 출력순서 (하위명령 2~7)
bool tdc_ble_map_field_electrode_in_range(int value);

// T · C 레벨과 자극 알람 크기 (하위명령 8~15 및 하위명령 1 의 후속 2바이트)
bool tdc_ble_map_field_level_uA_in_range(int value);

#endif
