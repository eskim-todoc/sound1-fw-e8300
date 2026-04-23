# LED 인지 기반 Dimming 보강 구현계획

작성자: 김은수
작성일: 2026-04-23
상태: 리뷰 대기
관련:
- [`[요구사항] LED 인지 기반 Dimming Rev.0`]([요구사항]%20LED%20인지%20기반%20Dimming%20Rev.0%20by%20김은수.md)
- [`[현상분석] LED 인지 기반 Dimming Rev.0`]([현상분석]%20LED%20인지%20기반%20Dimming%20Rev.0%20by%20김은수.md)

---

## 1. 결정

| # | 항목 | 결정 |
|---|---|---|
| Q1 | fade 자동 조정 비율 | `on_ms / 3` (peak 유지 ≥ on_ms / 3) |
| Q2 | fade 상한 | `LED_DIMMING_FADE_MAX_MS = 150 ms` |
| Q3 | LUT 단계 수 | 256 entry (uint8_t) |
| Q4 | LUT 곡선 | **CIE 1931 Lightness (L\*)** — Y = ((L+16)/116)³ if L>8, L/903.3 otherwise |
| Q5 | Cross-fade 시간 | 고정 `LED_DIMMING_FADE_MAX_MS` (패턴 ON 시간 무관) |

---

## 2. 구현 설계

### 2.1 매크로 정리

```c
/* fade 시간 상한 — 점멸 ON 시간 / 3 이 이 값 초과 시 cap */
#define LED_DIMMING_FADE_MAX_MS  150

/* PWM 단계: 1ms × N = (N) ms = (1000/N) Hz */
#define LED_DIMMING_PWM_STEPS    10

/* 점멸 fade 시간 자동 조정 비율 — on_ms / FADE_DIVISOR */
#define LED_DIMMING_FADE_DIVISOR 3
```

기존 `LED_DIMMING_FADE_MS` 는 `LED_DIMMING_FADE_MAX_MS` 로 이름 변경 (의미 명확화).

### 2.2 CIE 1931 L* lookup table

```c
/* perceived brightness (선형 0~255) → physical PWM duty (0~255).
 *
 * 사람 눈은 밝기 인지에 비선형 응답 (Stevens' Power Law: ≈ luminance^0.33).
 * 시간에 따라 입력값을 선형으로 증가시키면, 사람 눈에는 균등한 밝기
 * 변화로 인식된다.
 *
 * 표는 CIE 1931 Lightness (L*) 공식으로 자동 생성 (Python):
 *   L = i * 100 / 255   (입력 0~255 → L* 0~100)
 *   Y = ((L + 16) / 116)^3   if L > 8
 *   Y = L / 903.3            if L ≤ 8
 *   PWM = round(Y * 255)
 *
 * 참고: https://en.wikipedia.org/wiki/Relative_luminance
 *       https://gist.github.com/mathiasvr/19ce1d7b6caeab230934080ae1f1380e
 */
static const uint8_t k_perceptual_lut[256] = {
      0,   0,   0,   0,   0,   1,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,
      2,   2,   2,   2,   2,   2,   2,   3,   3,   3,   3,   3,   3,   3,   3,   4,
      4,   4,   4,   4,   4,   5,   5,   5,   5,   5,   6,   6,   6,   6,   6,   7,
      7,   7,   7,   8,   8,   8,   8,   9,   9,   9,  10,  10,  10,  10,  11,  11,
     11,  12,  12,  12,  13,  13,  13,  14,  14,  15,  15,  15,  16,  16,  17,  17,
     17,  18,  18,  19,  19,  20,  20,  21,  21,  22,  22,  23,  23,  24,  24,  25,
     25,  26,  26,  27,  28,  28,  29,  29,  30,  31,  31,  32,  32,  33,  34,  34,
     35,  36,  37,  37,  38,  39,  39,  40,  41,  42,  43,  43,  44,  45,  46,  47,
     47,  48,  49,  50,  51,  52,  53,  54,  54,  55,  56,  57,  58,  59,  60,  61,
     62,  63,  64,  65,  66,  67,  68,  70,  71,  72,  73,  74,  75,  76,  77,  79,
     80,  81,  82,  83,  85,  86,  87,  88,  90,  91,  92,  94,  95,  96,  98,  99,
    100, 102, 103, 105, 106, 108, 109, 110, 112, 113, 115, 116, 118, 120, 121, 123,
    124, 126, 128, 129, 131, 132, 134, 136, 138, 139, 141, 143, 145, 146, 148, 150,
    152, 154, 155, 157, 159, 161, 163, 165, 167, 169, 171, 173, 175, 177, 179, 181,
    183, 185, 187, 189, 191, 193, 196, 198, 200, 202, 204, 207, 209, 211, 214, 216,
    218, 220, 223, 225, 228, 230, 232, 235, 237, 240, 242, 245, 247, 250, 252, 255,
};
```

### 2.3 perceived brightness 산출 (점멸 패턴 — 자동 fade)

```c
static uint16_t calc_pattern_fade_ms(uint16_t on_ms)
{
    /* on_ms / 3 또는 MAX 중 작은 값 — 정점 도달 보장 */
    uint16_t one_third = on_ms / LED_DIMMING_FADE_DIVISOR;
    return (one_third < LED_DIMMING_FADE_MAX_MS) ? one_third : LED_DIMMING_FADE_MAX_MS;
}

/* 점멸 패턴 내 시간 t 의 perceived brightness 산출 (선형 0~255).
 * 패턴별 fade 시간이 ON 시간에 맞춰 자동 조정되어 정점 도달 보장. */
static uint8_t calc_perceived_pattern(uint16_t t, uint16_t on_ms, uint16_t period_ms)
{
    if (period_ms == 0)  return 255;  /* 지속 ON */
    if (t >= on_ms)      return 0;    /* OFF 구간 */

    uint16_t fade_ms = calc_pattern_fade_ms(on_ms);
    if (fade_ms == 0)    return 255;  /* on_ms < 3 (이론상 미발생, 안전 가드) */

    /* fade-in 영역 */
    if (t < fade_ms)
    {
        return (uint8_t) ((t * 255UL) / fade_ms);
    }
    /* fade-out 영역 */
    if (t >= (uint16_t) (on_ms - fade_ms))
    {
        return (uint8_t) (((on_ms - t) * 255UL) / fade_ms);
    }
    /* 정점 */
    return 255;
}
```

> [!NOTE]
> on_ms=100, fade_ms = 100/3 = 33 → t=0 부터 t=33 까지 fade-in (255 도달), t=33~67 peak, t=67~100 fade-out. 정점 도달 + 약 33 ms peak 유지 보장.

### 2.4 perceived → PWM 변환

```c
/* perceived (0~255) → physical PWM (0~LED_DIMMING_PWM_STEPS) */
static uint8_t perceived_to_pwm(uint8_t perceived)
{
    return (uint8_t) (((uint32_t) k_perceptual_lut[perceived] * LED_DIMMING_PWM_STEPS) / 255);
}
```

### 2.5 led_engine_run() 통합

```c
static void led_engine_run(led_state_t st, bool reset)
{
    const led_pattern_desc_t *p = &k_led_patterns[st];
    static uint16_t timer_ms       = 0;
    static uint8_t  burst_done_cnt = 0;

    if (reset) { /* (cross-fade Rev.0 그대로) */ }

    /* Phase A — 이전 색 fade-out (perceived 곡선 적용) */
    if (s_tx_phase == LED_TX_FADE_OUT)
    {
        LED_outputColor = s_tx_prev_color;
        /* 시간 ratio 0~1 → perceived 255 → 0 (역방향) */
        uint8_t perceived = (uint8_t) (((LED_DIMMING_FADE_MAX_MS - s_tx_ms) * 255UL)
                                       / LED_DIMMING_FADE_MAX_MS);
        s_led_pwm_on_count = perceived_to_pwm(perceived);

        s_tx_ms++;
        if (s_tx_ms >= LED_DIMMING_FADE_MAX_MS)
        {
            s_tx_phase = LED_TX_FADE_IN;
            s_tx_ms    = 0;
        }
        return;
    }

    /* 색상 결정 */
    if (p->period_ms == 0)
        LED_outputColor = p->color;
    else
        LED_outputColor = (timer_ms < p->on_ms) ? p->color : en__LED_BLACK;

    /* perceived brightness — 패턴 fade 자동 조정 적용 */
    uint8_t perceived = calc_perceived_pattern(timer_ms, p->on_ms, p->period_ms);

    /* Phase B fade-in (perceived ratio 곱하기) */
    if (s_tx_phase == LED_TX_FADE_IN)
    {
        uint8_t fade_in_perc = (uint8_t) ((s_tx_ms * 255UL) / LED_DIMMING_FADE_MAX_MS);
        /* 두 perceived 를 ratio 적용 (정수 곱셈) */
        perceived = (uint8_t) (((uint32_t) perceived * fade_in_perc) / 255);
        s_tx_ms++;
        if (s_tx_ms >= LED_DIMMING_FADE_MAX_MS)
            s_tx_phase = LED_TX_NONE;
    }

    /* perceived → PWM */
    s_led_pwm_on_count = perceived_to_pwm(perceived);

    /* 점멸 주기 진행 (기존 그대로) */
    if (p->period_ms != 0) { /* timer_ms++ + burst */ }
}
```

> [!NOTE]
> Phase B 의 fade-in ratio 와 패턴 brightness 를 곱셈으로 결합 — 두 항 모두 selled에 (perceived 0~255) 단위라, 결합 후 다시 perceived → physical 변환 1회. 두 번의 LUT lookup 누적이 아니라 1번만 적용해 부드러움 보장.

---

## 3. 단계 / 커밋

단일 커밋:

> Add : LED Dimming 인지 기반 곡선 (CIE 1931 L*) + 패턴별 fade 시간 자동 조정

영향 파일: `LedOutput.c` 만.

문서 3건은 별도 커밋:

> Docs : LED 인지 기반 Dimming 요구·분석·구현계획

---

## 4. 검증

### 4.1 정적

- 빌드 성공
- LUT 256 entry, 단조 증가
- `Grep "LED_DIMMING_FADE_MS\b"` 잔존 0 건 (모두 `_MAX_MS` 로 갱신)
- `led_dim_calc_brightness` 잔존 0 건 (제거 또는 `calc_perceived_pattern` 으로 대체)

### 4.2 실기 시나리오

| # | 시나리오 | 기대 |
|---|---|---|
| a | `LED_ST_MAPPING_ISD_BATT_LOW` (ON 100/OFF 900) | 정점 도달, peak ≈ 33 ms |
| b | `LED_ST_MAPPING_ISD_BATT_READY` (ON 200/OFF 800) | 정점 도달, peak ≈ 66 ms |
| c | `LED_ST_PAIR` (ON 500/OFF 500) | fade 150 ms (cap), peak ≈ 200 ms |
| d | `LED_ST_BATT_CRITICAL` (ON 1100/OFF 1100) | fade 150 ms, peak ≈ 800 ms (변화 없음) |
| e | `LED_ST_BATT_READY` (지속 ON) | brightness 255 → LUT[255] = 255 PWM full |
| f | 색상 전환 (BATT_READY → PAIR) | Phase A (녹색 perceived fade-out) + Phase B (파랑 fade-in) 부드러움 향상 |

---

## 5. 위험 / 잔여

- (R1) LUT 256 byte = Flash 사용량 증가 (영향 미미 — 임베디드 ROM 충분)
- (R2) PWM 단계 10 이라 LUT[i] 작은 값 (예: LUT[10] = 0~1) 이 PWM step 0 으로 떨어짐 → 매우 어두운 시작 부분이 한 동안 OFF 처럼 보일 수 있음. 시각적으로는 자연스러우나 인지 곡선 상으론 의도된 동작.
- (R3) 정수 연산 중 한 값이 너무 작아 (perceived × fade_in_perc / 255) 결과가 0 이 되는 경우 fade 시작이 잠깐 OFF 처럼 보임. 16-bit 중간 곱셈으로 손실 거의 없음.

---

## 6. 작업 순서

1. 본 문서 + 요구사항/현상분석 작성 — 완료
2. `LedOutput.c`:
   - 매크로 이름 변경
   - LUT 256 entry 추가
   - `led_dim_calc_brightness` → `calc_perceived_pattern` 으로 대체
   - `perceived_to_pwm` helper 추가
   - `led_engine_run` 갱신 (Phase A/B + 본문 모두 perceived 반영)
3. `Grep` 잔존 검증
4. 단일 커밋 (코드)
5. 사용자 확인 후 `claude_develop` 병합
