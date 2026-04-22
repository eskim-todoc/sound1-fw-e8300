/*
 * tdc_touch.c
 *
 * 터치 기능 레이어 — 상세는 tdc_touch.h 참조.
 * 하드웨어 접근은 전부 tdc_drv_iqs323 의 저수준 공개 API 를 경유한다.
 */

#include <tdc_touch.h>
#include <tdc_drv_iqs323.h>

#include <hw.h>
#include <ci_timer.h>
#include <ci_printf.h>

/* **********************************************************************
 * 초기화 상태머신 (내부 전용)
 */
typedef enum
{
    TDC_TOUCH_INIT_STATE_NONE = 0,   /* begin() 호출 전 */
    TDC_TOUCH_INIT_STATE_MCLR_DONE,  /* MCLR 완료, Auto-ATI 진행/완료 대기 */
    TDC_TOUCH_INIT_STATE_READY       /* 모든 설정 완료, 터치 감지 가능 */
} tdc_touch_init_state_t;

static tdc_touch_init_state_t s_init_state     = TDC_TOUCH_INIT_STATE_NONE;
static int                    s_mclr_done_tick = 0;

/* 런타임 폴링 상태 */
static int               s_touch_tick_old  = 0;
static tdc_touch_state_t s_touch_state_old = TDC_TOUCH_STATE_RESET;

/* **********************************************************************
 * Helper — 상태 enum 을 로그용 문자열로 변환
 */
static const char *state_name(tdc_touch_state_t s)
{
    switch (s)
    {
        case TDC_TOUCH_STATE_RESET:             return "RESET";
        case TDC_TOUCH_STATE_TOUCH:             return "TOUCH";
        case TDC_TOUCH_STATE_NOT_TOUCH:         return "NOT_TOUCH";
        case TDC_TOUCH_STATE_CALIBRATION_ERROR: return "CAL_ERROR";
        default:                                return "?";
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
 * 초기화 — Auto-ATI 완료 감지 후 설정 일괄 적용
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
        if (TDC_TOUCH_INIT_TIMEOUT_MS < (ci_timer_get_tick() - s_mclr_done_tick))
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

    tdc_drv_iqs323_apply_settings();

    SYS_WATCHDOG_REFRESH();

    /* READY 전이 직전 폴링 상태 초기화 */
    s_touch_tick_old  = ci_timer_get_tick();
    s_touch_state_old = TDC_TOUCH_STATE_RESET;

    ci_printi("[TOUCH] INIT FINISH DONE — ELAPSED=%d ms \r\n",
              ci_timer_get_tick() - s_mclr_done_tick);

    return true;
}

/* **********************************************************************
 * Public API
 */

void tdc_touch_init_begin(void)
{
    ci_printi("[TOUCH] INIT BEGIN \r\n");

    SYS_WATCHDOG_REFRESH();

    tdc_drv_iqs323_mclr_reset();

    s_mclr_done_tick = ci_timer_get_tick();
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
            return false;  /* begin() 호출 전 — no-op */

        case TDC_TOUCH_INIT_STATE_MCLR_DONE:
            if (try_finish_init())
            {
                s_init_state = TDC_TOUCH_INIT_STATE_READY;
            }
            return false;  /* 초기화 중엔 터치 판정 미수행 */

        case TDC_TOUCH_INIT_STATE_READY:
        default:
            break;
    }

    /* READY 상태: 100ms 폴링 + 롱터치 판정 */
    curr_tick = ci_timer_get_tick();

    if (TDC_TOUCH_POLL_INTERVAL <= (curr_tick - s_touch_tick_old))
    {
        s_touch_tick_old = curr_tick;

        if (tdc_touch_get_state(&curr_state))
        {
            if (s_touch_state_old != curr_state)
            {
                ci_printv("[TOUCH] STATE: %s -> %s \r\n",
                          state_name(s_touch_state_old), state_name(curr_state));
                s_touch_state_old = curr_state;
            }
        }

        if (proc_long_touch(curr_state))
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

    return true;
}
