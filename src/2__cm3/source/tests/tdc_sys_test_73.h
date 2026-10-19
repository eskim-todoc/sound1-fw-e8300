#ifndef TDC_SYS_TEST_73_H
#define TDC_SYS_TEST_73_H

#include <processorDirective.h>

#if TDC_SYS_TEST_73_ENABLE
/* argv[0]=sys73. init/write와 결함 이름, 또는 led를 받는다.
 * 반환: 0=응답 기록, -1=usage. 공통 전경이 실제 ISD/매핑 분리,
 * 기존 오류 없음 및 NOP HOLD를 먼저 보장해야 한다.
 * 생성된 오류는 관찰용으로 남긴다. 다음 사례/정상 동작은 재부팅한다. */
int tdc_sys_test_73_execute(int argc, char *argv[]);
#endif

#endif
