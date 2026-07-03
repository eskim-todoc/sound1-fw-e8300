---
name: nofm-output-pattern-배경조사-이전task이력
description: docs/tasks/signalProcessing/20260514_nofm-merge·nofm-tuning 이력 조사 결과 — 설계 근거·잔존 이슈
metadata:
  type: raw-working-doc
---

## TL;DR

NofM `frameNumPerOneChannel=3` 고정값의 설계 근거(16채널→8+8 Phase 분할, 8×3=24 슬롯 정합)를 확인했다. 또한 nofm-tuning에서 "임시, 후속 삭제 필요"로 명시했던 테스트벡터 강제 코드 2건이 현재도 그대로 남아있어, 실제 자극 진폭이 아닌 고정값일 수 있음에 유의(단, 이번 시각화는 진폭이 아닌 **타이밍/프레임 수**만 다루므로 직접 영향은 없음).

## 각 task 요약

- **nofm-merge (2026-05-18)**: 팀원(다니엘)이 CFX 측에 작성한 NofM 신호처리 함수를 병합 완료 상태에서 변경분 문서화만 수행. 코드 변경 없음.
- **nofm-tuning (2026-05-18)**: 실동작 검증 중 적용된 5개 변경 정리·커밋. ① 백텔(backtel) 강제, ② stride 정책 명시, ③④ 임시 테스트벡터 2건(후속 삭제 필요, **현재도 잔존**), ⑤ NofM 프레임/채널수 강제(`frameNumPerOneChannel=3`, `transferableChannelNum=8`).

## NofM 3프레임/8채널 고정의 설계 근거

Peak_Pick이 16채널을 뽑아도 Interleaving이 `half_ch=8`로 Phase0/Phase1 2분할하기 때문에, 1ms(24 슬롯) 안에 8채널을 모두 소화하려면 `frameNumPerOneChannel=3`이어야 정확히 맞음(8×3=24). 16 미만 밴드 가드도 "8채널 동시자극"이라는 설계 의도를 지키기 위함. → **이번 웹페이지에서 "왜 하필 3프레임·8채널·500pps인가"를 설명할 때 이 근거를 그대로 인용 가능**.

## 백텔 강제와 전력 관련 언급

`driver_PCM_liveStimulation.c`의 백텔 강제 로직 코멘트: "펄스 폭 설정에 따라 FPGA COLA 프로토콜 전송 시간이 1 프레임으로 끝날 가능성"이 있어 race 회피 목적으로 2프레임을 추가 강제 사용. 은수님 판단: "2 프레임 정도는 큰 의미 없음"(전력 영향 무시 가능 수준). → 이번 시각화의 핵심 타이밍 모델(§00 근거문서)에는 포함하지 않음(별도 안전장치이지 CIS/NofM 기본 패턴 자체는 아님).

## 잔존 이슈 (참고, 이번 작업 범위 밖)

- `stimulationStrategy.c`의 Phase0 진입 시 `g_pcm_amplitude_level[0..15]` 결정론적 테스트벡터 강제
- `ci_map.c`의 Map3 강제 NofM 설정
- 두 건 모두 "임시, 후속 삭제 필요"로 문서화됐으나 현재 코드에 그대로 존재. **진폭(자극 크기) 값에만 영향**, 이번 작업이 다루는 프레임 수·PCM/COLA 타이밍 로직에는 영향 없음 — 그대로 진행.

## 이번 작업과 유사한 이전 산출물

- nofm-merge 분석.md에 Mermaid 다이어그램(전체 파이프라인, Phase0/1 사이클) + CIS/NofM 비교표 존재하나, **duration별 수치 타이밍을 시각화한 산출물은 이전에 없었음** — 이번이 최초.
