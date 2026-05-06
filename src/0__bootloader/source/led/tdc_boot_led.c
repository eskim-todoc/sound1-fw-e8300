/**
 * @file tdc_boot_led.c
 * @brief 부트로더 단계 LED 조기 점등 — SW PWM SKYBLUE fade.
 *
 * 작업: LED/bootloader-power-on-indicator
 * 활성 조건: main.h 의 TDC_BOOT_LED_ENABLE = 1 + 런타임 UART 검증 핀 비활성.
 * 비활성 조건: TDC_BOOT_LED_ENABLE = 0 (디폴트) → 본 .c 는 빈 컴파일 단위.
 *
 * 클럭 체인 (Step 3 보정 후):
 *   SYSCLK 30.72 MHz (부트로더 T-B 시점)
 *     → SLOWCLK = SYSCLK / 24 = 1.28 MHz   (D_CLK->CFG_1 의 SLOWCLK_PRESCALE_24)
 *     → Timer = SLOWCLK / 32 = 40 kHz       (RSL10 General Timer 의 고정 분주, ci_timer.h:56)
 *   ISR 주기 = (tick+1) / 40 kHz, prescale=TIMER_PRESCALE_1 (= 2^0).
 *   tick=0 → ISR 25 us 주기 (= 40 kHz, Timer 최소 분해능).
 *
 *   PWM 분해능 100 bin, PWM 주기 = 100 × 25 us = 2.5 ms = 400 Hz (깜빡임 안전).
 *   1 ms 단위 phase tick 은 ISR 40 회 마다 호출 (40 × 25 us = 1 ms).
 *
 *   부트로더는 D_CLK 디폴트 분주값으로 시작하므로 본 init 에서 CM3 ci_power.c:154-156
 *   와 동일한 설정 (SLOWCLK 1.28 MHz) 명시 적용 + 5 ms 안정화 대기 후 Timer 가동.
 *
 * 색: k_led_mix[en__LED_SKYBLUE] = {R=0, G=30, B=40} (LedOutput.c 참조) 비율 차용.
 *     R 채널 미사용. G/B 만 active HIGH PWM.
 */

#include <main.h>

#if TDC_BOOT_LED_ENABLE

#include <hw.h>
#include <tdc_boot_led.h>

/* SW PWM 매개변수 — 분석.md D-2/D-5 + Step 3 보정 (Timer 40 kHz 기반) */

/* ISR 주기 = 25 us (= 1 / 40 kHz, Timer prescale_1 + tick 0).
 * Timer 40 kHz 가 본 모듈의 시간 분해능 한계. */
#define TDC_BOOT_LED_TIMER_RELOAD     0     /* (tick+1)/40 kHz = 25 us */

/* PWM 분해능 100, 주기 100 × 25 us = 2.5 ms = 400 Hz */
#define TDC_BOOT_LED_PWM_RESOLUTION   100

/* 1 ms 단위 phase tick — ISR 40 회 마다 1 ms 카운터 증가 */
#define TDC_BOOT_LED_ISR_PER_MS       40    /* 1 ms / 25 us */

/* k_led_mix[en__LED_SKYBLUE] = {R=0, G=30, B=40} 차용. PWM_RESOLUTION = 100 이라
 * G duty 30/100 = 30%, B duty 40/100 = 40%. */
#define TDC_BOOT_LED_DUTY_G_MAX       30
#define TDC_BOOT_LED_DUTY_B_MAX       40

/* Phase 시간 (ms) — D-5 (사후 갱신 2026-05-07: OFF 180 → 0).
 * OFF wait 는 CM3 진입 직후 자체 초기화 시간으로 자연 흡수 — 부트로더 wait 100 ms 단축.
 * 사용자가 보는 LED 패턴 (SKYBLUE fade → OFF → 정식 LED_ST_POWER_ON) 은 동일. */
#define TDC_BOOT_LED_T_FADE_IN_MS     30
#define TDC_BOOT_LED_T_PEAK_MS        300
#define TDC_BOOT_LED_T_FADE_OUT_MS    30
#define TDC_BOOT_LED_T_OFF_MS         0
#define TDC_BOOT_LED_T_TOTAL_MS       (TDC_BOOT_LED_T_FADE_IN_MS \
                                     + TDC_BOOT_LED_T_PEAK_MS \
                                     + TDC_BOOT_LED_T_FADE_OUT_MS \
                                     + TDC_BOOT_LED_T_OFF_MS)  /* 360 */

typedef enum {
    TDC_BOOT_LED_PHASE_IDLE = 0,
    TDC_BOOT_LED_PHASE_FADE_IN,
    TDC_BOOT_LED_PHASE_PEAK,
    TDC_BOOT_LED_PHASE_FADE_OUT,
    TDC_BOOT_LED_PHASE_OFF,
    TDC_BOOT_LED_PHASE_DONE,
} tdc_boot_led_phase_t;

/* ISR <-> main 공유 — volatile 필수. ISR 만 갱신, public read API 가 read. */
static volatile tdc_boot_led_phase_t s_tdc_boot_led_phase       = TDC_BOOT_LED_PHASE_IDLE;
static volatile uint32_t             s_tdc_boot_led_pwm_bin     = 0;   /* 0~PWM_RESOLUTION-1 */
static volatile uint32_t             s_tdc_boot_led_isr_in_ms   = 0;   /* 0~ISR_PER_MS-1 */
static volatile uint32_t             s_tdc_boot_led_elapsed_ms  = 0;   /* start 이후 누적 ms */
static volatile uint8_t              s_tdc_boot_led_duty_g      = 0;
static volatile uint8_t              s_tdc_boot_led_duty_b      = 0;
static          bool                 s_tdc_boot_led_initialized = false;

/* ISR 컨텍스트 호출 — 1 ms 마다 1 회 (= ISR 40 회 마다).
 * 본 함수가 phase, duty_g, duty_b 갱신. */
static void phase_tick(void)
{
    uint32_t t = s_tdc_boot_led_elapsed_ms;

    switch (s_tdc_boot_led_phase)
    {
        case TDC_BOOT_LED_PHASE_FADE_IN:
        {
            if (t >= TDC_BOOT_LED_T_FADE_IN_MS)
            {
                s_tdc_boot_led_phase  = TDC_BOOT_LED_PHASE_PEAK;
                s_tdc_boot_led_duty_g = TDC_BOOT_LED_DUTY_G_MAX;
                s_tdc_boot_led_duty_b = TDC_BOOT_LED_DUTY_B_MAX;
            }
            else
            {
                s_tdc_boot_led_duty_g = (uint8_t) ((TDC_BOOT_LED_DUTY_G_MAX * t)
                                                  / TDC_BOOT_LED_T_FADE_IN_MS);
                s_tdc_boot_led_duty_b = (uint8_t) ((TDC_BOOT_LED_DUTY_B_MAX * t)
                                                  / TDC_BOOT_LED_T_FADE_IN_MS);
            }
            break;
        }
        case TDC_BOOT_LED_PHASE_PEAK:
        {
            if (t >= TDC_BOOT_LED_T_FADE_IN_MS + TDC_BOOT_LED_T_PEAK_MS)
            {
                s_tdc_boot_led_phase = TDC_BOOT_LED_PHASE_FADE_OUT;
            }
            break;
        }
        case TDC_BOOT_LED_PHASE_FADE_OUT:
        {
            uint32_t fo_t = t - (TDC_BOOT_LED_T_FADE_IN_MS + TDC_BOOT_LED_T_PEAK_MS);
            if (fo_t >= TDC_BOOT_LED_T_FADE_OUT_MS)
            {
                s_tdc_boot_led_phase  = TDC_BOOT_LED_PHASE_OFF;
                s_tdc_boot_led_duty_g = 0;
                s_tdc_boot_led_duty_b = 0;
            }
            else
            {
                s_tdc_boot_led_duty_g = (uint8_t) ((TDC_BOOT_LED_DUTY_G_MAX
                                                  * (TDC_BOOT_LED_T_FADE_OUT_MS - fo_t))
                                                  / TDC_BOOT_LED_T_FADE_OUT_MS);
                s_tdc_boot_led_duty_b = (uint8_t) ((TDC_BOOT_LED_DUTY_B_MAX
                                                  * (TDC_BOOT_LED_T_FADE_OUT_MS - fo_t))
                                                  / TDC_BOOT_LED_T_FADE_OUT_MS);
            }
            break;
        }
        case TDC_BOOT_LED_PHASE_OFF:
        {
            if (t >= TDC_BOOT_LED_T_TOTAL_MS)
            {
                s_tdc_boot_led_phase = TDC_BOOT_LED_PHASE_DONE;
            }
            break;
        }
        case TDC_BOOT_LED_PHASE_DONE:
        case TDC_BOOT_LED_PHASE_IDLE:
        default:
            break;
    }
}

/* Timer2 ISR — 25 us 주기 (40 kHz, Timer 분해능 한계).
 *
 * Cortex-M3 NVIC 가 ISR entry/exit 시 active bit 자동 set/clear → ISR 본문 ack 호출 없음.
 * peripheral pending bit 도 RSL10 Timer 는 hardware auto-clear 가정 (Step 측정에서 재확인).
 *
 * 부트로더 외 다른 ISR 없음 (분석 §1 F-3, F-4) → uninterrupted.
 * 처리 시간 ~50 cycles @ 30.72 MHz ≈ 1.6 us / 25 us 주기 = 6% CPU. 무관. */
void TIMER_2_IRQHandler(void)
{
    /* PWM bin 진행 (0~99 cycle, 100 → 0 wrap) */
    uint32_t bin = s_tdc_boot_led_pwm_bin + 1;
    if (bin >= TDC_BOOT_LED_PWM_RESOLUTION)
    {
        bin = 0;
    }
    s_tdc_boot_led_pwm_bin = bin;

    /* PWM 출력 — active HIGH (D-8). bin < duty 면 ON (HIGH), else OFF (LOW). */
    Sys_GPIO_Write(DIO_NUM_LED_G_UART_RX_E8300, (bin < s_tdc_boot_led_duty_g) ? 1 : 0);
    Sys_GPIO_Write(DIO_NUM_LED_B,               (bin < s_tdc_boot_led_duty_b) ? 1 : 0);

    /* 1 ms 카운터 — 40 ISR 마다 1 ms 경과 */
    uint32_t isr_in_ms = s_tdc_boot_led_isr_in_ms + 1;
    if (isr_in_ms >= TDC_BOOT_LED_ISR_PER_MS)
    {
        isr_in_ms = 0;
        s_tdc_boot_led_elapsed_ms++;
        phase_tick();
    }
    s_tdc_boot_led_isr_in_ms = isr_in_ms;
}

void tdc_boot_led_init(void)
{
    if (s_tdc_boot_led_initialized)
    {
        return;
    }

    /* GPIO config — G/B 만 output 활성. R 핀은 hi-Z 유지 (UART TX 와 공유, 본 작업 미사용).
     * D-8: active HIGH 라 디폴트 LOW write 면 LED OFF.
     * 풀다운 자연 OFF 상태에서 정확히 OFF 출력으로 전환 — 스파이크 없음. */
    Sys_GPIO_Set_Low(DIO_NUM_LED_G_UART_RX_E8300);  /* OFF (active HIGH) */
    Sys_GPIO_Set_Low(DIO_NUM_LED_B);                /* OFF */
    Sys_DIO_Config(DIO_NUM_LED_G_UART_RX_E8300, DIO_CFG_LED);
    Sys_DIO_Config(DIO_NUM_LED_B,               DIO_CFG_LED);

    /* 클럭 분주 설정 — CM3 ci_power.c:154-156 동일. SYSCLK 30.72 MHz 가정.
     * SLOWCLK_PRESCALE_24 → SLOWCLK = 1.28 MHz → Timer = SLOWCLK/32 = 40 kHz.
     * UARTCLK_SRC_SYSCLK 유지 → 부트로더 UART (이미 30.72 MHz 가정으로 init) 영향 없음. */
    D_CLK->CFG_1 = (ADCCLK_PRESCALE_8 | ADCCLK_SRC_SYSCLK | SDMCLK_PRESCALE_2
                  | SLOWCLK_PRESCALE_24 | SLOWCLK_SRC_SYSCLK | UARTCLK_SRC_SYSCLK);
    D_CLK->CFG_2 = (UCLK_PRESCALE_8 | UCLK_SRC_ADCCLK);

    tdc_delay_ms(5);  /* 시스템 클럭 안정화 대기 — CM3 ci_power.c:158 동일 */

    /* Timer2 setup — 25 us tick (40 kHz). 분석 §1.4 O-1 (PRAM5 배치): 본 .c 가 부트로더
     * sections.ld 로 link 되므로 자동. ci_timer.c 와 동일 SDK API 사용. */
    Sys_Timer_Stop(TIMER2);  /* 안전 측 (이전 가동 상태 정리) */
    Sys_Timer_Config(TIMER2, TIMER_PRESCALE_1, TIMER_FREE_RUN, TDC_BOOT_LED_TIMER_RELOAD);

    /* O-2: NVIC IRQ enable (부트로더 main.c:40 의 Sys_NVIC_DisableAllInt 이후 명시적 enable).
     * init 시점의 pending clear 는 잔여 pending 보호 차원으로 1 회 수행 — ISR 본문 ack 와 별개. */
    NVIC_ClearPendingIRQ(TIMER_2_IRQn);
    NVIC_SetPriority(TIMER_2_IRQn, 0);  /* O-4: 부트로더에 다른 ISR 없음, 우선순위 무관 */
    NVIC_EnableIRQ(TIMER_2_IRQn);

    s_tdc_boot_led_initialized = true;
}

void tdc_boot_led_start(void)
{
    /* 상태 초기화 + Timer2 가동.
     * 가동 직전에 reset — start() 가 init() 직후 즉시 호출되는 케이스에 ISR 이
     * 깨끗한 상태에서 시작하도록. */
    s_tdc_boot_led_phase      = TDC_BOOT_LED_PHASE_FADE_IN;
    s_tdc_boot_led_pwm_bin    = 0;
    s_tdc_boot_led_isr_in_ms  = 0;
    s_tdc_boot_led_elapsed_ms = 0;
    s_tdc_boot_led_duty_g     = 0;
    s_tdc_boot_led_duty_b     = 0;

    Sys_Timer_Start(TIMER2);
}

void tdc_boot_led_stop(void)
{
    /* O-3: NVIC disable + Timer 정지 + LED OFF + 상태 초기화 */
    NVIC_DisableIRQ(TIMER_2_IRQn);
    Sys_Timer_Stop(TIMER2);

    Sys_GPIO_Set_Low(DIO_NUM_LED_G_UART_RX_E8300);  /* OFF (active HIGH) */
    Sys_GPIO_Set_Low(DIO_NUM_LED_B);

    s_tdc_boot_led_phase       = TDC_BOOT_LED_PHASE_DONE;
    s_tdc_boot_led_initialized = false;
}

bool tdc_boot_led_is_done(void)
{
    return (s_tdc_boot_led_phase == TDC_BOOT_LED_PHASE_DONE);
}

uint32_t tdc_boot_led_elapsed_ms(void)
{
    return s_tdc_boot_led_elapsed_ms;
}

#endif /* TDC_BOOT_LED_ENABLE */
