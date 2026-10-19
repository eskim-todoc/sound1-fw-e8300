#ifndef TDC_SYS_TEST_80_H
#define TDC_SYS_TEST_80_H

#include <processorDirective.h>

#if TDC_SYS_TEST_80_ENABLE
#include <stdbool.h>
/* 전경 전용. argv[0]=sys80/--sys80, argv[1]=arm/check/clear/restore.
 * 호출자는 실제 ISD/매핑 분리, 오류 없음, NOP hold와 파일 접근 독점을 보장한다.
 * arm은 SYS80.TST에 정상 CRC를 저장/검증한 뒤 데이터만 손상시킨다.
 * check는 CRC 불일치를 확인하면 같은 시험 파일을 공장 기본값으로 복구한다.
 * clear는 식별된 시험 파일만 삭제한다. 세 명령은 실제 사용자 설정을 쓰지 않는다.
 * restore만 이전 버전의 SYS80.BAK를 실제 사용자 설정으로 원복하는 호환 명령이다.
 * 시험 중 재부팅은 불필요하나 HOLD 해제/close 실패 복구에는 재부팅이 필요하다.
 * 0=결과/거절을 로그로 응답, -1=사용법 오류. 저장 성공을 반환값으로 판단하지 않는다.
 * 기존 시험 파일이 있으면 arm을 거절한다. restore 실패 시 기존 백업을 보존한다. */
int tdc_sys_test_80_execute(int argc, char *argv[]);
/* close 실패로 파일 수명이 불확실하면 공통 계층의 다른 FS 명령도 막는다. */
bool tdc_sys_test_80_requires_reboot(void);
#endif

#endif
