#ifndef __tdc_ble_map_measure_h__
#define __tdc_ble_map_measure_h__

#include <stdint.h>

// 매핑 프로토콜 0x62~0x64 (임피던스 측정 · eCAP 측정) 패킷 파싱
//
// 각 함수는 수신 패킷을 파싱해 mappingPacket 에 적재하고,
// 데이터 범위를 벗어나면 tdc_sys_error_send_to_app() 으로 에러를 응답한다.
// mappingPacket 접근은 tdc_ble_mapping_get_packet() 을 경유한다.

void tdc_ble_map_measure_impedance_check(const uint8_t *Rx_dataPacket);
void tdc_ble_map_measure_ecap_masking(const uint8_t *Rx_dataPacket);
void tdc_ble_map_measure_ecap_alternative(const uint8_t *Rx_dataPacket);

#endif  // __tdc_ble_map_measure_h__
