---
name: 분석 페르소나 01
purpose: DMIC HW 인터페이스 분석가 분석 결과
type: tasks
maturity: experimental
tags: [beamforming, agent-log, analysis]
---

# [단계1·분석 01] DMIC HW 인터페이스 분석가

**신뢰도**: high

## 결론
본 보드의 데이터 pad 분리+클럭 공유 구성에서 RE/FE 반주기 스큐(0.1302µs = 0.0021 샘플)는 빔포밍 최대 지연(58.3µs)의 0.22%에 불과하여 실질적 영향이 없다. 지배적 스큐는 decimation 시분할로 발생하는 채널쌍 고유 스큐(1/8 샘플 = 7.81µs)이며, 이는 DMIC2(ch2, Left)가 DMIC1(ch1, Right)보다 7.81µs 먼저 도달함을 의미한다. 빔포밍에 필요한 0.933 샘플 지연은 DELAY_INTEGER=7(0.875 샘플)과 DELAY_FRACTIONAL≈14로 HW에서 직접 구현 가능하며, 채널쌍 고유 스큐 1/8 샘플을 지연 예산에 반드시 포함해야 한다.

## 발견(근거)
- **RE/FE 반주기 스큐의 실제 크기: 0.1302µs = 0.0021 샘플**
  - 근거: DMIC 클럭 = ADCCLK = 3.84MHz(사실E, HW §18.2 p.563). 반주기 = 1/(2×3.84MHz) = 0.1302µs. 16kHz 샘플 주기 62.5µs 대비 0.1302/62.5 = 0.00208 샘플. DMIC1(RE)이 상승 엣지, DMIC2(FE)가 하강 엣지 캡처이므로 FE는 RE보다 오버샘플링 클럭 반주기 늦게 캡처(사실E). (HW §18.2 p.563 / 사실E)
- **RE/FE 반주기 스큐는 빔포밍 타이밍에 실질 영향 없음 — 빔포밍 지연(58.3µs)의 0.22%, fractional 해상도(0.26µs)보다도 작음**
  - 근거: 빔포밍 최대 지연 d/c = 0.02/343 = 58.3µs(사실F). RE/FE 스큐 0.1302µs ÷ 58.3µs = 0.22%. DELAY_FRACTIONAL 1스텝 해상도 = 1/(8×30) 샘플 = 0.26µs(사실D, SFCR=29). 즉 RE/FE 스큐는 HW 지연 레지스터의 최소 분해능(0.26µs)보다도 작아 보정조차 불필요한 수준. (사실D(HW p.450-451), 사실F)
- **데이터 pad 분리+클럭 공유 구성에서 RE/FE 분리의 실질 기능: 스테레오 1-pad 패턴을 별도 pad에 확장 적용한 형태이며, 반주기 스큐 자체보다 채널 할당 메커니즘이 핵심**
  - 근거: 스테레오(1 pad 공유) 시 low=RE, high=FE가 표준(사실E, HW §18.2 p.563). 본 보드는 DIO23(DMIC1)/DIO17(DMIC2) pad 분리+DIO22 클럭 공유 구성(lib_audio_in.c:121,139). DMIC1_DATA_RE→ch1, DMIC2_DATA_FE→ch2로 AUDIO_MUX 소스 구분(lib_audio_in.h:80, LIB_DMIC_AUDIO_MUX_CFG_INPUT_CH_SRC). 반주기 시차가 아니라 채널 소스 식별자가 decimation 필터 경로를 결정. (HW §18.2 p.563 / lib_audio_in.h:80 / lib_audio_in.c:121,139)
- **지배적 스큐: decimation 시분할로 DMIC2(ch2, Left)가 DMIC1(ch1, Right)보다 1/8 샘플(7.81µs) 먼저 IOC에 도달**
  - 근거: 사실B(HW p.445 §14.4): 짝수 채널(0,2)이 홀수 채널(1,3)보다 1/8 샘플 먼저 decimation 출력. 현재 매핑: ch1=DMIC1(Right/RE), ch2=DMIC2(Left/FE)(lib_audio_in.h:80). ch2가 짝수이므로 DMIC2(Left)가 DMIC1(Right)보다 1/8 샘플 = 7.81µs 먼저 도달. 사실A(HW p.222)와 일치: ADC0·ADC2가 ADC1·ADC3보다 1/8 샘플 먼저 도착. (HW p.222(사실A), HW p.445 §14.4(사실B) / lib_audio_in.h:80)
- **채널쌍 고유 스큐(1/8 샘플 = 7.81µs)는 빔포밍 지연 예산의 13.4%로, 반드시 지연 계산에 포함해야 함**
  - 근거: 7.81µs ÷ 58.3µs = 13.4%. 고유 스큐를 무시하면 빔포밍 지연 설정 오차 7.81µs 발생. DMIC2(Left,ch2)가 이미 1/8 샘플 앞서 도달하므로, end-fire 방향 기준으로 어느 마이크를 기준으로 지연할지에 따라 ch1에 0.933+0.125=1.058 샘플 또는 ch2에 0.933-0.125=0.808 샘플을 부여하는 방향 결정이 필요(미해결). (사실A(HW p.222), 사실B(HW p.445), 사실F)
- **필요 빔포밍 지연 0.933 샘플은 DELAY_INTEGER=7(0.875 샘플) + DELAY_FRACTIONAL≈14로 HW에서 직접 구현 가능**
  - 근거: 사실F: 필요 지연 = d/c / T_sample = 58.3µs/62.5µs = 0.9329 샘플. 사실G: DELAY_INTEGER 최대 7/8=0.875 샘플(54.69µs). 나머지 0.9329-0.875 = 0.0579 샘플 = 3.62µs를 DELAY_FRACTIONAL로 보정. DELAY_FRACTIONAL 해상도 = 1/(8×30) 샘플 = 0.2604µs(사실D, SFCR=29). 필요 FRAC 값 = 3.62µs÷0.2604µs = 13.9 → DELAY_FRACTIONAL=14. 유효 범위 0~29 내(사실D). (사실D(HW p.450-451), 사실F, 사실G)
- **DELAY_FRACTIONAL 해상도 0.26µs는 DMIC HW에도 동일하게 적용됨 (사실E 명시)**
  - 근거: 사실E(HW §18.2 p.563): 'DMIC pre-decimation 필터도 ADC와 동일하게 fractional delay 지원'. DMIC 입력도 6th order Sync(CIC) pre-decimation 필터를 거치며 decimation factor가 ADC와 동일(ADC_MODDIV_BY30 = SFCR 29). 따라서 채널 1(DMIC1)·채널 2(DMIC2) 각각에 AUDIO_ADC_DEC_CTRL 레지스터의 DELAY_INTEGER+DELAY_FRACTIONAL을 독립 설정 가능. (HW §18.2 p.563(사실E) / lib_audio_in.c:111,129)
- **현재 펌웨어(DELAY_INTEGER=0, DELAY_FRACTIONAL=0)는 빔포밍 지연을 전혀 부여하지 않는 상태 — 채널쌍 고유 스큐 7.81µs만 잠재적으로 존재**
  - 근거: lib_audio_in.h:96-98: LIB_SAMPLE_FRACTIONAL_DELAY=0, ADC_INTEGER_DELAY_0, LIB_ADC_DEC_CTRL_VAL 동일 값이 ch1(c:111)·ch2(c:129) 모두에 적용. 의도된 빔포밍 지연 없음. 사실B의 채널쌍 고유 스큐(ch2가 ch1보다 1/8 샘플 빠름)만 HW 구조상 존재. (lib_audio_in.h:96-98 / lib_audio_in.c:111,129 / 사실B(HW p.445))

## 미해결 질문
- 빔포밍 방향 기준 미결정: 착용 시 '앞 방향' 음원이 Left(DMIC2/ch2)와 Right(DMIC1/ch1) 중 어느 쪽에 먼저 도달하는지 — 착용 기하(in-ear 위치)에 따라 ch1에 지연을 주어야 할지 ch2에 주어야 할지가 결정됨. 이 정보 없이는 DELAY_INTEGER=7+FRAC≈14를 ch1에 줄지 ch2에 줄지 단정 불가.
- 채널쌍 고유 스큐 1/8 샘플의 보정 방향: DMIC2(ch2, Left)가 1/8 샘플 먼저 도착하므로, 의도 빔포밍 지연과 합산 시 ch1 기준 총 필요 지연이 0.933+0.125=1.058 샘플인지, 아니면 ch2를 기준으로 0.933-0.125=0.808 샘플인지 — 방향 질문_1이 해결되어야 확정 가능.
- DELAY_INTEGER=7(0.875 샘플)이 단일 채널 레지스터의 최대값임이 데이터시트에서 명확히 확인되었으나, 실제 최대 필요 지연 1.058 샘플(채널쌍 스큐 포함 시)이 DELAY_INTEGER 범위(0~7/8=0.875)를 초과하는 경우 HW 단독으로 커버 불가 — SW 버퍼 1샘플 시프트 병행 필요 여부 미결정(사실G).
- RE/FE 분리 의도성 확인 필요: 본 보드에서 DMIC2를 FE로 설정한 것이 하드웨어 설계 제약(QCC 기존 인터페이스 호환)인지, 의도적 타이밍 선택인지 — 회로도/설계 의도 문서 미확인. 양쪽 모두 RE로 통일 가능한지 여부도 확인 필요.
- decimation 절대 group delay의 채널 간 대칭성: 사실B는 상대 스큐(1/8 샘플)를 명시하나, 절대 group delay가 ch1·ch2 동일 대역(0K_8K) 설정에서 완전히 상쇄되는지 데이터시트 명시 없음 — 상대 지연만 유효하다는 전제를 데이터시트에서 재확인 필요(포인트_7).
- 2-DMIC 활성화 시 QCC와의 DIO17/DIO10 공유 충돌: lib_audio_in.c:104 주석 'QCC 담당'. 현재 LIB_AUDIO_IN_DMIC_ENABLE_COUNT=1이며, 2로 변경 시 QCC 측 DMIC2 사용과의 버스 충돌 및 SPI 프로토콜 협의(10ms 대기) 운용 제약이 미해결.
