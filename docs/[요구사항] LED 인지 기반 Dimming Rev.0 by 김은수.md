# LED 인지 기반 Dimming 보강 요구사항

작성자: 김은수
작성일: 2026-04-23
상태: 리뷰 대기
근거: 사용자 지시 — Cross-fade 적용 후 추가 피드백
선행 문서:
- [`[구현계획] LED 패턴 갱신·절전 가드·Dimming Rev.0`]([구현계획]%20LED%20패턴%20갱신·절전%20가드·Dimming%20Rev.0%20by%20김은수.md)
- [`[구현계획] LED Cross-fade 보강 Rev.0`]([구현계획]%20LED%20Cross-fade%20보강%20Rev.0%20by%20김은수.md)

---

## 1. 배경

직전 Rev (cross-fade) 까지의 dimming 은 다음 한계가 있다:

| # | 한계 | 결과 |
|---|---|---|
| L1 | `LED_DIMMING_FADE_MS` (150 ms) 고정. ON 시간이 fade × 2 보다 짧으면 삼각파 처리되어 **정점 (max brightness) 도달 못 함** | 짧은 점멸 (예: `LED_ST_MAPPING_ISD_BATT_LOW` ON 100 ms) 가 항상 어둡게 보임 |
| L2 | brightness 가 시간에 대해 **선형으로 증가** | 사람 눈은 밝기 변화를 비선형 (대략 cube-root) 으로 인식하므로, 선형 ramp 는 "처음에 빠르게 밝아지고 후반은 거의 정체" 처럼 부자연스럽게 보임 |

L1 은 짧은 ON 패턴의 시각 인식 강도가 약화되고, L2 는 fade 자체가 균등하게 느껴지지 않는 문제다. 두 항목 동시 보강이 필요.

---

## 2. 요구사항

### 2.1 패턴별 fade 시간 자동 조정 (L1 해결)

각 점멸 패턴의 ON 시간 (`on_ms`) 에 맞춰 **fade-in / fade-out 시간을 동적으로 줄여**, ON 구간 내에 반드시 정점 (max brightness) 에 도달하도록 한다.

- fade-in / fade-out 모두 동일하게 자동 조정 (사용자 명시: "Fade in 뿐만 아니고, Fade out도 마찬가지")
- 정점 유지 시간을 어느 정도 확보 (시각 인식 위해)

### 2.2 사람 눈의 인지 곡선 적용 (L2 해결)

시간 진행에 따른 brightness 변화가 **사람 눈에 균등하게 보이도록**, 표준 perceptual brightness curve 를 적용한다. 채택 곡선:

> **CIE 1931 Lightness (L\*)** — 색·밝기 인지 모델 표준
> Y = ((L + 16) / 116)³  if L > 8
> Y = L / 903.3          if L ≤ 8
> (L: 0~100 perceived lightness, Y: 0~1 relative luminance)

근거 (요약):
- 사람 눈의 밝기 인지는 비선형 (Stevens' Power Law: 지각 ∝ luminance^≈0.33)
- LED 의 PWM duty 는 luminance 와 거의 선형
- 따라서 **시간 → 인지 → PWM duty** 변환에 비선형 매핑이 필요
- CIE 1931 L* 는 색·밝기 인지 표준이며 gamma 보정 (γ ≈ 2.2) 보다 정확

### 2.3 통합 적용 범위

| 적용 대상 | 자동 fade 조정 | perceived 곡선 |
|---|---|---|
| 점멸 패턴 ON 구간 fade-in / fade-out | **O** (on_ms 기준) | **O** |
| 색상 전환 cross-fade (Phase A: 이전 색 fade-out / Phase B: 새 색 fade-in) | X (전환은 ON 구간 무관 → 고정 fade 시간) | **O** |
| 지속 ON (`period_ms == 0`) | N/A | brightness 255 = LUT[255] = 255 그대로 |

### 2.4 수용 기준

- [ ] `LED_ST_MAPPING_ISD_BATT_LOW` (ON 100 ms) 에서 **정점에 분명히 도달** (육안 가시)
- [ ] `LED_ST_MAPPING_ISD_BATT_READY` (ON 200 ms), `LED_ST_PAIR` (ON 500 ms) 등 모든 점멸 패턴에서 정점 도달
- [ ] fade-in / fade-out 의 시각적 변화가 **균등하게** 느껴진다 (선형 시 후반 정체감 해소)
- [ ] 색상 전환 cross-fade 도 perceived 곡선이 적용되어 자연스럽게 보임
- [ ] 지속 ON 패턴 (BATT_READY 녹색 등) 의 평상시 밝기는 변화 없음 (255 → 255)

---

## 3. 비목표

- LED PWM 단계 수 (`LED_DIMMING_PWM_STEPS = 10`) 변경 — 그대로 유지
- 색상별 (R/G/B) 별도 gamma 보정 — 본 작업 범위 외 (RGB 색감 보정은 후속)
- 런타임 조정 가능한 dimming 매개변수 — compile-time 만

---

## 4. 제약

- LUT 메모리: 256 byte const (Flash 영역) 1 개
- 런타임 연산: 매 1 ms 호출 시 LUT 1회 lookup + 정수 곱셈/나눗셈 1~2회 (오버헤드 미미)
- 부동소수점 사용 금지 (LUT 미리 계산해 정수 배열로 박기)
- Cross-fade 의 Phase A/B 시간 (`LED_DIMMING_FADE_MAX_MS = 150 ms`) 그대로 유지

---

## 5. 참고 (사람 눈 인지 / LED dimming 표준)

- [Relative luminance — Wikipedia](https://en.wikipedia.org/wiki/Relative_luminance) — CIE 1931 L* 공식
- [Weber–Fechner law — Wikipedia](https://en.wikipedia.org/wiki/Weber%E2%80%93Fechner_law) — 인지 강도와 자극 강도의 로그 관계
- [LED Brightness to your eye, Gamma correction – HP LED Shield](https://ledshield.wordpress.com/2012/11/13/led-brightness-to-your-eye-gamma-correction-no/) — gamma correction 비판과 CIE L* 권장
- [Convert LED brightness to PWM value based on CIE 1931 curve — GitHub gist (mathiasvr)](https://gist.github.com/mathiasvr/19ce1d7b6caeab230934080ae1f1380e) — 256-entry LUT 사례
- [Controlling LED Brightness Using PWM — mbedded.ninja](https://blog.mbedded.ninja/programming/firmware/controlling-led-brightness-using-pwm/) — embedded LED dimming 가이드
- [What are Dimming Curves and How to Choose? — uPowerTek](https://www.upowertek.com/what-are-dimming-curves-and-how-to-choose/) — square-law dimming 설명
- [Stevens' power law — Wikipedia](https://en.wikipedia.org/wiki/Stevens%27s_power_law) — 인지 강도의 지수 법칙 (밝기 exp ≈ 0.33)
- [Perceived Brightness — ScienceDirect](https://www.sciencedirect.com/topics/computer-science/perceived-brightness) — 인지 밝기 개관

---

## 6. 시각화 — fade 비교 (목표 동작)

### 6.1 ON 100 ms 패턴 (`LED_ST_MAPPING_ISD_BATT_LOW`)

```
선형 + 고정 150 ms fade (이전):
  brightness   ↑
       ~50 ━ ▲                   ← 정점에도 못 가고 50 부근에서 turnaround (삼각파)
              ╲
       0   ━ ━━━━━━━━━━━━━━━━━━━━ → time
              0    50     100 ms

CIE L* + 자동 fade (요구):
  brightness   ↑
       255 ━━━━━━ ▔▔▔▔ ━━━━━━     ← fade ≈ 33ms, peak ≈ 33ms, fade ≈ 33ms
                ╱        ╲
       0   ━━━━━━━━━━━━━━━━━━━━━ → time
              0   33   66    100 ms
```

### 6.2 ON 1100 ms 패턴 (`LED_ST_BATT_CRITICAL`)

```
선형 + 고정 150 ms fade (이전):  fade-in 150ms 동안 비선형으로 보임 (전반 빠름, 후반 정체)
CIE L* + 동일 150 ms fade (요구): fade-in 동안 사람 눈에 균등 증가
```

→ 긴 패턴은 fade 시간 변화 없음, perceived 곡선만 적용.
