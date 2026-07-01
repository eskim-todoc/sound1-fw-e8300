---
name: 빔포밍구현 분석 01
purpose: HW-채널·SEL·패리티-검증가
type: tasks
maturity: experimental
tags: [beamforming, impl-review, agent-log, analysis]
---

# [분석 01] HW 채널·SEL·패리티 검증가

**신뢰도**: high

## 결론
ch0/ch2 짝수 패리티(스큐0)·SEL↔엣지 정합·IOC IN0/IN2 매핑 모두 현재 활성 코드 기준으로 정확하다. 오류는 없으며, 구버전 주석 2건과 데드코드 1건만 잔존한다.

## 발견(severity·근거)
- **[info]** 짝수 패리티 스큐0 — ch0(DMIC0_DATA_RE/U7)·ch2(DMIC2_DATA_FE/U9) 모두 짝수 채널이므로 decimation 시분할 내 동일 슬롯 처리 → 채널 간 고유 스큐 0
  - 근거: lib_audio_in.h:89 활성 매크로 LIB_DMIC_AUDIO_MUX_CFG_INPUT_CH_SRC = (ADC3_OUT | DMIC2_DATA_FE | ADC1_OUT | DMIC0_DATA_RE). 슬롯 위치: ch3=ADC3_OUT, ch2=DMIC2_DATA_FE, ch1=ADC1_OUT, ch0=DMIC0_DATA_RE. ch0·ch2 모두 짝수 → beam-forming 분석.md §1 '짝수 채널이 동일 시분할 슬롯'과 일치. 이전 주석(line 88: ch1=DMIC1_DATA_RE+ch2=DMIC2_DATA_FE)은 홀수·짝수 혼재라 스큐 1/8 샘플이 발생했으나, 현재 코드는 이 문제를 해소한 상태.
- **[info]** SEL↔엣지 정합 — U7 SEL=GND↔DMIC0_DATA_RE(rising), U9 SEL=VDD↔DMIC2_DATA_FE(falling) 모두 정합
  - 근거: lib_audio_in.h:89 DMIC0_DATA_RE(ch0=U7/DIO23, rising 캡처). lib_audio_in.c:152 DIO->SRC_DMIC_DATA = DMIC0_DATA_SRC_DIO_23 | DMIC2_DATA_SRC_DIO_17. 조사_SEL엣지-그라운딩.md §3: 'SEL=GND → low data channel → rising(_RE)', 'SEL=VDD → high data channel → falling(_FE)'. 회로도 확인(2026-06-23): U7 SEL=GND → DMIC0_DATA_RE 정합 ✓, U9 SEL=VDD → DMIC2_DATA_FE 정합 ✓. 검증.md: confirmed 4/4, refuted 0.
- **[info]** IOC IN0→FA0_0(MIC0=U7)·IN2→FA0_1(MIC1=U9) 매핑 정확
  - 근거: lib_audio_in.h:58 활성 정의 LIB_IOC_ADC_CFG_VAL = (IOC_INPUT_CFG_IN0_FA0_0 | IOC_INPUT_CFG_IN1_NONE | IOC_INPUT_CFG_IN2_FA0_1 | IOC_INPUT_CFG_IN3_NONE). AUDIO_MUX ch0=DMIC0_DATA_RE → decimation → IOC IN0 → FA0_0 = FIFO MIC0 = HCT_A0_0. ch2=DMIC2_DATA_FE → IOC IN2 → FA0_1 = HCT_A0_1. lib_audio_in.c:228-229 clear_FIFO_all: 'FA0_0: MIC0, FA0_1: MIC1'. main.c:333 g_lib_dmic_in_buffers[0][0][i] = HCT_A0_0[i] (U7/DMIC0), line 346 g_lib_dmic_in_buffers[1][0][i] = HCT_A0_1[i] (U9/DMIC2). configure_audio_path_all() line 141-142: DEC_CTRL ch0(EZ)·ch2(QCC) 설정과 완전 일치.
- **[info]** 버퍼 인덱스 [0]=최신 가정 — 경계 처리 로직으로 유일하게 검증됨
  - 근거: audioMixer.c:74 p_mix[15] = no_delay[15]>>5 + delay_buf1[0]>>5. delay_buf0=current block, delay_buf1=previous block. [0]=최신이면 delay_buf0[15]는 current 최고령 샘플(T-15), delay_buf1[0]는 previous 최신 샘플(T-16) → 시간적으로 인접 ✓. [0]=최고령이면 delay_buf0[15]=T(최신)·delay_buf1[0]=T-31(직전 블록 최고령)으로 15샘플 갭 발생 ✗. main.c:330 내부 루프: g_lib_dmic_in_buffers[0][1][i] = g_lib_dmic_in_buffers[0][0][i] (이전 블록을 [1]에 시프트) 후 line 333 [0]에 신규 FIFO 데이터 저장 → [0]=최신 ✓.
- **[info]** HW fractional 지연(0_0250) front mic에만 맵 변경 시 부여 — 초기화 시 양 채널 delay=0이 유지됨
  - 근거: configure_audio_path_all() lib_audio_in.c:141-142에서 ch0·ch2 모두 LIB_ADC_DEC_CTRL_VAL(delay=0)으로 초기화. system_control.c Normal_PowerMode_event_mapChange() line 85-103: Left_Ear → ch0(EZ)=VAL(0), ch2(QCC)=VAL_0_0250. Right_Ear → ch0(EZ)=VAL_0_0250, ch2(QCC)=VAL(0). VAL_0_0250 = FRAC=6 = 6/240 = 0.025 샘플 fractional delay. lib_audio_set_front_mic()로 LEFT/RIGHT 동시 설정 → SW 1샘플 시프트(delay_buf0[i+1])와 조합 → 합계 1.025 샘플 = 64.06µs.
- **[minor]** [minor 결함] lib_audio_in.h:55 주석이 현재 활성 코드와 불일치 — 'U7: IN1: FA0_0'으로 기재되어 있으나 실제 활성 코드는 IN0
  - 근거: lib_audio_in.h:55 주석: '// U7, DMIC_CLK1/CAL, DMIC_OUT1 : IN1 : FA0_0'. 그러나 line 57이 주석 처리되고 line 58 활성: IOC_INPUT_CFG_IN0_FA0_0. 주석은 구 버전(IN1 경로) 기술이 삭제되지 않고 잔존. 읽는 사람이 IN1이 U7 경로라고 오인할 수 있어 추후 유지보수 오류 위험.
- **[minor]** [minor 결함] main.c:88 주석 'ADC 1, DMIC 1 : FIFO_A0_1 : QCC'가 틀림 — 실제 활성 채널은 ADC 2, DMIC 2(ch2)
  - 근거: main.c:88: '// 오디오 IN 1, ADC 1, DMIC 1   :   FIFO_A0_1   :   DMIC_CLK2 (DIO10) + DMIC_OUT2 (DIO17)   :   QCC'. 그러나 활성 AUDIO_MUX에서 QCC(U9/DIO17)는 ch2(DMIC2_DATA_FE) 슬롯에 있고, DEC_CTRL도 SYS_SET_ADC_DEC_CTRL(AUDIO, 2, ...) (lib_audio_in.c:142). 'ADC 1, DMIC 1'은 이전 ch1 기반 구성의 잔존 주석.
- **[info]** [info] LIB_ADC_DEC_CTRL_VAL_0_9958 정의만 존재, 호출처 없는 데드코드
  - 근거: lib_audio_in.h:110에 정의(INT=7, FRAC=29 = 7.875+0.12083 = 0.9958샘플). grep 결과 system_control.c·main.c·lib_audio_in.c 어디에도 이 매크로 호출 없음. 그라운딩에서도 '정의만 있고 호출 없음'으로 확인. 현재 사용 중인 0_0250(0.025샘플)과는 다른 지연값으로, 이전 22mm 단독 HW 지연 실험 흔적으로 추정. 빌드·동작에 영향 없으나 혼란 유발 가능.
- **[info]** [info] 1-DMIC 경로와 2-DMIC 경로의 DMIC SRC 레지스터 이름 불일치 — 동작 버그는 아님
  - 근거: lib_audio_in.c:134 (1-DMIC path): DIO->SRC_DMIC_DATA = DMIC1_DATA_SRC_DIO_23. lib_audio_in.c:152 (2-DMIC path): DMIC0_DATA_SRC_DIO_23 | DMIC2_DATA_SRC_DIO_17. 두 경로 모두 DIO23(U7)을 동일 물리 핀으로 지정하지만 SRC 레지스터 필드명이 다름(DMIC1 vs DMIC0). 1-DMIC 경로는 AUDIO_MUX ch1(DMIC1_DATA_RE)+SRC_DMIC_DATA DMIC1 슬롯으로 내부 일관성 ✓, 2-DMIC 경로는 ch0(DMIC0_DATA_RE)+DMIC0 슬롯으로 내부 일관성 ✓. 두 경로는 독립 #if 분기이므로 동시 활성화 불가 — 혼용 버그 없음.

## 개선 제안
- lib_audio_in.h:55 주석을 'U7, DMIC_CLK1/CAL, DMIC_OUT1 : IN0 : FA0_0'으로 수정 (IN1→IN0)
- main.c:88 주석을 '오디오 IN 2, ADC 2, DMIC 2 : FIFO_A0_1 : DMIC_CLK2 (DIO10) + DMIC_OUT2 (DIO17) : QCC'로 수정 (DMIC1→DMIC2, ADC1→ADC2)
- LIB_ADC_DEC_CTRL_VAL_0_9958 매크로(lib_audio_in.h:109-110)를 제거하거나 TODO 주석으로 의도 명시(현재 사용 안 함)
