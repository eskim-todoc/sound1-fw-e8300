#ifndef TDC_UT_VECTORS_CM3_H
#define TDC_UT_VECTORS_CM3_H

#if defined(TDC_UNIT_TEST)
#include <stdint.h>

#define TDC_UT_CM3_VECTOR_CAPACITY 256
#define TDC_UT_CM3_VECTOR_OK 0
#define TDC_UT_CM3_VECTOR_PARTIAL 1
#define TDC_UT_CM3_VECTOR_OBSERVED 2
#define TDC_UT_CM3_VECTOR_OBSERVED_NO_INPUT 3
#define TDC_UT_CM3_VECTOR_UNSUPPORTED (-1)
#define TDC_UT_CM3_VECTOR_BUSY (-2)
#define TDC_UT_CM3_VECTOR_UNSAFE (-3)
#define TDC_UT_CM3_VECTOR_OVERFLOW (-4)
#define TDC_UT_CM3_OBSERVER_NOT_CONFIGURED (-5)

extern int g_tdc_ut_cm3_vector_actual[TDC_UT_CM3_VECTOR_CAPACITY];
extern int g_tdc_ut_cm3_vector_expected[TDC_UT_CM3_VECTOR_CAPACITY];
extern unsigned g_tdc_ut_cm3_vector_count;
extern int g_tdc_ut_cm3_vector_overflow;
extern volatile int g_tdc_ut_cm3_vector_done;
/* 1이면 expected를 비교하지 않는다. root의 observed_begin에 expected=NULL을 전달한다. */
extern int g_tdc_ut_cm3_vector_observed;

/* 4/6/30의 선택적인 수동 observer gateway. 기본 NULL이면 제공된 RAM 벡터 실행.
 * 모든 pointer는 기본 NULL이다. begin/apply/restore 세 개가 모두 있을 때만 입력 적용.
 * begin: 공유 영역 이외의 private 상태를 보관하고 격리 fixture를 준비, 성공=0.
 * apply: 실제 제품 경로에 아래 case 입력 적용, 성공=0. 논리를 대체하면 안 된다.
 * sample_extra: 실제 private 변경 flag 등 최대 4개 관측, 반환은 길이 0..4.
 * restore: begin에서 보관한 private 상태 복원, 성공=0. 공유 RAM은 runner가 복원.
 * 4: case 1/2/3은 미준비(0)/준비(1)/실행 금지(0).
 * 6: case 1/2는 맵 요청 1/2. 초기 맵1과 구분 가능한 system flag는 begin에서 준비.
 * 30: case 1/2/3은 상태 A(1) 유지/B(2) 변경/changed 해제(0).
 * 무한 main/UART, 제품 저장소, NVM/리셋/전원/실기 자극 호출은 금지한다.
 * 사용자 gateway를 지정한 경우만 요청값/현재 조회를 OBSERVED로 기록한다.
 */
typedef struct
{
    int (*begin)(unsigned unit);
    int (*apply)(unsigned unit, unsigned case_id, int requested);
    int (*sample_extra)(unsigned unit, int values[4]);
    int (*restore)(unsigned unit);
} tdc_ut_cm3_observer_ops_t;
extern const tdc_ut_cm3_observer_ops_t *g_tdc_ut_cm3_observer_ops;

/* Setup: 정상 루프, CFX와 LED ISR을 시험 소유자가 정지한 상태에서 호출한다.
 * Input: 현재 TD24A CM3 유닛 번호. 한 유닛씩 실행하며 RAM 상태를 복원한다.
 * Output: [-999, unit, case, length, actual...]와 별도의 expected 배열.
 * done은 실행 종료만 뜻한다. OBSERVED는 기대값 비교/합격 판정을 하지 않는다.
 * 미지원은 오류 반환이며 합격 레코드를 만들지 않는다.
 * Restore: RTT reporter가 모두 배출한 뒤 release한다. 그 전 재실행은 BUSY다.
 * IO done이 남아 있어도 BUSY이며 저장 결과를 변경하지 않는다. 이 검사는 lock이
 * 아니므로 호출자의 정상 루프/코어/ISR 정지 책임은 그대로 유지된다.
 * 실기 자극, 파일/NVM 쓰기, 리셋을 실행하지 않는다. CFX 허가가 켜져 있으면 거부한다.
 */
int tdc_ut_cm3_vectors_run_unit(unsigned unit);
/* CM3-029 실패 경로 관찰. DAC 범위 초과 입력을 실제 함수로 계산한다.
 * 실패 시 지역 T/C 배열 읽기는 시험 경계에서 막고 복사 시도만 기록한다.
 * 이전 공유 T/C가 남는 것은 이 시험 경계의 효과이지 제품의 보존 정책이 아니다.
 * 완료 플래그/PCM/실행 허용 상태는 실제 반환 직전 값을 관찰한다.
 * 실패 후 상태 계약이 확정되지 않았으므로 OBSERVED/expected=NULL만 출력한다.
 * case 91/92는 이전 완료 플래그 0/1이다. 각 payload 16개는 다음 순서다.
 * 반환, 복사 시도, 지역 배열 읽기 생략, 프레임, 전송 채널, PCM 모드,
 * 완료 전/후, CFX 허용, 자료 오류, T 첫/끝, C 첫/끝, 임시 값 1/2.
 * run_unit과 같은 정지/배출/복원 조건이며 자동 자극/장치 접근은 하지 않는다. */
int tdc_ut_cm3_vectors_observe_calculation_failure(void);

/* 0은 원래 복사 경로다. 시험 중 non-NULL callback은 복사 직전 성공 여부를
 * 받아 0이면 원래 복사, 1이면 지역 배열 읽기를 생략한다. 입력/계산/반환값을
 * 대체하지 않는다. callback은 배열 포인터를 받지 않으며 실패 배열을 읽지 않는다.
 * 호출자가 정상 루프/ISR/CFX를 정지하고 이전 포인터를 복원해야 한다. */
extern int (*g_tdc_ut_cm3_calculation_before_copy)(int calculation_done);
/* 보고 bridge는 이 포인터를 사용한다. observer이면 NULL, 나머지는 expected 배열. */
const int *tdc_ut_cm3_vectors_expected(void);
void tdc_ut_cm3_vectors_release(void);

/* 0은 원래 주소 확인. 시험 중에만 비교 대상 주소를 교환한다. */
extern uintptr_t g_tdc_ut_cm3_expected_shared_address;
/* UART 공유 디버그 플래그 쓰기를 시험 중에만 억제한다. 0은 원래 경로. */
extern int g_tdc_ut_cm3_suppress_uart_flag;
/* 0은 원래 진단 출력. 실행 중에는 legacy ANSI 로그가 결과 채널에 섞이지 않게 한다. */
extern int g_tdc_ut_cm3_suppress_diagnostics;
#endif
#endif
