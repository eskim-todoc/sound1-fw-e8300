---
name: 리팩토링 검증 11
purpose: 로직불변 preserved
type: tasks
maturity: experimental
tags: [beamforming, refactor, agent-log, verify-stage4]
---

# [검증 11] 로직불변 — `PRESERVED`

## 근거
모든 제어흐름·조건식·배열 인덱스 산술·함수 호출 인수 순서가 리팩토링 전과 동일하게 보존됨을 실파일 Read 기반으로 확인.

1. 신규 상수: LIB_AUDIO_DMIC_COUNT_SINGLE=1, LIB_AUDIO_DMIC_COUNT_DUAL=2, LIB_DMIC_IDX_DMIC1=0, LIB_DMIC_IDX_DMIC2=1, AUDIO_MIX_NORMALIZE_RSHIFT=1 모두 선언 확인.

2. 빔포밍 3인수 순서(지연현재/지연직전/기준현재) 보존:
   - Left: DMIC2[BUF_MAX_CNT-2=0], DMIC2[BUF_MAX_CNT-1=1], DMIC1[0] → 지연=DMIC2, 기준=DMIC1
   - Right: DMIC1[BUF_MAX_CNT-2=0], DMIC1[BUF_MAX_CNT-1=1], DMIC2[0] → 지연=DMIC1, 기준=DMIC2
   요구사항(Left:지연=DMIC2/기준=DMIC1, Right:지연=DMIC1/기준=DMIC2)과 정확히 일치.

3. BUF_MAX_CNT 산술: LIB_AUDIO_IN_BUF_MAX_CNT=2이므로 -2=0(현재), -1=1(직전). tdc_copy_DMIC_buffers의 shift 루프(j<1, [1]←[0])도 일치.

4. 빔포밍 믹서 내부: p_mix[0..14]=no_delay[i]+delay_buf0[i+1], p_mix[15]=no_delay[15]+delay_buf1[0]로 블록 경계 처리 정확. 16줄 전부 >> AUDIO_MIX_NORMALIZE_RSHIFT(=1).

5. HW 소수점 지연 방향: Left_Ear는 ch2(DMIC2)=front에 VAL_0_0250, ch0(DMIC1)=rear에 VAL. Right_Ear는 ch0(DMIC1)=front에 VAL_0_0250, ch2(DMIC2)=rear에 VAL. SW 빔포밍 지연 채널 배정과 일치.

6. 데드코드 완전 제거: g_enabled_mic_count 변수 없음(grep 0건), LIB_ADC_DEC_CTRL_VAL_0_9958/FRACTIONAL_DELAY_29 없음, 믹서 내 raw >>1 리터럴 없음.

7. I2S/Mapping 1-DMIC 폴백: I2S 스트리밍+DUAL→enable_1_DMIC(), 비스트리밍+SINGLE→enable_2_DMICs(), Mapping Live→enable_1_DMIC() 모두 보존.

토큰 의미 변화 없음, regression 없음.

