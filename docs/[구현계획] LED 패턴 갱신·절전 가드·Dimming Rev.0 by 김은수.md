# LED 패턴 갱신 · 절전 모드 가드 · LED Dimming 구현계획

작성자: 김은수
작성일: 2026-04-23
상태: 리뷰 대기
관련:
- [`[요구사항] LED 패턴 갱신·절전 가드·Dimming Rev.0 by 김은수.md`]([요구사항]%20LED%20패턴%20갱신·절전%20가드·Dimming%20Rev.0%20by%20김은수.md)
- [`[현상분석] LED 패턴 갱신·절전 가드·Dimming Rev.0 by 김은수.md`]([현상분석]%20LED%20패턴%20갱신·절전%20가드·Dimming%20Rev.0%20by%20김은수.md)

---

## 1. 결정 사항

| # | 항목 | 결정 | 근거 |
|---|---|---|---|
| Q1 | LOW 임계 히스테리시스 | **`pct ≤ 20` 진입 / `pct ≥ 22` 해제 (±2%)** | 단독 BATT 와 동일 패턴 (10/12, 78/80) 으로 일관성 유지 |
| Q2 | PAIR latch | **1000 ms 로 변경** | ON 500/OFF 500 의 한 주기 보장 (이전 의도 보존) |
| Q3 | 매핑 vs PAIR | prio 유지 (매핑 75 > PAIR 70). 변경 없음 | |
| Q4 | 기존 매핑 enum | **완전 삭제** (`LED_ST_MAPPING_ISD`, `LED_ST_MAPPING_NO_ISD`) | UI `tdc_ui_command.c` 의 `k_state_map` 도 4종으로 갱신 |
| Q5 | Dimming fade 시간 | **150 ms (매크로화)** | 부드러우면서 점멸 짧은 패턴(ON 100/OFF 900) 도 fade 가능. 매크로로 튜닝 여지 |

---

## 2. 단계 분할 (커밋 단위)

회귀 추적 가능하도록 5개 커밋으로 분할.

| # | 커밋 메시지 (요지) | 영향 |
|---|---|---|
| C1 | LED 패턴 사진 반영 (PAIR 1000 ms, 매핑 4종 분화) | `LedOutput.h/c`, `main.c`, `tdc_ui_command.c` |
| C2 | 절전 모드 가드 — 매핑/페어링/OTA 활성 시 진입 보류 | `main.c` |
| C3 | `func_sleep()` ULP 루프 이후 dead code 제거 | `main.c` |
| C4 | `debug_led_pattern()` 제거 (미사용 함수) | `main.c` |
| C5 | LED Dimming 효과 (타이머 3 PWM 듀티 변조) | `LedOutput.c` (`led_engine_run`, `LED_OUT`) |

---

## 3. 세부 변경 지점

### 3.1 LED 패턴 enum / 디스크립터 / 우선순위

**[LedOutput.h]** `led_state_t` 변경:

```c
/* Mapping (4종 분화 - 배터리 LOW 임계 20%, ISD 연결 여부 조합) */
LED_ST_MAPPING_ISD_BATT_READY,    /* 파랑 ON 200ms / OFF 800ms 점멸 (>20%, ISD 연결) */
LED_ST_MAPPING_NO_ISD_BATT_READY, /* 파랑 지속 ON                  (>20%, ISD 미연결) */
LED_ST_MAPPING_ISD_BATT_LOW,      /* 보라 ON 100ms / OFF 900ms 점멸 (≤20%, ISD 연결) */
LED_ST_MAPPING_NO_ISD_BATT_LOW,   /* 보라 지속 ON                  (≤20%, ISD 미연결) */
```

기존 `LED_ST_MAPPING_NO_ISD`, `LED_ST_MAPPING_ISD` **제거**.

**[LedOutput.c]** `k_led_patterns[]` 변경:

```c
[LED_ST_PAIR]                      = { en__LED_BLUE,   500, 1000, 0 },  // 파랑 ON 500ms / OFF 500ms 점멸 (변경: 180/180 → 500/500)

[LED_ST_MAPPING_ISD_BATT_READY]    = { en__LED_BLUE,   200, 1000, 0 },  // 파랑 ON 200ms / OFF 800ms 점멸
[LED_ST_MAPPING_NO_ISD_BATT_READY] = { en__LED_BLUE,     0,    0, 0 },  // 파랑 지속 ON
[LED_ST_MAPPING_ISD_BATT_LOW]      = { en__LED_PURPLE, 100, 1000, 0 },  // 보라 ON 100ms / OFF 900ms 점멸
[LED_ST_MAPPING_NO_ISD_BATT_LOW]   = { en__LED_PURPLE,   0,    0, 0 },  // 보라 지속 ON
```

**[LedOutput.c]** `led_prio_of()` switch — 4개 신규 모두 prio **75** (기존 매핑과 동일):

```c
case LED_ST_MAPPING_ISD_BATT_READY:
case LED_ST_MAPPING_NO_ISD_BATT_READY:
case LED_ST_MAPPING_ISD_BATT_LOW:
case LED_ST_MAPPING_NO_ISD_BATT_LOW:
    return 75;
```

**[LedOutput.c]** `led_request()` PAIR latch — 1000 ms 로 갱신:

```c
if (src == LED_SRC_BLE_IND && st == LED_ST_PAIR)
{
    s_pair_latch_until_tick = ci_timer_get_tick() + 1000;  // 한 주기 보장
}
```

### 3.2 main.c LED 요청 로직 (매핑 분기)

[main.c:521-528] 변경 (기존 2분기 → 4분기 + 히스테리시스):

```c
/* Mapping LOW 임계 — 단독 BATT 와 동일 ±2% 히스테리시스 */
static bool s_map_low_active = false;
if (pct <= 20)        s_map_low_active = true;
else if (pct >= 22)   s_map_low_active = false;
/* 그 외 구간(pct == 21) 은 직전 상태 유지 */

#ifdef ENABLE_UI_CMD
if (!tdc_ui_command_is_led_override(LED_SRC_MAPPING))
#endif
{
    if (map_conn)
    {
        led_state_t map_st;
        if (s_map_low_active)
        {
            map_st = isd_conn ? LED_ST_MAPPING_ISD_BATT_LOW
                              : LED_ST_MAPPING_NO_ISD_BATT_LOW;
        }
        else
        {
            map_st = isd_conn ? LED_ST_MAPPING_ISD_BATT_READY
                              : LED_ST_MAPPING_NO_ISD_BATT_READY;
        }
        led_request(LED_SRC_MAPPING, map_st);
    }
    else
    {
        led_request(LED_SRC_MAPPING, LED_ST_NONE);
    }
}
```

### 3.3 UI 커맨드 키워드 갱신 (`tdc_ui_command.c`)

`k_state_map` 의 `"MAPPING_ISD"`, `"MAPPING_NO_ISD"` 항목 → 4종으로 교체:

```c
{ "MAPPING_ISD_READY",   LED_ST_MAPPING_ISD_BATT_READY },
{ "MAPPING_NO_ISD_READY",LED_ST_MAPPING_NO_ISD_BATT_READY },
{ "MAPPING_ISD_LOW",     LED_ST_MAPPING_ISD_BATT_LOW },
{ "MAPPING_NO_ISD_LOW",  LED_ST_MAPPING_NO_ISD_BATT_LOW },
```

> 정확한 키워드 형식은 코드 내 기존 컨벤션 확인 후 결정 (구현 단계에서).

### 3.4 절전 모드 가드 (`main.c`)

[main.c:589-592] 단일 지점 가드:

```c
if (systemState.systemOff == true)
{
    /* 매핑 / 페어링 / OTA 진행 중에는 절전 진입을 보류한다.
     * (사용자 조작이 끝날 때까지 동작 유지) */
    tdc_led_ind_state_t ind = tdc_led_get_ind_state();
    bool ota_active   = (ind == TDC_LED_IND_STATE_OTA_QCC) || (ind == TDC_LED_IND_STATE_OTA_EZAIRO);
    bool pair_active  = (ind == TDC_LED_IND_STATE_PAIR);
    bool map_active   = BLE_communicationState.mappingConnection;

    if (map_active || pair_active || ota_active)
    {
        ci_printw("[SYSTEM] SLEEP DEFERRED (map=%d pair=%d ota=%d) \r\n",
                  map_active, pair_active, ota_active);
        systemState.systemOff = false;  /* 다음 iteration 에서 재평가 */
    }
    else
    {
        break;  /* Escape this main loop to enter the ULP mode */
    }
}
```

> [!NOTE]
> 가드 발동 시 `systemStatus.systemOff = false` 로 되돌리므로, 다음 iteration 에서 트리거 조건이 다시 성립해야 재시도된다. 충전 케이스 커버 닫힘 카운터 (`CounterAfterCoverClosed`) 는 `systemControl.c` 내부에서 매번 누적되므로 자연스럽게 재진입 시도가 이뤄진다.

### 3.5 `func_sleep()` ULP 루프 이후 정리

[main.c:728-747] 의 `while(1)` 종료 이후 코드 모두 제거. `while(1)` 자체가 무한 루프(`SYS_WATCHDOG_RESET()` 호출 포함) 이므로 함수가 반환할 일이 없다. 컴파일러 경고 회피 위해 `return 0;` 한 줄만 남긴다.

```c
while (1)  // ULP loop
{
    SYS_WAIT_FOR_INTERRUPT;
    SYS_WATCHDOG_REFRESH();

    tdc_touch_state_t state = TDC_TOUCH_STATE_RESET;
    if (tdc_touch_get_state(&state) && state == TDC_TOUCH_STATE_TOUCH)
    {
        touch_cnt++;
        if (touch_cnt >= ULP_LONG_TOUCH_COUNT)
        {
            ci_printi("[MAIN] LONG TOUCH DETECTED, RESET \r\n");
            delay_ms(20);
            SYS_WATCHDOG_RESET();
            /* 도달 불가 — 칩 리셋 */
        }
    }
    else
    {
        touch_cnt = 0;
    }
}

return 0;  /* 도달 불가 — 컴파일러 만족용 */
```

기존 `break;` 도 도달 불가이므로 제거. `ci_timer_uninit()`, `ci_power_normal()`, sharedMemory 클리어, `SYSCTRL_CFX_CMD->CFX_CMD_0_ALIAS = 1` 모두 도달 불가 → 삭제.

### 3.6 `debug_led_pattern()` 제거

- [main.c:55] 선언 제거
- [main.c:601-635] 정의 제거

호출 지점 0건이므로 안전.

### 3.7 LED Dimming (Step F)

> [!IMPORTANT]
> Dimming 은 **시각 품질 개선** 이라 단계가 가장 정교해야 한다. 단계 C5 로 분리.

#### 설계

타이머 3 IRQ 가 1 ms 주기 (Run 모드) 로 발생하면서 `enable_iteration()` 으로 메인 루프 게이트만 열고 있다. `LED_OUT()` 은 메인 루프 안에서 1 ms 마다 호출되며, 내부에 `pwmDuty_rate = 3` 카운터로 매 3 회 중 1 회를 OFF 처리하는 거친 PWM 이 이미 있다.

신설 방식:
1. `led_engine_run()` 이 매 ms `LED_outputColor` 와 함께 **목표 brightness (0–255)** 를 갱신
   - 점멸: ON 구간 시작 시 0 → 255 까지 `LED_DIMMING_FADE_MS` 시간 동안 선형 증가, OFF 구간 시작 시 255 → 0 로 선형 감소. ON 시간이 fade 시간의 2 배 이상이면 중간은 max 유지, 미만이면 삼각파 형태(끝에서 절반에서 turnaround)
   - 색상 변경 시: 직전 색을 fade-out → 새 색을 fade-in (총 fade 시간 = 150 ms × 2)
2. `LED_OUT()` 은 `LED_outputColor` + `s_brightness` 를 받아, 1 ms 주기 16-step PWM 으로 출력

#### 구현 스케치

`LedOutput.c` 상단 매크로:
```c
#define LED_DIMMING_FADE_MS  150  /* fade-in / fade-out 각각 시간 */
#define LED_DIMMING_PWM_BITS 4    /* 1 ms 주기 16 단계 PWM */
```

`led_engine_run()` 끝부분에 brightness 산출 로직 추가 (timer_ms 와 on_ms 를 사용해 0–255 산출).

`LED_OUT()` 의 `pwmDuty_rate` 카운터 → brightness 기반 16-step PWM 으로 교체.

색상 전환 fade:
- `prev_best` 가 변경될 때 (= 현재 `reset = true` 시점) 추가 상태 `s_color_transition_remaining_ms = LED_DIMMING_FADE_MS` 를 두고, fade-out 동안엔 직전 색을, fade-in 동안엔 새 색을 출력.
- 단순화 안: 색상 변경 시 새 색을 0 brightness 로 시작 → 점진 증가. (구분 fade-out 생략, 시각적으로는 충분히 부드러움)

> [!NOTE]
> 단순화 안을 우선 채택. 시각적 품질이 부족하면 후속 Rev.1 에서 cross-fade 도입.

---

## 4. 브랜치 전략

- 브랜치: `claude_feature_led-spec-and-sleep-guard` (베이스: `claude_develop`)
- 5개 커밋 (C1~C5) 단계별 진행
- 모든 단계 완료 후 사용자 확인 → `--no-ff` 로 `claude_develop` 병합
- 사용자 추가 요청 시 `claude_main` 병합 진행

---

## 5. 검증 계획

### 5.1 단위 검증 (정적)

- 빌드 성공 (사용자 환경 Eclipse 빌드)
- `Grep`: `LED_ST_MAPPING_ISD\b`, `LED_ST_MAPPING_NO_ISD\b` 잔존 0 건 (4종 enum 만 사용)
- `Grep`: `debug_led_pattern` 잔존 0 건

### 5.2 시나리오 검증 (실기)

| # | 시나리오 | 기대 |
|---|---|---|
| a | 정상 부팅 → 매핑 연결 + ISD 연결 + 배터리 50% | 파랑 ON 200/OFF 800 점멸 |
| b | 매핑 연결 + ISD 미연결 + 배터리 50% | 파랑 지속 ON |
| c | 매핑 연결 + ISD 연결 + 배터리 15% | 보라 ON 100/OFF 900 점멸 |
| d | 매핑 연결 + ISD 미연결 + 배터리 15% | 보라 지속 ON |
| e | PAIR | 파랑 ON 500/OFF 500 점멸 |
| f | 매핑 중 + 커버 닫기 | 절전 진입 X (로그 `SLEEP DEFERRED`) |
| g | 페어링 중 + 커버 닫기 | 절전 진입 X |
| h | OTA(QCC) 중 + 커버 닫기 | 절전 진입 X |
| i | 위 셋 모두 비활성 + 커버 닫기 | 절전 진입 O (기존과 동일) |
| j | 절전 진입 후 롱-터치 3 s | 워치독 리셋 → 재부팅 (기존과 동일) |
| k | LED 색상 전환 (예: PAIR → IDLE) | 색이 부드럽게 사라지고 새 색이 점진 등장 |
| l | LED 점멸 패턴 (예: PAIR 500/500) | ON 구간 시작 시 fade-in, OFF 시작 시 fade-out |

---

## 6. 잔여 위험

- (R1) PAIR latch 1000 ms 변경으로 BLE 인증 짧은 펄스 시 LED 표시 길이 증가 (사용자 시각적 변화). 의도된 변경.
- (R2) `tdc_ui_command.c` 의 매핑 키워드 변경으로 기존 UI 커맨드 사용자 (수동 LED 강제) 가 새 키워드 학습 필요. 사용 빈도 낮을 것으로 추정 — 변경 강행.
- (R3) Dimming PWM 16-step 이 1 ms 주기로 충분히 부드러운지 실기 확인 필요. 부족하면 매크로 (`LED_DIMMING_PWM_BITS`) 로 단계 증가.
- (R4) 보라색 (`en__LED_PURPLE`) GPIO 매핑이 모든 LED_OUT 변형(LED_IS_ACTIVELOW / LED_B_pin_CFX_test / 일반) 에서 정의되어 있는지 빌드 시 확인 — 현재 `LedOutput.c:535-540, 639-644, 755-760` 모두 케이스 존재 확인 완료.

---

## 7. 작업 순서 (실제 진행)

1. [요구사항 / 현상분석 / 구현계획] 3 문서 작성 ← 본 문서 포함, 완료
2. 사용자 승인 (Auto mode 이므로 즉시 진행 가능)
3. 브랜치 `claude_feature_led-spec-and-sleep-guard` 생성
4. C1 → C2 → C3 → C4 → C5 순서로 구현·커밋
5. 사용자 확인 → `claude_develop` 으로 `--no-ff` 병합
