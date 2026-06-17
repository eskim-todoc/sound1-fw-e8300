---
name: LED 시스템 분석가 결과
purpose: LED GPIO 제어 메커니즘 및 enable/disable 패턴 분석 결과
type: tasks/에이전트-로그
maturity: done
tags: [agent-result, led, gpio, enable-disable, compute-best-state]
---

# LED 시스템 분석가 결과

**TL;DR**: LED OFF 최적 삽입점은 `compute_best_state()` 시작부 조기 IDLE 반환. 기존 `s_isd_conn` 패턴과 동급 위치. 최하위 GPIO: `tdc_led_write_gpio()` (LedOutput.c:983).

---

## 1. GPIO 출력 계층 (하향)

```
compute_best_state()     LedOutput.c:687  — 우선순위 기반 최고 상태 결정
  ↓
led_engine_run()         LedOutput.c:520  — 패턴 엔진 (cross-fade, 점멸)
  ↓
LED_OUT()                LedOutput.c:1010 — PWM duty 계산
  ↓
tdc_led_write_gpio()     LedOutput.c:983  — 실제 Sys_GPIO_Set_High/Low 호출
```

## 2. 기존 LED 억제 메커니즘

| 변수/플래그 | 위치 | 역할 |
|---|---|---|
| `s_isd_conn` | LedOutput.c:87 | ISD 연결 중 특정 상태 스킵 (`led_set_isd_conn_state()`) |
| `user_off` | compute_best_state():689 | 사용자 LED OFF 설정 시 ERROR/POWER_ON/OFF 외 억제 |
| `s_led_isr_active` | LedOutput.c:68 | ISR arbiter 활성화 여부 |
| `s_led_isr_suspended` | LedOutput.c:68 | sleep 진입 직전 ISR 차단 |
| `s_tdc_burst_pending` | LedOutput.c:425 | POWER_ON/OFF burst 진행 중 표지 |

## 3. compute_best_state() 구조 (LedOutput.c:687)

```c
static led_state_t compute_best_state(void)
{
    bool user_off = (readLED_indicatorOnOff() == 2);  // 사용자 OFF 플래그
    led_state_t best = LED_ST_IDLE;
    // ... 소스별 우선순위 순회
    if (s_isd_conn == 1) { continue; }  // ISD 연결 시 특정 src 스킵
    return best;
}
```

## 4. 권장 삽입 위치

**1순위: `compute_best_state()` 시작부 조기 반환** (LedOutput.c:688)
```c
if (s_tdc_led_cradle_inhibit) { return LED_ST_IDLE; }
```
- 이유: 모든 소스 순회 전에 차단 → 완전 억제, 기존 `s_isd_conn` 패턴과 일관

**2순위: `LED_OUT()` 내부** (LedOutput.c:1029)
- 이유: GPIO 직전 차단, 패턴 엔진은 동작 → 약간의 오버헤드

**→ 1순위 채택 권장**

## 5. LedOutput.h 선언 추가 위치

`led_set_isd_conn_state()` 선언 근처에 함께 추가.
