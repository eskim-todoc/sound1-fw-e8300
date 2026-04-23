# LED Dimming Cross-fade 보강 요구사항

작성자: 김은수
작성일: 2026-04-23
상태: 리뷰 대기
근거: 사용자 지시 — Rev.0 dimming 동작 검증 후 즉시 피드백
선행 문서: [`[구현계획] LED 패턴 갱신·절전 가드·Dimming Rev.0 by 김은수.md`]([구현계획]%20LED%20패턴%20갱신·절전%20가드·Dimming%20Rev.0%20by%20김은수.md)

---

## 1. 배경

Rev.0 dimming 구현 (`LedOutput.c` 의 `led_engine_run()` + `s_color_changed_ms`) 은 **새 색상의 fade-in 만** 처리한다. 이전 색이 켜져 있는 상태에서 새 패턴이 들어오면 다음과 같은 시각적 부작용이 관찰됐다:

> "현재 녹색 LED 가 켜진 상태에서 `--led pair` 명령을 쳐보니, **팍! 꺼진 다음 FADE IN** 되는 상태로 느껴짐."

즉 — 이전 색 → 즉시 OFF → 새 색 fade-in. 자연스러운 LED 전환은 **이전 색의 fade-out → 새 색의 fade-in** 이어야 한다.

---

## 2. 요구사항

### 2.1 색상 전환 cross-fade

LED 색상이 변경되는 시점 (Arbiter 가 best 를 다른 상태로 갱신했을 때) 다음 순서로 표시한다:

1. **Phase A (fade-out)**: 이전 색을 brightness 255 → 0 으로 `LED_DIMMING_FADE_MS` 동안 점진 감소
2. **Phase B (fade-in)**: 새 색을 brightness 0 → 255 으로 `LED_DIMMING_FADE_MS` 동안 점진 증가
3. 총 전환 시간 = 2 × `LED_DIMMING_FADE_MS` (현재 매크로 값 기준 300 ms)

### 2.2 예외 처리

- **이전 색이 OFF (`en__LED_BLACK`) 였을 때**: Phase A 생략, Phase B 부터 시작 (기존 Rev.0 동작과 동일)
- **fade-out 중 또 다른 색상 변경이 들어온 경우**: 진행 중인 fade-out 을 그대로 끝내고 새 색으로 fade-in. (이전 색 → 다음 색 → 또다음 색 사이 시각적 끊김 방지)
- **점멸 패턴 내 ON/OFF 토글**: 색상 전환과 무관 — 기존 Rev.0 로직 (점멸 fade-in/fade-out) 그대로

### 2.3 수용 기준

- [ ] 녹색 LED 켜진 상태에서 `--led pair` 입력 시: **녹색이 부드럽게 사라진 다음 파랑이 부드럽게 등장** (팍! 꺼지는 단절감 없음)
- [ ] 색상 전환 총 소요 시간 ≈ 300 ms (육안 체감)
- [ ] 같은 색 안에서 점멸할 때 (예: PAIR ON 500/OFF 500) 기존 fade-in/fade-out 부드러움 유지
- [ ] 부팅 시 (이전 색 BLACK) → POWER_ON 첫 표시: Phase A 생략, 즉시 fade-in

---

## 3. 비목표

- LED 우선순위 변경 — 그대로
- `LED_DIMMING_FADE_MS` 값 변경 — 150 ms 그대로 유지 (전환 시간 = 300 ms 가 적절)
- PWM 단계 (`LED_DIMMING_PWM_STEPS`) 변경 — 10 그대로
- LED HW 신호 경로 (`LED_OUT()` GPIO 매핑) 변경 — 그대로

---

## 4. 제약

- Rev.0 의 매크로 (`LED_DIMMING_FADE_MS`, `LED_DIMMING_PWM_STEPS`) 그대로 활용
- `led_engine_run()` 의 호출 주기는 1 ms 유지 (Arbiter tick 주기)
- 기존 burst (POWER_ON/OFF 게이트) 동작 영향 없음

---

## 5. 참고

- Rev.0 dimming 구현: [`src/2__cm3/Cortex-M3-src/systemControl/LedOutput.c`](../src/2__cm3/Cortex-M3-src/systemControl/LedOutput.c) 의 `led_dim_calc_brightness()`, `led_engine_run()`
