---
name: 분석 페르소나 02
purpose: Decimation 필터 체인 분석가 — WDF 5/7/11차 group delay·ban 분석 결과
type: tasks
maturity: experimental
tags: [beamforming, agent-log, analysis]
---

# [단계1·분석 02] Decimation 필터 체인 분석가 — WDF 5/7/11차 group delay·band select·채널 간 상대 위상 영향 전문

**신뢰도**: high

## 결론
WDF decimation 체인(5/7/11차)의 절대 group delay는 두 채널이 동일 BAND_SELECT_ADC 설정이면 완전히 상쇄되므로, 빔포밍 상대 지연 예산에서 decimation 절대 지연 항은 제거된다. 단, 사실 A/B(HW p.222, p.445)에서 확인된 decimation 시분할 스큐(짝수 채널 ch2가 홀수 채널 ch1보다 1/8 샘플 먼저 출력)는 WDF 처리 자체에 기인한 고유 비대칭이며, 이는 절대 group delay 상쇄 이후에도 잔존하는 채널 간 상대 지연이다. 현재 두 채널 모두 BAND_SELECT_ADC_0K_8K로 동일 설정이므로 이 조건은 충족되어 있다.

## 발견(근거)
- **동일 BAND_SELECT 설정 시 decimation WDF 절대 group delay는 두 채널 간 완전 상쇄된다**
  - 근거: lib_audio_in.h:98에서 ch1(DMIC1)과 ch2(DMIC2) 모두 동일 매크로 LIB_ADC_DEC_CTRL_VAL을 적용(lib_audio_in.c:111,129). LIB_ADC_DEC_CTRL_VAL에는 BAND_SELECT_ADC_0K_8K가 공통으로 포함(h:98). WDF 5/7/11차 필터의 group delay는 계수·차수·대역 선택에 의해 결정되는 상수값이며, 동일 필터 구성을 두 채널에 동시 적용하면 채널 간 절대 group delay 차이는 0이 된다. 즉 'decimation 절대 group delay는 동일 대역 상쇄' 조건이 현재 구성에서 충족되어 있다. (HW p.445 §14.4, lib_audio_in.h:98, lib_audio_in.c:111,129)
- **BAND_SELECT_ADC가 다를 경우 채널 간 group delay 차이가 발생하며 빔포밍 위상 오류를 유발한다**
  - 근거: AUDIO_ADC_DEC_CTRL bits 30:28이 BAND_SELECT_ADC(조사_데이터시트-그라운딩.md §3). WDF 차수가 5/7/11차로 대역별로 달라지면 각 대역의 FIR/IIR 계수·탭 수가 변하고, 이에 따른 절대 group delay가 달라진다. 만약 ch1=0K_8K, ch2=0K_4K처럼 다른 대역을 선택하면 두 채널의 WDF group delay 절대값이 다르게 되어 채널 간 상대 지연이 생긴다. 현재 구성은 둘 다 BAND_SELECT_ADC_0K_8K이므로 이 오류 원인은 비활성이다. (lib_audio_in.h:98, 조사_데이터시트-그라운딩.md §3 (BAND_SELECT_ADC 필드 설명))
- **decimation 시분할(ch2 vs ch1)이 만드는 1/8 샘플 고유 스큐는 WDF group delay 상쇄 이후에도 잔존하는 별도의 채널 간 상대 지연이다**
  - 근거: 사실 A(HW p.222): 'ADC0·ADC2 데이터가 ADC1·ADC3보다 1/8 샘플 먼저 도착'. 사실 B(HW p.445 §14.4): 'time multiplexed into two pairs (channels 0,1; channels 2,3) resulting in longer processing delays for channels 1,3'. 이 스큐는 WDF 필터 계수와 무관하게 decimation 하드웨어의 시분할 스케줄러(짝수쌍 우선 처리)가 고정적으로 발생시키는 것이다. 따라서 두 채널의 BAND_SELECT가 동일하여 WDF 절대 group delay가 상쇄되더라도, 이 1/8 샘플 스큐는 남는다. 현재 매핑에서 DMIC2(Left, ch2)가 DMIC1(Right, ch1)보다 1/8 샘플(62.5µs/8 = 7.8µs) 먼저 도착. (HW p.222, HW p.445 §14.4, 조사_데이터시트-그라운딩.md §2)
- **현재 BAND_SELECT_ADC_0K_8K 설정에서 WDF는 5/7/11차 중 하나로 동작하며, 두 채널에 동일 차수·계수가 적용된다**
  - 근거: lib_audio_in.h:98에서 LIB_ADC_DEC_CTRL_VAL에 BAND_SELECT_ADC_0K_8K가 포함되고, ch1과 ch2 모두 이 동일 값으로 SYS_SET_ADC_DEC_CTRL 호출(lib_audio_in.c:111, 129). WDF 5/7/11차는 대역 선택에 따라 내부적으로 결정되며(E8300 HW Reference §14), 동일 BAND_SELECT이면 동일 차수·동일 계수·동일 절대 group delay가 양 채널에 적용된다. (lib_audio_in.h:98, lib_audio_in.c:111,129, HW p.445 §14.4)
- **빔포밍 상대 지연 예산에서 고려할 채널 간 순 지연 성분은 (1) 고유 1/8 샘플 스큐 + (2) DELAY_INTEGER/FRACTIONAL 설정값 차이이며, WDF 절대 group delay는 예산에서 제거된다**
  - 근거: WDF 절대 group delay: 동일 BAND_SELECT이면 두 채널 동일값 → 상쇄. 고유 1/8 스큐: 사실 A/B에 따라 ch2가 ch1보다 1/8 샘플(7.8µs) 먼저 도착 → 잔존. DELAY_INTEGER/FRACTIONAL: 현재 둘 다 0 → 차이 0(조사_데이터시트-그라운딩.md §1 'integer=0, fractional=0'). 따라서 현재 순 채널 간 상대 지연 = 0(레지스터) - 1/8 샘플(고유 스큐) = ch2가 ch1보다 1/8 샘플 먼저 도착. 사실 F(빔포밍 기하) 기준 필요 상대 지연 0.933 샘플을 맞추려면 이 1/8 스큐를 지연 예산의 일부로 활용하거나 보정해야 한다. (HW p.222, HW p.445 §14.4, HW p.450-451, 조사_데이터시트-그라운딩.md §1,§3,§5)
- **DELAY_INTEGER/FRACTIONAL 레지스터는 채널별 샘플링 지연을 부여하는 것이지 WDF 필터 계수를 바꾸지는 않는다 — band select와 독립적인 별도 메커니즘이다**
  - 근거: AUDIO_ADC_DEC_CTRL 레지스터 구조(HW p.450-451): BAND_SELECT_ADC(30:28)과 DELAY_INTEGER(26:24), DELAY_FRACTIONAL(21:16)이 독립 비트필드로 분리되어 있다(조사_데이터시트-그라운딩.md §3). DELAY_INTEGER는 '1/8 샘플 단위로 샘플링을 지연' — 즉 ADC/DMIC 입력 캡처 시점 자체를 앞당기거나 늦추는 것이며, WDF 필터의 주파수 응답·group delay 특성은 BAND_SELECT에 의해 결정된다. 둘은 독립적으로 설정 가능하다. (HW p.450-451, 조사_데이터시트-그라운딩.md §3)

## 미해결 질문
- WDF 5/7/11차 각 차수의 정확한 절대 group delay 수치(샘플 단위)가 데이터시트에 명시되어 있는지 확인 필요 — HW Reference §14에 해당 수치 표가 있는지 미확인. 빔포밍 자체에는 영향 없으나 레이턴시 예산에 필요.
- BAND_SELECT_ADC_0K_8K에서 활성화되는 WDF가 5차/7차/11차 중 정확히 어느 구성인지 그라운딩 데이터에서 특정되지 않음 — HW §14 표를 직접 확인해야 함.
- 고유 1/8 샘플 스큐의 부호 확인: 사실 A/B는 ch0,ch2가 ch1,ch3보다 '먼저 도착'이라고 명시(HW p.222, p.445). DMIC2=ch2(Left/먼저), DMIC1=ch1(Right/나중) 매핑 하에서, 빔포밍 타깃 방향(front end-fire)을 기준으로 어느 마이크를 기준(reference)으로 삼고 어느 쪽에 지연을 추가할지는 음향 기하 분석가의 영역 — 본 페르소나 범위 밖.
- 두 채널에 동일 LIB_ADC_DEC_CTRL_VAL을 적용한다고 코드에 나와 있지만(lib_audio_in.c:111,129), 실제 2-DMIC 활성화 분기(LIB_AUDIO_IN_DMIC_ENABLE_COUNT==2)의 런타임 동작을 하드웨어에서 검증한 기록 없음 — 시뮬레이션/측정 필요.
- 사실 C(HW p.219 NOTE): fractional delay 변경이 ADC 입력 간 동기화 자체에 영향을 준다고 명시 — DELAY_FRACTIONAL 값을 채널별로 다르게 줄 때 예기치 않은 클럭 도메인 영향이 있는지 데이터시트 추가 확인 필요.
