#ifndef TDC_REMOTE_GAIN_CONTROL_H_
#define TDC_REMOTE_GAIN_CONTROL_H_

/*
 * tdc_remote_gain_control.h
 *
 * Gain control 프로토콜(EN__SND_BT_CMD_GAIN_CONTROL, 0x8C) 처리 인터페이스.
 * 앱이 QCC 를 거쳐 E8300 까지 보내는 패킷으로, Gain Conversion Table 의
 * 인덱스를 읽거나 쓴다. 설정된 인덱스는 공유 메모리를 통해 CFX 로 전달되고
 * CFX 의 믹싱 단계에서 Gain_A(마이크) · Gain_B(I2S 크래들 마이크)로 적용된다.
 *
 * 앱 명령 (헤더 제외 페이로드):
 *   data[0] = Control type      (0 = Read, 1 = Write)
 *   data[1] = Gain type         (0 = Invalid, 1 = Gain_A, 2 = Gain_B)
 *   data[2] = Gain table index  (0~255)
 *
 * 기기 응답 (5 워드):
 *   [0] = 0x8C, [1] = rsp_code (0 = Failed, 1 = Success),
 *   [2] = Control type, [3] = Gain type, [4] = Gain table index
 */

#include "remoteControl.h"  // ST__REMOTECONTROL_PACKET

/* 게인 테이블 인덱스 기본값(유니티)과 유효 범위는 영속화 모듈이 단일 출처로 갖는다. */
#include <tdc_fs_gain.h>

/* Control type */
#define TDC_GAIN_CONTROL_TYPE_READ  0
#define TDC_GAIN_CONTROL_TYPE_WRITE 1

/* Gain type */
#define TDC_GAIN_TYPE_INVALID 0
#define TDC_GAIN_TYPE_A       1
#define TDC_GAIN_TYPE_B       2

/* 응답 코드 */
#define TDC_GAIN_RSP_FAILED  0
#define TDC_GAIN_RSP_SUCCESS 1

/* 공유 메모리의 게인 인덱스를 기본값으로 초기화한다.
 * CFX iteration 이 열리기 전에 호출해야 한다(인덱스 0 이 뮤트이므로
 * 미초기화 값이 그대로 쓰이면 무음이 될 수 있다). */
void tdc_remote_gain_control_init(void);

/*
 * Gain control 프로토콜을 처리하고 응답을 tx_buf 에 채운다.
 *  - packet   : 수신된 remote control 패킷(읽기 전용)
 *  - tx_buf   : SPI 송신 버퍼(호출부 지역 배열) 포인터
 *  - tx_index : 현재 쓰기 오프셋(진입 시점)
 *  - 반환값   : 응답을 채운 뒤의 tx_index
 */
int tdc_remote_gain_control_handle(const ST__REMOTECONTROL_PACKET *packet, int *tx_buf, int tx_index);

#endif /* TDC_REMOTE_GAIN_CONTROL_H_ */
