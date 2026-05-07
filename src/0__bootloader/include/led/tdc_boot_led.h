#ifndef __tdc_boot_led_h__
#define __tdc_boot_led_h__

#include <stdbool.h>
#include <stdint.h>

/* 부트로더 단계 LED 조기 점등 (작업: LED/bootloader-power-on-indicator).
 *
 * 본 모듈은 main.h 의 TDC_BOOT_LED_ENABLE 빌드 가드 활성 시에만 동작한다.
 * 외부 (bootloader.c) 는 자기 측 가드 안에서 본 API 호출.
 * 가드 OFF 시 본 .c 의 본체가 컴파일에서 제외되어 빈 컴파일 단위가 되며,
 * 함수 선언은 헤더에 남지만 호출 측이 가드 안이라 link 영향 없음. */

/* GPIO config (G/B = active HIGH output) + Timer2 setup + NVIC IRQ enable.
 * 호출 후 ISR 가 동작 가능 상태. start() 가 호출되어야 phase 진입.
 * 멱등 — 중복 호출 안전 (이미 init 됐으면 no-op). */
void tdc_boot_led_init(void);

/* FADE_IN phase 진입 — ISR tick 으로 카운터 증가, 360 ms 진행
 * (OFF 0, CM3 진입 후 자연 OFF 흡수 — 사후 갱신 2026-05-07).
 * init() 선행 필요. */
void tdc_boot_led_start(void);

/* 즉시 정지 — Timer2 stop + NVIC IRQ disable + LED 핀 GPIO LOW write (OFF).
 * 분석.md §1.4 O-3 의무. CM3 점프 직전 호출. */
void tdc_boot_led_stop(void);

/* Phase == DONE 도달 여부. 360 ms 경과 시 true. */
bool tdc_boot_led_is_done(void);

/* start() 이후 경과 시간 ms. wait 로직에서 start() 호출 여부 판단에도 사용
 * (== 0 이면 미시작 케이스 = 가드 ON + UART 활성 분기). */
uint32_t tdc_boot_led_elapsed_ms(void);

#endif /* __tdc_boot_led_h__ */
