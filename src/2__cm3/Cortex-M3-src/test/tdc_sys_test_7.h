#ifndef TDC_SYS_TEST_7_H
#define TDC_SYS_TEST_7_H

#include "processorDirective.h"

#if TDC_SYS_TEST_7_ENABLE
#include <stdbool.h>

/* 전경 RTT UI 전용. argv[0]은 sys7. t=<uA> c=<uA> w=<us> 또는
 * charge/dac/led/status/stop을 받는다. 입력은 0<=t<=c<=65535, 1<=c,w<=65535 정수.
 * 실제 연결 상태를 전달한다. -1은 입력 오류, 0은 처리/거절, 1은 이전 로그 배출 중.
 * t/c/w의 PASS는 시험 명령의 C*w 비교만 뜻한다. 실제 맵/DAC 검증이나 자극 적용 없음. */
int tdc_sys_test_7_command(int argc, char *argv[], bool mapping_connected);

/* 시험 진입 후 재부팅까지 true. 일반 ISD/맵 제어와 절전 진입을 대신한다. */
bool tdc_sys_test_7_is_active(void);
bool tdc_sys_test_7_log_pending(void);
void tdc_sys_test_7_poll(void);

/* 내부 계산 관찰용: 전경 시험 정지 상태에서만 호출한다.
 * 기존 계산식을 실행하고 DAC 설정을 복원한다. 오류 코드는 관찰을 위해 남긴다.
 * 맵/CFX T/C 배열/NVM/하드웨어에는 계산 결과를 적용하지 않는다. */
bool tdc_sys_test_7_calculate(int pulse_width_us, int t_uA, int c_uA);
#endif

#endif
