# LED 절전 직전 잔상색 잔류 제거 현상 분석

작성자: 김은수
작성일: 2026-04-23
상태: 리뷰 대기
근거 요구사항: [`[요구사항] LED 절전 직전 잔상색 잔류 제거 Rev.0`]([요구사항]%20LED%20절전%20직전%20잔상색%20잔류%20제거%20Rev.0%20by%20김은수.md)

---

## 1. 직전 수정 ((turnOffLED fade-out)) 의 한계

`turnOffLED()` 는 호출 시점의 `LED_outputColor` 를 그대로 두고 perceived brightness 만 점진 감소시킨다 ([LedOutput.c:589~619](../src/2__cm3/Cortex-M3-src/systemControl/LedOutput.c#L589)). 즉:

- 호출 시점 `LED_outputColor` 가 GREEN 이면 → GREEN 의 fade-out 이 사람 눈에 보임
- 호출 시점이 PWM cycle 의 ON 단계라면 GPIO 도 GREEN 색으로 출력 중

→ turnOffLED() 보강만으로는 **호출 시점의 LED_outputColor 가 무엇인지** 결정짓는 직전 흐름의 부작용을 막지 못함.

## 2. cross-fade Phase B 가 새 색을 LED_outputColor 에 주입

[`led_engine_run()`](../src/2__cm3/Cortex-M3-src/systemControl/LedOutput.c#L348) 의 Phase B 진입 (`reset == true && prev_color == BLACK`) 시:

```c
if (p->period_ms == 0)
    LED_outputColor = p->color;       /* 지속 ON — 새 색 즉시 설정 */
else
    LED_outputColor = (timer_ms < p->on_ms) ? p->color : en__LED_BLACK;
```

→ Phase B 진입 직후 `LED_outputColor = 새 best 색` (예: BATT_READY = GREEN). brightness (perceived × s_tx_ms / FADE) 는 0 부터 시작.

PWM 단계 ≥ 1 인 cycle 에서 GPIO 가 새 색으로 출력 → 그 직후 `func_sleep()` → `turnOffLED()` 가 새 색을 fade-out → 사용자 잔상 인지.

## 3. POWER_OFF burst 자가 해제 → 새 best 결정 흐름

| Iteration | 동작 |
|---|---|
| N | POWER_OFF burst 진행 (파랑 점멸) |
| ... | (4 회 점멸 진행) |
| N+k | burst 마지막 사이클의 OFF 구간 → led_engine_run() 안에서 자가 해제 → s_req[POWER] = LED_ST_NONE, current_led_pattern = en__LED_NA |
| N+k+1 | systemControl() 가 (`current_led_pattern != en__LED_POWER_Off`) 조건으로 systemOff = true 결정 |
| N+k+1 (cont.) | led 요청 갱신 (BATTERY/ISD/MAPPING) → led_arbiter_tick() → best = BATT_READY (또는 다른 src) |
| N+k+1 (cont.) | reset = true → cross-fade Phase B 시작 (LED_outputColor = GREEN) |
| N+k+1 (cont.) | systemOff true → 가드 통과 → break → func_sleep() |
| (func_sleep) | turnOffLED() — LED_outputColor = GREEN 상태로 fade-out 진행 |

→ 사용자가 본 GREEN 잔상의 정확한 발생 경로.

## 4. 채택 해법 — 모든 src NONE + Phase A fade-out 보장

`systemOff = true` 가드 통과 직후, **break 전에**:

1. 모든 src 의 `s_req[]` 를 `LED_ST_NONE` 으로 강제 (Arbiter 무조건 IDLE 결정)
2. `LED_DIMMING_FADE_MAX_MS + α` 동안 매 1 ms `led_arbiter_tick()` 반복 호출
   - 첫 호출: reset = true → LED_outputColor != BLACK 이면 Phase A 진입 (현재 색을 prev_color 로 캡처, fade-out 시작)
   - 이후 호출: Phase A 진행 → fade-out 완료 → Phase B → IDLE = BLACK
3. break → func_sleep() → turnOffLED() (이미 BLACK)

이 방식의 장점:
- **cross-fade 자체 메커니즘을 활용** — 새 색 진입을 차단하고 prev_color 의 자연스러운 fade-out 진행
- turnOffLED() 의 perceived fade-out 도 그대로 유지 (다른 호출 path 의 안전망)

## 5. 영향 범위

| 항목 | 변경 |
|---|---|
| `led_force_fade_off()` 신설 (LedOutput.c, public) | 신규 |
| LedOutput.h 에 함수 선언 추가 | 신규 |
| main.c func_normal() 절전 가드 통과 후 호출 | 추가 1 줄 |
| 다른 호출자 / 외부 API | 변경 없음 |
| turnOffLED() 본체 | **변경 없음** (안전망 유지) |

## 6. 결정 사항

| # | 항목 | 결정 |
|---|---|---|
| Q1 | helper 위치 | `LedOutput.c` (s_req[] 접근 위해 같은 파일) |
| Q2 | fade 시간 | `LED_DIMMING_FADE_MAX_MS + 10` ms (안정화 마진) |
| Q3 | tdc_led_set_ind_state(NONE) 도 호출? | s_req[] 를 직접 강제하므로 불필요 |
| Q4 | turnOffLED() 변경 | 그대로 유지 — 다른 호출 path 안전망 |

## 7. 검증

### 7.1 정적

- 빌드 성공
- `Grep led_force_fade_off` — 정의 1 + 호출 1

### 7.2 실기

| # | 시나리오 | 기대 |
|---|---|---|
| a | 충전기 연결 + 롱-터치 (직전 BATT_READY GREEN) | POWER_OFF 4 회 → fade-out (GREEN 점진) → BLACK. 잔상 없음 |
| b | 매핑 중 (BLUE/PURPLE) 충전기 연결 → 절전 | fade-out (BLUE/PURPLE 점진) → BLACK |
| c | ISD 분리 3 분 후 자동 절전 | 동일 |
| d | 부팅 시 turnOffLED 호출 (POWER_ON 직전) | 변화 없음 (LED 가 BLACK 인 상태) |
