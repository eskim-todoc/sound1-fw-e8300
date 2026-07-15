/*
 * tdc_touch_time.h
 *
 * 터치 시간 상수 한 곳. ms 수치만 바꾸면 카운트/임계 파생값이 자동으로 따라온다.
 * 3레이어 탈피 간결 재작성 2026-06-20.
 *
 * 역할 분리: 기능 ON/OFF 빌드가드는 tdc_touch_config.h, 시간 '수치'는 본 파일이 단일 소유.
 * 순수 제어 로직(tdc_touch_logic.c)이 본 매크로를 직접 참조하므로 HW 의존이 없다.
 */

#ifndef TDC_TOUCH_TIME_H_
#define TDC_TOUCH_TIME_H_

/* 핵심 파생 매크로: ms 를 주기로 나눠 올림(최소 1회 보장). */
#define TDC_TIME_TO_CNT(ms, interval_ms)  (((ms) + (interval_ms) - 1) / (interval_ms))

/* ====================== (A) 노말 모드 - ci_timer ms 차분 기준 ====================== */
#define TDC_TOUCH_POLL_INTERVAL_MS   100    /* 폴링 주기 (ms) */
#define TDC_TOUCH_LONG_TOUCH_MS      2400   /* 롱터치 -> 절전 트리거 (ms) */
#define TDC_TOUCH_INIT_TIMEOUT_MS    2500   /* 부팅 auto-ATI 대기 타임아웃 (ms) */
#define TDC_TOUCH_BOOT_WARN_MS       5000   /* 부팅 터치 무시 경고 (ms) */
#define TDC_TOUCH_STUCK_TIMEOUT_MS   30000  /* stuck 단계 간격 (ms). 0 이면 stuck 비활성 */

/* ms 기반 카운트형 상수 - 매 폴링 1증감하는 종류에만 적용 (요구사항: ms 만 바꾸면 자동 파생). */
#define TDC_TOUCH_RE_ATI_COOLDOWN_MS 2000   /* Re-ATI 재발행 금지 구간 (ms) */
#define TDC_TOUCH_READ_FAIL_HOLD_MS  2000   /* read 실패 hold 구간 (ms) */
#define TDC_TOUCH_BOOT_RELEASE_DEBOUNCE_MS 300  /* 부팅 터치 해제 디바운스 (ms) - 연속 not-pressed 유지 */

#define TDC_TOUCH_RE_ATI_COOLDOWN_CNT \
        TDC_TIME_TO_CNT(TDC_TOUCH_RE_ATI_COOLDOWN_MS, TDC_TOUCH_POLL_INTERVAL_MS)  /* @200 = 10 */
#define TDC_TOUCH_READ_FAIL_HOLD_CNT \
        TDC_TIME_TO_CNT(TDC_TOUCH_READ_FAIL_HOLD_MS, TDC_TOUCH_POLL_INTERVAL_MS)   /* @200 = 10 */
#define TDC_TOUCH_BOOT_RELEASE_DEBOUNCE_CNT \
        TDC_TIME_TO_CNT(TDC_TOUCH_BOOT_RELEASE_DEBOUNCE_MS, TDC_TOUCH_POLL_INTERVAL_MS)  /* @200 = 2 */

/* ====================== (B) 절전 ULP - 웨이크업 카운트 기준 ====================== */
#define TDC_TOUCH_ULP_WAKE_MS        100    /* 웨이크업 주기 (ms) - HW 타이머와 일치 */

/* 절전 ULP 터치 감시(boot_ignore 철학): 게이트 해제(노터치 확정)·강제 RESEED·재부팅 트리거 */
#define TDC_TOUCH_ULP_NOTOUCH_DEBOUNCE_MS 300    /* 첫 노터치 확정(게이트 해제) 연속 시간 */
#define TDC_TOUCH_ULP_FORCE_RESEED_MS     10000  /* 노터치 미확정 시 강제 RESEED(무한 스턱 방지) */
#define TDC_TOUCH_ULP_REBOOT_TOUCH_MS     2400   /* 게이트 해제 후 재부팅 터치 확정 연속 시간 */

#define TDC_TOUCH_ULP_NOTOUCH_DEBOUNCE_CNT \
        TDC_TIME_TO_CNT(TDC_TOUCH_ULP_NOTOUCH_DEBOUNCE_MS, TDC_TOUCH_ULP_WAKE_MS)  /* @200 = 2 */
#define TDC_TOUCH_ULP_FORCE_RESEED_CNT \
        TDC_TIME_TO_CNT(TDC_TOUCH_ULP_FORCE_RESEED_MS, TDC_TOUCH_ULP_WAKE_MS)      /* @200 = 50 */
#define TDC_TOUCH_ULP_REBOOT_TOUCH_CNT \
        TDC_TIME_TO_CNT(TDC_TOUCH_ULP_REBOOT_TOUCH_MS, TDC_TOUCH_ULP_WAKE_MS)      /* @200 = 2 */

/* 절전 HW 타이머 - 자동 파생 불가(HW 공식 역산). ULP_WAKE_MS 변경 시 재계산 [실측 게이트].
 * 공식: T[ms] = PRESCALE분주 x (TIMEOUT+1) / 40  (SLOWCLK 40kHz).
 * 사용가능 매크로 = TIMER_PRESCALE_1 / _128. PRESCALE_128 격자는 100ms 불가(최근접 99.2).
 * → PRESCALE_1 x (3999+1) / 40 = 4000/40 = 100.0ms 정확 일치 (ULP_WAKE_MS=100). */
#define TDC_TOUCH_ULP_TIMER_PRESCALE       TIMER_PRESCALE_1
#define TDC_TOUCH_ULP_TIMER_TIMEOUT_VALUE  3999

/* 빌드타임 0-division 가드. */
#if (TDC_TOUCH_POLL_INTERVAL_MS == 0) || (TDC_TOUCH_ULP_WAKE_MS == 0)
#  error "TDC_TOUCH_POLL_INTERVAL_MS / TDC_TOUCH_ULP_WAKE_MS must be > 0"
#endif

#endif /* TDC_TOUCH_TIME_H_ */
