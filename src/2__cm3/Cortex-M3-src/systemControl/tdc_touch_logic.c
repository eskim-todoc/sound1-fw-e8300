/*
 * tdc_touch_logic.c
 *
 * 순수 터치 제어 FSM 구현 (HW 비의존). 3레이어 탈피 간결 재작성 2026-06-20.
 *
 * 현 tdc_touch 의 롱터치 / Re-ATI 게이트 / stuck 3단계 / read 실패 hold / 부팅 무시를
 * 단일 step 순수 함수로 통합한다. 동작 정본은 B99 이벤트표(E1~E10, S1~S3).
 *
 * 순수성: include 는 본 헤더 + 시간 상수뿐. HW/timer/printf 0건 - 호스트 유닛테스트 가능.
 */

#include <tdc_touch_logic.h>
#include <tdc_touch_time.h>

/* stuck 3단계 에스컬레이션 (순수). TOUCH 가 STUCK_TIMEOUT_MS 마다 단계 상승.
 * 0->1 RESEED, 1->2 Re-ATI(게이트 우회), 2->재부팅. 노터치/해제 시 리셋. */
static tdc_touch_act_t stuck_eval(tdc_touch_logic_state_t *st,
                                  tdc_touch_state_t curr_state, uint32_t now_ms)
{
#if (TDC_TOUCH_STUCK_TIMEOUT_MS > 0)
    if (curr_state != TDC_TOUCH_STATE_TOUCH)        /* 노터치/해제 -> 리셋 */
    {
        st->stuck_stage     = 0;
        st->stuck_anchor_ms = now_ms;
        return TDC_TOUCH_ACT_NONE;
    }
    if (st->prev_state != TDC_TOUCH_STATE_TOUCH)    /* TOUCH 진입 엣지 -> anchor 기록 */
    {
        st->stuck_anchor_ms = now_ms;
        st->stuck_stage     = 0;
        return TDC_TOUCH_ACT_NONE;
    }
    if (TDC_TOUCH_STUCK_TIMEOUT_MS <= (now_ms - st->stuck_anchor_ms))
    {
        st->stuck_anchor_ms = now_ms;               /* 단계마다 재카운트 */
        switch (st->stuck_stage)
        {
            case 0:
            {
                st->stuck_stage = 1;
                return TDC_TOUCH_ACT_RESEED;
            }
            case 1:
            {
                st->stuck_stage = 2;
                return TDC_TOUCH_ACT_RE_ATI;
            }
            default:
            {
                return TDC_TOUCH_ACT_MCLR;
            }
        }
    }
#else
    (void) st;
    (void) curr_state;
    (void) now_ms;
#endif
    return TDC_TOUCH_ACT_NONE;
}

void tdc_touch_logic_init(tdc_touch_logic_state_t *st, uint32_t now_ms)
{
    st->prev_state               = TDC_TOUCH_STATE_RESET;
    st->read_fail_cnt            = 0;
    st->boot_ignore              = false;
    st->boot_ready_ms            = now_ms;
    st->boot_5s_warned           = false;
    st->boot_release_cnt         = 0;
    st->first_touch_ms           = now_ms;
    st->long_latched             = false;
    st->re_ati_cooldown_until_ms = 0;
    st->stuck_anchor_ms          = now_ms;
    st->stuck_stage              = 0;
}

void tdc_touch_logic_set_boot_ignore(tdc_touch_logic_state_t *st, uint32_t now_ms)
{
    st->boot_ignore   = true;
    st->boot_ready_ms = now_ms;
    st->boot_5s_warned = false;
    st->boot_release_cnt = 0;
}

void tdc_touch_logic_step(tdc_touch_logic_state_t *st,
                          const tdc_touch_in_t    *in,
                          tdc_touch_out_t         *out)
{
    tdc_touch_state_t curr_state;

    out->action        = TDC_TOUCH_ACT_NONE;
    out->long_touch    = false;
    out->state_changed = false;
    out->boot_event    = TDC_TOUCH_BOOT_NONE;

    /* --- read 실패 hold / 정규화 --- */
    if (!in->read_ok)
    {
        st->read_fail_cnt++;
        if (st->read_fail_cnt <= TDC_TOUCH_READ_FAIL_HOLD_CNT)
        {
            /* hold 이내: 상태머신 진입 자체를 동결. prev_state 미갱신. */
            out->action     = TDC_TOUCH_ACT_HOLD;
            out->curr_state = st->prev_state;
            return;
        }
        curr_state = TDC_TOUCH_STATE_NOT_TOUCH;     /* hold 초과 -> NOT_TOUCH 강제 */
    }
    else
    {
        st->read_fail_cnt = 0;
        curr_state = in->pressed ? TDC_TOUCH_STATE_TOUCH : TDC_TOUCH_STATE_NOT_TOUCH;
    }

    out->curr_state    = curr_state;
    out->state_changed = (st->prev_state != curr_state);

    /* --- 부팅 무시 구간 (게이트/stuck/롱터치 전 차단) --- */
    if (st->boot_ignore)
    {
        if (in->read_ok && curr_state != TDC_TOUCH_STATE_TOUCH)
        {
            /* 연속 not-pressed 가 디바운스 카운트(400ms = 2폴링) 도달 시에만 진짜 해제 - 채터 방지.
             * (read 실패 시엔 상단 hold 에서 선처리, 여기 도달은 hold 초과 NOT_TOUCH = read_ok false
             *  → 본 분기 미진입 → else 에서 카운터 리셋.) */
            st->boot_release_cnt++;
            if (st->boot_release_cnt >= TDC_TOUCH_BOOT_RELEASE_DEBOUNCE_CNT)
            {
                st->boot_ignore = false;
                out->boot_event = TDC_TOUCH_BOOT_RELEASED;
            }
            else
            {
                out->boot_event = TDC_TOUCH_BOOT_IGNORING;
            }
        }
        else if (curr_state == TDC_TOUCH_STATE_TOUCH
                 && !st->boot_5s_warned
                 && TDC_TOUCH_BOOT_WARN_MS <= (in->now_ms - st->boot_ready_ms))
        {
            st->boot_release_cnt = 0;               /* 재터치 -> 디바운스 리셋 */
            st->boot_5s_warned = true;
            out->boot_event    = TDC_TOUCH_BOOT_WARN_5S;
        }
        else
        {
            st->boot_release_cnt = 0;               /* TOUCH 지속/재터치 또는 read 실패 -> 리셋 */
            out->boot_event = TDC_TOUCH_BOOT_IGNORING;
        }
        st->prev_state = curr_state;                /* 그 tick 도 무시 (게이트/stuck/롱터치 0) */
        return;
    }

    /* --- Re-ATI 게이트 (노터치 전용, 4조건) --- */
    if (curr_state == TDC_TOUCH_STATE_NOT_TOUCH
        && !in->ati_active && in->ati_error
        && in->now_ms >= st->re_ati_cooldown_until_ms)
    {
        out->action                  = TDC_TOUCH_ACT_RE_ATI;
        st->re_ati_cooldown_until_ms = in->now_ms + TDC_TOUCH_RE_ATI_COOLDOWN_MS;
    }

    /* --- stuck 3단계 (TOUCH 전용 - 게이트와 상태 배타라 액션 충돌 0) --- */
    {
        tdc_touch_act_t sa = stuck_eval(st, curr_state, in->now_ms);
        if (sa != TDC_TOUCH_ACT_NONE)
        {
            out->action = sa;
        }
    }

    /* --- 롱터치 엣지 (절전 트리거, ms 차분) --- */
    if (curr_state == TDC_TOUCH_STATE_TOUCH)
    {
        if (st->prev_state != TDC_TOUCH_STATE_TOUCH)
        {
            st->first_touch_ms = in->now_ms;        /* 진입 엣지 -> 기록, 래치 해제 */
            st->long_latched   = false;
        }
        else if (!st->long_latched
                 && TDC_TOUCH_LONG_TOUCH_MS <= (in->now_ms - st->first_touch_ms))
        {
            st->long_latched = true;
            out->long_touch  = true;                /* 1회 래치 */
        }
    }
    else
    {
        st->long_latched = false;
    }

    st->prev_state = curr_state;
}
