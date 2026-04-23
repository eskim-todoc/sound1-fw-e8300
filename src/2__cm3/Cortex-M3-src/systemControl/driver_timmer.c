
#include <ci_timer.h>
#include <stdbool.h>
#include <LedOutput.h>  /* led_arbiter_tick — CFX ISR 직접 구동 */

// Ezairo7150의 CM3에는 타이머가 들어있지 않다.
// CFX에서 일정주기의 인터럽트를 수신하여 타이머처럼 동작시킨다.

void enable_iteration(void);

void CFX_0_IRQHandler(void)
{
    enable_iteration();
    ci_timer_increase_tick();  // 시스템 사용 시간, 로그 시간 등을 위해 카운터 증가

    /* LED arbiter/engine/PWM — main loop 의 I2C/EEPROM 폴링 블록으로 인한
     * fade/PWM 타이밍 jitter 방지 목적. CFX 가 평상시 1ms tick 소스. */
    led_arbiter_tick();
}

void FIFO_5_IRQHandler(void)
{
    // FIFO 이벤트 발생 시 CFX가 아니라, FIFO 이벤트 자체를 직접 받음
    CFX_0_IRQHandler();
}
