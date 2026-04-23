# LED 색감·타이밍 보강 구현계획

작성자: 김은수
작성일: 2026-04-23
상태: 리뷰 대기
관련:
- [`[요구사항] LED 색감·타이밍 보강 Rev.0`]([요구사항]%20LED%20색감·타이밍%20보강%20Rev.0%20by%20김은수.md)
- [`[현상분석] LED 색감·타이밍 보강 Rev.0`]([현상분석]%20LED%20색감·타이밍%20보강%20Rev.0%20by%20김은수.md)

---

## 1. 결정

| # | 항목 | 결정 |
|---|---|---|
| Q1 | 색감 아키텍처 | 색상별 R/G/B cap 테이블 |
| Q2 | cap 적용 위치 | post-LUT |
| Q3 | 디폴트 cap | ORANGE (100,20,0) · WHITE (100,30,100) · PURPLE (60,0,100) · SKYBLUE (0,50,100) |
| Q4 | FADE_MAX_MS | **80** (150 에서 단축) |
| Q5 | ISR 이동 범위 | `led_arbiter_tick()` 전체 (arbiter + engine + LED_OUT) |
| Q6 | turnOffLED race | `s_led_isr_suspended` 플래그 suspension |
| Q7 | PWM 해상도 향상 | 본 작업 제외 (후속) |

---

## 2. 구현 설계

### 2.1 매크로 변경 (`LedOutput.c`)

```c
/* fade 시간 상한 — 점멸 ON / 3 이 이 값 초과 시 cap. */
#define LED_DIMMING_FADE_MAX_MS  80   /* (구) 150 */
#define LED_DIMMING_FADE_DIVISOR 3
#define LED_DIMMING_PWM_STEPS    10
```

### 2.2 색상별 cap 테이블 (`LedOutput.c`)

```c
/* ========================================================================
 *  LED 색상별 R/G/B PWM duty cap (%) — 조합색 색감 교정용
 * ========================================================================
 *  저항 R13=R14=R15=1.2kΩ + 3.3V_STBY 고정에서, 각 조합색 체감 색감을
 *  의도와 일치시키기 위해 색상마다 R/G/B cap 을 독립 설정.
 *
 *  PWM_STEPS=10 → cap 10% 단위 양자화.
 *  아래 값은 실측 튜닝 전 초안 — Rev.1 에서 흰 벽 반사 육안 매칭으로 갱신.
 * ======================================================================== */

typedef struct
{
    uint8_t cap_pc_r;  /* 0..100 */
    uint8_t cap_pc_g;
    uint8_t cap_pc_b;
} tdc_led_mix_t;

static const tdc_led_mix_t k_led_mix[] = {
    [en__LED_BLACK]   = {   0,   0,   0 },
    [en__LED_RED]     = { 100,   0,   0 },
    [en__LED_GREEN]   = {   0, 100,   0 },
    [en__LED_BLUE]    = {   0,   0, 100 },
    [en__LED_ORANGE]  = { 100,  20,   0 },  /* 주황색 (R 우세 · G 약화) */
    [en__LED_SKYBLUE] = {   0,  50, 100 },
    [en__LED_PURPLE]  = {  60,   0, 100 },
    [en__LED_WHITE]   = { 100,  30, 100 },
};
```

### 2.3 GPIO 극성 helper (`LedOutput.c`)

```c
static void tdc_led_write_gpio(bool on_r, bool on_g, bool on_b)
{
#if defined(LED_IS_ACTIVELOW)
    if (on_r) Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
    else      Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
    if (on_g) Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
    else      Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
  #if !defined(LED_B_pin_CFX_test)
    if (on_b) Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
    else      Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
  #else
    (void) on_b;
  #endif
#else  /* Active HIGH (Board_OTE_ver1_5) */
    if (on_r) Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
    else      Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
    if (on_g) Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
    else      Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
    if (on_b) Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
    else      Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
#endif
}
```

### 2.4 LED_OUT 리팩토링 (`LedOutput.c`)

기존 3 `#ifdef` 분기 삭제 → 단일 본체:

```c
void LED_OUT(void)
{
    static int timerCounter = 0;

    /* Test trigger: 강제 BLUE. 테이블 lookup 으로 일관 처리. */
    EN__LED_COLOR color = isTestTriggerEanbled() ? en__LED_BLUE : LED_outputColor;

    /* 배열 bound 가드 — 미등록 색상은 OFF */
    const tdc_led_mix_t *mix;
    if ((unsigned) color < (sizeof(k_led_mix) / sizeof(k_led_mix[0])))
    {
        mix = &k_led_mix[color];
    }
    else
    {
        static const tdc_led_mix_t k_off = { 0, 0, 0 };
        mix = &k_off;
    }

    /* 채널별 감쇠 duty — base × cap / 100 */
    uint8_t duty_r = (uint8_t) ((uint32_t) s_led_pwm_on_count * mix->cap_pc_r / 100U);
    uint8_t duty_g = (uint8_t) ((uint32_t) s_led_pwm_on_count * mix->cap_pc_g / 100U);
    uint8_t duty_b = (uint8_t) ((uint32_t) s_led_pwm_on_count * mix->cap_pc_b / 100U);

    /* 채널별 PWM 비교 */
    bool on_r = (timerCounter < duty_r);
    bool on_g = (timerCounter < duty_g);
    bool on_b = (timerCounter < duty_b);

    tdc_led_write_gpio(on_r, on_g, on_b);

    timerCounter++;
    if (timerCounter >= LED_DIMMING_PWM_STEPS)
    {
        timerCounter = 0;
    }
}
```

### 2.5 ISR suspension 플래그 (`LedOutput.c` + `LedOutput.h`)

`LedOutput.h` 에 suspension API 는 노출하지 않음 (내부 플래그), `turnOffLED` 내에서만 set/clear.

```c
/* ISR 에서 engine/LED_OUT 을 일시 정지하는 플래그.
 * main loop 가 직접 LED state 를 조작하는 구간 (turnOffLED 의 수동 fade)
 * 에서 경쟁 방지 목적. */
static volatile bool s_led_isr_suspended = false;
```

### 2.6 arbiter_tick 에 suspension 가드 추가 (`LedOutput.c`)

```c
void led_arbiter_tick(void)
{
    if (s_led_isr_suspended) return;  /* turnOffLED 수동 fade 구간 pass */

    bool user_off = (readLED_indicatorOnOff() == 2);
    /* ... 기존 로직 ... */
}
```

### 2.7 turnOffLED 수정 (`LedOutput.c`)

```c
void turnOffLED(void)
{
    s_led_isr_suspended = true;   /* ISR engine 정지 */

    for (uint16_t t = 0; t < LED_DIMMING_FADE_MAX_MS; t++)
    {
        uint8_t perceived = (uint8_t) (((uint32_t) (LED_DIMMING_FADE_MAX_MS - t) * 255UL)
                                       / LED_DIMMING_FADE_MAX_MS);
        s_led_pwm_on_count = perceived_to_pwm(perceived);
        LED_OUT();
        delay_ms(1);
    }

    LED_outputColor    = en__LED_BLACK;
    s_led_pwm_on_count = 0;
    LED_OUT();

    s_led_isr_suspended = false;  /* ISR 복귀 */
}
```

> [!NOTE]
> `LED_OUT()` 은 이제 `k_led_mix[LED_outputColor]` lookup 을 거침. `en__LED_BLACK` entry 는 {0,0,0} → 모든 채널 OFF. 정상 동작.

### 2.8 ISR 핸들러 변경

**중요**: 정상 동작 시 1ms tick 은 **`CFX_0_IRQHandler`** (`driver_timmer.c`) 에서 온다.
`TIMER_3_IRQHandler` (`ci_timer.c`) 는 ULP 모드 전용 fallback (주석 "Use when the CFX is not working").
두 핸들러 모두에 `led_arbiter_tick()` 호출 추가 (둘 다 fire 할 경우 mutually exclusive 전제).

`driver_timmer.c`:

```c
#include <ci_timer.h>
#include <stdbool.h>
#include <LedOutput.h>   /* led_arbiter_tick — CFX ISR 직접 구동 */

void CFX_0_IRQHandler(void)
{
    enable_iteration();
    ci_timer_increase_tick();
    led_arbiter_tick();   /* 평상시 1ms tick 구동 */
}
```

`ci_timer.c` (ULP 모드 fallback):

```c
#include <ci_timer.h>
#include <LedOutput.h>

void TIMER_3_IRQHandler(void)
{
    g_ci_timer_main_tick++;
    enable_iteration();
    led_arbiter_tick();   /* ULP 모드 — 500ms 주기, 정적 상태 유지용 */
}
```

### 2.9 main loop 에서 호출 제거 (`main.c`)

```c
/* 기존 */
led_arbiter_tick();

/* 변경 — 줄 삭제. ISR 에서 호출됨. */
```

main.c 근처의 `led_request()` 등 호출은 유지 (여전히 source 상태 업데이트).

---

## 3. Fade 타임라인 검증

### 3.1 패턴별 (FADE_MAX=80)

| 패턴 | on_ms | fade=min(on/3, 80) | peak | 총합 | 비고 |
|---|---|---|---|---|---|
| **POWER_ON** | **80** | **26** | **26** | **78 ≈ 80 ✓** | **최단 ON, /3 지배** |
| POWER_OFF | 100 | 33 | 33 | 99 ≈ 100 ✓ | |
| MAPPING_ISD_BATT_LOW | 100 | 33 | 33 | 99 ✓ | |
| ERROR_* | 180 | 60 | 60 | 180 ✓ | |
| OTA_EZAIRO | 180 | 60 | 60 | 180 ✓ | |
| MAPPING_ISD_BATT_READY | 200 | 66 | 66 | 198 ≈ 200 ✓ | |
| PAIR | 500 | 80 | 340 | 500 ✓ | cap hit |
| OTA_QCC | 1100 | 80 | 940 | 1100 ✓ | cap hit |
| BATT_CRITICAL | 1100 | 80 | 940 | 1100 ✓ | cap hit |

→ 모든 패턴 fade ≤ 80ms 달성. 짧은 패턴 (POWER_ON/POWER_OFF) 은 /3 룰이 지배 — 80 cap 변경 영향 없음. 긴 패턴 (≥240ms, PAIR 이상) 에서만 150→80 로 실제 단축 → peak 길어짐.

### 3.2 Cross-fade

Phase A (이전 색 fade-out) 80ms + Phase B (새 색 fade-in) 80ms = 전체 160ms (기존 300ms).

### 3.3 turnOffLED

80ms 수동 fade-out (기존 150ms). 셧다운 시점 70ms 빠름.

---

## 4. 단계 / 커밋

단일 기능 커밋 (1건):

> Add : LED 색감·타이밍 보강 — 색상별 PWM cap 테이블, fade 상한 80ms, Timer 3 ISR 이동

영향 파일:
- `src/2__cm3/Cortex-M3-src/systemControl/LedOutput.c`
- `src/2__cm3/Cortex-M3-src/systemControl/driver_timmer.c` — CFX ISR (평상시 1ms tick 소스)
- `src/2__cm3/Gen1_5/common/ci_timer.c` — Timer 3 ISR (ULP 모드 fallback)
- `src/2__cm3/Cortex-M3-src/main.c`

문서 3건 별도 커밋:

> Docs : LED 색감·타이밍 보강 요구·분석·구현계획

---

## 5. 검증

### 5.1 정적

- 빌드 성공 (`Board_OTE_ver1_5` active HIGH 기본)
- 빌드 성공 (`LED_IS_ACTIVELOW` 변형 + `LED_B_pin_CFX_test` 변형)
- `Grep`:
  - `k_led_mix` 정의 1건 + 사용 1건
  - `tdc_led_write_gpio` 정의 1건 + 호출 1건
  - `s_led_isr_suspended` 정의 1건 + 3 지점 (arbiter guard, turnOffLED set, turnOffLED clear)
  - `LED_DIMMING_FADE_MAX_MS` 값 80 1건
  - `led_arbiter_tick` 호출 : `ci_timer.c` 1건 (신규), `main.c` 0건 (제거)
- `LED_OUT` 중복 블록 3→1 통합

### 5.2 실기 시나리오

| # | 시나리오 | 기대 |
|---|---|---|
| a | RED / GREEN / BLUE 단일 풀 ON | 자연스러운 색 |
| b | **ORANGE (`LED_ST_BATT_MID`)** | **주황색** |
| c | WHITE (`LED_ST_IN_USE`) | 편향 없는 흰색 |
| d | PURPLE (`LED_ST_MAPPING_*_LOW`) | 보라/마젠타 |
| e | POWER_OFF burst × 4 | 각 100ms 패턴 fade 33 · peak 33 (jitter 없음) |
| f | PAIR (on 500) | fade 80 · peak 340 (peak 길어진 느낌) |
| g | BATT_CRITICAL (on 1100) | fade 80 · peak 940 |
| h | 색 전환 (BATT_READY → PAIR) | 160ms snappy |
| i | I2C 폴링 블록 중 LED | jitter 없음 (ISR 구동) |
| j | shutdown (turnOffLED) | 80ms fade-out 후 OFF 유지 |
| k | POWER_ON 패턴 | 정상 재생 (turnOffLED 이후 s_led_isr_suspended 복귀) |

### 5.3 튜닝 절차 (Rev.1 — 별도 작업)

1. 빌드 · flashing
2. 각 색상 테스트 모드 고정
3. 흰 벽 반사 육안 매칭
4. `k_led_mix[]` 10% 단위 조정, 재빌드 반복
5. 최종 값으로 Rev.1 문서 갱신

---

## 6. 위험 / 잔여

- (R1) 디폴트 cap 값 실측 차이 — Rev.1 튜닝
- (R2) 10% cap 해상도 양자화 — PWM 해상도 향상 후속 작업
- (R3) ISR 실행시간 증가 — 현재 15~62μs, 기능 추가 시 감시
- (R4) `s_led_isr_suspended` 플래그 오용 — 단일 함수 scope 로 제한, paired set/clear 확인
- (R5) `ci_timer.c` 에 `LedOutput.h` 의존 추가 — 디렉토리 간 include 경로 확인 필요 (.metadata 재빌드 가능성)

---

## 7. 작업 순서

1. 본 문서 3건 — 완료
2. 사용자 승인
3. `LedOutput.c`:
   - FADE_MAX 값 80 변경
   - `tdc_led_mix_t` + `k_led_mix[]` 추가
   - `tdc_led_write_gpio()` 추가
   - `LED_OUT()` 통합 리팩토링
   - `s_led_isr_suspended` + `led_arbiter_tick` 가드
   - `turnOffLED()` 에 플래그 set/clear
4. `driver_timmer.c`:
   - `#include <LedOutput.h>`
   - `CFX_0_IRQHandler()` 에 `led_arbiter_tick()` 추가 (**주요 1ms tick 소스**)
5. `ci_timer.c`:
   - `#include <LedOutput.h>`
   - `TIMER_3_IRQHandler()` 에 `led_arbiter_tick()` 추가 (ULP 모드 fallback)
6. `main.c`:
   - `led_arbiter_tick()` 호출 제거
6. 빌드 확인 (Board_OTE_ver1_5 기본 + 2 `#ifdef` 변형)
7. 단일 코드 커밋 + 문서 커밋
8. 실기 튜닝 (Rev.1 — 별도)

---

## 8. Rev.1 예정 항목

- 실측 기반 `k_led_mix[]` 갱신
- 필요 시 PWM 해상도 향상 (10→100 steps, Timer3 tick 100μs) — 별도 분석 필요
