/*
 * tdc_touch.c
 *
 * 터치 연결층 + 초기화 상태머신. 3레이어 탈피 간결 재작성 2026-06-20.
 *
 *   순수 제어 FSM(tdc_touch_logic)과 IQS323 직접 접근(tdc_touch_iqs323)을 잇는 얇은 연결.
 *   책임 4가지: ① init 상태머신(MCLR -> auto-ATI 폴링 -> apply_settings -> READY),
 *   ② 폴링 게이팅(ms 차분), ③ read -> FSM 입력 정규화(실패 시 전부 false),
 *   ④ FSM 액션 -> IQS323 직접 호출 + 로그. 터치 판정 로직은 전부 logic 에 위임한다.
 *
 *   공개 API 시그니처 불변 -> main.c / initialize.c 호출처 0변경.
 *   절전 ULP 는 main.c func_sleep 이 get_state 를 직접 호출(순수 FSM 미경유).
 */

#include <tdc_touch.h>
#include <tdc_touch_logic.h>
#include <tdc_touch_iqs323.h>
#include <tdc_touch_time.h>

#include <hw.h>
#include <tdc_hal_timer.h>
#include <tdc_printf.h>
#include <tdc_util.h> /* tdc_util_delay_ms() - MCLR 전 RTT 드레인 */

#include <LedOutput.h> /* led_request() - 부팅 터치 무시 디버그 피드백 */

/* 초기화 상태 (HW 본질이라 순수 FSM 밖, 연결 소유). */
typedef enum
{
    TDC_TOUCH_INIT_NONE = 0,  /* begin() 전 */
    TDC_TOUCH_INIT_MCLR_DONE, /* MCLR 완료, auto-ATI 대기 */
    TDC_TOUCH_INIT_READY      /* 설정 완료, 폴링 가능 */
} tdc_touch_init_state_t;

/* 연결 소유 static (최소). FSM 상태는 s_logic 단일 구조체로 격리. */
static tdc_touch_init_state_t  s_init_state     = TDC_TOUCH_INIT_NONE;
static int                     s_mclr_done_tick = 0; /* t3 tick 기준 */
static int                     s_poll_tick_old  = 0; /* 폴링 게이팅 기준 */
static tdc_touch_logic_state_t s_logic;
static tdc_touch_state_t       s_log_prev_state = TDC_TOUCH_STATE_RESET;

/* 무선 통신을 통한 디버깅을 위한 측정 값 저장 버퍼 */
static uint16_t s_debug_recent_lta;
static uint16_t s_debug_recent_count;
static uint16_t s_debug_recent_delta;
static uint16_t s_debug_recent_abs_thr;
static uint8_t  s_debug_recent_pressed;
static uint8_t  s_debug_recent_ati_error;
static uint8_t  s_debug_recent_ati_active;

/* debug getter 계열: BLE 0x8F option 1(터치센서 디버깅 프로토콜)이 사용한다.
 * 짝이던 set_recent_* 7개는 fake_func_sleep() 이 유일한 호출자였으므로 함께
 * 제거했다(2026-07-20). 값 공급은 tdc_touch_process() 가 아래 s_debug_recent_*
 * 를 매 tick 직접 대입하는 경로로 유지된다.
 * 상세: docs/tasks/main/20260720_fake-sleep-removal/ */
uint16_t tdc_touch_debug_get_recent_lta(void)
{
    return s_debug_recent_lta;
}

uint16_t tdc_touch_debug_get_recent_count(void)
{
    return s_debug_recent_count;
}

uint16_t tdc_touch_debug_get_recent_delta(void)
{
    return s_debug_recent_delta;
}

uint16_t tdc_touch_debug_get_recent_abs_thr(void)
{
    return s_debug_recent_abs_thr;
}

uint8_t tdc_touch_debug_get_recent_pressed(void)
{
    return s_debug_recent_pressed;
}

uint8_t tdc_touch_debug_get_recent_ati_error(void)
{
    return s_debug_recent_ati_error;
}

uint8_t tdc_touch_debug_get_recent_ati_active(void)
{
    return s_debug_recent_ati_active;
}

const char *tdc_touch_state_name(tdc_touch_state_t s)
{
    switch (s)
    {
        case TDC_TOUCH_STATE_RESET:
        {
            return "RESET";
        }
        case TDC_TOUCH_STATE_TOUCH:
        {
            return "TOUCH";
        }
        case TDC_TOUCH_STATE_NOT_TOUCH:
        {
            return "NOT_TOUCH";
        }
        case TDC_TOUCH_STATE_CALIBRATION_ERROR:
        {
            return "CAL_ERROR";
        }
        default:
        {
            return "?";
        }
    }
}

/* 초기화 - auto-ATI 완료 감지 후 설정 일괄 적용, READY 전이, 부팅 터치 판정. */
static void try_finish_init(void)
{
    if (!tdc_touch_iqs323_is_ati_done())
    {
        if (TDC_TOUCH_INIT_TIMEOUT_MS >= (tdc_hal_timer_get_t3_tick() - s_mclr_done_tick))
        {
            return; /* 대기 - 다음 tick 재시도 */
        }
        TDC_PRINTF_W("[TOUCH] AUTO-ATI: TIMEOUT, FORCING FINISH \r\n");
    }

    SYS_WATCHDOG_REFRESH();
    tdc_touch_iqs323_apply_settings(); /* Full ATI / 임계 / Beta / Power / 부팅 Re-ATI */
    SYS_WATCHDOG_REFRESH();

    uint32_t now = (uint32_t) tdc_hal_timer_get_tick();
    tdc_touch_logic_init(&s_logic, now);
    s_log_prev_state = TDC_TOUCH_STATE_RESET;
    s_poll_tick_old  = (int) now;
    s_init_state     = TDC_TOUCH_INIT_READY;
    TDC_PRINTF_I("[TOUCH] INIT FINISH DONE \r\n");

    /* 누른 채 부팅 방어 - 첫 read 로 터치 판정 후 FSM 에 무시 설정. */
    tdc_touch_iqs323_status_t st;
    if (tdc_touch_iqs323_read_status(&st) && st.pressed)
    {
        tdc_touch_logic_set_boot_ignore(&s_logic, now);
        TDC_PRINTF_W("[TOUCH] BOOT TOUCH ignoring until released \r\n");
    }
}

void tdc_touch_init_begin(void)
{
    TDC_PRINTF_I("[TOUCH] INIT BEGIN \r\n");

    SYS_WATCHDOG_REFRESH();
    tdc_touch_iqs323_mclr();

    s_mclr_done_tick = tdc_hal_timer_get_t3_tick();
    s_init_state     = TDC_TOUCH_INIT_MCLR_DONE;
}

void led_debug_blink_blue(int cnt, int on_ms, int off_ms)
{
    int lap_end;

    for (int i = 0; i < cnt; i++)
    {
        lap_end = tdc_hal_timer_get_tick() + on_ms;
        while (tdc_hal_timer_get_tick() < lap_end)
        {
            turnON_BlueLED();
            SYS_WATCHDOG_REFRESH();
        }

        lap_end = tdc_hal_timer_get_tick() + off_ms;
        while (tdc_hal_timer_get_tick() < lap_end)
        {
            Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
            Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
            Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
            SYS_WATCHDOG_REFRESH();
        }
    }

    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
}

void led_debug_blink_12bits(uint16_t bits)
{
    int lap_end;

    for (int i = 0; i < 12; i++)
    {
        uint8_t bit = (bits >> (11 - i)) & 1;

        lap_end = tdc_hal_timer_get_tick() + 500;

        while (tdc_hal_timer_get_tick() < lap_end)
        {
            if (bit == 0)
            {
                turnON_GreenLED();
                SYS_WATCHDOG_REFRESH();
            }
            else
            {
                turnON_RedLED();
                SYS_WATCHDOG_REFRESH();
            }
        }

        lap_end = tdc_hal_timer_get_tick() + 500;

        while (tdc_hal_timer_get_tick() < lap_end)
        {
            Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
            Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
            Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
            SYS_WATCHDOG_REFRESH();
        }
    }
}

void tdc_touch_led_debug(uint16_t lta, uint16_t count, uint16_t delta)
{
    int lap_start;
    int lap_end;

    // lta
    led_debug_blink_blue(1, 1000, 0);
    led_debug_blink_12bits(lta);

    // count
    led_debug_blink_blue(2, 600, 300);
    led_debug_blink_12bits(count);

    // delta
    led_debug_blink_blue(3, 300, 200);
    led_debug_blink_12bits(delta);
}

bool tdc_touch_process(void)
{
    /* --- init 상태머신 (READY 전) --- */
    switch (s_init_state)
    {
        case TDC_TOUCH_INIT_NONE:
        {
            return false; /* begin() 전 no-op */
        }
        case TDC_TOUCH_INIT_MCLR_DONE:
        {
            try_finish_init();
            return false;
        }
        case TDC_TOUCH_INIT_READY:
        default:
        {
            break;
        }
    }

    /* --- 폴링 게이팅 (ms 차분) --- */
    int now = tdc_hal_timer_get_tick();
    if (TDC_TOUCH_POLL_INTERVAL_MS > (now - s_poll_tick_old))
    {
        return false;
    }
    s_poll_tick_old = now;

    /* --- read -> FSM 입력 정규화 (실패 시 status 가 전부 false 보장) --- */
    tdc_touch_iqs323_status_t st;
    tdc_touch_in_t            in;
    in.read_ok    = tdc_touch_iqs323_read_status(&st);
    in.now_ms     = (uint32_t) now;
    in.pressed    = st.pressed;
    in.ati_error  = st.ati_error;
    in.ati_active = st.ati_active;

#if (TDC_TOUCH_DEBUG_PRINT_ENABLE)
    /* --- 디버그 계측 (LTA/Counts/절대임계/밴드초과) - 실측 튜닝용. read_ok 시에만 --- */
    if (in.read_ok)
    {
        tdc_touch_iqs323_debug_t dbg;
        if (tdc_touch_iqs323_read_debug(&dbg) && dbg.ok)
        {
            /* self-cap: 터치 시 counts 감소 -> delta(=LTA-Counts) 증가. 절대임계 = 계수 x LTA / 256.
             * 밴드초과(터치 판정 방향)는 delta > abs_thr 로 본다(counts 직접 비교 아님). */
            uint16_t delta    = (dbg.lta > dbg.counts) ? (uint16_t) (dbg.lta - dbg.counts) : 0;
            uint16_t abs_thr  = (uint16_t) (((uint32_t) TDC_TOUCH_IQS323_THRESHOLD * dbg.lta) / 256u);
            uint16_t pabs_thr = (uint16_t) (((uint32_t) TDC_TOUCH_IQS323_PROX_THRESHOLD * dbg.lta) / 256u);

#if 0
            TDC_PRINTF_D("[T] LTA=%3u  CNT=%3u  D=%3u  THR=%3u (k=%3u  H=%3u)  %s   PTHR=%3u (pk=%3u)  %s \r\n",  //
                      dbg.lta,
                      dbg.counts,
                      delta,
                      abs_thr,
                      TDC_TOUCH_IQS323_THRESHOLD,
                      TDC_TOUCH_IQS323_HYSTERESIS,
                      in.pressed ? "T" : ".",
                      pabs_thr,
                      TDC_TOUCH_IQS323_PROX_THRESHOLD,
                      st.prox ? "P" : ".");
#endif
            s_debug_recent_lta        = dbg.lta;
            s_debug_recent_count      = dbg.counts;
            s_debug_recent_delta      = delta;
            s_debug_recent_abs_thr    = abs_thr;
            s_debug_recent_pressed    = in.pressed ? 1 : 0;
            s_debug_recent_ati_error  = in.ati_error ? 1 : 0;
            s_debug_recent_ati_active = in.ati_active ? 1 : 0;

#if 0 /* --- LED를 사용한 터치 디버깅 --- */
            tdc_touch_led_debug(dbg.lta, dbg.counts, delta);
#endif
        }
    }
#endif

    /* --- ATI 에러 감지(드리프트 신호) 경고 - Re-ATI 게이트 조건과 동일 시점 --- */
    if (in.read_ok && in.ati_error && !in.ati_active)
    {
        TDC_PRINTF_W("[TOUCH] ATI ERROR (drift) \r\n");
    }

    /* --- 순수 FSM 1회 --- */
    tdc_touch_out_t out;
    tdc_touch_logic_step(&s_logic, &in, &out);

    /* --- 로그 --- */
    if (out.state_changed)
    {
        TDC_PRINTF_V("[TOUCH] STATE: %s -> %s \r\n", tdc_touch_state_name(s_log_prev_state), tdc_touch_state_name(out.curr_state));
        s_log_prev_state = out.curr_state;
    }
    switch (out.boot_event)
    {
        case TDC_TOUCH_BOOT_RELEASED:
        {
            TDC_PRINTF_I("[TOUCH] BOOT TOUCH RELEASED, sensing resumed \r\n");
            break;
        }
        case TDC_TOUCH_BOOT_WARN_5S:
        {
            TDC_PRINTF_W("[TOUCH] BOOT TOUCH > 5s \r\n");
            led_request(LED_SRC_DBG, LED_ST_DBG_LONG_TOUCH_IGNORE);
            break;
        }
        case TDC_TOUCH_BOOT_IGNORING:
        case TDC_TOUCH_BOOT_NONE:
        default:
        {
            break;
        }
    }

    /* --- 액션 -> IQS323 직접 호출 --- */
    switch (out.action)
    {
        case TDC_TOUCH_ACT_RE_ATI:
        {
            /* 원인 구분: NOT_TOUCH 경로 = 드리프트 게이트, TOUCH 경로 = stuck stage2(게이트 우회). */
            if (out.curr_state == TDC_TOUCH_STATE_TOUCH)
            {
                TDC_PRINTF_I("[TOUCH] RE-ATI (stuck stage2) \r\n");
            }
            else
            {
                TDC_PRINTF_I("[TOUCH] RE-ATI (drift) \r\n");
            }
            (void) tdc_touch_iqs323_re_ati();
            break;
        }
        case TDC_TOUCH_ACT_RESEED:
        {
            TDC_PRINTF_I("[TOUCH] RESEED (stuck stage1) \r\n");
            (void) tdc_touch_iqs323_reseed();
            break;
        }
        case TDC_TOUCH_ACT_MCLR:
        {
            TDC_PRINTF_I("\r\n[TOUCH] STUCK -> MCLR RESET \r\n");
            tdc_util_delay_ms(20); /* RTT 드레인 */
            SYS_WATCHDOG_RESET();
            break;
        }
        case TDC_TOUCH_ACT_HOLD:
        case TDC_TOUCH_ACT_NONE:
        default:
        {
            break;
        }
    }

    if (out.long_touch)
    {
        TDC_PRINTF_I("\r\n[TOUCH] EVENT: LONG TOUCH \r\n");
        return true; /* 절전 트리거 */
    }
    return false;
}

bool tdc_touch_get_state(tdc_touch_state_t *p_state)
{
    tdc_touch_iqs323_status_t st;

    if (!tdc_touch_iqs323_read_status(&st))
    {
        return false;
    }
    *p_state = st.pressed ? TDC_TOUCH_STATE_TOUCH : TDC_TOUCH_STATE_NOT_TOUCH;
    return true;
}
