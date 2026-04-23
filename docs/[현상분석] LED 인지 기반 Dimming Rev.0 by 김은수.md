# LED 인지 기반 Dimming 보강 현상 분석

작성자: 김은수
작성일: 2026-04-23
상태: 리뷰 대기
근거 요구사항: [`[요구사항] LED 인지 기반 Dimming Rev.0`]([요구사항]%20LED%20인지%20기반%20Dimming%20Rev.0%20by%20김은수.md)

---

## 1. 현행 (cross-fade Rev.0) 한계 분석

### 1.1 brightness 산출 (`led_dim_calc_brightness()`)

[LedOutput.c:31-69](../src/2__cm3/Cortex-M3-src/systemControl/LedOutput.c#L31-L69)

```c
static uint8_t led_dim_calc_brightness(uint16_t t, uint16_t on_ms, uint16_t period_ms)
{
    if (period_ms == 0) return 255;
    if (t >= on_ms)     return 0;

    /* 삼각파 처리: ON 시간 < 2 × fade_ms 면 정점 미도달 */
    if (on_ms <= 2 * LED_DIMMING_FADE_MS)
    {
        uint16_t mid = on_ms / 2;
        if (t <= mid) return (uint8_t) ((t * 255UL) / LED_DIMMING_FADE_MS);
        return (uint8_t) (((on_ms - t) * 255UL) / LED_DIMMING_FADE_MS);
    }

    if (t < LED_DIMMING_FADE_MS) return (uint8_t) ((t * 255UL) / LED_DIMMING_FADE_MS);
    if (t >= on_ms - LED_DIMMING_FADE_MS) return (uint8_t) (((on_ms - t) * 255UL) / LED_DIMMING_FADE_MS);
    return 255;
}
```

**문제 1**: ON 100 ms (`LED_ST_MAPPING_ISD_BATT_LOW`) 일 때
- on_ms (100) ≤ 2 × FADE_MS (300) 조건 성립 → 삼각파
- t = mid = 50 일 때 brightness = (50 × 255) / 150 ≈ 85 → **최대 brightness 1/3 수준에서 turnaround**

**문제 2**: 산출된 brightness 가 시간에 대해 **선형 ramp** → 사람 눈에 비선형 (전반 급증, 후반 정체) 으로 인식.

### 1.2 PWM 변환 (`led_engine_run()`)

```c
s_led_pwm_on_count = (uint8_t) (((uint32_t) bright * LED_DIMMING_PWM_STEPS) / 255);
```

→ brightness (0~255) 를 PWM step (0~10) 으로 단순 비례 변환. 인지 곡선 미적용.

### 1.3 Cross-fade Phase A/B brightness 산출

```c
/* Phase A — 이전 색 fade-out */
uint32_t b = (uint32_t) 255 * (LED_DIMMING_FADE_MS - s_tx_ms) / LED_DIMMING_FADE_MS;
s_led_pwm_on_count = (uint8_t) (b * LED_DIMMING_PWM_STEPS / 255);

/* Phase B — fade-in (점멸 brightness 와 곱하기) */
bright = (uint8_t) (((uint32_t) bright * s_tx_ms) / LED_DIMMING_FADE_MS);
```

→ 둘 다 시간 ratio 가 brightness 에 직접 비례 (선형). 인지 곡선 미적용.

---

## 2. 사람 눈 인지의 비선형성

### 2.1 표준 모델

| 모델 | 식 | 비고 |
|---|---|---|
| Weber–Fechner Law | 인지 ∝ ln(자극) | 19세기 고전 |
| Stevens' Power Law (밝기) | 인지 ∝ luminance^0.33 (cube-root) | 현대 표준 |
| CIE 1931 L* (Lightness) | Y = ((L+16)/116)^3 (L>8) | 색공간 표준, Stevens 와 거의 일치 |

→ 모두 **사람 눈은 약한 빛에 민감, 강한 빛엔 둔감** 한 비선형 응답을 의미.

### 2.2 LED PWM 응답

PWM 듀티 ↔ luminance 는 거의 선형 관계 (LED 자체 비선형성 미미). 따라서:

> **시간 (선형) → perceived brightness (선형) → physical luminance / PWM duty (비선형)**

가 정답. 즉, 시간을 선형으로 진행하면서 **perceived 를 시간에 맞춰 직접 산출** 하고, 마지막에 **CIE L* 역함수** 로 PWM duty 변환 한다.

### 2.3 실제 LUT 비교 (CIE 1931 L*)

```
input (perceived)  →  output (physical PWM)   /255
   0                       0                    0%
  32                       4                  ~ 1.6%
  64                      16                  ~ 6.3%
 128                      72                  ~28%
 192                     157                  ~62%
 255                     255                  100%
```

→ 입력 50% (128/255) 일 때 PWM 은 28% — 사람 눈에 "절반 밝기" 인식이 PWM 28% 에 해당.

---

## 3. 영향 범위

| 변경 대상 | 파일 |
|---|---|
| LUT 신설 (`k_perceptual_lut[256]`) | `LedOutput.c` |
| `led_dim_calc_brightness()` → `calc_perceived_t()` 형태로 단순화 (자동 fade 시간 + perceived linear) | `LedOutput.c` |
| `led_engine_run()` 에서 perceived → PWM 변환 | `LedOutput.c` |
| Cross-fade Phase A/B 의 ratio → perceived → PWM 변환 | `LedOutput.c` |
| 매크로 이름: `LED_DIMMING_FADE_MS` → `LED_DIMMING_FADE_MAX_MS` (의미 명확화) | `LedOutput.c` |

→ 변경 범위 단일 파일 (`LedOutput.c`) 만. 외부 API 영향 없음.

## 4. 결정 필요 사항

| # | 항목 | 옵션 | 권장 |
|---|---|---|---|
| Q1 | fade 자동 조정 비율 | (A) on_ms / 2 (정점 = 0 폭) (B) on_ms / 3 (peak ≈ on/3) (C) on_ms / 4 (peak ≈ on/2) | **(B)** — peak ≥ on/3 으로 정점 인식 충분 |
| Q2 | fade 자동 조정 상한 | (A) `LED_DIMMING_FADE_MAX_MS` 까지 (B) 패턴 무관 고정 150 ms | **(A)** — 긴 패턴은 fade 자체도 길게 (자연스러움) |
| Q3 | LUT 단계 수 | (A) 256 (B) 64 (C) 32 | **(A) 256** — 메모리 부담 미미, fade 부드러움 최대 |
| Q4 | LUT 정확도 | (A) CIE 1931 L\* (B) Gamma 2.2 (C) sqrt | **(A)** — 표준, 정확 |
| Q5 | Cross-fade 시간 | (A) 자동 조정 적용 (B) `LED_DIMMING_FADE_MAX_MS` 고정 | **(B)** — 색 전환은 패턴 ON 시간과 무관 |

---

## 5. 검증

### 5.1 정적

- LUT 256 entry 모두 단조 증가 (decreasing 없음)
- LUT[0] = 0, LUT[255] = 255
- `Grep`: `LED_DIMMING_FADE_MS` 직접 사용 0 건 (모두 `LED_DIMMING_FADE_MAX_MS` 로 갱신)

### 5.2 실기 시나리오

| # | 시나리오 | 기대 |
|---|---|---|
| a | `LED_ST_MAPPING_ISD_BATT_LOW` (보라 ON 100 / OFF 900) | 정점 (max brightness) 에 분명히 도달, 약 33 ms peak 유지 |
| b | `LED_ST_MAPPING_ISD_BATT_READY` (파랑 ON 200 / OFF 800) | 정점 도달, peak 약 66 ms |
| c | `LED_ST_PAIR` (파랑 ON 500 / OFF 500) | fade ≈ 150 ms (상한 도달), peak 약 200 ms |
| d | `LED_ST_BATT_CRITICAL` (노랑 ON 1100 / OFF 1100) | fade 150 ms (상한), peak 약 800 ms — 변화 없음 |
| e | 지속 ON (`LED_ST_BATT_READY` 녹색) | 시작 시 cross-fade 후 정점 brightness 255 유지 |
| f | 색상 전환 (BATT_READY → PAIR) | Phase A (녹색 fade-out 150 ms) + Phase B (파랑 fade-in 150 ms) — 전환 자체 부드러움 향상 (perceived 적용) |
