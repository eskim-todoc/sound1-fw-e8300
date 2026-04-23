# LED 절전 직전 잔상색 잔류 제거 구현계획

작성자: 김은수
작성일: 2026-04-23
상태: 리뷰 대기
관련:
- [`[요구사항] LED 절전 직전 잔상색 잔류 제거 Rev.0`]([요구사항]%20LED%20절전%20직전%20잔상색%20잔류%20제거%20Rev.0%20by%20김은수.md)
- [`[현상분석] LED 절전 직전 잔상색 잔류 제거 Rev.0`]([현상분석]%20LED%20절전%20직전%20잔상색%20잔류%20제거%20Rev.0%20by%20김은수.md)

---

## 1. 결정

| # | 결정 |
|---|---|
| Q1 | helper 신설 — `led_force_fade_off()` (LedOutput.c) |
| Q2 | fade 시간 = `LED_DIMMING_FADE_MAX_MS + 10` ms |
| Q3 | `s_req[]` 직접 강제 (tdc_led_set_ind_state 불필요) |
| Q4 | `turnOffLED()` 그대로 유지 (안전망) |

---

## 2. 코드 변경

### 2.1 LedOutput.c — helper 신설

```c
/* ========================================================================
 *  Force fade-off — 절전 진입 직전 cross-fade Phase A 보장
 * ======================================================================== */
/* 모든 src 를 LED_ST_NONE 으로 강제 → best = IDLE → cross-fade Phase A
 * 가 현재 색 (LED_outputColor) 을 prev_color 로 캡처하고 fade-out 진행.
 *
 * 이후 main loop 가 break → func_sleep() → turnOffLED() 가 호출될 때 이미
 * GPIO 가 BLACK 상태이므로 잔상색 없음.
 *
 * func_sleep() 진입 직전 1 회만 호출. 호출 비용 ≈ FADE_MAX_MS + 10 ms. */
void led_force_fade_off(void)
{
    /* 모든 src 강제 NONE */
    for (int src = 0; src < LED_SRC__MAX; src++)
    {
        s_req[src] = LED_ST_NONE;
    }

    /* PAIR latch 도 무효화 — Arbiter 가 즉시 IDLE 로 결정하도록 */
    s_pair_latch_until_tick = 0;

    /* Phase A 진행 + 안정화 마진. led_arbiter_tick() 이 매 호출 시 LED_OUT()
     * 까지 처리하므로 GPIO 도 같이 갱신. */
    for (int i = 0; i < LED_DIMMING_FADE_MAX_MS + 10; i++)
    {
        led_arbiter_tick();
        delay_ms(1);
    }
}
```

### 2.2 LedOutput.h — 선언 추가

```c
/* 절전 진입 직전 1 회 호출. 모든 src LED_ST_NONE 강제 + cross-fade
 * Phase A 자연 fade-off 보장 (소요 ≈ LED_DIMMING_FADE_MAX_MS + 10 ms). */
void led_force_fade_off(void);
```

### 2.3 main.c — 가드 통과 후 호출

```c
if (systemState.systemOff == true)
{
    /* 가드 ... */

    if (map_active || pair_active || ota_active)
    {
        ci_printw("[SYSTEM] SLEEP DEFERRED ...");
        systemState.systemOff = false;
    }
    else
    {
        ci_printi("[SYSTEM] ENTERING SLEEP MODE \r\n");
        led_force_fade_off();   /* ← 추가: cross-fade Phase A 보장 */
        break;
    }
}
```

---

## 3. 단계 / 커밋

단일 커밋:

> Fix : 절전 진입 직전 LED cross-fade Phase A 강제 — GREEN/SKYBLUE 등 잔상색 잔류 제거

영향 파일: `LedOutput.c`, `LedOutput.h`, `main.c`.

문서 3 건 별도 커밋:

> Docs : 절전 직전 잔상색 잔류 제거 요구·분석·구현계획

---

## 4. 검증

### 4.1 정적

- `Grep led_force_fade_off` — 정의 1 + 호출 1
- 빌드 성공

### 4.2 실기 — 시나리오

요구사항 §2.3 / 현상분석 §7.2 표.

---

## 5. 위험 / 잔여

- (R1) 절전 진입 추가 ~160 ms 지연 — 사용자 체감 없음 (직전 POWER_OFF burst 1.2 s 대비 미미)
- (R2) helper 가 `s_req[]` 직접 강제 — `tdc_ui_command` override (`s_tdc_led_override[]`) 가 활성이어도 무시됨. 절전 진입은 디버깅 모드 외에서 발생하므로 영향 없음.
- (R3) `led_arbiter_tick()` 안의 user_off 검사 — 사용자 LED off 플래그가 활성이면 ERROR/POWER 외 다른 색 표시 안 함. fade-off 호출 시점엔 LED_outputColor 가 BLACK 일 수 있어 Phase A 가 즉시 종료. 정상.

---

## 6. 작업 순서

1. 본 문서 + 요구사항/현상분석 작성 — 완료
2. LedOutput.c + .h 에 `led_force_fade_off()` 추가
3. main.c 가드 통과 후 호출 추가
4. 단일 커밋
5. 사용자 확인
