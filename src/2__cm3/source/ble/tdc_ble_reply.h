#ifndef __tdc_ble_reply_h__
#define __tdc_ble_reply_h__

#include <stdint.h>

// BLE 응답 조립 빌더.
//
// 버퍼와 현재 인덱스를 주입받고 갱신된 인덱스를 반환한다.
// 인덱스는 호출자가 소유하므로 기존 조립 흐름(함수 진입 시 0, 이후 누적)이
// 그대로 유지된다. tdc_ble_general_debug.c 에서 확립된 계약과 같다.
//
// 사용 예:
//   tx_index = tdc_ble_reply_header(bufferForSPI_tx, tx_index, packet.command);
//   tx_index = tdc_ble_reply_u8(bufferForSPI_tx, tx_index, value);
//   tx_index = tdc_ble_reply_u16(bufferForSPI_tx, tx_index, level_uA);
//   tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, tx_index);
//
// 버퍼 크기 검사는 하지 않는다. 기존 조립 코드에도 없었고, 검사를 넣으면
// 넘치던 경로의 동작이 바뀌기 때문이다(동작 보존 원칙).

// 응답 첫 바이트인 명령 에코. 구현은 tdc_ble_reply_u8 과 같으나
// "첫 바이트는 명령 에코"라는 프로토콜 규약을 코드에 드러내기 위해 분리한다.
int tdc_ble_reply_header(uint8_t *tx_buf, int tx_index, int command);

// 8비트 값 하나. 하위 8비트만 저장된다.
int tdc_ble_reply_u8(uint8_t *tx_buf, int tx_index, int value);

// 16비트 값을 상위 -> 하위 바이트 순으로 저장한다.
// 프로토콜 전 구간이 상위 바이트 먼저이며, 역순인 지점은 없다.
int tdc_ble_reply_u16(uint8_t *tx_buf, int tx_index, int value);

#endif  // __tdc_ble_reply_h__
