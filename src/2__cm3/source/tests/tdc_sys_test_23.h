#ifndef TDC_SYS_TEST_23_H
#define TDC_SYS_TEST_23_H

#include <processorDirective.h>

#if TDC_SYS_TEST_23_ENABLE
#include <stdbool.h>

/* CM3 전경 RTT 전용. argv[0]=sys23, argv[1]=start/status/stop.
 * 실제 매핑 연결 상태를 전달한다. -1=형식 오류, 0=처리/거절, 1=로그 배출 중.
 * 실제 자극을 요청하므로 착용 제품/이식 내부기가 아닌 시험용 지그에서만 사용한다. */
int tdc_sys_test_23_command(int argc, char *argv[], bool mapping_connected);
bool tdc_sys_test_23_is_active(void);
bool tdc_sys_test_23_log_pending(void);

/* tdc_sys_control_step 직전의 전경에서 호출. 조건 상실/5초 경과 시 주입만 해제한다.
 * 실제 배터리, 충전, 오류, 버튼 및 절전 보호 경로는 변경하거나 정지하지 않는다. */
void tdc_sys_test_23_prepare(bool mapping_connected);

/* 저배터리 알람 비교에만 사용. 전원/LED/QCC 보고 잔량에 사용하지 않는다. */
int tdc_sys_test_23_alarm_percent(int real_percent);

/* 기존 tdc_stim_indicator_out 직후 호출. CM3 요청/공유 게이트만 관찰한다.
 * 완료는 CM3 알람 한 회차의 끝이며 실제 전극 출력/시스템 시험 PASS가 아니다. */
void tdc_sys_test_23_observe(bool low_power_trigger);
void tdc_sys_test_23_poll(void);
#endif

#endif
