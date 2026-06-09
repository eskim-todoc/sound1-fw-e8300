/*
 * tdc_touch.c
 *
 * 터치 기능 레이어 ? 상세는 tdc_touch.h 참조.
 * 하드웨어 접근은 전부 tdc_drv_iqs323 의 저수준 공개 API 를 경유한다.
 */

#include <tdc_touch.h>
#include <tdc_drv_iqs323.h>

#include <hw.h>
#include <ci_timer.h>
#include <ci_printf.h>

#include <LedOutput.h> /* led_request() ? 부팅 터치 무시 피드백 + ATI CALIB 모드 */

/* **********************************************************************
 * 초기화 상태머신 (내부 전용)
 *
 * 전이:
 *   NONE       → MCLR_DONE   : tdc_touch_init_begin() 호출 (Initialize P3-Early 병렬 블록, I2C init 후)
 *   MCLR_DONE  → READY       : tdc_timer_get_t3_tick() - s_mclr_done_tick >= TDC_TOUCH_INIT_TIMEOUT_MS
 *                              또는 tdc_drv_iqs323_is_auto_ati_done() == true (메인 루프 polling)
 *
 * 시간 기준: g_tdc_timer_t3_tick (TIMER3 LED·터치 공유 카운터).
 *   tdc_touch_init_begin() 시점 = CFX iteration 미활성 → g_ci_timer_main_tick 미증가.
 *   이후 READY 전이 후 폴링은 g_ci_timer_main_tick 기준 (CFX iteration 활성 상태).
 */
typedef enum
{
    TDC_TOUCH_INIT_STATE_NONE = 0,  /* begin() 호출 전 */
    TDC_TOUCH_INIT_STATE_MCLR_DONE, /* MCLR 완료, Auto-ATI 진행/완료 대기 */
    TDC_TOUCH_INIT_STATE_READY      /* 모든 설정 완료, 터치 감지 가능 */
} tdc_touch_init_state_t;

static tdc_touch_init_state_t s_init_state     = TDC_TOUCH_INIT_STATE_NONE;
static int                    s_mclr_done_tick = 0; /* g_tdc_timer_t3_tick 기준 */

/* 런타임 폴링 상태 */
static int               s_touch_tick_old  = 0;
static tdc_touch_state_t s_touch_state_old = TDC_TOUCH_STATE_RESET;

/* 부팅 직후 터치 무시 상태
 *   READY 전이 시 터치 중이면 ignore=true. 해제 시 자동 복귀.
 *   5초 이상 지속 시 보라 깜빡임(디버깅). */
static bool s_boot_touch_ignore = false;
static int  s_boot_ready_tick   = 0;
static bool s_boot_5s_warned    = false;

#if TDC_TOUCH_SLEEP_MEASURE_MODE
/* CALIB 루프에서 's' 입력 시 설정 — func_normal() 에서 소비하여 절전 전환. */
static bool s_sleep_request = false;
#endif

/* **********************************************************************
 * Helper ? 상태 enum 을 로그용 문자열로 변환
 */
const char *tdc_touch_state_name(tdc_touch_state_t s)
{
    switch (s)
    {
        case TDC_TOUCH_STATE_RESET:
            return "RESET";
        case TDC_TOUCH_STATE_TOUCH:
            return "TOUCH";
        case TDC_TOUCH_STATE_NOT_TOUCH:
            return "NOT_TOUCH";
        case TDC_TOUCH_STATE_CALIBRATION_ERROR:
            return "CAL_ERROR";
        default:
            return "?";
    }
}

/* **********************************************************************
 * 롱터치 판정 (3초)
 */
static bool proc_long_touch(tdc_touch_state_t state_now)
{
    static tdc_touch_state_t s_state_old        = TDC_TOUCH_STATE_RESET;
    static int               s_tick_first_touch = 0;
    static bool              s_is_long_touch    = false;
    bool                     ret                = false;

    switch (s_state_old)
    {
        case TDC_TOUCH_STATE_RESET:
        {
            if (state_now == TDC_TOUCH_STATE_TOUCH)
            {
                s_tick_first_touch = ci_timer_get_tick();
                s_is_long_touch    = false;
            }
        }
        break;

        case TDC_TOUCH_STATE_TOUCH:
        {
            if (state_now == TDC_TOUCH_STATE_TOUCH)
            {
                if (!s_is_long_touch)
                {
                    if (TDC_TOUCH_LONG_TOUCH_MS <= (ci_timer_get_tick() - s_tick_first_touch))
                    {
                        s_is_long_touch = true;
                        ret             = true;
                    }
                }
            }
            else
            {
                s_is_long_touch = false;
            }
        }
        break;

        case TDC_TOUCH_STATE_NOT_TOUCH:
        case TDC_TOUCH_STATE_CALIBRATION_ERROR:
        default:
        {
            if (state_now == TDC_TOUCH_STATE_TOUCH)
            {
                s_tick_first_touch = ci_timer_get_tick();
                s_is_long_touch    = false;
            }
        }
        break;
    }

    s_state_old = state_now;
    return ret;
}

/* **********************************************************************
 * ATI Calibration Mode ? TDC_TOUCH_ATI_CALIB_MODE 빌드 전용
 */
#if TDC_TOUCH_ATI_CALIB_MODE

/* 이진 LED 표시 파라미터
 *   SEP    : 구분자(파랑=MULT, 빨강=COMP) 표시 시간
 *   BIT_ON : 비트 표시 시간 (녹=1 / 노랑?주황=0)
 *   BIT_OFF: 비트 사이 소등 시간 */
#define TDC_TOUCH_CALIB_SEP_MS     1200
#define TDC_TOUCH_CALIB_BIT_ON_MS  700
#define TDC_TOUCH_CALIB_BIT_OFF_MS 250

static void calib_delay_ms(int ms)
{
    int t = ci_timer_get_tick();
    while (ms > (ci_timer_get_tick() - t))
    {
        SYS_WATCHDOG_REFRESH();
    }
}

static void calib_led_off(void)
{
    for (led_src_t s = 0; s < LED_SRC__MAX; s++)
    {
        led_request(s, LED_ST_NONE);
    }
}

/* MULT/COMP 16비트 값을 LED 이진수로 1회 출력.
 *   구분자: 파랑(MULT 시작) / 빨강 점멸(COMP 시작)
 *   비트:   녹색=1 / 노랑?주황=0   (MSB→LSB 순) */
static void tdc_touch_calib_led_binary_once(uint16_t mult16, uint16_t comp16)
{
    /* ── MULT 구분자: 파란색 고정 ── */
    led_request(LED_SRC_MAPPING, LED_ST_MAPPING_NO_ISD_BATT_READY);
    calib_delay_ms(TDC_TOUCH_CALIB_SEP_MS);
    calib_led_off();
    calib_delay_ms(TDC_TOUCH_CALIB_BIT_OFF_MS);

    /* ── MULT 16비트 (MSB→LSB) ── */
    for (int8_t bit = 15; bit >= 0; bit--)
    {
        if ((mult16 >> bit) & 1)
            led_request(LED_SRC_BATTERY, LED_ST_BATT_READY); /* 1 = 녹색 */
        else
            led_request(LED_SRC_BATTERY, LED_ST_BATT_MID); /* 0 = 노랑(주황 근사) */
        calib_delay_ms(TDC_TOUCH_CALIB_BIT_ON_MS);
        calib_led_off();
        calib_delay_ms(TDC_TOUCH_CALIB_BIT_OFF_MS);
    }

    /* ── COMP 구분자: 빨간색 점멸 ── */
    led_request(LED_SRC_ERROR, LED_ST_ERROR_MAP);
    calib_delay_ms(TDC_TOUCH_CALIB_SEP_MS);
    calib_led_off();
    calib_delay_ms(TDC_TOUCH_CALIB_BIT_OFF_MS);

    /* ── COMP 16비트 (MSB→LSB) ── */
    for (int8_t bit = 15; bit >= 0; bit--)
    {
        if ((comp16 >> bit) & 1)
            led_request(LED_SRC_BATTERY, LED_ST_BATT_READY);
        else
            led_request(LED_SRC_BATTERY, LED_ST_BATT_MID);
        calib_delay_ms(TDC_TOUCH_CALIB_BIT_ON_MS);
        calib_led_off();
        calib_delay_ms(TDC_TOUCH_CALIB_BIT_OFF_MS);
    }
}

#endif /* TDC_TOUCH_ATI_CALIB_MODE */

/* **********************************************************************
 * 초기화 ? Auto-ATI 완료 감지 후 설정 일괄 적용
 *
 * 반환: true  = READY 전이 준비 완료
 *       false = 아직 Auto-ATI 중. 다음 tick 에서 재시도
 *
 * 타임아웃 초과 시 경고 로그와 함께 강제 진행 (터치 누른 채 부팅 대비).
 */
static bool try_finish_init(void)
{
    if (!tdc_drv_iqs323_is_auto_ati_done())
    {
        if (TDC_TOUCH_INIT_TIMEOUT_MS < (tdc_timer_get_t3_tick() - s_mclr_done_tick))
        {
            ci_printw("[TOUCH] AUTO-ATI: TIMEOUT, FORCING FINISH \r\n");
            /* 타임아웃 경로도 설정 적용은 진행 */
        }
        else
        {
            return false;
        }
    }

    SYS_WATCHDOG_REFRESH();

    tdc_drv_iqs323_apply_settings(); /* 센서 설정 — CALIB/운용 공통. ATI_DUMP=1이면 1회 덤프 포함. */

#if TDC_TOUCH_ATI_CALIB_MODE
    {
        uint16_t mult16 = 0, comp16 = 0;
        calib_delay_ms(400); /* 부팅 LED 완료 대기 */
        calib_led_off();
        while (1)
        {
            tdc_drv_iqs323_calib_read_ati(&mult16, &comp16);
#if TDC_TOUCH_ATI_CALIB_LED_ENABLE
            tdc_touch_calib_led_binary_once(mult16, comp16);
#else
            ci_printi("[CALIB] MULT=0x%04X  COMP=0x%04X\r\n", mult16, comp16);
            calib_delay_ms(500);
#endif
#if TDC_TOUCH_SLEEP_MEASURE_MODE
            if (SEGGER_RTT_HasKey() && 's' == SEGGER_RTT_GetKey())
            {
                ci_printi("[CALIB] 's' — exit CALIB, sleep measure scheduled.\r\n");
                s_sleep_request = true;
                break;
            }
#endif
            tdc_drv_iqs323_calib_re_ati();
        }
    }
#endif

    SYS_WATCHDOG_REFRESH();

    /* READY 전이 직전 폴링 상태 초기화 */
    s_touch_tick_old  = ci_timer_get_tick();
    s_touch_state_old = TDC_TOUCH_STATE_RESET;

    ci_printi("[TOUCH] INIT FINISH DONE ? ELAPSED=%d ms \r\n", tdc_timer_get_t3_tick() - s_mclr_done_tick);

    return true;
}

/* **********************************************************************
 * Public API
 */

#if TDC_TOUCH_SLEEP_MEASURE_MODE
bool tdc_touch_consume_sleep_request(void)
{
    if (!s_sleep_request)
    {
        return false;
    }
    s_sleep_request = false;
    return true;
}
#endif

void tdc_touch_init_begin(void)
{
    ci_printi("[TOUCH] INIT BEGIN \r\n");

    SYS_WATCHDOG_REFRESH();

    tdc_drv_iqs323_mclr_reset();

    s_mclr_done_tick = tdc_timer_get_t3_tick();
    s_init_state     = TDC_TOUCH_INIT_STATE_MCLR_DONE;
}

bool tdc_touch_process(void)
{
    int               curr_tick;
    tdc_touch_state_t curr_state;

    /* 초기화 상태머신 진행 */
    switch (s_init_state)
    {
        case TDC_TOUCH_INIT_STATE_NONE:
            return false; /* begin() 호출 전 ? no-op */

        case TDC_TOUCH_INIT_STATE_MCLR_DONE:
            if (try_finish_init())
            {
                s_init_state      = TDC_TOUCH_INIT_STATE_READY;
                s_boot_ready_tick = ci_timer_get_tick();

                /* READY 전이 직후 즉시 터치 상태 확인 ? 누른 채 부팅 방어 */
                tdc_touch_state_t boot_state = TDC_TOUCH_STATE_RESET;
                tdc_touch_get_state(&boot_state);
                if (boot_state == TDC_TOUCH_STATE_TOUCH)
                {
                    s_boot_touch_ignore = true;
                    ci_printw("[TOUCH] BOOT TOUCH ? ignoring until released \r\n");
                }
            }
            return false; /* 초기화 중엔 터치 판정 미수행 */

        case TDC_TOUCH_INIT_STATE_READY:
        default:
            break;
    }

    /* READY 상태: 100ms 폴링 + 롱터치 판정 */
    curr_tick = ci_timer_get_tick();

    if (TDC_TOUCH_POLL_INTERVAL <= (curr_tick - s_touch_tick_old))
    {
        s_touch_tick_old = curr_tick;

        bool got_state = tdc_touch_get_state(&curr_state);
        if (got_state)
        {
            if (s_touch_state_old != curr_state)
            {
                ci_printv("[TOUCH] STATE: %s -> %s \r\n", tdc_touch_state_name(s_touch_state_old), tdc_touch_state_name(curr_state));
                s_touch_state_old = curr_state;
            }
        }

        /* 부팅 직후 터치 무시 구간 */
        if (s_boot_touch_ignore)
        {
            /* read 실패 시 curr_state 는 쓰레기 값 ? 상태 불명이므로 해제 판정 보류 */
            if (got_state && curr_state != TDC_TOUCH_STATE_TOUCH)
            {
                s_boot_touch_ignore = false;
                ci_printi("[TOUCH] BOOT TOUCH RELEASED ? sensing resumed \r\n");
            }
            else if (!s_boot_5s_warned && 5000 <= (curr_tick - s_boot_ready_tick))
            {
                s_boot_5s_warned = true;
                ci_printw("[TOUCH] BOOT TOUCH > 5s \r\n");
                led_request(LED_SRC_DBG, LED_ST_DBG_LONG_TOUCH_IGNORE);
            }
            return false;
        }

        if (got_state && proc_long_touch(curr_state))
        {
            ci_printi("\r\n[TOUCH] EVENT: LONG TOUCH \r\n");
            return true;
        }
    }

    return false;
}

bool tdc_touch_get_state(tdc_touch_state_t *p_state)
{
    bool pressed   = false;
    bool ati_error = false;

    if (!tdc_drv_iqs323_read_status(&pressed, &ati_error))
    {
        return false;
    }

    if (ati_error)
    {
        *p_state = TDC_TOUCH_STATE_CALIBRATION_ERROR;
    }
    else if (pressed)
    {
        *p_state = TDC_TOUCH_STATE_TOUCH;
    }
    else
    {
        *p_state = TDC_TOUCH_STATE_NOT_TOUCH;
    }

#if TDC_TOUCH_CRX0_DISCHARGE_ENABLE
    /* 상태 결정 후 방전 — 다음 호출 시점에 IC가 신선한 ESD-free 측정값을 준비.
     * 방전 실패는 다음 읽기 품질에만 영향, 현재 상태 반환은 이미 성공이므로 무시. */
    (void)tdc_drv_iqs323_discharge_crx0();
#endif

    return true;
}
