# LED Dimming Cross-fade 보강 현상 분석

작성자: 김은수
작성일: 2026-04-23
상태: 리뷰 대기
근거 요구사항: [`[요구사항] LED Cross-fade 보강 Rev.0 by 김은수.md`]([요구사항]%20LED%20Cross-fade%20보강%20Rev.0%20by%20김은수.md)

---

## 1. 현행 Rev.0 dimming 구조

### 1.1 핵심 로직

`led_engine_run()` ([LedOutput.c:103-167](../src/2__cm3/Cortex-M3-src/systemControl/LedOutput.c#L103))

```c
if (reset)
{
    timer_ms             = 0;
    burst_done_cnt       = 0;
    s_color_changed_ms   = 0;  /* 색상 전환 fade-in 시작 */
}

/* 출력 색상 결정 */
if (p->period_ms == 0)
{
    LED_outputColor = p->color;        /* 지속 ON */
}
else
{
    LED_outputColor = (timer_ms < p->on_ms) ? p->color : en__LED_BLACK;  /* 점멸 */
}

/* Brightness 산출 (점멸 fade) */
uint8_t bright = led_dim_calc_brightness(timer_ms, p->on_ms, p->period_ms);

/* 색상 전환 fade-in: 새 색이 점진적으로 등장 */
if (s_color_changed_ms < LED_DIMMING_FADE_MS)
{
    bright = (uint8_t) (((uint32_t) bright * s_color_changed_ms) / LED_DIMMING_FADE_MS);
    s_color_changed_ms++;
}
```

### 1.2 문제 시나리오

**상황**: `LED_outputColor = en__LED_GREEN` (BATT_READY 지속 ON, brightness 255)

1. 사용자가 UI 명령 `--led pair` 입력 → Arbiter best = `LED_ST_PAIR` (이전 best `LED_ST_BATT_READY` 와 다름)
2. `led_arbiter_tick()` 이 `prev_best != best` 감지 → `reset = true`
3. `led_engine_run(LED_ST_PAIR, true)` 호출
4. 함수 진입 즉시:
   - `timer_ms = 0`, `s_color_changed_ms = 0`
   - `LED_outputColor = en__LED_BLUE` (PAIR 의 ON 구간 시작이므로 파랑)
   - **이전 색 GREEN 이 출력에서 사라짐 — 첫 번째 단절**
   - bright = `led_dim_calc_brightness(0, 500, 1000)` = `(0 * 255) / 150` = 0 (fade-in 시작점)
   - bright × `s_color_changed_ms / FADE_MS` = 0 × 0 / 150 = 0
5. PWM duty = 0 → LED 완전히 OFF
6. 다음 1 ms tick 마다 `s_color_changed_ms` 증가 → 파랑 brightness 점진 증가

**결과**: 녹색이 갑자기 꺼진 후 파랑 fade-in. 사용자가 관찰한 "팍! 꺼진 다음 FADE IN".

### 1.3 근본 원인

`reset == true` 시점에 **`LED_outputColor` 가 즉시 새 색으로 갱신** 된다. 이전 색을 유지하면서 fade-out 할 단계 (Phase A) 가 없기 때문에, brightness 만 0 부터 시작하더라도 LED 출력 색상은 이미 새 색이다.

---

## 2. 영향 범위

| 변경 대상 | 파일 |
|---|---|
| `led_engine_run()` 색상 전환 처리 | `LedOutput.c` |
| 신규 상태 변수 (`s_tx_phase`, `s_tx_prev_color`, `s_tx_ms`) | `LedOutput.c` (static) |
| `LED_OUT()` | **변경 없음** — `LED_outputColor` + `s_led_pwm_on_count` 인터페이스 그대로 |
| 헤더 / 외부 API | **변경 없음** |

→ 변경 범위 단일 파일. 회귀 위험 낮음.

## 3. 결정 필요 사항

| # | 항목 | 옵션 |
|---|---|---|
| Q1 | fade-out 중 burst 패턴 (POWER_ON ×5) 시작이면? | (A) 즉시 새 패턴 (B) fade-out 끝내고 새 패턴. **(B)** 채택 — 시각적 일관성 |
| Q2 | fade-out 중 same-color 변경 (예: BATT_READY → IN_USE 흰색→녹색?실제는 다른 색) | 색이 같으면 cross-fade 불필요 → reset 시 prev_color 와 신규 color 같으면 Phase A 생략 |
| Q3 | fade-out 중 또 reset? | 진행 중인 fade-out 끝낸 후 새 색 fade-in (Q1 과 동일 정책) |

---

## 4. 검증 방법 (실기)

| # | 시나리오 | 기대 |
|---|---|---|
| a | 녹색 BATT_READY 상태에서 UI `--led pair` | 녹색 fade-out 150 ms → 파랑 fade-in 150 ms |
| b | 부팅 직후 (LED OFF 상태) → POWER_ON | Phase A 생략, 파랑 즉시 fade-in |
| c | POWER_ON 버스트 종료 후 BATT_READY 표시 시작 | 파랑 fade-out → 녹색 fade-in |
| d | PAIR (점멸 중) → IDLE (LED OFF) | 파랑 fade-out → BLACK 으로 끝 (fade-in 생략) |
| e | PAIR ON 구간 (점멸 중) brightness 변화 | 기존 fade 효과 유지 (cross-fade 영향 없음) |
