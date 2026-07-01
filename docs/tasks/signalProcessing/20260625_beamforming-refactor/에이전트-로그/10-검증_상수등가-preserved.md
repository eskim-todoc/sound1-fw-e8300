---
name: 리팩토링 검증 10
purpose: 상수등가 preserved
type: tasks
maturity: experimental
tags: [beamforming, refactor, agent-log, verify-stage4]
---

# [검증 10] 상수등가 — `PRESERVED`

## 근거
모든 신규 상수의 정의값이 치환 전 원 리터럴과 정확히 일치합니다.

1. LIB_AUDIO_DMIC_COUNT_SINGLE=1, LIB_AUDIO_DMIC_COUNT_DUAL=2 (lib_audio_in.h:32-33) — get_enabled_DMIC_count()==1/==2 치환 완전 대응.
2. LIB_DMIC_IDX_DMIC1=0, LIB_DMIC_IDX_DMIC2=1 (lib_audio_in.h:36-37) — g_lib_dmic_in_buffers[0]/[1] 마이크 차원 치환 완전 대응.
3. AUDIO_MIX_NORMALIZE_RSHIFT=1 (audioMixer.h:24) — audioMixer.c 빔포밍 16줄 전체 >> 1 치환 확인(라인 74~89).
4. 배열 첫 차원 [LIB_AUDIO_IN_DMIC_ENABLE_COUNT] — LIB_AUDIO_IN_DMIC_ENABLE_COUNT=2이므로 원래 [2]와 동일. lib_audio_in.h:150, lib_audio_in.c:19 양쪽 모두 적용.
5. 동작 보존: I2S 스트리밍 1-DMIC 폴백(main.c:169, system_control.c:71,115), 빔포밍 블록 경계 p_mix[15]=delay_buf1[0](audioMixer.c:89), L→DMIC2 지연/R→DMIC1 지연 조향(main.c:456-462), HW FRAC=6 소수점 지연(system_control.c:89,95) 모두 이상 없음.
6. 데드코드: LIB_ADC_DEC_CTRL_VAL_0_9958, g_enabled_mic_count 어디에도 없음 — 깔끔히 제거됨.

