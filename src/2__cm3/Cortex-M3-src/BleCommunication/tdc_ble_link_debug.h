#ifndef TDC_BLE_LINK_DEBUG_H
#define TDC_BLE_LINK_DEBUG_H

#include <stdint.h>

/* CM3 전경. data는 명령 바이트 뒤의19개 바이트 값(int 원소)이다.
 * tx는 최소20바이트. 반환은 정상/오류 프레임 길이6..20이며 직접 송신하지 않는다.
 * 호출자가 한 번만 SPI 송신하고 명령을 끝낸다. 기존 RX에는 실길이 정보가 없다. */
int tdc_ble_link_debug_handle(int command, const int *data, uint8_t *tx);

#endif
