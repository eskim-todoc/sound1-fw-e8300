# LED 절전 진입 잔상색 제거 요구사항

작성자: 김은수
작성일: 2026-04-23
상태: 리뷰 대기
근거: 사용자 관찰 — "절전모드 진입시 파란색 LED 가 깜빡거린 후, 마지막에 하늘색이 아주 살짝 보였다가 사라짐. 다른 색상에서도 그런 것 같음"

---

## 1. 배경

LED Dimming (cross-fade + perceived 곡선) 적용 후에도, **절전 모드 진입 시점에 LED 가 마지막으로 의도치 않은 중간색을 짧게 표시한 뒤 꺼지는** 현상이 보고됨. 대표 사례: POWER_OFF 4 회 점멸 (파랑) 종료 → SKYBLUE 잠깐 → OFF.

---

## 2. 요구사항

### 2.1 절전 진입 시 LED 잔상색 제거

`func_sleep()` 진입 시 LED 가 OFF 되는 과정에서 의도되지 않은 색이 표시되지 않도록 보정한다. 절전 진입 직전 LED 가 어떤 색이었든, 사용자가 인식 가능한 잔상색이 없어야 한다.

### 2.2 다른 호출 경로의 turnOffLED() 도 동일 적용

`turnOffLED()` 는 절전 진입 외에도 부팅 시 `POWER_ON` 트리거 직전 등에서도 호출됨. 모든 호출 경로에서 동일한 안전한 OFF 처리가 보장되어야 한다.

### 2.3 수용 기준

- [ ] 절전 진입 시 POWER_OFF 점멸 4 회 종료 후 의도치 않은 중간색 (SKYBLUE 등) 없이 OFF
- [ ] 부팅 시 (`StartFlag = false → true` 경로) `turnOffLED()` 호출 후 POWER_ON 시작 시 잔상 없음
- [ ] 다른 색에서 절전 진입 시 (예: 매핑 중 충전기 연결) 도 동일하게 잔상 없음
- [ ] 기존 cross-fade / perceived 곡선 동작은 영향 없음

---

## 3. 비목표

- LED Pattern 자체 변경 — 그대로
- 절전 진입 트리거 / 가드 로직 변경 — 그대로
- LED HW 신호 경로 변경 — 그대로
- LED_OUT() 자체 PWM 동작 변경 — 그대로 (turnOffLED() 만 보강)

---

## 4. 제약

- 절전 진입 시퀀스 (FPGA reset, NRF off, QCC shutdown, PMIC off, turnOffLED, ULP 진입) 의 순서 / 타이밍 영향 최소화
- LED 전원 (배터리 직결) 이 PMIC OFF 후에도 살아있다는 가정 (현재 동작 기준 fade-out 가시 가능)

---

## 5. 참고

- 직전 작업: [`[구현계획] LED 인지 기반 Dimming Rev.0`]([구현계획]%20LED%20인지%20기반%20Dimming%20Rev.0%20by%20김은수.md) 의 perceived → PWM LUT 활용
