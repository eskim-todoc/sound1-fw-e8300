# LED 패턴 갱신 · 절전 모드 가드 · LED Dimming 현상 분석

작성자: 김은수
작성일: 2026-04-23
상태: 리뷰 대기
근거 요구사항: [`[요구사항] LED 패턴 갱신·절전 가드·Dimming Rev.0 by 김은수.md`]([요구사항]%20LED%20패턴%20갱신·절전%20가드·Dimming%20Rev.0%20by%20김은수.md)

---

## 1. 현행 LED 매핑 상태 정의

[LedOutput.h:69-71](../src/2__cm3/Cortex-M3-src/systemControl/LedOutput.h#L69-L71)
```c
LED_ST_MAPPING_NO_ISD,  /* 파랑 ON 1100ms / OFF 1100ms 점멸 */
LED_ST_MAPPING_ISD,     /* 파랑 지속 ON */
```

[LedOutput.c:30-31](../src/2__cm3/Cortex-M3-src/systemControl/LedOutput.c#L30-L31)
```c
[LED_ST_MAPPING_NO_ISD] = { en__LED_BLUE,   1100, 2200, 0 },  // 파랑 ON 1100ms / OFF 1100ms 점멸
[LED_ST_MAPPING_ISD]    = { en__LED_BLUE,    0,    0,    0 }, // 파랑 지속 ON
```

→ 배터리 레벨에 따라 색상 / 점멸 패턴이 달라지지 않는다. 사진 갱신본은 4가지로 분화.

[main.c:521-528](../src/2__cm3/Cortex-M3-src/main.c#L521-L528)
```c
if (map_conn)
{
    led_request(LED_SRC_MAPPING, isd_conn ? LED_ST_MAPPING_ISD : LED_ST_MAPPING_NO_ISD);
}
else
{
    led_request(LED_SRC_MAPPING, LED_ST_NONE);
}
```

→ 배터리 레벨 정보(`pct`)는 직전 블록 ([main.c:472-476](../src/2__cm3/Cortex-M3-src/main.c#L472-L476)) 에서 이미 산출되어 있어 같은 스코프에서 재사용 가능.

## 2. 현행 PAIR 패턴

[LedOutput.c:33](../src/2__cm3/Cortex-M3-src/systemControl/LedOutput.c#L33)
```c
[LED_ST_PAIR]           = { en__LED_BLUE,   180,  360,  0 },   // 파랑 ON 180ms / OFF 180ms
```

→ 신스펙 ON 500ms / OFF 500ms (`{ ..., 500, 1000, 0 }`) 로 변경.

> [!WARNING]
> PAIR latch (500 ms 최소 표시) 와 ON 시간이 우연히 같아진다. 코드 의미는 latch 가 "한 주기 보장" 이었으므로 신스펙에선 `MAX(latch, on_ms)` 같은 의미로 latch 를 1 주기 = 1000 ms 로 늘리는 것이 자연스럽다. → 구현계획에서 결정.

## 3. 현행 절전 모드 진입 결정 위치

`systemStatus.systemOff = true` 가 설정되는 지점:

| # | 위치 | 트리거 |
|---|---|---|
| 1 | [systemControl.c:187](../src/2__cm3/Cortex-M3-src/systemControl/systemControl.c#L187) | 충전 케이스 커버 닫힘 → `LED_OnTime_afterCoverClosed` 시간 경과 |
| 2 | [systemControl.c:287](../src/2__cm3/Cortex-M3-src/systemControl/systemControl.c#L287) | `POWER_OFF` 버스트 종료 → 자가 진입 |
| 3 | [systemControl.c:355](../src/2__cm3/Cortex-M3-src/systemControl/systemControl.c#L355) | 충전기 분리 ↔ 케이스 미연결 (전이 감지) |

[main.c:589-592](../src/2__cm3/Cortex-M3-src/main.c#L589-L592)
```c
if (systemState.systemOff == true)
{
    break; /* Escape this main loop to enter the ULP mode */
}
```

→ 가드 위치 후보:
- (A) `systemControl.c` 의 3개 진입 지점 각각에 동일한 가드 추가 — 분산
- (B) `main.c` 의 `if (systemState.systemOff == true) break;` 지점에 단일 가드 — 집중

매핑 / 페어링 / OTA 상태는 이미 `main.c` 로컬 변수 `BLE_communicationState`, 그리고 전역 `tdc_led_get_ind_state()` 로 접근 가능 → **(B) 안 채택**. 가드 한 곳에 모이면 검증·유지 보수가 쉬움.

## 4. 현행 ULP 루프 이후 잔여 코드

[main.c:728-747](../src/2__cm3/Cortex-M3-src/main.c#L728-L747)
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
            SYS_WATCHDOG_RESET();   /* ← 칩 리셋 — 이후 코드는 도달 불가 */
            break;
        }
    }
    else
    {
        touch_cnt = 0;
    }
}

SYS_WATCHDOG_REFRESH();
ci_timer_uninit();
ci_power_normal();

p_int32 = (int *) &cfx_cm3_sharedMemoryAll;
for (int i = 0; i < (sizeof(ST__CFX_CM3_SharedMemory_ALL) / 4); i++)
{
    p_int32[i] = 0;
}

SYSCTRL_CFX_CMD->CFX_CMD_0_ALIAS = 1;
return 0;
```

→ `SYS_WATCHDOG_RESET()` 은 **시스템 리셋** 이므로 이후 라인은 절대 실행되지 않는다. 함수 시그니처가 `int` 라 컴파일러 만족용 `return 0` 도 필요 없음(컴파일러는 무한 while 후 도달 안 함을 인지). → 모두 제거하고 `return 0` 만 남기거나, `for (;;) {}` 로 함수가 절대 반환하지 않음을 명시.

## 5. 현행 `debug_led_pattern()`

[main.c:55, 601-635](../src/2__cm3/Cortex-M3-src/main.c#L55) — 선언과 정의.

`Grep` 결과: 호출 지점 0건.

→ 선언·정의 모두 안전하게 제거 가능.

## 6. 현행 타이머 3 활용

[ci_timer.c:17-24](../src/2__cm3/Gen1_5/common/ci_timer.c#L17-L24)
```c
void TIMER_3_IRQHandler(void)
{
    g_ci_timer_main_tick++;
    enable_iteration();  /* Use when the CFX is not working */
    // 절전모드에서 정말 원하는 시간 마다 타이머 이벤트가 발생하는지 확인하는 용도
    // Sys_GPIO_Toggle(DIO19);
}
```

- Run 모드: `ci_timer_init(19)` → 0.5 ms 주기 IRQ → `enable_iteration()` 으로 메인 루프 1회 실행 게이트
- ULP 모드: `ci_timer_init_prescaled(TIMER_PRESCALE_128, 155)` → 약 500 ms 주기

[LedOutput.c:474-571](../src/2__cm3/Cortex-M3-src/systemControl/LedOutput.c#L474-L571) — `LED_OUT()` 내부에 PWM 비슷한 카운터 (`timerCounter % pwmDuty_rate`) 가 있으나 단순 ON/OFF 토글에 가까움. 듀티 = 2/3 또는 1/2.

→ Dimming 추가 위치는 이 `LED_OUT()` 함수 내부, 또는 `led_engine_run()` 에서 `LED_outputColor` 를 갱신할 때 의도된 시간 변화량(0–255 brightness 등) 을 같이 저장하고 `LED_OUT()` 에서 이를 PWM 듀티로 변환.

## 7. 영향 범위 요약

| 변경 | 영향 파일 |
|---|---|
| LED 매핑 4종 분화 + PAIR 1000ms | `LedOutput.h`, `LedOutput.c`, `main.c` |
| 절전 모드 가드 | `main.c` (단일 지점) |
| `func_sleep` ULP 루프 이후 정리 | `main.c` |
| `debug_led_pattern()` 제거 | `main.c` |
| LED Dimming | `LedOutput.c` (`LED_OUT()`, `led_engine_run()`) |

기존 호출자 영향:
- `tdc_ui_command.c` 가 `LED_ST_MAPPING_*` 을 enum 으로 직접 참조 — 새 4종 enum 추가 시 UI 명령 키워드도 추가 필요한지 확인 (구현계획 §3.2 참조).

## 8. 결정 필요 사항

| # | 항목 | 옵션 |
|---|---|---|
| Q1 | LOW 임계 히스테리시스 | (A) 단순 `pct ≤ 20` (B) `pct < 20` 진입 / `pct ≥ 22` 해제 |
| Q2 | PAIR latch 시간 | (A) 500 ms 유지 (B) 1000 ms 로 늘려 1 주기 보장 |
| Q3 | 매핑 LOW 시 페어링 PAIR 표시? | 매핑(prio 75) > PAIR(prio 70). 매핑 우선 — 변경 불필요 |
| Q4 | 기존 `LED_ST_MAPPING_ISD/_NO_ISD` enum 값 처리 | (A) 완전 삭제 (B) deprecated 주석 후 enum 유지 (UI 호환) |
| Q5 | LED Dimming fade 시간 | (A) 200 ms (B) 100 ms (C) 사용자 튜닝 가능한 매크로 |

→ 구현계획 §결정 사항 참조.
