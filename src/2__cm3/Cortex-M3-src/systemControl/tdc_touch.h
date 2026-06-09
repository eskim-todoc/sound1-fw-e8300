/*
 * tdc_touch.h
 *
 * 터치 기능 레이어 — IC 독립.
 *
 * 부팅 후 초기화 상태머신 진행, 200ms 폴링, 2.8초 롱터치 판정 등 UX 로직을
 * 담당한다. 실제 하드웨어 통신은 드라이버 레이어(`tdc_drv_iqs323`) 에
 * #include 로 위임하며, IC 교체 시 본 파일은 수정 대상이 아니다.
 *
 * 공개 API:
 *   tdc_touch_init_begin()  — POWER_ON 진입 시 1회
 *   tdc_touch_process()     — 매 tick 호출 (초기화 진행 + 폴링 + 롱터치)
 *   tdc_touch_get_state()   — 즉시 상태 조회 (ULP 웨이크 판정 등)
 */

#ifndef TDC_TOUCH_H_
#define TDC_TOUCH_H_

#include <stdbool.h>
#include <tdc_touch_config.h>

/* **********************************************************************
 * 상수
 */
#define TDC_TOUCH_POLL_INTERVAL     200   /* 폴링 간격 (ms) */
#define TDC_TOUCH_LONG_TOUCH_MS     2800  /* 롱터치 판정 시간 (ms) — 노말→절전 */
#define TDC_TOUCH_INIT_TIMEOUT_MS   2500  /* 초기화 Auto-ATI 대기 타임아웃.
                                           * 터치 중 부팅 등 대비 */

/* **********************************************************************
 * 상태 enum (레이어 중립 — 드라이버 에러도 일반화)
 */
typedef enum
{
    TDC_TOUCH_STATE_RESET,              /* 초기 상태 */
    TDC_TOUCH_STATE_TOUCH,              /* 터치 중 */
    TDC_TOUCH_STATE_NOT_TOUCH,          /* 비터치 */
    TDC_TOUCH_STATE_CALIBRATION_ERROR   /* (미사용) 과거 IQS323 ATI_ERROR 매핑.
                                         * 운용 모드는 ATI 미사용이라 get_state가 발생시키지
                                         * 않음 — enum/문자열 호환 위해 유지. */
} tdc_touch_state_t;

/* **********************************************************************
 * Public API
 */

/* POWER_ON 진입 시 1회 호출. 드라이버 MCLR 리셋만 트리거 후 즉시 리턴. */
void tdc_touch_init_begin(void);

/* 매 tick 호출.
 *  - 초기화 미완이면 상태머신 진행 (Auto-ATI 폴링 → 설정 일괄 적용)
 *  - 초기화 완료 후엔 TDC_TOUCH_POLL_INTERVAL 주기로 터치 상태 읽고
 *    TDC_TOUCH_LONG_TOUCH_MS 이상 눌림 지속 시 true 반환 (최초 1회).
 */
bool tdc_touch_process(void);

/* 현재 터치 상태 즉시 조회.
 * 반환: true = read 성공, *p_state 에 상태 enum 저장. */
bool tdc_touch_get_state(tdc_touch_state_t *p_state);

/* 상태 enum → 로그용 문자열 변환. */
const char *tdc_touch_state_name(tdc_touch_state_t s);

#if TDC_TOUCH_SLEEP_MEASURE_MODE
/* CALIB 루프에서 's' 입력으로 예약된 절전 요청을 소비(1회 리셋).
 * true 반환 시 tdc_on_sleep_measure_cmd() + func_sleep() 전환 필요. */
bool tdc_touch_consume_sleep_request(void);
#endif

#endif /* TDC_TOUCH_H_ */
