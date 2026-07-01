---
name: 모듈화 ④검증 15
purpose: 맵정합 issues
type: tasks
maturity: experimental
tags: [modularize, agent-log, verify-stage4]
---

# [④검증 15] 맵정합 — `issues`

## 근거
전체 파일 Read 후 맵↔코드 배너 교차 검증 결과, 3개 불일치 발견.

[동작 보존·빌드 안전 — 이상 없음]
- tdc_ 9심볼 모두 정의(lib_audio_in.c/h, audioMixer.c/h)·선언 정합, call-site(main.c, system_control.c) 전수 갱신 확인.
- 구 심볼(audio_mix_1_buffer, enable_1_DMIC 등 접두어 없는 버전) 잔존 없음.
- dead code: audio_mix_internal_mic_only/audio_mix_external_mic_only 완전 제거(def+decl 모두 없음). I2S_update_state(lib_i2s.c:192)·I2S_handle(main.c:359)은 [DEAD] 배너 부착, 호출부 주석 처리(main.c:431, 435) 확인.

[불일치 이슈]
1. 맵 §1 U1 행 tdc_ 목표 함수명 오류: 유닛-모듈-테스트맵.md §1 U1 행의 tdc_ 목표 이름이 `tdc_audio_mix_beamforming`으로 기재되어 있으나, 실제 코드(audioMixer.c:99, audioMixer.h:29) 및 M1 MODULE 배너(audioMixer.c:9)는 모두 `tdc_audio_mix_2_buffers_for_beamforming`. `tdc_audio_mix_beamforming`은 코드 어디에도 존재하지 않는 심볼 — 맵만 수정 필요.
2. lib_i2s.c U7·U8 [UNIT] 배너 누락: `I2S_get_buffer_with_fade_in_process`(U7, lib_i2s.c:333)와 `I2S_get_buffer_with_fade_out_process`(U8, lib_i2s.c:380)에 [UNIT] U7/U8 배너가 없음. [MODULE] M5 배너(lib_i2s.c:174)만 있고 두 함수 직전에 개별 [UNIT] 주석 없음 — 맵 §1 U7/U8 행의 코드↔맵 추적성 단절.
3. U11 배너 의존성 명세 불일치: 맵 §1 유닛 테이블 U11 의존=`—`이나, lib_audio_in.c:107-108의 M3+U11 복합 배너는 `전제(의존)=M2 마이크 라우팅`으로 기술. M3 모듈의 의존이 유닛 배너에 혼입되어 맵 §1 유닛 테이블과 불일치(맵 §2 모듈 테이블 M3 행의 M2 의존은 올바름).

## issues
- [맵 U1 함수명 오류] 유닛-모듈-테스트맵.md §1 U1행 tdc_ 목표명이 `tdc_audio_mix_beamforming`이나 실제 코드는 `tdc_audio_mix_2_buffers_for_beamforming` (audioMixer.c:99, audioMixer.h:29, M1 MODULE 배너 audioMixer.c:9 모두 일치). `tdc_audio_mix_beamforming`은 코드 어디에도 없는 존재하지 않는 심볼. 맵 문서 §1 U1 행 수정 필요.
- [U7·U8 [UNIT] 배너 누락] lib_i2s.c의 I2S_get_buffer_with_fade_in_process(line 333)와 I2S_get_buffer_with_fade_out_process(line 380) 함수 직전에 [UNIT] U7·[UNIT] U8 배너가 없음. [MODULE] M5 배너(line 174)만 존재. 맵 §1 U7·U8 행(위치=lib_i2s.c)과 코드 배너 1:1 대응 실패 — 두 함수 위에 [UNIT] U7·U8 배너 추가 필요.
- [U11 배너 의존성 불일치] 맵 §1 유닛 테이블 U11 의존=`—`이나, lib_audio_in.c:107-108 M3+U11 복합 배너는 `전제(의존)=M2 마이크 라우팅`으로 기술되어 U11에 M2 의존이 있는 것처럼 읽힘. M3 모듈의 M2 의존이 유닛 배너에 혼입. 맵 §2 M3 행(의존=M2)은 올바르나 맵 §1 U11 행과 코드 배너 간 불일치. 배너를 `전제(의존)=없음 ※M3 모듈 전제=M2`로 분리하거나 맵 U11 행에 M3 레벨 의존 명시 필요.
