#ifndef __tdc_ble_cmd_0x65_specific_h__
#define __tdc_ble_cmd_0x65_specific_h__

#include <stdint.h>

// 매핑 프로토콜 0x65 (특정 자극) 패킷 파싱
//
// 수신 패킷을 파싱해 mappingPacket 에 적재하고, 데이터 범위를 벗어나면
// tdc_sys_error_send_to_app() 으로 에러를 응답한다.
// mappingPacket 접근은 tdc_ble_mapping_get_packet() 을 경유한다.
//
// 명령 단위로 파일을 나누고 함수 이름에 명령 코드를 넣는 계층 네이밍
// 체계를 따른다. 0x66(라이브 자극)은 tdc_ble_cmd_0x66_live.c 에 있다.
// 0x65 는 하위 명령·데이터 인덱스가 없어 subNN_idxKK 세그먼트가 없다.

void tdc_ble_cmd_0x65_specific_stim(const uint8_t *Rx_dataPacket);  // 0x65

#endif  // __tdc_ble_cmd_0x65_specific_h__
