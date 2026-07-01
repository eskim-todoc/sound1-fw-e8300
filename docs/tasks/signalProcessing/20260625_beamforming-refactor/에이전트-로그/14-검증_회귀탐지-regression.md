---
name: 리팩토링 검증 14
purpose: 회귀탐지 regression
type: tasks
maturity: experimental
tags: [beamforming, refactor, agent-log, verify-stage4]
---

# [검증 14] 회귀탐지 — `REGRESSION`

## 근거
파일 6개를 전부 Read + git diff HEAD로 실제 변경 내용을 검증했다. 대부분 변경은 내부 정합성이 유지되지만, 한 가지 실질적 회귀와 두 가지 잠재적 빌드 위험을 발견했다.

--- 발견된 회귀 / 문제점 ---

[REGRESSION-1] LIB_AUDIO_IN_DMIC_ENABLE_COUNT=1 폴백 경로의 오디오 MUX 불일치 (lib_audio_in.h/c)
리팩토링이 활성 경로(COUNT=2)의 오디오 MUX를 변경했다: 구버전 DMIC1_DATA_RE at ADC channel 1 → 신버전 DMIC0_DATA_RE at ADC channel 0. 그러나 #elif (COUNT==1) 브랜치 내부의 세 라인은 수정되지 않았다:
  • SYS_SET_ADC_DEC_CTRL(AUDIO, 1, ...) → channel 1에 DEC 필터 설정
  • DIO->SRC_DMIC_DATA = DMIC1_DATA_SRC_DIO_23 → DMIC1 data line 사용
  • 반면 새 MUX에서 channel 1 위치는 ADC1_OUT (순수 ADC), DMIC0_DATA_RE는 channel 0에 있다
COUNT=1로 전환 시 EZ 마이크 신호가 decimation filter에 전달되지 않는다. 현재 COUNT=2라 실제 빌드에서는 비활성이지만, "// 1" 주석 잔재가 있어 스위치백 위험이 실재한다.

[REGRESSION-2] 인터럽트 게이트 조건에 dac1 추가 — 잠재적 초기화 교착 (main.c)
구버전: (pcm_out==1) && (mic0==1)  ← mic1, dac1 주석 처리됨
신버전: (pcm_out==1) && (mic0==1) && (mic1==1) && (dac1==1)
dac1은 FA0_3 (DAC1 FIFO) ISR에서 설정된다. DAC1 FIFO에 auto-mute가 설정되어 있으므로 정상 동작 중에는 계속 interrupt를 발생시킬 것으로 추정되지만, 시스템 초기화 직후 첫 번째 DAC1 FIFO 인터럽트가 발생하기 전까지 처리 루프가 실행되지 않는 타이밍 창(startup window)이 존재한다. 이 조건은 기존 코드에 없던 것이며 하드웨어 문서로 확인 필요.

--- 이상 없음이 확인된 항목 ---
• LIB_DMIC_IDX_DMIC1=0↔DMIC2=1 인덱스: tdc_copy_DMIC_buffers에서 HCT_A0_0→[0], HCT_A0_1→[1]로 일관되게 매핑 ✓
• >> AUDIO_MIX_NORMALIZE_RSHIFT 치환: 16줄 전체 AUDIO_MIX_NORMALIZE_RSHIFT=1로 치환됨, >> 1 누락·과잉 없음 ✓
• 배열 차원 [LIB_AUDIO_IN_DMIC_ENABLE_COUNT][LIB_AUDIO_IN_BUF_MAX_CNT][16]: 정의·선언·사용 모두 일치 ✓
• 빔포밍 L/R 조향: Left→DMIC2 지연·DMIC1 기준, Right→DMIC1 지연·DMIC2 기준; system_control.c HW 지연 설정과 main.c 믹서 인수 모두 정합 ✓
• 블록 경계 샘플: p_mix[15]에서 delay_buf1[0]= 직전 프레임 최신 샘플 사용, 링버퍼 구조와 수학적으로 정확 ✓
• 주석 사실 값: FRAC=6/240=0.025샘플, 1.025샘플≈64.06µs≈22mm 모두 수식 검증 ✓
• AUDIO_INPUT_RSHIFT=5 출처: definitionsForAlgorithm.h line 103에서 확인 ✓
• g_enabled_mic_count, LIB_ADC_DEC_CTRL_VAL_0_9958, FRACTIONAL_DELAY_29: HEAD와 현재 작업 트리 모두에서 미존재 — 이 브랜치 이전에 이미 제거된 심볼로, 이번 리팩토링의 데드코드 제거 대상이 아니었음 (무해)
• tdc_copy_DMIC_buffers() 호출 순서: normal_loop에서 PCM_LiveStimulation_Mode() 호출 전에 실행되므로 g_lib_dmic_in_buffers[0][0]은 항상 최신 FIFO 데이터 ✓

## issues
- [REGRESSION-1] LIB_AUDIO_IN_DMIC_ENABLE_COUNT=1 폴백 경로 MUX 불일치 (/mnt/e/Claude/projects/Sound1/src/1__cfx/lib_cfx/lib_audio_in.c): 신규 MUX(DMIC0_DATA_RE at ch0)와 COUNT=1 브랜치(channel 1 dec filter + DMIC1_DATA_SRC_DIO_23)가 불일치. COUNT를 1로 되돌리면 EZ 마이크 신호가 소실됨. 현재 COUNT=2이므로 빌드는 통과하나, 헤더 주석 '// 1' 로 인한 스위치백 위험 실재.
- [REGRESSION-2] 인터럽트 처리 게이트 조건에 dac1 추가 (/mnt/e/Claude/projects/Sound1/src/1__cfx/systemControl/main.c line 194): 구버전은 pcm_out+mic0만 요구했으나 신버전은 pcm_out+mic0+mic1+dac1을 모두 요구. DAC1 FIFO(FA0_3) 인터럽트가 시스템 초기화 직후 아직 발생하지 않은 상태라면 처리 루프가 교착될 수 있음. dac1 조건 추가의 필요성은 의도적일 수 있으나 E8300 FIFO 동작 하드웨어 문서로 확인 필요.
