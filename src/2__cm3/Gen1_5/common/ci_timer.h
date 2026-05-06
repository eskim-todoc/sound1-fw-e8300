/**
 * @file OTE_1_5gen_timer.h
 */

#ifndef __OTE_1_5gen_timer_h__
#define __OTE_1_5gen_timer_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <main.h>
#include <processorDirective.h>

#define OTE_1_5_GEN_TIMER_INSTANCE TIMER3
#define OTE_1_5_GEN_TIMER_IRQn     TIMER_3_IRQn

#if (PCM_CLK_SRC == PCM_CLK_SRC_USRCLK)
#define OTE_1_5_GEN_TIMER_TICK_1MS_PM_POR    39  /* SYSCLK 7.68MHz */
#define OTE_1_5_GEN_TIMER_TICK_1MS_PM_NORMAL 39  // 159 /* SYSCLK 30.72MHz */
#elif (PCM_CLK_SRC == PCM_CLK_SRC_SLOWCLK)
#define OTE_1_5_GEN_TIMER_TICK_1MS_PM_POR    14 /* SYSCLK 7.68MHz */
#define OTE_1_5_GEN_TIMER_TICK_1MS_PM_NORMAL 14 /* SYSCLK 7.68MHz */
#else
#error "Invalid 'PCM_CLK_SRC'"
#endif

// "Very important!" : SLOWCLK_32 is disabled when using the standby clock. So, Calculating the timer delay must be
// using SLOWCLK.
#define OTE_1_5_GEN_TIMER_TICK_500MS_PM_LP 10999

#define CI_TIMER_BASE_YEAR 2000

typedef struct
{
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
} CI_TIMER_TIME_T;

void TIMER_3_IRQHandler(void);
void ci_timer_increase_tick(void);
int  ci_timer_get_tick(void);

/* TIMER3 전담 카운터(`g_tdc_timer_t3_tick`) 조회.
 * `ci_timer_get_tick()` 의 `g_ci_timer_main_tick` 는 CFX FIFO 가 증가시키는
 * 시스템 시간 전담 카운터이며, 본 카운터는 LED · 터치 초기화 동기에만 사용. */
int  tdc_timer_get_t3_tick(void);

/* PRESCALE 고정(=1) 초기화. 기존 호출자 호환용.
 * 내부적으로 ci_timer_init_prescaled(TIMER_PRESCALE_1, tick) 호출. */
int  ci_timer_init(uint32_t tick);

/* PRESCALE·TIMEOUT 둘 다 지정. 긴 주기 타이머(예: ULP 모드 500 ms) 용.
 * 공식: T = 2^prescale_field × (tick + 1) / 40 kHz  (SLOWCLK=1.28 MHz 기준)
 *   prescale_field: TIMER_PRESCALE_1 ~ _128 중 하나
 *   tick: TIMEOUT_VALUE 필드 (24-bit) */
int  ci_timer_init_prescaled(uint32_t prescale_field, uint32_t tick);

int  ci_timer_uninit(void);

CI_TIMER_TIME_T ci_timer_get_reference_time_after_self_update(void);
void            ci_timer_update_reference_time(CI_TIMER_TIME_T *p_time, uint32_t count_init_value);

#endif  // __OTE_1_5gen_timer_h__
