---
name: 모듈화 검증 12
purpose: 방법론정합 issues
type: tasks
maturity: experimental
tags: [modularize, agent-log, verify]
---

# [검증 12] 방법론정합 — `issues`

## 근거
실파일 6종(audioMixer.c/h, lib_audio_in.c/h, main.c, system_control.c, lib_i2s.c/h) 전수 Read 후, 10개 에이전트 분해를 4개 기준 대조.

【기준 1 — 유닛이 유닛 써도 자동 모듈 승격 안 했는지】
전 에이전트에서 pcm_live_stimulation_dispatch, beamforming_dispatch, i2s_dmic_switcher 등 다수 유닛을 사용하는 유닛이 모듈로 승격된 사례 없음. 이 기준은 전원 준수.

【기준 2 — 통합테스트 = 모듈내 전유닛 동시활성화】
검증방법 라벨링가가 [integration-test]를 개별 유닛 온타깃 검증(DMIC_링버퍼_복사, I2S_FIFO_복사, I2S_페이드_처리, HW_FRAC_지연_설정, 빔포밍_디스패치, I2S_믹서_핸들러)에 부여함. 방법론 정의("모듈 내 모든 유닛을 함께 활성화해도 각 유닛이 원래대로 동작하는지 확인")는 모듈 수준 동시검증이며, 개별 유닛 온타깃 테스트는 [unit-test] 또는 [logic-explanation]에 해당. 정의 오용 위반.

【기준 3 — 단순·특화(과도추상화·군더더기 없음)】
(A) 단순·특화 균형 검토가: U_DAS_커널, U_1버퍼믹서, U_2버퍼독립믹서를 순수(pure)로 표기. 실코드 확인: audio_mix_2_buffers_for_beamforming()(audioMixer.c:109-129), audio_mix_1_buffer()(audioMixer.c:32-41), audio_mix_2_buffers()(audioMixer.c:43-54) 모두 함수 첫 줄에서 `p_mix = (int _XMEM *) HEAR_ADDR_AUDIO_MIX` 로 HW 주소를 직접 기록. 출력이 HW 메모리 사이드이펙트이므로 순수 조건(HW/전역 비의존) 불충족. 오라벨 위반.
(B) HW결합·CFX제약 분석가: das_beamform_kernel(순수)와 audio_mix_to_hear_mem을 별개 유닛으로 분리하면서, audio_mix_to_hear_mem이 audioMixer.c 전함수를 포괄한다고 명시. 그런데 das_beamform_kernel이 audio_mix_2_buffers_for_beamforming()(audioMixer.c:109)의 산술 커널이므로 동일 함수가 두 유닛에 중복 귀속되는 모순. 비순수 함수에서 순수 커널 추출은 유효하나, 그 경우 audio_mix_to_hear_mem은 해당 함수를 포함하지 않아야 함. 모순 분해 위반.

【기준 4 — 의존성을 테스트순서로 표현】
책임·데이터흐름 매핑가, 유닛 식별가(순수로직), 모듈 식별가, 검증방법 라벨링가, 단순·특화 균형 검토가, 목표 파일/심볼 구조 설계가, 동작보존 위험 감별가 — 7개 에이전트 모두 유닛/모듈 목록만 제시하고 "A는 B 통과 전제" 형식의 명시적 테스트 순서를 기술하지 않음. 방법론은 "의존성은 테스트 순서로 표현"을 명시 요구. 의존성 그래프 분석가만 "오디오 입력 설정 → DMIC 버퍼 수집 → 믹서/빔포밍 → 모드전환 디스패치 4단 직렬 + 빔포밍 믹서 3선행조건"으로 명시 준수. 나머지 7개 에이전트는 위반.

## flagged
- [기준2] 검증방법 라벨링가 — [integration-test]를 개별 유닛 온타깃 검증(DMIC_링버퍼_복사 등 6종)에 부여. 정의는 모듈내 전유닛 동시활성화 확인이며 개별 유닛 온타깃은 [unit-test]/[logic-explanation] 대상.
- [기준3-A] 단순·특화 균형 검토가 — U_DAS_커널·U_1버퍼믹서·U_2버퍼독립믹서를 순수(pure) 표기. 실코드(audioMixer.c:32,43,109)에서 세 함수 모두 첫 줄에 HEAR_ADDR_AUDIO_MIX(HW 주소) 직접 기록. 순수 조건(HW/전역 비의존) 불충족.
- [기준3-B] HW결합·CFX제약 분석가 — das_beamform_kernel(순수)와 audio_mix_to_hear_mem 양쪽에 audio_mix_2_buffers_for_beamforming(audioMixer.c:109) 동일 함수 중복 귀속. 산술 커널 추출과 전함수 포괄은 공존 불가 — 모순 분해.
- [기준4] 7개 에이전트(책임·데이터흐름 매핑가, 유닛 식별가, 모듈 식별가, 검증방법 라벨링가, 단순·특화 균형 검토가, 목표 파일/심볼 구조 설계가, 동작보존 위험 감별가) — 유닛/모듈 목록만 제시, 'A는 B 통과 전제' 형식 테스트 순서 부재. 의존성 그래프 분석가만 명시 준수.
