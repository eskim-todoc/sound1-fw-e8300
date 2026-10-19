#ifndef TDC_SYS_TEST_77_H
#define TDC_SYS_TEST_77_H
#include <processorDirective.h>
#if TDC_SYS_TEST_77_ENABLE
#include <stdbool.h>
/* 0=응답 기록, -1=usage. 공통 전경이 NOP HOLD/분리/오류 없음 조건을 보장한다.
 * 파일 작업 실패 뒤에는 scratch를 보존하며 재부팅 전 재실행을 거절한다. */
int tdc_sys_test_77_execute(int argc, char *argv[]);
bool tdc_sys_test_77_requires_reboot(void);
#endif
#endif
