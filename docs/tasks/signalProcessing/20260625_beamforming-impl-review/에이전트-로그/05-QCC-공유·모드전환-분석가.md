---
name: 빔포밍구현 분석 05
purpose: QCC-공유·모드전환-분석가
type: tasks
maturity: experimental
tags: [beamforming, impl-review, agent-log, analysis]
---

# [분석 05] QCC 공유·모드전환 분석가

**신뢰도**: high

## 결론
I2S FLAG 기반 1-DMIC 폴백 메커니즘은 구조적으로 정합하며 중복호출 가드도 올바르게 구현되어 있다. 다만 beamforming 버퍼 인덱스 해석 불일치, I2S 스트리밍 경로의 빔포밍 미적용(의도 불명), 그리고 QCC 통화 종료 시점과 I2S FLAG 해제 타이밍 간 결합도 미검증이 주요 결함으로 식별됨.

## 발견(severity·근거)
- **[minor]** normal_loop 1↔2 전환 가드: 조건부 호출로 중복호출은 방지되나, 전환 직후 1프레임 간 PCM_LiveStimulation_Mode가 stale 카운터로 분기할 수 있다
  - 근거: main.c:171 `if (I2S_isStreaming() && (get_enabled_DMIC_count() == 2)) enable_1_DMIC()` — 가드 자체는 정확. 그러나 enable_1_DMIC()(lib_audio_in.c:175 s_enabled_dmic_cnt=1 즉시 반영)와 PCM_LiveStimulation_Mode(main.c:440 get_enabled_DMIC_count() 참조)는 동일 루프 반복 내에서 순차 실행이 보장됨. 단일 코어 CFX에서 인터럽트 기반 플래그 체크(198행 조건) 이후 410행 PCM_LiveStimulation_Mode 실행 전에 enable_1_DMIC가 이미 호출되므로, 해당 프레임 내 count는 이미 1 — 실제 stale 문제 없음. 단, 1→2 복구 경로(main.c:189~191)에서 enable_2_DMICs 호출 후 동일 프레임에 PCM_LiveStimulation_Mode가 count==2로 분기하면 beamform 함수 실행이 즉시 시작됨 — g_lib_dmic_in_buffers[1]에 CLK 재개 후 첫 FIFO 데이터가 아직 도착하지 않았을 수 있음(DMIC 웜업 지연). 이 경우 직전 zeroed 또는 stale 값으로 beamforming 실행.
- **[info]** I2S 스트리밍 중 PCM_LiveStimulation_Mode가 beamforming을 아예 건너뛰고 audio_mix_2_buffers(dmic1, i2s_buf)로 처리하는데, 이 경로에서 DMIC2는 완전히 비활성(enable_1_DMIC 선행)이므로 DMIC2 버퍼 미사용이 보장됨 — 의도 부합
  - 근거: main.c:416~437: I2S_isStreaming() true 분기에서 audio_mix_2_buffers(&g_lib_dmic_in_buffers[0][0][0], i2s_buf) 호출 — dmic1(EZ)과 I2S 믹싱. dmic2(QCC)는 참조 없음. enable_1_DMIC(lib_audio_in.c:177 DIO10=DISABLE)로 CLK 정지 상태이므로 FA0_1 FIFO에 QCC 데이터 유입 없음. DMIC2 충돌 완전 회피됨.
- **[info]** Mapping Live(mapNum<0) 분기에서 enable_1_DMIC()는 I2S_isStreaming() 조건 없이 무조건 호출 — I2S 비활성 상태에서도 1-DMIC로 강제 고정됨
  - 근거: system_control.c:106~114: else 분기(mapNum<0)에서 조건 없이 enable_1_DMIC() 직호출. 동시에 normal_loop(main.c:189)에서 !I2S_isStreaming()&&count==1이면 enable_2_DMICs()로 복구 시도함 — Mapping Live 중에는 맵변경이 주기적으로 발생하므로 enable_1_DMIC가 반복 재설정되어 normal_loop의 복구 경로를 덮어씀. 실질적으로 Mapping Live 세션 전체에서 1-DMIC 유지됨 — 의도된 동작으로 보임.
- **[info]** beamforming 버퍼 인덱스: BUF_MAX_CNT=2일 때 delay_buf0=buffers[mic][0], delay_buf1=buffers[mic][1]로 전달되며, audioMixer.c에서 delay_buf0[i+1](i=0..14)과 delay_buf1[0](i=15)을 읽음 — 인덱스 0=최신, 1=이전 가정과 부합하나 인덱스 0=최신임이 tdc_copy_DMIC_buffers에서 검증 필요
  - 근거: main.c:454 LEFT 케이스: audio_mix_2_buffers_for_beamforming(&buffers[1][LIB_AUDIO_IN_BUF_MAX_CNT-2][0], &buffers[1][LIB_AUDIO_IN_BUF_MAX_CNT-1][0], ...) = buffers[1][0], buffers[1][1]. tdc_copy_DMIC_buffers(main.c:327~347): j=0일 때 buffers[mic][1][i] = buffers[mic][0][i](이전 데이터 shift), 이후 buffers[mic][0][i] = FIFO(신규). 따라서 [0]=최신, [1]=직전 ✓. audioMixer.c:59~74에서 p_delay_buf0[1..15]와 p_delay_buf1[0]을 사용 — SW 1샘플 지연 구현 정확. BUF_MAX_CNT=2 현재값에서 MAX_CNT-2=0, MAX_CNT-1=1이므로 인덱스 참조 정확.
- **[minor]** enable_1_DMIC → enable_2_DMICs 복구 시 DMIC2 웜업 지연 동안 beamforming이 쓰레기 데이터로 실행될 수 있다
  - 근거: enable_2_DMICs(lib_audio_in.c:180~184): DIO10을 ADCCLK 모드로 복구 → DMIC U9(QCC) CLK 재개. DMIC은 CLK 재개 후 내부 decimation filter 정착까지 수 ms(일반적으로 수십 샘플) 필요. 이 기간 FA0_1 FIFO에 invalid 데이터가 채워지고, tdc_copy_DMIC_buffers가 이를 g_lib_dmic_in_buffers[1][0]에 복사. 다음 PCM_LiveStimulation_Mode(main.c:445~462)에서 count==2로 beamforming 호출 시 delay 채널에 무효 데이터 포함. 수 ms 지속. 전환 후 클릭/잡음 가능성.
- **[major]** QCC BLE 통화 종료 시점과 I2S FLAG 핀 해제 타이밍의 결합이 코드에서 명시적으로 검증되지 않음 — FLAG 해제가 늦으면 통화 중 DMIC2 CLK 재개될 수 있다
  - 근거: I2S_FLAG_ACTIVE_LEVEL=0, I2S_FLAG_DIO_NUM=DIO29(lib_i2s.h:78~79). enable_2_DMICs 트리거: normal_loop main.c:189에서 !I2S_isStreaming()이 조건 — isStreaming은 lib_g_i2s_state==ENABLED(lib_i2s.c:181). tdc_i2s_set_streaming_state(DISABLED)는 main.c:182에서 FLAG핀 비액티브(GPIO=1) 감지 즉시 설정. QCC가 통화 중이라도 FLAG 핀이 비액티브가 되는 순간(I2S 세션 전환 중 순간적 LOW→HIGH) DMIC2 CLK이 재개될 위험. FLAG 핀과 QCC 내부 BLE 통화 채널 점유 상태의 정확한 연동 관계는 QCC 펌웨어 사양에 의존하며 CFX 코드에서 별도 가드 없음.
- **[minor]** audio_mix_2_buffers(I2S 경로)는 두 채널 합산 후 >>1 나눗셈을 하지 않는다 — beamforming 경로의 >>1과 레벨 불일치
  - 근거: audioMixer.c:51: p_mix[i] = (p_buf1[i]>>5) + (p_buf2[i]>>5) (나누기 없음). audioMixer.c:59~74: p_mix[i] = ((no_delay[i]>>5) + (delay[i+1]>>5)) >> 1 (나누기 2 있음). I2S 스트리밍 경로(main.c:423)는 audio_mix_2_buffers 호출 — beamforming 대비 출력 레벨이 약 2배. I2S↔DMIC 전환 시 AGC 입력 레벨 급변 발생.
- **[info]** LIB_ADC_DEC_CTRL_VAL_0_9958(INT=7/FRAC=29) 매크로가 정의(lib_audio_in.h:110)되어 있으나 어디에도 호출되지 않는 데드코드
  - 근거: grep 결과 LIB_ADC_DEC_CTRL_VAL_0_9958는 lib_audio_in.h:110 정의 1건만 존재. system_control.c에서 사용되는 것은 LIB_ADC_DEC_CTRL_VAL(delay 0, line 87/94/100)과 LIB_ADC_DEC_CTRL_VAL_0_0250(FRAC=6, line 88/93)뿐. 22mm 지연이 레지스터 최대값(0.9958샘플)을 초과하므로 이 매크로는 원래 설계 시도였다가 하이브리드(HW 0.025 + SW 1샘플) 방식으로 대체된 흔적.

## 개선 제안
- enable_2_DMICs() 호출 직후 DMIC2 웜업 완료 전 beamforming 진입 방지: s_enabled_dmic_cnt를 2가 아닌 중간 상태(예: WARMING_UP=3)로 설정하고 N프레임(예: 8프레임=8ms) 후 2로 확정하는 방어 로직 추가. PCM_LiveStimulation_Mode의 count==2 분기를 WARMING_UP 중에는 count==1과 동일하게 처리.
- QCC 통화 중 DMIC2 CLK 재개 방어: I2S_FLAG 핀 외에 QCC→CM3 BLE 통화 상태 플래그를 공유 메모리에 추가하고, enable_2_DMICs() 호출 조건에 통화 상태 비활성을 AND 조건으로 추가. 또는 I2S_FLAG 비액티브 후 일정 디바운스(예: 10ms = 10 루프) 유지 후에만 복구.
- I2S 스트리밍 경로 audio_mix_2_buffers의 레벨 정규화: audio_mix_2_buffers(audioMixer.c:51)에서 >> 1 추가하거나, AGC 입력 레퍼런스 레벨을 경로별로 분기하여 I2S↔DMIC 전환 시 레벨 급변 방지.
- LIB_ADC_DEC_CTRL_VAL_0_9958 매크로 제거 또는 주석 처리: lib_audio_in.h:110에서 데드코드 명시('#if 0' 또는 삭제) 처리하여 혼동 방지. 하이브리드 지연 설계 근거를 주석으로 인라인 문서화.
