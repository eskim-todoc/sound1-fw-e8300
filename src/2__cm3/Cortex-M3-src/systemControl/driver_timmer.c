
#include <ci_timer.h>
#include <stdbool.h>

// Ezairo7150의 CM3에는 타이머가 들어있지 않다.
// CFX에서 일정주기의 인터럽트를 수신하여 타이머처럼 동작시킨다.

void enable_iteration(void);

void CFX_0_IRQHandler(void)
{
    enable_iteration();
    ci_timer_increase_tick();  // 시스템 사용 시간, 로그 시간 등을 위해 카운터 증가

    /* LED arbiter 호출은 TIMER_3_IRQHandler (`ci_timer.c`) 가 전담 — ISR 책임 분리. */
}

void FIFO_5_IRQHandler(void)
{
    // FIFO 이벤트 발생 시 CFX가 아니라, FIFO 이벤트 자체를 직접 받음
    CFX_0_IRQHandler();
}
