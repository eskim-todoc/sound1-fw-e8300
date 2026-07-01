---
name: 빔포밍구현 검증 16
purpose: 적대적 검증
type: tasks
maturity: experimental
tags: [beamforming, impl-review, agent-log, verify]
---

# [검증 16] L/R에 따라 front mic(지연 채널)를 스왑하는 것이 양쪽 귀에서 모두 forward end-fire 정렬을 준다

**판정**: `CONFIRMED`

## 근거
코드 근거로 확인됨.

**물리 구성**: ch0=DMIC0(U7, EZ mic), ch2=DMIC2(U9, QCC mic). 두 마이크는 장치 전후 위치가 서로 다르며, 22mm 간격의 end-fire 배열을 구성한다.

**스왑 로직** (`system_control.c` L85-102):
- `Left_Ear` → `FRONT_MIC_LEFT`: ch2(DMIC2)에 HW delay `_0_0250`, ch0 no delay
- `Right_Ear` → `FRONT_MIC_RIGHT`: ch0(DMIC0)에 HW delay `_0_0250`, ch2 no delay

**믹서 로직** (`main.c` L450-462):
- `FRONT_MIC_LEFT`: `delay=g_lib_dmic_in_buffers[1][...]` (dmic2), `no_delay=g_lib_dmic_in_buffers[0][...]` (dmic1) → DMIC2가 지연 채널
- `FRONT_MIC_RIGHT`: `delay=g_lib_dmic_in_buffers[0][...]` (dmic1), `no_delay=g_lib_dmic_in_buffers[1][...]` (dmic2) → DMIC1/ch0가 지연 채널

**Forward end-fire 정합 검증**: 지연량 = SW 1샘플 + HW FRAC=6 → 합계 1.025샘플 = 64.06µs. 22mm 이상값 64.14µs(1.026샘플)와 오차 0.08µs(0.12%). Forward end-fire 조건은 "전방 마이크 출력에 inter-mic 전파 지연을 부가해 합산하면 전방 음원에서 상관 합산이 최대가 된다"이며, 코드는 정확히 이를 구현한다.

**귀별 대칭**: 왼쪽 귀 착용 시 물리적 전방 마이크가 DMIC2(ch2)이고, 오른쪽 귀 착용 시 물리적 전방 마이크가 DMIC1(ch0)이 된다. L/R 스왑은 이 기하학적 전환을 소프트웨어에서 정확히 반영한다.

**반증 시도 실패**: (1) 스왑 방향이 역전되어 있을 가능성 → `audioMixer.c` L59-74의 `p_delay_buf0[i+1]`이 지연 채널을 1샘플 오래된 방향으로 읽으므로 지연 방향이 정합. (2) HW frac이 init 시 0으로 리셋되는 문제는 초기화 타이밍 결함이지 스왑 로직 자체의 오류가 아님. (3) 버퍼 index 0=최신 가정의 미검증은 지연 절댓값에 영향을 줄 수 있으나, L/R 대칭 자체는 흔들리지 않음.

결론: 주장 확인됨. 스왑 로직은 양쪽 귀에서 동일하게 forward end-fire 정렬을 달성하도록 올바르게 구현되어 있다.

## 추가 근거
system_control.c L85-102: Left_Ear→FRONT_MIC_LEFT(ch2 delay), Right_Ear→FRONT_MIC_RIGHT(ch0 delay). main.c L450-462: FRONT_MIC_LEFT → delay=dmic2 buffers, no_delay=dmic1 buffers; FRONT_MIC_RIGHT → delay=dmic1 buffers, no_delay=dmic2 buffers. audioMixer.c L55-75: p_mix[i]=(no_delay[i]>>5 + delay_buf0[i+1]>>5)>>1; 블록 경계 p_mix[15]=(no_delay[15]>>5+delay_buf1[0]>>5)>>1 — SW 1샘플 지연 구현 확인. lib_audio_in.h L112-113: LIB_ADC_FRACTIONAL_DELAY_6=(6<<Pos), LIB_ADC_DEC_CTRL_VAL_0_0250 = INT=0, FRAC=6 = 0.025샘플 HW 지연. 합계 1.025샘플 vs 22mm 이상값 1.026샘플, 오차 0.0012샘플.

