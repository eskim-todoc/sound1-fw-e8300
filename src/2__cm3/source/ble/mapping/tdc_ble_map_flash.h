#ifndef __tdc_ble_map_flash_h__
#define __tdc_ble_map_flash_h__

#include <stdint.h>

// 매핑 프로토콜 0x68~0x71 (슬롯·맵 데이터 읽기/쓰기/삭제/복구) 패킷 파싱
//
// 각 함수는 수신 패킷을 파싱해 mappingPacket 또는 공유 메모리 저장소에 적재하고,
// 데이터 범위를 벗어나면 tdc_sys_error_send_to_app() 으로 에러를 응답한다.
// mappingPacket 접근은 tdc_ble_mapping_get_packet() 을 경유한다.
//
// 0x68 · 0x6C · 0x6D 는 여러 패킷에 나눠 도착하므로 데이터 인덱스 연속성을
// tdc_ble_mapping_get/set_seq_index() 로 검사한다.

void tdc_ble_cmd_0x69_read_original_isd_user(void);                                     // 0x69
void tdc_ble_cmd_0x68_write_original_isd_user(const uint8_t *Rx_dataPacket);            // 0x68
void tdc_ble_cmd_0x6A_read_slot_data(const uint8_t *Rx_dataPacket);                     // 0x6A
void tdc_ble_cmd_0x6C_write_slot_data(const uint8_t *Rx_dataPacket);                    // 0x6C
void tdc_ble_cmd_0x6B_read_map_data(const uint8_t *Rx_dataPacket);                      // 0x6B
void tdc_ble_cmd_0x6D_write_map_data(const uint8_t *Rx_dataPacket);                     // 0x6D
void tdc_ble_cmd_0x6E_erase_slot(int command, const uint8_t *Rx_dataPacket);            // 0x6E
void tdc_ble_cmd_0x6F_erase_map(int command, const uint8_t *Rx_dataPacket);             // 0x6F
void tdc_ble_cmd_0x70_recover_except_slot1(int command, const uint8_t *Rx_dataPacket);  // 0x70
void tdc_ble_cmd_0x71_recover_all(int command);                                         // 0x71

#endif  // __tdc_ble_map_flash_h__
