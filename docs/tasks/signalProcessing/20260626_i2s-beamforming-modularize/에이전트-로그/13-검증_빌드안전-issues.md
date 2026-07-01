---
name: 모듈화 ④검증 13
purpose: 빌드안전 issues
type: tasks
maturity: experimental
tags: [modularize, agent-log, verify-stage4]
---

# [④검증 13] 빌드안전 — `issues`

## 근거
Build safety 관점에서 이번 변경(tdc_ 리네임 9심볼·dead code 배너·module/unit 배너)이 도입한 신규 빌드 브레이커는 없음. 옛 이름 잔존 0, tdc_ 정의 각 1개·선언 시그니처 일치, audio_mix_internal/external_mic_only 참조 0, [DEAD] 함수(I2S_handle/I2S_update_state) 실 호출 0, 변경 파일 내 괄호·주석·매크로 종료 모두 정상. 단, 맵↔코드 배너 정합 기준에서 맵 문서 U1의 최종 함수명이 코드와 다름: 맵은 tdc_audio_mix_beamforming, 코드는 tdc_audio_mix_2_buffers_for_beamforming. 이 불일치는 빌드를 깨지 않으나 검증 기준 중 '맵↔코드 배너 정합' 항목을 위반하므로 issues로 분류. 또한 기존 코드(변경 전부터 존재)에서 lib_i2s.c:82에 #else가 아닌 C 키워드 else가 #elif==5 블록 안에 있어 I2S_BUFFER_UNDERRUN_CNT==5 시 파일스코프 구문 오류 위험이 있으나, git diff 확인으로 이번 변경에서 도입된 것이 아님을 확인.

## issues
- [맵↔코드 불일치] 유닛-모듈-테스트맵.md U1 행 '함수(현행→tdc_)' 열에 최종 이름을 tdc_audio_mix_beamforming으로 기재하나, 코드 실제 구현은 tdc_audio_mix_2_buffers_for_beamforming(audioMixer.h:29 선언, audioMixer.c:99 정의, main.c:469·474 호출). 코드 내부 일관성에는 문제없어 빌드 영향 없음. 맵 문서 U1 행 함수명 수정 필요.
- [pre-existing, 이번 변경 미도입] lib_i2s.c:82에 #else가 아닌 C 키워드 else가 #elif (I2S_BUFFER_UNDERRUN_CNT == 5) 블록 내에 위치. I2S_BUFFER_UNDERRUN_CNT==5일 때 파일 스코프 else 구문 오류 + 해당 블록 내 #error 발동으로 빌드 실패 위험. git diff로 이번 배너 추가와 무관한 기존 코드임을 확인. 후속 별도 수정 필요(#else로 교체).
