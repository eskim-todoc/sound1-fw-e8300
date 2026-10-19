#ifndef TDC_SYS_TEST_AUDIT_H
#define TDC_SYS_TEST_AUDIT_H

#include "processorDirective.h"

#if TDC_SYS_TEST_AUDIT_ENABLE
#include <stdbool.h>

/* CM3 전경 전용. test_id=50/51, argv[0]=sys50/sys51,
 * 추가 인자 없음 또는 show/stop. -1=입력 오류, 1=다른 출력 진행 중, 0=접수.
 * 고정 시험 데이터를 표시할 뿐 실제 연결/손상 감지/감사 파일 쓰기는 하지 않는다. */
int tdc_sys_test_audit_command(unsigned test_id, int argc, char *argv[]);
bool tdc_sys_test_audit_pending(void);

/* UI poll에서 한 번 호출할 때 최대 60바이트 한 행만 전송한다.
 * RTT 용량 부족 시 같은 행을 보존하고 다음 poll에서 재시도한다. */
void tdc_sys_test_audit_poll(void);
#endif

#endif
