#ifndef __tdc_ble_map_stim_h__
#define __tdc_ble_map_stim_h__

#include <stdint.h>

// 매핑 프로토콜 0x65 (특정 자극) 패킷 파싱
//
// 수신 패킷을 파싱해 mappingPacket 에 적재하고, 데이터 범위를 벗어나면
// tdc_sys_error_send_to_app() 으로 에러를 응답한다.
// mappingPacket 접근은 tdc_ble_mapping_get_packet() 을 경유한다.
//
// 0x66(라이브 자극)은 tdc_ble_cmd_0x66_live.c 로 분리됐다. 명령 단위로
// 파일을 나누고 함수 이름에 명령 코드를 넣는 체계를 그쪽에서 시범
// 적용 중이며, 0x65 도 확대 단계에서 같은 규칙으로 옮긴다.

void tdc_ble_map_stim_specific(const uint8_t *Rx_dataPacket);  // 0x65

#endif  // __tdc_ble_map_stim_h__
