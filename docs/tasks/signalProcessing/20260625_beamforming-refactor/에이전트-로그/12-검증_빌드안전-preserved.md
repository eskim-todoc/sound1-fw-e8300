---
name: 리팩토링 검증 12
purpose: 빌드안전 preserved
type: tasks
maturity: experimental
tags: [beamforming, refactor, agent-log, verify-stage4]
---

# [검증 12] 빌드안전 — `PRESERVED`

## 근거
제거된 심볼(LIB_ADC_DEC_CTRL_VAL_0_9958, FRACTIONAL_DELAY_29, g_enabled_mic_count) src 트리 전체 grep 결과 0건 — 잔존 참조 없음. 신규 상수 가시성: lib_audio_in.h 상수는 main.c(직접), system_control.c(system_control.h→stimulationStrategy.h→main.h→lib_audio_in.h 간접), lib_audio_in.c(직접) 모두 include 경로 확인. AUDIO_MIX_NORMALIZE_RSHIFT는 audioMixer.c가 audioMixer.h를 직접 include. 빔포밍 16줄 전부 AUDIO_MIX_NORMALIZE_RSHIFT 치환 완료, raw >> 1 잔존 없음. 배열 인덱스: DMIC1=0, DMIC2=1 모두 크기 2 범위 내. BUF_MAX_CNT-2=0(현재), BUF_MAX_CNT-1=1(직전) 산술 정확. COUNT_SINGLE=1, COUNT_DUAL=2는 원래 리터럴 값과 동일하여 동작 보존. I2S/Mapping 폴백, L/R 빔포밍 조향, HW FRAC=6 지연 경로 모두 유지.

