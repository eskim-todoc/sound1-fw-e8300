---
name: 모듈화 ④검증 14
purpose: 동작보존 issues
type: tasks
maturity: experimental
tags: [modularize, agent-log, verify-stage4]
---

# [④검증 14] 동작보존 — `issues`

## 근거
동작 보존(제어흐름·연산·인덱스·호출 인수)은 완전히 확인됨. 빔포밍 16행(p_mix[0..14]=no_delay[i]+delay[i+1], p_mix[15]=no_delay[15]+delay_buf1[0]) 정확. tdc_copy_DMIC_buffers 시프트(BUF_MAX_CNT=2, j-loop 1회: [1][i]=[0][i] → [0][i]=FIFO) 정확. L/R 분기(LEFT→DMIC2[0]/DMIC2[1]/DMIC1[0], RIGHT→DMIC1[0]/DMIC1[1]/DMIC2[0]) 정확. 9개 tdc_ 심볼 def+decl+call-site 전수 갱신 확인, 구 이름 잔존 0건. dead 제거 2종(audio_mix_internal/external_mic_only) 0건 확인. I2S_update_state·I2S_handle 물리 잔존·라이브 호출 0건 확인. 단, 맵↔코드 배너 정합 기준에서 2건 발견: (1) 유닛-모듈-테스트맵.md U1 행이 목표 이름을 tdc_audio_mix_beamforming으로 기록하나 코드는 tdc_audio_mix_2_buffers_for_beamforming 사용(맵 문서 미갱신), (2) lib_i2s.h 100행 I2S_update_state 선언에 [DEAD] 마커 없음(lib_i2s.c 정의부에는 있으나 헤더 미갱신).

## issues
- [맵↔코드 U1 이름 불일치] /mnt/e/Claude/projects/Sound1/docs/tasks/signalProcessing/20260626_i2s-beamforming-modularize/유닛-모듈-테스트맵.md 22행: U1 목표 이름이 `tdc_audio_mix_beamforming`으로 기록되어 있으나, 실제 코드(audioMixer.c:99, audioMixer.h:29, main.c:469/474)는 `tdc_audio_mix_2_buffers_for_beamforming`을 사용함. 맵 문서가 구 계획명을 그대로 반영 — 맵↔코드 배너 정합 기준 불충족.
- [lib_i2s.h DEAD 마커 누락] /mnt/e/Claude/projects/Sound1/src/1__cfx/lib_cfx/lib_i2s.h 100행: `void I2S_update_state(void);` 선언에 [DEAD] 마커 없음. lib_i2s.c 190행 정의부에는 `[DEAD]` 주석이 존재하나 헤더 선언은 미갱신. 빌드 안전상 무해하나 정합 불완전.
