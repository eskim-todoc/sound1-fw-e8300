#ifndef __tdc_ble_map_stim_h__
#define __tdc_ble_map_stim_h__

#include <stdint.h>

// 매핑 프로토콜 0x65~0x66 (특정 자극 · 라이브 자극) 패킷 파싱
//
// 각 함수는 수신 패킷을 파싱해 mappingPacket 에 적재하고,
// 데이터 범위를 벗어나면 tdc_sys_error_send_to_app() 으로 에러를 응답한다.
// mappingPacket 접근은 tdc_ble_mapping_get_packet() 을 경유한다.
//
// 0x66 은 하위 명령 1~8 로 다시 분기하며, 하위 명령 1(전체 파라미터)은
// 데이터 인덱스 1~15 에 나눠 도착하므로 연속성을
// tdc_ble_mapping_get/set_seq_index() 로 검사한다.

void tdc_ble_map_stim_specific(const uint8_t *Rx_dataPacket);  // 0x65
void tdc_ble_map_stim_live(const uint8_t *Rx_dataPacket);      // 0x66

#endif  // __tdc_ble_map_stim_h__
