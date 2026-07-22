#ifndef __tdc_ble_general_debug_h__
#define __tdc_ble_general_debug_h__

/*
 * tdc_ble_general_debug.h
 *
 * 범용 디버깅 프로토콜(EN__SND_BT_CMD_GENERAL_DEBUG, 0x8F) 처리 인터페이스.
 * tdc_ble_remote_step.c의 remote command 핸들러에서 분리(2026-07-01).
 * packet->data[0]=option 으로 서브 프로토콜을 분기한다.
 */

#include <tdc_ble_remote.h>  // ST__REMOTECONTROL_PACKET

/*
 * 범용 디버깅 프로토콜을 처리하고 응답을 tx_buf에 채운다.
 *  - packet   : 수신된 remote control 패킷(읽기 전용, option·페이로드·command 소스)
 *  - tx_buf   : SPI 송신 버퍼(호출부 지역 배열) 포인터
 *  - tx_index : 현재 쓰기 오프셋(진입 시점)
 *  - 반환값   : 응답을 채운 뒤의 tx_index
 */
int tdc_ble_general_debug_handle(const ST__REMOTECONTROL_PACKET *packet,
                                    int *tx_buf, int tx_index);

#endif /* __tdc_ble_general_debug_h__ */
