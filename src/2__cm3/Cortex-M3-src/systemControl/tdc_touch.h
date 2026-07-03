/*
 * tdc_touch.h
 *
 * 터치 공개 API + 레이어 중립 상태 enum. 3레이어 탈피 간결 재작성 2026-06-20.
 *
 * 구현은 3파일로 분리: 순수 제어 FSM(tdc_touch_logic) / IQS323 I2C 직접(tdc_touch_iqs323) /
 * 연결+초기화 상태머신(tdc_touch.c). 시간 상수는 tdc_touch_time.h 가 단일 소유한다.
 * 본 헤더는 main.c 가 부르는 공개 API 시그니처를 고정해 호출처 변경을 막는다.
 *
 * 공개 API:
 *   tdc_touch_init_begin()  ? POWER_ON 진입 시 1회 (MCLR 트리거)
 *   tdc_touch_process()     ? 매 tick 호출 (초기화 진행 + 폴링 + 액션 디스패치)
 *   tdc_touch_get_state()   ? 즉시 상태 조회 (ULP 웨이크 판정 등)
 *   tdc_touch_state_name()  ? 상태 enum -> 문자열
 */

#ifndef TDC_TOUCH_H_
#define TDC_TOUCH_H_

#include <hw.h>
#include <stdbool.h>
#include <tdc_touch_config.h>
#include <tdc_touch_time.h> /* 시간 상수 단일 소유 (POLL/LONG/INIT 등) ? 본 파일이 재노출 */

/* **********************************************************************
 * 상태 enum (레이어 중립 ? 드라이버 에러도 일반화)
 */
typedef enum
{
    TDC_TOUCH_STATE_RESET,            /* 초기 상태 */
    TDC_TOUCH_STATE_TOUCH,            /* 터치 중 */
    TDC_TOUCH_STATE_NOT_TOUCH,        /* 비터치 */
    TDC_TOUCH_STATE_CALIBRATION_ERROR /* (미사용) 과거 IQS323 ATI_ERROR 매핑.
                                       * 운용 모드는 ATI 미사용이라 get_state가 발생시키지
                                       * 않음 ? enum/문자열 호환 위해 유지. */
} tdc_touch_state_t;

/* **********************************************************************
 * Public API
 */

uint16_t tdc_touch_debug_get_recent_lta(void);
void     tdc_touch_debug_set_recent_lta(uint16_t lta);
uint16_t tdc_touch_debug_get_recent_count(void);
void     tdc_touch_debug_set_recent_count(uint16_t count);
uint16_t tdc_touch_debug_get_recent_delta(void);
void     tdc_touch_debug_set_recent_delta(uint16_t delta);
uint16_t tdc_touch_debug_get_recent_abs_thr(void);
void     tdc_touch_debug_set_recent_abs_thr(uint16_t abs_thr);
uint8_t  tdc_touch_debug_get_recent_pressed(void);
void     tdc_touch_debug_set_recent_pressed(uint8_t pressed);
uint8_t  tdc_touch_debug_get_recent_ati_error(void);
void     tdc_touch_debug_set_recent_ati_error(uint8_t ati_error);
uint8_t  tdc_touch_debug_get_recent_ati_active(void);
void     tdc_touch_debug_set_recent_ati_active(uint8_t ati_active);

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

#endif /* TDC_TOUCH_H_ */
