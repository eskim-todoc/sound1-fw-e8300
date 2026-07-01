---
name: 검증 페르소나 24
purpose: 적대적 검증 - DELAY_INTEGER는 정수가 아니라 1/8 샘플 단위(0~7/8)로 지연한다
type: tasks
maturity: experimental
tags: [beamforming, agent-log, verify]
---

# [단계3·검증 24] DELAY_INTEGER는 정수가 아니라 1/8 샘플 단위(0~7/8)로 지연한다

**판정**: `CONFIRMED`

## 근거
HW p.450-451 및 p.29746-29767 레지스터 설명에서 DELAY_INTEGER(bits 26:24)는 "Delay the sampling by N/8 of the sampling frequency"로 명시되어 있다. 값 범위 0~7은 ADC_INTEGER_DELAY_0(0/8)~ADC_INTEGER_DELAY_7(7/8) 즉 1/8 샘플 스텝이며, 최대 7/8 샘플(≈54.7µs)까지만 가능하고 정수 1샘플(62.5µs) 지연은 이 필드 단독으로 불가능하다. 필드명 "INTEGER"는 3비트 정수 인덱스(0~7)를 뜻할 뿐이며, 물리 단위는 1/8 샘플이다. DELAY_FRACTIONAL(bits 21:16)이 1/8 샘플 이하 미세조정을 담당하는 별개 필드로 분리된 구조가 이 해석을 더 강하게 지지한다. 반증 시도: 필드명 자체("INTEGER")가 정수 샘플 단위임을 암시할 수 있으나, 데이터시트 원문 "N/8"이 이를 명확히 부정한다. 회의적 검토 후에도 주장이 데이터시트 사실 D와 완전히 일치한다.

## 데이터시트 근거
HW p.450-451, p.29746-29767: DELAY_INTEGER bits 26:24 — "Delay the sampling by N/8 of the sampling frequency", 범위 ADC_INTEGER_DELAY_0(0)~_7(7) = 0/8~7/8 샘플. 조사_데이터시트-그라운딩.md §3 (p.57): "정수 이름이지만 물리적으로 1/8 샘플 스텝".

