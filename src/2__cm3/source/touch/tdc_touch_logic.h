/*
 * tdc_touch_logic.h
 *
 * 순수 터치 제어 FSM (HW 비의존). 3레이어 탈피 간결 재작성 2026-06-20.
 *
 * 순수성 게이트: 본 헤더와 tdc_touch_logic.c 는 hw.h / tdc_hal_timer.h / tdc_printf.h /
 *   tdc_led_output.h / tdc_touch_iqs323.h 를 절대 include 하지 않는다. 허용 include 는
 *   <stdbool.h> / <stdint.h> / tdc_touch.h(상태 enum) / tdc_touch_time.h(시간 상수)뿐.
 *   이 화살표 부재가 호스트(PC) 유닛테스트의 구조적 보증이다(의료기기 SW 밸리데이션).
 *
 * 입력은 연결층(tdc_touch.c)이 정규화해 채우고(read 실패 시 pressed/ati_error/ati_active
 * 전부 false), 출력 액션을 연결층이 IQS323 직접 호출로 변환한다. 모든 상태는 인자 구조체로
 * 외부 소유(전역/static 0) - 테스트가 주입/검사한다.
 */

#ifndef TDC_TOUCH_LOGIC_H_
#define TDC_TOUCH_LOGIC_H_

#include <stdbool.h>
#include <stdint.h>
#include <tdc_touch.h>       /* tdc_touch_state_t (레이어 중립 상태 enum) */

/* 출력 액션 - 연결층이 IQS323 호출로 변환한다. */
typedef enum
{
    TDC_TOUCH_ACT_NONE = 0,   /* 무동작 */
    TDC_TOUCH_ACT_HOLD,       /* read 실패 hold 이내: 전이/게이트/stuck/롱터치 평가 동결 */
    TDC_TOUCH_ACT_RE_ATI,     /* Re-ATI 발행 (노터치 게이트 드리프트 또는 stuck stage1) */
    TDC_TOUCH_ACT_RESEED,     /* RESEED 발행 (stuck stage0) */
    TDC_TOUCH_ACT_MCLR        /* CM3 재부팅 요청 (stuck stage2) - 연결층이 watchdog reset */
} tdc_touch_act_t;

/* 부팅 무시 이벤트 - 연결층이 로그/LED 분기에 사용 (불리언 3개를 1 enum 으로 압축). */
typedef enum
{
    TDC_TOUCH_BOOT_NONE = 0,  /* 정상 폴링 */
    TDC_TOUCH_BOOT_IGNORING,  /* 부팅 터치 무시 중 - 상위가 입력 무시 */
    TDC_TOUCH_BOOT_RELEASED,  /* 이번 step 에 무시 해제 - 연결층 1회 로그 */
    TDC_TOUCH_BOOT_WARN_5S    /* 5초 경과 1회 - 연결층 1회 로그 + 경고 LED */
} tdc_touch_boot_event_t;

/* 입력 (연결층이 read 결과를 정규화해 채운다. read_ok=false 면 아래 3개 false). */
typedef struct
{
    uint32_t now_ms;      /* 현재 절대시각 ms (연결층이 tdc_hal_timer 에서 주입) - 시간 판정 단일 소스 */
    bool     read_ok;     /* read 성공 여부 */
    bool     pressed;     /* read_ok 시 유효: CH0 터치 눌림 */
    bool     ati_error;   /* read_ok 시 유효: ATI Error (드리프트 신호) */
    bool     ati_active;  /* read_ok 시 유효: ATI burst 진행 중 */
} tdc_touch_in_t;

/* 출력 (5필드). */
typedef struct
{
    tdc_touch_act_t        action;         /* 발행할 HW 액션 (연결층 -> IQS323) */
    bool                   long_touch;     /* 롱터치 발동 1회 (절전 트리거) */
    tdc_touch_state_t      curr_state;     /* 이번 step 산출 상태 (로그/디버그) */
    bool                   state_changed;  /* prev != curr (STATE 로그 트리거) */
    tdc_touch_boot_event_t boot_event;     /* 부팅 무시 이벤트 (로그/LED 분기) */
} tdc_touch_out_t;

/* 상태 (현 분산 static 통합 - 단일 소유, 연결층이 보관). */
typedef struct
{
    tdc_touch_state_t prev_state;               /* 폴링 직전 상태 */
    int               read_fail_cnt;            /* read 연속 실패 횟수 */
    bool              boot_ignore;              /* 부팅 터치 무시 중 */
    uint32_t          boot_ready_ms;            /* READY 전이 시각 */
    bool              boot_5s_warned;           /* 5초 경고 1회 래치 */
    uint8_t           boot_release_cnt;         /* 부팅 해제 디바운스: 연속 not-pressed 폴링 카운트 */
    uint32_t          first_touch_ms;           /* 터치 진입 시각 (롱터치 기준) */
    bool              long_latched;             /* 롱터치 발동 래치 */
    uint32_t          re_ati_cooldown_until_ms; /* Re-ATI 쿨다운 만료 절대시각 */
    uint32_t          stuck_anchor_ms;          /* stuck 단계 기준 시각 */
    uint8_t           stuck_stage;              /* 0 / 1 / 2 */
} tdc_touch_logic_state_t;

/* === 공개 함수 (cfg 인자 없음 - 시간 상수는 .c 가 tdc_touch_time.h 매크로 직접 참조) === */

/* 상태 초기화 - RESET 초기값 명시(부팅 직후 첫 TOUCH 보존). READY 전이 시 호출. */
void tdc_touch_logic_init(tdc_touch_logic_state_t *st, uint32_t now_ms);

/* 부팅 무시 진입 (READY 직후 터치 중일 때 연결층이 호출). */
void tdc_touch_logic_set_boot_ignore(tdc_touch_logic_state_t *st, uint32_t now_ms);

/* 순수 step. 전역/HW/timer/printf 접근 0. 연결층이 폴링 tick 1회당 1회 호출. */
void tdc_touch_logic_step(tdc_touch_logic_state_t *st,
                          const tdc_touch_in_t    *in,
                          tdc_touch_out_t         *out);

#endif /* TDC_TOUCH_LOGIC_H_ */
