---
name: 검증 페르소나 31
purpose: 적대적 검증 - 현재 매핑상 DMIC2(Left,ch2)가 DMIC1(Right,ch1)보다 먼저
type: tasks
maturity: experimental
tags: [beamforming, agent-log, verify]
---

# [단계3·검증 31] 현재 매핑상 DMIC2(Left,ch2)가 DMIC1(Right,ch1)보다 먼저 도착한다

**판정**: `CONFIRMED`

## 근거
세 가지 독립 근거가 모두 주장을 지지하며, 반증 시도 4건 모두 기각됩니다.

[채널 매핑 확인] lib_audio_in.h:80 — LIB_DMIC_AUDIO_MUX_CFG_INPUT_CH_SRC = (ADC3_OUT | DMIC2_DATA_FE | DMIC1_DATA_RE | ADC0_OUT). 비트 위치 기준 ch3=ADC3, ch2=DMIC2, ch1=DMIC1, ch0=ADC0. lib_audio_in.c:128-129(2-DMIC 분기)에서 SYS_SET_ADC_DEC_CTRL(ch1=DMIC1, ch2=DMIC2) 확인.

[decimation 스큐 방향 확인] 사실 A(HW p.222): "ADC0 and ADC2 arrives 1/8th of a sample period ahead of ADC1 and ADC3." 사실 B(HW p.445 §14.4): "time multiplexed into two pairs (channels 0,1; channels 2,3) resulting in longer processing delays for channels 1,3." 두 사실 모두 ch2가 ch1보다 1/8 샘플(7.8µs) 먼저 출력됨을 명시. DMIC도 ADC와 동일한 decimation 필터 엔진 사용(사실 E, HW §18.2 p.563) — 채널 번호 기반 시분할 스큐는 ADC/DMIC 소스 무관.

[반증 시도 결과] (1) DMIC에 ADC 사실 미적용 가능성 → 사실 E로 기각. (2) DMIC2의 FE 캡처(RE보다 반주기 늦음)가 역전 야기 가능성 → FE/RE 지연 ≈ 0.13µs << 채널 스큐 7.8µs로 60배 차이, 기각. (3) DMIC_ENABLE_COUNT==1로 ch2 현재 비활성 → 주장이 "매핑 구조"에 대한 것이므로 2-DMIC 경로(lib_audio_in.c:122~139 분기)의 설정 구조에 그대로 적용됨, 기각. (4) audio mux 비트 위치 오류 가능성 → 코드 직접 확인으로 기각.

[단서] 주장은 "현재 동작 중인 시스템"이 아닌 "매핑 구조"로서 성립. 실제 2-DMIC 동작에서 이 스큐(ch2 1/8 샘플 선행)는 빔포밍 지연 예산에서 보정 대상이 됩니다.

## 데이터시트 근거
사실 A (HW p.222): "ADC0 and ADC2 arrives 1/8th of a sample period ahead of ADC1 and ADC3." | 사실 B (HW p.445 §14.4): "time multiplexed into two pairs (channels 0,1; channels 2,3) resulting in longer processing delays for channels 1,3." | 사실 E (HW §18.2 p.563): DMIC pre-decimation 필터 = ADC와 동일 fractional delay 지원 | 코드 lib_audio_in.h:80: ch2=DMIC2_DATA_FE, ch1=DMIC1_DATA_RE 매핑 확인

