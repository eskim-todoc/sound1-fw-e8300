---
name: 모듈화 검증 11
purpose: 할루시네이션 issues
type: tasks
maturity: experimental
tags: [modularize, agent-log, verify]
---

# [검증 11] 할루시네이션 — `issues`

## 근거
모든 구체적 file:line 인용(main.c:326, main.c:355, system_control.c L83-105)과 핵심 데이터 구조(g_lib_dmic_in_buffers[2][2][16], lib_g_i2s_buffers[8][16]), 매크로/상수(LIB_ADC_DEC_CTRL_VAL_0_0250, AUDIO_MIX_NORMALIZE_RSHIFT, LIB_AUDIO_IN_BUF_MAX_CNT=2), 실존 함수(audio_mix_1_buffer, audio_mix_2_buffers, audio_mix_2_buffers_for_beamforming, tdc_copy_DMIC_buffers, configure_audio_path_all, lib_i2s_copy_data_from_fifo, I2S_isStreaming 등)는 모두 실파일에서 확인됨. 단, 두 가지 할루시네이션 발견: (1) 책임·데이터흐름 매핑가에서 audio_mix_fixed_fifo를 실재 함수(audio_mix_1_buffer 등)와 동등 수준으로 나열했으나 audioMixer.c에 해당 이름의 함수 미존재(실재: audio_mix_internal_mic_only, audio_mix_external_mic_only). (2) 모듈 식별가가 '존재 심볼/라인만 인용'을 명시했음에도 dmic_count_accessor, beamforming_das_mixer, single_channel_mixer, dual_channel_mixer, lr_front_mic_hw_delay, i2s_fifo_ring_copy, i2s_fade_in_buffer, i2s_fade_out_buffer, i2s_hw_enable_disable, normal_loop_i2s_dmic_branch, pcm_live_stimulation_dispatch, i2s_handle_dispatch 등 실파일에 없는 함수명을 유닛 목록에 포함.

## flagged
- audio_mix_fixed_fifo (책임·데이터흐름 매핑가): audioMixer.c에 없음. 실재: audio_mix_internal_mic_only, audio_mix_external_mic_only
- dmic_count_accessor (모듈 식별가): 없음. 실재: get_enabled_DMIC_count
- dmic_enable_select (모듈 식별가): 없음. 실재: enable_1_DMIC, enable_2_DMICs
- dmic_clock_onoff (모듈 식별가): 없음. 실재: enable_1_DMIC 내 Sys_DIO_Config 직접 호출
- beamforming_das_mixer (모듈 식별가): 없음. 실재: audio_mix_2_buffers_for_beamforming
- single_channel_mixer (모듈 식별가): 없음. 실재: audio_mix_1_buffer
- dual_channel_mixer (모듈 식별가): 없음. 실재: audio_mix_2_buffers
- lr_front_mic_hw_delay (모듈 식별가): 없음. Normal_PowerMode_event_mapChange 내 인라인 블록
- i2s_fifo_ring_copy (모듈 식별가): 없음. 실재: lib_i2s_copy_data_from_fifo
- i2s_fade_in_buffer (모듈 식별가): 없음. 실재: I2S_get_buffer_with_fade_in_process
- i2s_fade_out_buffer (모듈 식별가): 없음. 실재: I2S_get_buffer_with_fade_out_process
- i2s_hw_enable_disable (모듈 식별가): 없음. 실재: lib_enable_I2S, lib_i2s_disable
- normal_loop_i2s_dmic_branch (모듈 식별가): 없음. normal_loop 내 인라인 분기
- pcm_live_stimulation_dispatch (모듈 식별가): 없음. 실재: PCM_LiveStimulation_Mode
- i2s_handle_dispatch (모듈 식별가): 없음. I2S_handle(main.c:355)은 존재하나 호출부 주석처리된 dead code이며 별도 dispatch 함수 미존재
