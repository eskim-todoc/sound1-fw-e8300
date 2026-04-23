# LED 절전 진입 잔상색 제거 구현계획

작성자: 김은수
작성일: 2026-04-23
상태: 리뷰 대기
관련:
- [`[요구사항] LED 절전 진입 잔상색 제거 Rev.0`]([요구사항]%20LED%20절전%20진입%20잔상색%20제거%20Rev.0%20by%20김은수.md)
- [`[현상분석] LED 절전 진입 잔상색 제거 Rev.0`]([현상분석]%20LED%20절전%20진입%20잔상색%20제거%20Rev.0%20by%20김은수.md)

---

## 1. 결정

| # | 항목 | 결정 |
|---|---|---|
| Q1 | 처리 방식 | **(B)** perceived 곡선 fade-out 후 BLACK |
| Q2 | fade-out 시간 | `LED_DIMMING_FADE_MAX_MS` (150 ms) — 기존 매크로 |
| Q3 | 부팅 시 호출 | LED 가 BLACK 인 상태에서 fade-out 은 PWM 0 으로 즉시 종료 (no-op 가까움) |
| Q4 | LED_OUT() 자체 GPIO 보정 | 본 작업 범위 외 — turnOffLED() 보강 효과 확인 후 결정 |

---

## 2. 동작 원리

### 2.1 fade-out 흐름

```
시간 t (0 ~ FADE_MAX_MS):
  perceived = 255 × (FADE_MAX_MS - t) / FADE_MAX_MS    /* 255 → 0 선형 */
  s_led_pwm_on_count = perceived_to_pwm(perceived)     /* CIE L* LUT */
  LED_OUT()                                             /* GPIO 갱신 */
  delay_ms(1)
```

PWM duty 가 점진 감소 → enablePatternOut 가 false 인 PWM cycle 비율 증가 → GPIO 가 BLACK base 로 자동 수렴. 마지막에 LED_outputColor = BLACK 명시 적용.

### 2.2 의도치 않은 중간색 회피 원리

- fade-out 진행 중 LED_outputColor 는 **그대로 유지** → 매 LED_OUT() 호출이 같은 색 GPIO 적용 시도
- PWM duty 감소로 enablePatternOut = false cycle 이 늘어남 → GPIO 가 점점 더 자주 BLACK base 로 LOW 처리됨
- duty = 0 도달 시 LED_OUT() 의 else 블록 ([LedOutput.c:840-847](../src/2__cm3/Cortex-M3-src/systemControl/LedOutput.c#L840-L847)) 만 실행 → GPIO 매번 LOW (BLACK)
- 이 시점부터 GPIO 는 BLACK 유지 → 추가 GPIO 변경 없음
- 마지막 `LED_outputColor = BLACK` 적용은 already-BLACK 상태에서 LED_OUT() 호출 → 변화 없음

→ **의도치 않은 중간색 발생 없음**. 직전 LED 색이 무엇이든 매끄럽게 OFF.

---

## 3. 코드 변경

### 3.1 include

`LedOutput.c` 상단에 `<ci_util.h>` 추가 (delay_ms).

```c
#include <ci_util.h>  /* delay_ms */
```

### 3.2 turnOffLED() 본체 교체

기존 (active high 경로):
```c
void turnOffLED(void)
{
    LED_outputColor = en__LED_BLACK;
    /* 기존 active low / B_pin_CFX_test 분기들 */
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
}
```

새:
```c
void turnOffLED(void)
{
    /* GPIO R/G/B 순차 호출 사이의 transient 로 의도치 않은 중간색이 보이는
     * 현상 회피 — perceived 곡선으로 brightness 를 점진 감소시키면서 LED_OUT()
     * 을 통해 GPIO 를 갱신. PWM duty 0 도달 후엔 LED_OUT() 이 GPIO 모두 LOW
     * (BLACK base) 만 실행 → 추가 GPIO 변화 없음. 마지막 LED_outputColor 도
     * BLACK 으로 명시 적용. */
    for (uint16_t t = 0; t < LED_DIMMING_FADE_MAX_MS; t++)
    {
        uint8_t perceived = (uint8_t) (((uint32_t)(LED_DIMMING_FADE_MAX_MS - t) * 255UL)
                                       / LED_DIMMING_FADE_MAX_MS);
        s_led_pwm_on_count = perceived_to_pwm(perceived);
        LED_OUT();
        delay_ms(1);
    }

    LED_outputColor    = en__LED_BLACK;
    s_led_pwm_on_count = 0;
    LED_OUT();
}
```

기존의 `LED_IS_ACTIVELOW` / `LED_B_pin_CFX_test` 분기는 모두 제거 — `LED_OUT()` 자체가 보드별 active level 처리하므로 호출만 위임.

---

## 4. 단계 / 커밋

단일 커밋:

> Fix : 절전 진입 LED 잔상색 제거 — turnOffLED() 를 perceived fade-out 으로 교체

영향 파일: `LedOutput.c` 만.

문서 3 건은 별도 커밋:

> Docs : LED 절전 진입 잔상색 제거 요구·분석·구현계획

---

## 5. 검증

### 5.1 정적

- 빌드 성공 (사용자 환경)
- `Grep "turnOffLED"` 호출자 변화 없음

### 5.2 실기

요구사항 §2.3 / 현상분석 §5.2 표 a~e 확인:
- 절전 진입 시 잔상색 사라짐
- 부팅 시 turnOffLED 호출 영향 없음

---

## 6. 위험 / 잔여

- (R1) **150 ms 지연 추가** — 절전 진입 시퀀스에서 turnOffLED 부분이 150 ms 길어짐. ULP 진입 직전 단계라 사용자 체감 없음.
- (R2) **부팅 시 호출** ([systemControl.c:211](../src/2__cm3/Cortex-M3-src/systemControl/systemControl.c#L211)) 도 150 ms 지연. 부팅 직후 LED 가 BLACK (perceived 0, PWM 0) 이면 LED_OUT() 매 호출이 GPIO LOW 만 → 시각 차이 없음. 다만 150 ms × 1 ms loop = 150 ms 추가는 부팅 시간에 영향 — POWER_ON burst (≈ 1.5 s) 보다 짧아 큰 영향 없음.
- (R3) **LED 전원이 PMIC 와 연결된 경우** fade-out 가시 안 될 수 있음. 1.5 세대 보드 LED 는 일반적으로 배터리 직결로 fade 가시 가능 — 실기 확인 필요.
- (R4) **LED_OUT() 자체 GPIO 순서 보정 (cross-fade Phase A/B 등)** 은 본 작업에 미포함 — 현상이 turnOffLED 보강 만으로 해소되지 않으면 후속.

---

## 7. 작업 순서

1. 본 문서 + 요구사항/현상분석 작성 — 완료
2. `LedOutput.c` 수정:
   - `<ci_util.h>` include 추가
   - `turnOffLED()` 본체 교체 (기존 LED_IS_ACTIVELOW / LED_B_pin_CFX_test 분기 모두 제거)
3. 단일 커밋 (코드)
4. 사용자 확인 후 `claude_develop` 병합
