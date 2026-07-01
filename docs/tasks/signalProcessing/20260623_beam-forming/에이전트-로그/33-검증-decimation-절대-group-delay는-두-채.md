---
name: 검증 페르소나 33
purpose: 적대적 검증 - decimation 절대 group delay는 두 채널 동일 대역이면 상대지연에
type: tasks
maturity: experimental
tags: [beamforming, agent-log, verify]
---

# [단계3·검증 33] decimation 절대 group delay는 두 채널 동일 대역이면 상대지연에서 상쇄된다

**판정**: `UNCERTAIN`

## 근거
주장은 부분적으로만 참이며, 본 구성에서 결정적 예외가 존재한다.

[참인 부분] 동일 BAND_SELECT_ADC를 사용하면 두 채널이 동일한 decimation 필터 전달함수를 통과하므로, 그 필터 계수에서 비롯되는 bulk group delay(위상 응답 유래)는 차분에서 소거된다. 이것은 LTI 선형 시스템의 표준 성질이다.

[반증: 시분할 구조 스큐가 잔류] 사실 B (HW p.445): decimation 필터는 4채널을 "two pairs (ch0,1; ch2,3)"으로 시분할 처리하며, "resulting in longer processing delays for channels 1,3"이라고 명시한다. 현재 매핑 ch1=DMIC1(Right), ch2=DMIC2(Left)는 서로 다른 쌍에 속한다. 사실 A (HW p.222)는 이를 정량화하여 "ADC0·ADC2 data arrives 1/8th of a sample period ahead of ADC1·ADC3"라고 확인한다. 이 1/8 샘플 스큐(Δ)는 필터 계수가 아니라 decimation 하드웨어 아키텍처에서 오므로, 대역 선택이 동일해도 없어지지 않는다.

따라서 전체 절대 지연 = (필터 bulk delay) + Δ(시분할 스큐)이며, ch1에만 Δ가 추가되어 있으므로 상대 지연에서 Δ는 상쇄되지 않고 잔류한다. 주장이 "절대 group delay 전부"를 지칭한다면 refuted에 해당하나, "필터 bulk delay만"으로 한정한다면 confirmed다. 이 이중 해석 가능성 때문에 uncertain 판정이 적절하다.

실제 설계에서의 함의: 0.933 샘플 필요 상대 지연 예산을 DELAY_INTEGER/FRACTIONAL로 설정할 때, ch1-ch2 간 1/8 샘플 구조 스큐를 반드시 예산에 포함해야 한다(사실 포인트_5). 이를 무시하고 "상쇄된다"고 처리하면 실제 지연이 1/8 샘플 어긋난다.

## 데이터시트 근거
사실 A (HW p.222): "ADC0·ADC2 data arrives 1/8th of a sample period ahead of ADC1·ADC3" — 짝수/홀수 채널 간 절대 지연이 동일 대역에서도 1/8 샘플만큼 다름을 명시. 사실 B (HW p.445 §14.4): "time multiplexed into two pairs (ch0,1; ch2,3) resulting in longer processing delays for channels 1,3" — 이 스큐의 원인이 decimation 시분할 아키텍처임을 명시하며 필터 대역 선택과 무관함을 함의.

