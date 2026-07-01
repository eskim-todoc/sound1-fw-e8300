---
name: 검증 페르소나 28
purpose: 적대적 검증 - SW 버퍼 시프트 없이 HW DEC delay만으로 빔포밍 지연을 부여할 수 있다
type: tasks
maturity: experimental
tags: [beamforming, agent-log, verify]
---

# [단계3·검증 28] SW 버퍼 시프트 없이 HW DEC delay만으로 빔포밍 지연을 부여할 수 있다

**판정**: `CONFIRMED`

## 근거
세 핵심 조건을 데이터시트 사실로 교차 검증한 결과 모두 지지된다.

[조건 1 — DMIC에 HW fractional delay 지원]
사실 E(HW §18.2 p.563): DMIC pre-decimation 필터가 "ADC와 동일하게 fractional delay 지원"이며, 현재 구성 코드(lib_audio_in.c:111, c:129)에서 DMIC1(ch1)·DMIC2(ch2) 모두 SYS_SET_ADC_DEC_CTRL(AUDIO, 1/2, ...) 동일 레지스터 인터페이스로 설정됨이 확인된다. HW 기능 존재 확정.

[조건 2 — 레지스터 물리 범위가 필요 지연 0.933 샘플을 커버]
사실 D(HW p.450-451): DELAY_INTEGER 최대=7 → 7/8=0.875 샘플(54.7µs). DELAY_FRACTIONAL 해상도 ≈ 0.0042 샘플(0.26µs)/스텝(SFCR≈29). 부족분 0.933-0.875=0.058 샘플 = FRACTIONAL ≈14 스텝(14 < 29이므로 범위 내). INTEGER_7 + FRACTIONAL ≈14로 0.933 샘플 정밀 도달 가능.
사실 G의 "DELAY_INTEGER 최대 7/8 < 1.0 샘플, 정수 1샘플 불가"는 필요값이 1.0이라면 반증이 됐겠으나, 실제 필요값이 0.933이므로 7/8+fractional 조합으로 도달 가능하다. 이 부분은 반증 후보였으나 수치 계산으로 기각됨.

[조건 3 — 구조적 우회 불가 제약 없음]
채널 쌍 1/8 스큐(사실 A/B, HW p.222/p.445): DMIC2(ch2)가 DMIC1(ch1)보다 1/8 샘플 먼저 도착하는 고유 스큐는 DEC delay로 ch1에 추가 지연을 더해 흡수 가능하며 총 필요량이 레지스터 범위를 초과하지 않는다.
RE/FE 엣지 차(사실 E): 오버샘플링 클럭(3.84MHz) 반주기 ≈ 0.13µs = 약 0.002 샘플로 DEC delay 최소 해상도(0.26µs)보다도 작아 실용적으로 무시 가능하다.
클럭 공유: 두 DMIC가 동일 DMIC_CLK1을 공유하므로 DEC delay 기준이 동기화되어 있어 비동기 문제 없음.

[반증 미성립 이유]
유일한 잠재 반증 후보는 "DMIC 채널에 AUDIO_ADC_DEC_CTRL이 실제로 매핑되는지 여부"였으나, 코드에서 SYS_SET_ADC_DEC_CTRL(AUDIO, ch1/ch2, ...)로 DMIC 채널에 직접 적용됨이 확인되어 반증 실패.
현재 DMIC 1개만 활성(LIB_AUDIO_IN_DMIC_ENABLE_COUNT==1)이고 QCC DMIC2 공유 제약이 존재하나, 이는 현재 구현 상태의 문제이지 HW DEC delay 기능 자체의 구조적 불가 근거가 아니다.

결론: 데이터시트 사실 범위 내에서 주장은 반증되지 않으며 수치·기능 근거 모두 지지한다. confirmed.

## 데이터시트 근거
HW p.450-451 (AUDIO_ADC_DEC_CTRL: DELAY_INTEGER bits 26:24 = 0~7/8 샘플, DELAY_FRACTIONAL bits 21:16 = 0~SFCR≈29, 해상도 ≈0.26µs); HW §18.2 p.563 (DMIC fractional delay ADC와 동일 지원); HW p.222 (ADC0/2가 ADC1/3보다 1/8 샘플 먼저 출력); HW p.445 §14.4 (채널쌍 0,1·2,3 시분할, 홀수 채널 1/8 지연); HW p.219 NOTE (fractional delay 변경이 ADC 입력 간 동기화에 영향); lib_audio_in.c:111,129 (SYS_SET_ADC_DEC_CTRL AUDIO ch1/ch2에 적용 확인); 빔포밍 산술: d/c=0.02/343=58.3µs=0.933샘플, INTEGER_7=0.875+FRACTIONAL≈14스텝=0.058샘플 → 합 0.933 샘플 도달 가능

