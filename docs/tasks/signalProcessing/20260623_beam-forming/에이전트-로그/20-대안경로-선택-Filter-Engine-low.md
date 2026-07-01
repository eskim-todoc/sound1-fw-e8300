---
name: 토론 페르소나 20
purpose: 대안경로 선택: Filter Engine low-delay 경로 vs decimation  토론 수렴
type: tasks
maturity: experimental
tags: [beamforming, agent-log, discuss]
---

# [단계2·토론 20] 대안경로 선택: Filter Engine low-delay 경로 vs decimation DEC delay 경로 (E8300 DMIC 빔포밍)

## 찬

**Filter Engine low-delay/undecimated 경로 찬성 (이론적 장점)**

근거_1 — 스큐 원인 자체를 우회 가능 (조건부):
사실 B(HW p.445 §14.4)에서 확인된 1/8 샘플 채널 스큐는 "decimation WDF 시분할(time-multiplexed processing)" 구조에서 발생한다. Filter Engine low-delay 경로가 WDF 처리 이전 CIC 출력 단계를 탭하는 방식이라면, 스큐 발생 원인인 WDF 시분할 자체를 통과하지 않으므로 이론상 ch1/ch2 간 1/8 샘플 스큐가 존재하지 않게 된다. 이 경우 빔포밍 지연 예산에서 스큐 보정항 ±0.125 샘플을 제거할 수 있어 설계 단순화 효과가 있다 (참조: HW p.404 Table 41 경로 위치 — 단, WDF 전/후 여부 미확인).

근거_2 — 오버샘플 도메인에서 정수 샘플 정밀도 달성:
decimation 출력 16kHz에서 필요 지연 0.933 샘플은 비정수라 DELAY_FRACTIONAL 조정이 필수이고 양자화 오차 0.0243µs가 잔존한다. 반면 undecimated 경로(≈128kHz = 3.84MHz/30)를 사용하면 1샘플 = 7.8125µs이므로 end-fire 필요 지연 58.31µs를 7.47 샘플, 즉 정수 7 또는 8샘플로 근사 가능하다. 절대 시간 해상도 자체는 동등(0.26µs vs 7.8µs×1/30≈0.26µs)하나, 정수 샘플 연산으로 CFX DSP 구현 복잡도가 낮아질 수 있다.

근거_3 — WDF group delay 절대값 제거 가능성:
decimation 경로에서는 두 채널의 WDF group delay가 동일 BAND_SELECT 조건에서 이론적으로 상쇄되지만, 이 상쇄가 실측 또는 데이터시트에서 명시적으로 보장된 적이 없다 (미해결 포인트_7). undecimated 경로는 WDF 자체를 거치지 않으므로 이 불확실성을 제거한다.


## 반

**Filter Engine low-delay 경로 반대 / decimation DEC delay 경로 유지 (실용적 우위)**

반론_1 — CFX 파이프라인 전면 재설계 불가피 (치명적 실용성 문제):
현재 IOC→FIFO FA0_0/FA0_1→CFX DSP 전체 파이프라인이 16kHz 기준으로 설계되어 있다 (lib_audio_in.h:50, lib_audio_in.c:41-67). undecimated 경로를 사용하면 출력 샘플레이트가 약 128kHz로 올라가며, IOC FIFO 대역폭·CFX 처리 사이클 예산·block_size(현재 16샘플 기준)·인터럽트 발화 빈도 전부가 연동되어 변경된다. 이는 빔포밍 레지스터 2개 설정(DELAY_INTEGER/FRACTIONAL)과는 차원이 다른 재설계이다.

반론_2 — low-group-delay 경로는 채널 간 상대 지연 부여 수단이 아님:
low-group-delay 필터는 전달함수 계수를 교체하여 양 채널의 절대 group delay를 낮추는 것이다. 두 채널에 동일하게 적용하므로 채널 간 상대 지연은 생성되지 않는다. 빔포밍에 필요한 것은 채널 간 비대칭 지연(사실 F: 0.933 샘플)이며, 이는 AUDIO_ADC_DEC_CTRL의 DELAY_INTEGER/FRACTIONAL을 ch1/ch2에 독립 설정함으로써만 가능하다 (HW p.450-451, 사실 D). Filter Engine 경로 자체에는 채널별 지연을 비대칭적으로 설정하는 메커니즘이 없다.

반론_3 — decimation DEC delay 경로는 필요 지연 0.933 샘플을 HW에서 직접 완결:
사실 D에서 확인된 DELAY_INTEGER=7(0.875 샘플) + DELAY_FRACTIONAL=14(0.058 샘플)으로 총 0.9333 샘플을 달성하며 양자화 오차 0.0243µs에 불과하다 (타이밍 산술 분석가 검산 일치). DELAY_INTEGER 최대 7/8=0.875 < 목표 0.933이지만 FRACTIONAL과 조합하면 최대 0.9958 샘플까지 도달 가능하므로 HW 범위를 벗어나지 않는다. CFX DSP, FIFO, 인터럽트 경로 변경이 전혀 불필요하다.

반론_4 — 채널 간 1/8 샘플 스큐는 decimation 경로에서 이미 예산에 흡수 가능:
사실 A/B 스큐(ch2가 ch1보다 1/8=0.125 샘플 선행)는 착용 기하상 Left(DMIC2)가 앞 방향에 먼저 닿는 구조에서 ch1(Right)에 부여해야 할 순수 기하 지연 0.933 샘플에서 고유 스큐 0.125를 차감한 0.808 샘플만 DEC delay로 설정하면 된다. 이 값은 DELAY_INTEGER=6(0.75)+DELAY_FRACTIONAL=14(0.058)=0.808 샘플로 HW 최대(0.9958) 범위 내에서 처리 가능하다. 스큐가 설계를 복잡하게 만드는 것이 아니라 지연 예산에서 자연스럽게 상쇄된다.

반론_5 — Filter Engine 대안경로의 핵심 전제가 미확인:
찬성 측 근거_1의 "WDF 이전 CIC 출력 탭"이라는 가정이 HW p.404 Table 41에서 확인되지 않았다 (Filter Engine 대안경로 분석가의 openQuestion 명시). WDF 이전 탭이 아닌 경우 스큐 우회 효과도 없으며, 128kHz 처리 부담만 추가되는 최악의 조합이 된다. 불확실한 전제에 기반한 대안 경로를 선택하는 것은 설계 리스크다.

반론_6 — DMIC 경로에서 undecimated 탭 가능성 자체가 불명확:
사실 E(HW §18.2 p.563)는 DMIC가 ADC와 동일한 CIC pre-decimation 구조를 사용한다고 명시하나, Filter Engine 경로에서 DMIC 채널을 undecimated로 탭하는 것이 IOC 레지스터·FIFO 구성상 지원되는지 데이터시트에 명시가 없다.


## 수렴 결론

**수렴 결론: decimation DEC delay 경로가 유일한 실용 선택. Filter Engine 대안경로는 현 구성에서 적용 불가.**

핵심 비대칭은 "채널 간 상대 지연 부여 수단"에 있다. Filter Engine 경로(low-delay 또는 undecimated)는 양 채널에 동일하게 적용되는 전달함수 변형이므로, 빔포밍에 필수적인 채널 간 비대칭 지연을 생성하는 메커니즘이 아니다. 빔포밍 지연은 반드시 채널별 독립 제어가 필요하고, E8300에서 이를 지원하는 HW 수단은 AUDIO_ADC_DEC_CTRL의 DELAY_INTEGER+DELAY_FRACTIONAL 뿐이다 (HW p.450-451, 사실 D).

확정된 설계 수치:
- 착용 기하: Left(DMIC2/ch2)가 정면 음원에 먼저 노출 → ch1(Right/DMIC1)에 지연 부여
- 고유 스큐(ch2가 ch1보다 1/8=0.125 샘플 선행) 반영 후 ch1에 설정해야 할 순수 DEC 지연 = 0.933 - 0.125 = 0.808 샘플
- 레지스터값: ch1에 DELAY_INTEGER=6, DELAY_FRACTIONAL=14 → 0.8083 샘플(50.52µs), 오차 +0.0243µs
- ch2는 현재 설정(INTEGER=0, FRACTIONAL=0) 유지

잔여 불확실성 (2개):
1. HW p.404 Table 41의 low-delay 경로 탭 위치(WDF 전/후) 미확인 — 단, 위의 결론에 영향 없음. 해당 경로를 채택해도 채널 간 비대칭 지연 수단이 없으므로 DEC delay 경로와 병행해야 하며, 그 경우 DEC delay 경로가 지배적이다.
2. 착용 기하 최종 확인 필요 — "앞=Left" 전제가 이미지·복기 문서 분석에서 고신뢰로 확인되었으나(음향 기하 분석가: medium, 이미지 직접 참조 근거), 실기기 착용 방향의 물리적 재확인이 완료되면 착수 가능. 반대 기하(앞=Right)인 경우 ch2에 1.058 샘플이 필요하여 DELAY_INTEGER 최대(0.875) 초과 → SW 1샘플 오프셋이 추가로 필요한 대안 경로가 된다.


## 실행 인사이트


