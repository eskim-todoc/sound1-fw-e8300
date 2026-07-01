---
name: 검증 페르소나 25
purpose: 적대적 검증 - DELAY_FRACTIONAL은 0~SFCR 범위로 1/8 샘플 미만 미세 지연을
type: tasks
maturity: experimental
tags: [beamforming, agent-log, verify]
---

# [단계3·검증 25] DELAY_FRACTIONAL은 0~SFCR 범위로 1/8 샘플 미만 미세 지연을 준다

**판정**: `CONFIRMED`

## 근거
사실 D(HW p.450-451)에서 세 가지 근거가 모두 주장을 지지한다.

첫째, 범위: DELAY_FRACTIONAL valid 범위는 명시적으로 "0 ~ SFCR"이며, SFCR = round(f_adcclk/8/f_sample)-1 = round(3.84MHz/8/16kHz)-1 = 29. 주장의 "0~SFCR" 범위 기술은 정확하다.

둘째, 물리적 의미(상한 검증): SFCR=29의 물리적 의미는 1/8 샘플 주기(7.8125µs)를 ADCCLK 주기(≈0.260µs)로 나눈 값(≈30)에서 1을 뺀 것이다. 즉 DELAY_FRACTIONAL 최대값(SFCR=29)은 정확히 "1/8 샘플 - 1 ADCCLK 사이클"에 해당하고, 1/8 샘플 경계에는 도달하지 않는다. 따라서 범위 [0, SFCR]은 물리적으로 [0, 1/8 샘플) 구간을 덮는다. "1/8 샘플 미만"이라는 주장은 수치적으로 정확하다.

셋째, 기능 분리 구조: DELAY_INTEGER가 1/8 샘플 단위 정수 배수(0~7/8 샘플)를 담당하고, DELAY_FRACTIONAL이 그 사이 서브-1/8 영역을 채우는 2단 구조가 HW 레지스터 설계에서 명확히 분리되어 있다. 이는 DELAY_FRACTIONAL이 정의상 1/8 샘플을 초과할 수 없음을 아키텍처적으로 보장한다.

반증 시도: SFCR이 1/8 샘플 경계를 초과하는지 확인했으나, 수식상 SFCR+1 = round(f_adcclk/8/f_sample)이 정확히 1/8 샘플의 ADCCLK 사이클 수이므로 SFCR 자체는 항상 그보다 1 작다. 주장을 반증할 근거 없음.

## 데이터시트 근거
HW p.450-451: DELAY_FRACTIONAL bits 21:16, "Fractional sample delay", valid 0~SFCR. SFCR = round(f_adcclk/8/f_sample)-1 (HW p.29813 / p.29693). 16kHz·3.84MHz 기준 SFCR=29 → 한 스텝 ≈ 1/(8×30) 샘플 ≈ 0.26µs. 최대값 SFCR=29 = (1/8 샘플 - 1 ADCCLK 사이클), 상한 개방.

