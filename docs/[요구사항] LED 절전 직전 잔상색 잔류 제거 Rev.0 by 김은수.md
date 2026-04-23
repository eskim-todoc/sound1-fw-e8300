# LED 절전 직전 잔상색 잔류 제거 (cross-fade 새 색) 요구사항

작성자: 김은수
작성일: 2026-04-23
상태: 리뷰 대기
근거: 사용자 추가 보고 — "여전히 절전모드 진입할 때 파란색 깜빡이고 나서 다른색 LED 가 켜져. 이번엔 녹색"

선행 문서:
- [`[구현계획] LED 절전 진입 잔상색 제거 Rev.0`]([구현계획]%20LED%20절전%20진입%20잔상색%20제거%20Rev.0%20by%20김은수.md) — turnOffLED() perceived fade-out (이번 보강과 별개로 유지)

---

## 1. 배경

직전 수정 (turnOffLED() perceived fade-out + GPIO 순서 보정) 후에도 절전 진입 시 잔상색 (파랑 점멸 후 GREEN) 이 보임. 색이 매번 다른 것은 **POWER_OFF burst 종료 직후 cross-fade Phase B 가 시작되어 새 best 색 (배터리/ISD 등 잔존 src 의 색) 이 LED_outputColor 에 잠깐 들어가기 때문**.

흐름:
1. POWER_OFF 4 회 점멸 burst 진행 (파랑)
2. burst 자가 해제 → `s_req[LED_SRC_POWER] = LED_ST_NONE`
3. 다음 `led_arbiter_tick()` → best 가 BATTERY (BATT_READY = GREEN) 등으로 변경
4. cross-fade Phase B 시작 — `LED_outputColor = GREEN`, brightness 0 부터 fade-in
5. 같은 iteration 의 systemControl() 이 `systemOff = true` 결정 → main loop break
6. func_sleep() 진입 → turnOffLED() — `LED_outputColor` 가 GREEN 인 상태로 fade-out
7. 사용자 눈에 **POWER_OFF (파랑) → GREEN 잔상 → OFF**

각 절전 진입 케이스마다 마지막 best 색이 다르므로 잔상색도 다르게 (이전 SKYBLUE, 이번 GREEN 등) 보임.

---

## 2. 요구사항

### 2.1 절전 진입 직전 LED 강제 fade-off

`func_sleep()` 진입 전에 **모든 LED src 를 LED_ST_NONE 으로 강제** → Arbiter best = IDLE → cross-fade Phase A 가 현재 색 (prev_color) 을 자연스럽게 fade-out → BLACK 도달 후 `func_sleep()` 진입.

이러면 turnOffLED() 가 호출될 시점에 이미 GPIO 가 BLACK 상태 — 추가 잔상 없음.

### 2.2 동작 흐름

```
systemOff = true 결정
  ↓
가드 통과 (매핑/페어링/OTA 비활성)
  ↓
모든 LED src → LED_ST_NONE 강제 (s_req[*] = LED_ST_NONE)
  ↓
LED_DIMMING_FADE_MAX_MS + 안정화 ms 동안 매 1ms led_arbiter_tick() 반복 호출
  → cross-fade Phase A 가 현재 색을 fade-out
  → 도달 후 BLACK 유지
  ↓
main loop break → func_sleep() → turnOffLED() (이미 BLACK, no-op)
```

### 2.3 수용 기준

- [ ] POWER_OFF 4 회 점멸 후 GREEN/SKYBLUE 등 잔상색 없이 자연스럽게 어두워짐
- [ ] 충전 중 (BATT_READY 녹색 직전) 절전 진입도 잔상 없음
- [ ] 매핑 중 (BLUE/PURPLE 직전) 절전 진입도 잔상 없음
- [ ] turnOffLED() 단독 호출 (부팅 path) 동작 영향 없음

---

## 3. 비목표

- LED_OUT() 자체 GPIO atomic write 적용 — 본 작업 범위 외
- cross-fade 자체 알고리즘 변경
- Arbiter 우선순위 조정

---

## 4. 제약

- 절전 진입 추가 지연 ≤ 200 ms (사용자 체감 미미)
- main loop break 전에만 동작 (PMIC OFF 후엔 영향 없음)
