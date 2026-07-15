/**
 * @file OTE_1_5gen_timer.c
 */

#include <ci_timer.h>
#include <LedOutput.h>  /* led_arbiter_tick - Timer 3 ISR 직접 구동 */

static bool    _ci_is_leap(uint16_t year);
static uint8_t _ci_day_in_month(uint8_t year, uint8_t month);

static volatile int g_ci_timer_main_tick = 1; /* CFX FIFO 가 증가 - 시스템 시간 전담 */
static volatile int g_tdc_timer_t3_tick  = 0; /* TIMER3 가 증가 - LED · 터치 초기화 동기 전담 */

static volatile uint32_t        _ci_timer_elapsed_1msec_counter = 0;
static volatile uint32_t        _ci_timer_count_init_value      = 0;
static volatile CI_TIMER_TIME_T _ci_timer_reference_time        = {0};
static volatile bool            _ci_timer_update_flag           = false;

void TIMER_3_IRQHandler(void)
{
    /* normal 모드: LED · 터치 공유 카운터 + LED arbiter 전담.
     * `g_ci_timer_main_tick` 증가와 `enable_iteration()` 호출은 CFX FIFO ISR
     * (`CFX_0_IRQHandler` / `FIFO_5_IRQHandler`) 가 담당 - 책임 분리. */
    g_tdc_timer_t3_tick++;
    led_arbiter_tick();

    // 절전모드에서 정말 원하는 시간 마다 타이머 이벤트가 발생하는지 확인하는 용도
    // Sys_GPIO_Toggle(DIO19);
}

static bool _ci_is_leap(uint16_t year)
{
    return ((year % 4) == 0) && (((year % 100) != 0) || ((year % 400) == 0));
}

static uint8_t _ci_day_in_month(uint8_t year, uint8_t month)
{
    static const uint8_t dim[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint16_t             y       = (uint16_t) (CI_TIMER_BASE_YEAR + (uint16_t) year);
    uint8_t              day     = dim[(uint8_t) (month - 1u)];

    if (month == 2 && _ci_is_leap(y))
    {
        day = 29;
    }

    return day;
}

void ci_timer_self_update_with_elapsed_1msec_counter(void)
{
    CI_TIMER_TIME_T time;
    uint32_t        cnt;
    uint8_t         dim;  // day in month;

    cnt  = _ci_timer_elapsed_1msec_counter;
    time = _ci_timer_reference_time;

    while (1000 <= cnt)
    {
        cnt -= 1000;

        // 초
        time.sec++;

        if (60 <= time.sec)
        {
            time.sec = 0;
            time.min++;
        }

        // 분
        if (60 <= time.min)
        {
            time.min = 0;
            time.hour++;
        }

        // 시
        if (24 <= time.hour)
        {
            time.hour = 0;
            time.day++;
        }

        // 일
        dim = _ci_day_in_month(time.year, time.month);
        if (dim < time.day)
        {
            time.day = 1;
            time.month++;
        }

        // 월
        if (12 < time.month)
        {
            time.month = 1;
            time.year++;
        }

        // 년 : uint8_t 형식에서 255년까지 지속될 일이 없으므로 오버플로우 생략
    }

    ci_timer_update_reference_time(&time, cnt);
}

CI_TIMER_TIME_T ci_timer_get_reference_time_after_self_update(void)
{
    ci_timer_self_update_with_elapsed_1msec_counter();
    return _ci_timer_reference_time;
}

void ci_timer_update_reference_time(CI_TIMER_TIME_T *p_time, uint32_t count_init_value)
{
    //_ci_timer_reference_time   = *p_time;
    _ci_timer_reference_time.year  = p_time->year;
    _ci_timer_reference_time.month = p_time->month;
    _ci_timer_reference_time.day   = p_time->day;
    _ci_timer_reference_time.hour  = p_time->hour;
    _ci_timer_reference_time.min   = p_time->min;
    _ci_timer_reference_time.sec   = p_time->sec;

    _ci_timer_count_init_value = count_init_value;

    _ci_timer_update_flag = true;

    ci_printv("[TIMER] UPDATE REFERENCE TIME {%02d-%02d-%02d-%02d-%02d-%02d} INIT VALUE {%d} \r\n", _ci_timer_reference_time.year, _ci_timer_reference_time.month, _ci_timer_reference_time.day, _ci_timer_reference_time.hour, _ci_timer_reference_time.min, _ci_timer_reference_time.sec, _ci_timer_count_init_value);
}

void ci_timer_increase_tick(void)
{
    g_ci_timer_main_tick++;
    _ci_timer_elapsed_1msec_counter++;

    if (_ci_timer_update_flag)
    {
        _ci_timer_elapsed_1msec_counter = _ci_timer_count_init_value;
        _ci_timer_count_init_value      = 0;
        _ci_timer_update_flag           = false;
    }
}

int ci_timer_get_tick(void)
{
    return g_ci_timer_main_tick;
}

int tdc_timer_get_t3_tick(void)
{
    return g_tdc_timer_t3_tick;
}

int ci_timer_init_prescaled(uint32_t prescale_field, uint32_t tick)
{
    Sys_Timer_Stop(OTE_1_5_GEN_TIMER_INSTANCE);

    NVIC_ClearPendingIRQ(OTE_1_5_GEN_TIMER_IRQn);
    NVIC_EnableIRQ(OTE_1_5_GEN_TIMER_IRQn);

    // Sys_DIO_Config(DIO19, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT));

    Sys_Timer_Config(OTE_1_5_GEN_TIMER_INSTANCE, prescale_field, TIMER_FREE_RUN, tick);
    Sys_Timer_Start(OTE_1_5_GEN_TIMER_INSTANCE);

    return df_True;
}

int ci_timer_init(uint32_t tick)
{
    return ci_timer_init_prescaled(TIMER_PRESCALE_1, tick);
}

int ci_timer_uninit(void)
{
    Sys_Timer_Stop(OTE_1_5_GEN_TIMER_INSTANCE);

    NVIC_DisableIRQ(OTE_1_5_GEN_TIMER_IRQn);
    NVIC_ClearPendingIRQ(OTE_1_5_GEN_TIMER_IRQn);

    return df_True;
}
