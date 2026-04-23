# LED 절전 진입 잔상색 제거 현상 분석

작성자: 김은수
작성일: 2026-04-23
상태: 리뷰 대기
근거 요구사항: [`[요구사항] LED 절전 진입 잔상색 제거 Rev.0`]([요구사항]%20LED%20절전%20진입%20잔상색%20제거%20Rev.0%20by%20김은수.md)

---

## 1. 보드 LED 출력 매핑 (1.5 세대 — Active High)

[`Board_OTE_ver1_5.h:129`](../src/2__cm3/99_includeBoard/Board_OTE_ver1_5.h#L129) 에서 `LED_IS_ACTIVELOW` 가 주석 처리됨 → **Active High** 동작.

| 색 | R | G | B | GPIO 출력 (Active High) |
|---|:---:|:---:|:---:|---|
| BLACK | OFF | OFF | OFF | LOW, LOW, LOW |
| BLUE | OFF | OFF | ON | LOW, LOW, HIGH |
| GREEN | OFF | ON | OFF | LOW, HIGH, LOW |
| RED | ON | OFF | OFF | HIGH, LOW, LOW |
| SKYBLUE | OFF | ON | ON | LOW, HIGH, HIGH |
| ORANGE | ON | ON | OFF | HIGH, HIGH, LOW |
| PURPLE | ON | OFF | ON | HIGH, LOW, HIGH |
| WHITE | ON | ON | ON | HIGH, HIGH, HIGH |

`turnOffLED()` 의 active high 경로 ([LedOutput.c:692-696](../src/2__cm3/Cortex-M3-src/systemControl/LedOutput.c#L692-L696)):

```c
Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
```

→ R, G, B **순차 LOW** 처리.

---

## 2. 잔상색 발생 시나리오

### 2.1 GPIO 순차 호출 transient

`Sys_GPIO_Set_Low()` 는 단일 핀 비트 변경. 3 회 순차 호출 사이에 GPIO 상태가 다음 표와 같이 진행 → 직전 LED 색에 따라 의도치 않은 중간색이 짧게 표시.

| 직전 색 | 1) R LOW 후 | 2) G LOW 후 | 3) B LOW 후 (BLACK) |
|---|---|---|---|
| BLUE  (L L H) | L L H — BLUE     | L L H — BLUE  | L L L — BLACK ✓ |
| GREEN (L H L) | L H L — GREEN    | L L L — BLACK | L L L — BLACK ✓ |
| RED   (H L L) | L L L — BLACK    | L L L — BLACK | L L L — BLACK ✓ |
| ORANGE(H H L) | L H L — **GREEN** | L L L — BLACK | L L L — BLACK |
| PURPLE(H L H) | L L H — **BLUE**  | L L H — BLUE  | L L L — BLACK |
| **SKYBLUE** (L H H) | L H H — SKYBLUE | L L H — **BLUE**  | L L L — BLACK |
| **WHITE**   (H H H) | L H H — **SKYBLUE** | L L H — **BLUE** | L L L — BLACK |

**관찰 결과 ↔ 표 일치**:
- "파란색 깜빡 후 마지막에 하늘색 살짝" 패턴은 **WHITE → SKYBLUE → BLUE → BLACK** 시퀀스의 일부.
- 다른 색 (ORANGE → GREEN, PURPLE → BLUE) 도 잠깐 다른 색 보일 수 있음 → 사용자가 "다른 색상에서도 그런 것 같다" 고 한 부분과 일치.

### 2.2 절전 진입 직전 LED 가 어떤 색일 수 있는가

POWER_OFF 4 회 점멸 burst 자가 해제 직후, 다음 main loop iteration 에서:
- `systemStatus.systemOff = true` 결정 ([systemControl.c:287](../src/2__cm3/Cortex-M3-src/systemControl/systemControl.c#L287))
- `led_arbiter_tick()` 호출 — best 가 다른 src (BATTERY / ISD / MAPPING) 의 요청에 따라 새 색으로 갱신
- cross-fade Phase B 시작 — `LED_outputColor` 가 새 색으로 변경
- main loop 가 `if (systemState.systemOff == true) break;` 로 즉시 break → `func_sleep()` 진입

→ `func_sleep()` 진입 시 `LED_outputColor` 와 GPIO 상태가 임의의 색일 수 있음.

또한 절전 진입 케이스 별로 직전 색이 다를 수 있다:
- 충전 케이스 cover 닫힘: BATT_READY (GREEN) / BATT_MID (ORANGE) 등
- ISD 분리 3 분: 같은 케이스
- 매핑 → 충전기 연결: MAPPING_*_BATT_LOW (PURPLE), MAPPING_*_BATT_READY (BLUE) 등 잔존 가능

`func_sleep()` 안에서 `turnOffLED()` 가 호출되며, 직전 GPIO 색에 따라 §2.1 표의 잔상이 발생.

### 2.3 GPIO transient 가 사람 눈에 보이는 시간 단위인가

`Sys_GPIO_Set_Low()` 자체는 µs 단위 — 보통 안 보임. 그러나:

- `turnOffLED()` 직전 PWM cycle 의 ON 구간이라면, 마지막 LED_OUT() 호출이 GPIO 를 색상 ON 상태로 만들었을 가능성. 그 후 즉시 `turnOffLED()` 의 GPIO LOW 순차 처리가 시작 → 색 → 중간색 → BLACK.
- 만약 직전 PWM cycle 의 OFF 구간이면 GPIO 가 이미 LOW (BLACK) → `turnOffLED()` 영향 없음 → 잔상 없음.

→ **PWM cycle phase 에 따라 잔상 유무 달라짐**. 사용자가 매번 보지 않을 수 있음.

추가 — `func_sleep()` 의 클럭 변경 (`ci_power_sleep()`) 으로 SYSCLK 이 30.72 MHz → 2.56 MHz 로 12배 느려지면, GPIO 호출 사이 µs 도 12배 길어짐. 그러나 `turnOffLED()` 는 `ci_power_sleep()` 호출 전에 실행되므로 이 영향은 별개.

---

## 3. 영향 범위

| 변경 대상 | 파일 |
|---|---|
| `turnOffLED()` 본체 — fade-out 처리로 교체 | `LedOutput.c` |
| `delay_ms` include | `LedOutput.c` (이미 ci_util.h 참조 가능) |
| `func_sleep()` 호출 흐름 — 변경 없음 (turnOffLED 내부에서만 보강) | `main.c` 영향 없음 |
| 다른 호출자 (`systemControl.c` 의 부팅 path) — 변경 없음 (함수 시그니처 동일) | 영향 없음 |

→ 단일 파일 수정. 호출자 호환성 유지.

## 4. 결정 필요 사항

| # | 항목 | 옵션 | 권장 |
|---|---|---|---|
| Q1 | turnOffLED 처리 방식 | (A) GPIO atomic write (B) fade-out 후 BLACK (C) 색별 GPIO 순서 최적화 | **(B)** — perceived 곡선 활용, 시각 자연스러움 |
| Q2 | fade-out 시간 | (A) `LED_DIMMING_FADE_MAX_MS` (150ms) (B) 더 짧게 | **(A)** — 기존 매크로 일관성 |
| Q3 | 부팅 시 turnOffLED 호출 영향 | 부팅 시 LED 가 BLACK 이면 fade-out 즉시 종료 → 영향 미미 | 그대로 |
| Q4 | LED_OUT() 자체 GPIO 순서 보정 | (A) 함께 수정 (B) 별도 작업 | **(B)** — 본 작업은 turnOffLED 만, 효과 확인 후 결정 |

---

## 5. 검증

### 5.1 정적

- 빌드 성공
- `Grep`: `turnOffLED` 호출자 변화 없음 (시그니처 동일)
- `delay_ms` 의 include 누락 없음

### 5.2 실기

| # | 시나리오 | 기대 |
|---|---|---|
| a | 충전 중 절전 진입 (직전 BATT_MID = ORANGE) | ORANGE → GREEN 잔상 없이 부드럽게 OFF |
| b | 매핑 중 충전기 연결 → 절전 (직전 MAPPING_BATT_LOW = PURPLE) | PURPLE → BLUE 잔상 없음 |
| c | 사용자 보고 시나리오 (BLUE 깜빡 후 SKYBLUE) | POWER_OFF 4 회 점멸 후 잔상 없이 OFF |
| d | 부팅 시 POWER_ON 직전 turnOffLED | 부팅 시간 변화 미미, 정상 진행 |
| e | UI 명령으로 LED 강제 표시 후 turnOffLED 호출 | 잔상 없이 OFF |
