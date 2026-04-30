/**
 * @file tdc_boot_led.c
 * @brief 부트로더 단계 LED 조기 점등 — SW PWM SKYBLUE fade.
 *
 * 작업: LED/bootloader-power-on-indicator
 * 활성 조건: main.h 의 TDC_BOOT_LED_ENABLE = 1 + 런타임 UART 검증 핀 비활성.
 * 비활성 조건: TDC_BOOT_LED_ENABLE = 0 (디폴트) → 본 .c 는 빈 컴파일 단위.
 */

#include <main.h>

#if TDC_BOOT_LED_ENABLE

#include <tdc_boot_led.h>

/* Step 1: 빈 stub. 빌드 시스템 통합 + 가드 OFF 회귀 검증용.
 * Step 2 에서 본체 채움 (Timer2 ISR, PWM, phase 머신). */

void tdc_boot_led_init(void)
{
    /* TODO Step 2: GPIO config + Timer2 setup + NVIC IRQ enable */
}

void tdc_boot_led_start(void)
{
    /* TODO Step 2: phase = FADE_IN, 카운터 reset, Timer2 start */
}

void tdc_boot_led_stop(void)
{
    /* TODO Step 2: NVIC disable + Timer2 stop + LED OFF */
}

bool tdc_boot_led_is_done(void)
{
    return true;  /* Step 2 까지는 wait 분기 즉시 통과 */
}

uint32_t tdc_boot_led_elapsed_ms(void)
{
    return 0;     /* Step 2 까지는 미시작으로 인식 → wait 분기 미진입 */
}

#endif /* TDC_BOOT_LED_ENABLE */
