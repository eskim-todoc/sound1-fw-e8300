---
name: 검증 페르소나 26
purpose: 적대적 검증 - DMIC 입력도 ADC와 동일하게 fractional delay를 지원한다
type: tasks
maturity: experimental
tags: [beamforming, agent-log, verify]
---

# [단계3·검증 26] DMIC 입력도 ADC와 동일하게 fractional delay를 지원한다

**판정**: `CONFIRMED`

## 근거
HW p.563 §18.2.1 원문이 "the DMIC pre-decimation filters also support fractional delay in the same way that the ADCs do"라고 명시적으로 서술한다. DMIC는 ADC와 동일한 decimation filter 경로(AUDIO_MUX_CFG_INPUT_CH*_SRC로 소스 선택)를 공유하므로 AUDIO_ADC_DEC_CTRL의 DELAY_INTEGER(bits 26:24, 1/8 샘플 스텝)·DELAY_FRACTIONAL(bits 21:16, 0~SFCR)이 채널 소스가 DMIC일 때도 동일하게 동작한다(HW p.450–451). 반증 근거 없음 — 레지스터 설명 어디에도 ADC 소스 전용 제한이 없다.

## 데이터시트 근거
HW p.563 §18.2.1: "The DMIC pre-decimation filters also support fractional delay in the same way that the ADCs do." / HW p.445 §14.4: decimation filter 소스 선택이 ADC·Digital microphone·Bypass 중 채널별로 가능하며 동일 register(AUDIO_ADC_DEC_CTRL) 적용. / HW p.450–451: AUDIO_ADC_DEC_CTRL DELAY_INTEGER/DELAY_FRACTIONAL 필드 — 소스 종류 한정 없음.

