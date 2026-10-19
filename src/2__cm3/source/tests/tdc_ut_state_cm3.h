#ifndef TDC_UT_STATE_CM3_H
#define TDC_UT_STATE_CM3_H
#if defined(TDC_UNIT_TEST)
#define TDC_UT_CM3_STATE_WORDS 30u
/* CM3-30 실제 상태 setter/clear 시험. 배열 단위는 int 원소, 각 30개 필요.
 * actual/expected/count는 서로 겹치지 않는 유효 버퍼여야 한다.
 * CFX와 CM3 정상 루프/ISR 생산자를 정지한 전경에서만 호출한다.
 * 0=기록 완료(합격 아님), -1=잘못된 인자/용량, -2=재진입, -3=CFX 실행 중.
 * 실패 시 count=0(유효하고 겹치지 않는 포인터), 배열과 제품 상태는 변경하지 않는다.
 * count가 결과 영역과 겹치면 count도 쓰지 않고 거부한다.
 * 기록: [-999,30,case,length,values...]. 기존 동일 상태 플래그 불일치를 보존한다. */
int tdc_ut_cm3_state_run(int *actual, int *expected, unsigned capacity, unsigned *count);
#endif
#endif
