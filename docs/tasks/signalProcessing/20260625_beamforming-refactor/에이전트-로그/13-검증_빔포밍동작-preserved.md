---
name: 리팩토링 검증 13
purpose: 빔포밍동작 preserved
type: tasks
maturity: experimental
tags: [beamforming, refactor, agent-log, verify-stage4]
---

# [검증 13] 빔포밍동작 — `PRESERVED`

## 근거
전체 6개 파일을 직접 Read로 읽어 확인한 결과, 모든 빔포밍 동작이 보존되어 있습니다.

1. ch0/ch2 동일 패리티: LIB_IOC_ADC_CFG_VAL = IOC_INPUT_CFG_IN0_FA0_0 | IOC_INPUT_CFG_IN2_FA0_1 (짝수 채널 IN0/IN2만 활성), system_control.c에서 ADC 채널 0과 2에만 DEC_CTRL 적용. 변경 없음.

2. 하이브리드 1.025샘플 지연: lib_audio_in.h에 FRAC=6=6/240=0.025샘플 주석 및 LIB_ADC_DEC_CTRL_VAL_0_0250 매크로 정의. audioMixer.c에서 i=0..14는 p_delay_buf0[i+1], i=15는 p_delay_buf1[0]로 블록 경계까지 정확히 SW 1샘플 지연 구현. 합계 1.025샘플 달성.

3. L/R front mic 스왑: main.c의 LIB_AUDIO_FRONT_MIC_LEFT 분기에서 DMIC2를 delay 채널, DMIC1을 no-delay 채널로 넘김. RIGHT 분기에서 반전. system_control.c에서 Left 귀 → ch2(DMIC2) = front 0.025샘플 지연, ch0(DMIC1) = rear 0. Right 귀 → 반전. 스왑 논리 일치.

4. ÷2 정규화: AUDIO_MIX_NORMALIZE_RSHIFT=1 상수로 정의(audioMixer.h L24), audioMixer.c 빔포밍 16줄 전부 >> AUDIO_MIX_NORMALIZE_RSHIFT 적용. 이전 리터럴 >> 1과 수치 동일.

5. 1-DMIC 폴백: main.c에서 I2S 스트리밍 시 enable_1_DMIC(), 비스트리밍 & SINGLE 상태 시 enable_2_DMICs(). PCM_LiveStimulation_Mode에서 DMIC_COUNT_SINGLE 분기 → audio_mix_1_buffer(DMIC1). LIB_AUDIO_DMIC_COUNT_SINGLE=1, DUAL=2로 이전 리터럴 1/2와 동치.

데드코드 제거(LIB_ADC_DEC_CTRL_VAL_0_9958, g_enabled_mic_count)는 실제 호출 경로에서 사용되지 않던 코드로, 동작에 영향 없음 확인. 리터럴→상수 치환은 모두 값 동일. 회귀 없음.

