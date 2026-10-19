#ifndef __tdc_ble_cmd_0x66_live_h__
#define __tdc_ble_cmd_0x66_live_h__

#include <stdint.h>

// 매핑 프로토콜 0x66 (라이브 자극) 패킷 파싱
//
// 이 파일의 함수 이름은 프로토콜 계층을 그대로 따른다.
//
//   tdc_ble_cmd_0x66_live              명령 0x66 진입점
//   tdc_ble_cmd_0x66_subNN_<이름>      하위 명령 NN (1~8)
//   tdc_ble_cmd_0x66_sub01_idxNN_<이름>  하위 명령 1 의 데이터 인덱스 NN (1~15)
//
// 프로토콜이 바뀌면 해당 계층의 함수 하나만 찾아가면 된다.
// grep "0x66_" 로 이 명령의 전체를, grep "0x66_sub01" 로 전체 파라미터만
// 훑을 수 있고, docs/참고/ble/프로토콜/명령 카탈로그.md 의 0x66 항목과
// 이름이 1:1 로 대조된다.
//
// 각 함수는 수신 패킷을 파싱해 mappingPacket 에 적재하고, 데이터 범위를
// 벗어나면 tdc_sys_error_send_to_app() 으로 에러를 응답한다.
// mappingPacket 접근은 tdc_ble_mapping_get_packet() 을 경유한다.
//
// 하위 명령 1(전체 파라미터)은 20바이트 MTU 제약 때문에 데이터 인덱스
// 1~15 에 나눠 도착하므로 연속성을 tdc_ble_mapping_get/set_seq_index() 로
// 검사한다. 이 카운터는 map_flash 와 공유한다.

void tdc_ble_cmd_0x66_live(const uint8_t *Rx_dataPacket);  // 0x66

#endif  // __tdc_ble_cmd_0x66_live_h__
