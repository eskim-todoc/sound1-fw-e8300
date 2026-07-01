---
name: 분석 페르소나 10
purpose: Filter Engine 대안경로 분석가 — E8300 빔포밍 지연 부여 경로 비교 분석 결과
type: tasks
maturity: experimental
tags: [beamforming, agent-log, analysis]
---

# [단계1·분석 10] Filter Engine 대안경로 분석가 — E8300 빔포밍 지연 부여 경로 비교

**신뢰도**: medium

## 결론
HW p.404 Table 41의 low-group-delay/undecimated Filter Engine 경로는 빔포밍 지연 부여의 이론적 대안이 될 수 있으나, CFX DSP 입력 호환성·클럭 도메인 전환·다운스트림 처리 복잡도 면에서 decimation 경로(DELAY_INTEGER + DELAY_FRACTIONAL) 대비 실용성이 현저히 낮다. 현재 구성(16kHz, SFCR≈29)에서는 decimation 경로만으로도 필요 지연 0.933샘플을 HW 레벨에서 직접 해결 가능하므로 Filter Engine 대안경로는 추가 복잡도 없이 얻을 이점이 없다.

## 발견(근거)
- **Filter Engine undecimated/low-group-delay 경로는 오버샘플링 도메인(128kHz급) 데이터를 노출하므로 빔포밍 지연 해상도가 이론상 대폭 향상된다.**
  - 근거: 사실 D(HW p.450-451): ADCCLK 3.84MHz, SFCR+1=30 → decimation 후 16kHz. pre-decimation 출력이라면 유효 샘플레이트 ≈ 3.84MHz/30 = 128kHz. 128kHz 기준 1샘플 = 7.8µs → end-fire 58.3µs(사실 F)는 7.5샘플로 정수 근사 가능. decimation 경로의 유효 해상도 0.26µs(≈1/240 샘플@16kHz, 사실 D) 대비 절대 시간 해상도 자체는 유사하나 정수 샘플 처리 여지가 생김. (HW p.450-451(사실 D), HW p.404 Table 41)
- **undecimated 경로를 사용해도 채널 간 1/8샘플 스큐의 발생 원인(decimation WDF 시분할)은 우회되지 않는다 — 단, 스큐가 아직 발생하기 전 단계라면 스큐 자체가 존재하지 않을 수도 있다.**
  - 근거: 사실 B(HW p.445 §14.4): 1/8샘플 스큐는 decimation 필터 내 시분할(time-multiplexed WDF) 처리에서 발생. undecimated 경로가 WDF 처리 이전 CIC 출력을 탭한다면 스큐 미발생. 반대로 low-group-delay 경로가 WDF 처리 후 단순 group-delay 저감이면 스큐는 동일하게 존재. p.404 Table 41의 경로 구체 위치(WDF 전/후)가 불확실 → openQuestions 이관. (HW p.445 §14.4(사실 B), HW p.222(사실 A), HW p.404 Table 41)
- **undecimated 경로의 출력은 CFX DSP가 기대하는 16kHz FIFO 포맷과 호환되지 않아 다운스트림 전체 재설계가 필요하다.**
  - 근거: 현재 구성(펌웨어 그라운딩): IOC→FIFO FA0_0/FA0_1 → CFX DSP 전체 파이프라인이 16kHz 기준 설계. undecimated 출력(128kHz)을 CFX에 넣으려면 FIFO 대역폭·IOC 설정·CFX 처리 사이클 예산 전면 재조정 필요. 사실 E(HW §18.2 p.563): DMIC pre-decimation = 6th order Sync(CIC), decimation factor는 ADC와 동일 — 즉 DMIC 계통도 동일 decimation 구조를 따르므로 우회 시 동일 문제 발생. (HW §18.2 p.563(사실 E), 펌웨어 구성 그라운딩)
- **low-group-delay 경로는 전달함수 변경(필터 계수 교체)으로 group delay를 줄이는 방식이며, 채널 간 상대 지연 부여 수단으로는 부적합하다.**
  - 근거: low-group-delay 필터는 양 채널에 동일하게 적용되는 주파수 응답 변형 — 채널 0·2 vs 1·3의 1/8샘플 스큐(사실 A·B)를 비대칭적으로 제거하거나 추가하는 기능이 아님. 빔포밍에 필요한 것은 두 채널 간 상대 지연(사실 F: 0.933샘플)이며, 이는 채널별 독립 지연 부여가 필요 → 사실 D의 DELAY_INTEGER/DELAY_FRACTIONAL이 직접 대응. (HW p.404 Table 41, HW p.450-451(사실 D), HW p.222(사실 A))
- **decimation 경로(DELAY_INTEGER + DELAY_FRACTIONAL)는 필요 지연 0.933샘플을 채널별 독립 HW 레지스터로 직접 구현 가능하며, CFX 파이프라인 변경이 불필요하다.**
  - 근거: 사실 D(HW p.450-451): DELAY_INTEGER_7 = 7/8 = 0.875샘플. 사실 F: 목표 0.933샘플 → 잔여 0.058샘플 = 3.6µs → DELAY_FRACTIONAL로 보정. SFCR=29 기준 1 fractional step = 1/(8×30) ≈ 0.26µs → 약 14 step(3.6µs/0.26µs) 추가로 0.933 달성. 사실 G: DELAY_INTEGER 최대 7/8 < 1.0샘플이지만 목표가 0.933이라 범위 내. CFX 파이프라인(FIFO, DSP) 변경 불요. (HW p.450-451(사실 D), 사실 F·G)
- **채널 간 고유 1/8샘플 스큐(ch2 빠름, ch1 늦음)는 decimation 경로에서 빔포밍 지연 예산에 포함 계산해야 한다 — Filter Engine 대안경로 선택과 무관하게 존재하는 고정 오프셋이다.**
  - 근거: 사실 A(HW p.222)·사실 B(HW p.445): DMIC2(ch2, Left)가 DMIC1(ch1, Right)보다 1/8샘플 먼저 출력. 이는 decimation WDF 구조에서 기인하는 고정값. DELAY_INTEGER/DELAY_FRACTIONAL로 이를 보정(상쇄)하거나 활용(지연 예산에 흡수)하는 계산이 필요. undecimated 경로에서 이 스큐가 회피되는지는 p.404 경로 위치에 달려 있어 불확실. (HW p.222(사실 A), HW p.445(사실 B))

## 미해결 질문
- HW p.404 Table 41의 low-group-delay/undecimated 경로가 decimation WDF 처리 이전을 탭하는지(→ 1/8샘플 스큐 미발생) vs 이후 group-delay만 저감하는지 — 데이터시트 p.404 직접 인용 없이 판단 불가. 해당 페이지의 경로 블록다이어그램·탭 위치 확인 필요.
- DMIC 계통에서 undecimated 경로를 탭했을 때 IOC/FIFO가 해당 샘플레이트(128kHz급)를 수용 가능한지 — IOC 최대 클럭·FIFO 깊이 스펙 확인 필요(HW §IOC/FIFO 절).
- low-group-delay 필터의 실제 채널별 group-delay 차이가 decimation WDF의 1/8샘플 스큐를 부분 상쇄 또는 증폭하는지 — Table 41의 각 경로별 group-delay 수치 직접 확인 필요.
- DMIC2(FE, falling-edge)가 DMIC1(RE, rising-edge)보다 DMIC 오버샘플링 클럭 반주기 늦게 캡처되는 효과가 최종 출력 샘플에 의미 있는 추가 스큐를 만드는지 — CIC pre-decimation이 이를 흡수하는지 정량 검증 필요(사실 E 토론 포인트_3).
- Filter Engine 대안경로를 선택했을 때 CFX 내 delay-and-sum 빔포밍 알고리즘이 오버샘플 데이터를 직접 처리할 명령어·사이클 예산이 있는지 — CFX DSP 아키텍처 스펙 확인 필요.
