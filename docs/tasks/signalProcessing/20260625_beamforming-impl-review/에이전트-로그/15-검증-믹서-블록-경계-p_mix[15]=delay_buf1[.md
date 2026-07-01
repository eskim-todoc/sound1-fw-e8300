---
name: 빔포밍구현 검증 15
purpose: 적대적 검증
type: tasks
maturity: experimental
tags: [beamforming, impl-review, agent-log, verify]
---

# [검증 15] 믹서 블록 경계 p_mix[15]=delay_buf1[0] 이어붙임이 시간적으로 연속적이며 샘플 중복/누락이 없다

**판정**: `UNCERTAIN`

## 근거

검증 결과를 두 계층으로 분리한다.

[계층 1 — 코드에서 확정 가능한 사실]

tdc_copy_DMIC_buffers(main.c:321-348)의 롤링 로직: LIB_AUDIO_IN_BUF_MAX_CNT=2일 때 내부 루프(j=0)는 buffers[mic][1][i] = buffers[mic][0][i] 후 buffers[mic][0][i] = FIFO[i]를 실행한다. 따라서 블록 축 방향은 [0]=최신 블록, [1]=직전 블록으로 코드에서 직접 확정된다.

믹서 호출 인자(main.c:454, LEFT 케이스): p_delay_buf0 = &buffers[1][BUF_MAX_CNT-2][0] = buffers[1][0] (dmic2 최신 블록), p_delay_buf1 = &buffers[1][BUF_MAX_CNT-1][0] = buffers[1][1] (dmic2 직전 블록). 블록 간 인수 전달은 올바르게 확정된다.

audioMixer.c:74의 경계 라인: p_mix[15] = (no_delay_buf0[15] >> 5 + p_delay_buf1[0] >> 5) >> 1. 여기서 p_delay_buf0[15]는 현재(최신) 블록의 인덱스 15, p_delay_buf1[0]는 직전 블록의 인덱스 0을 읽는다.

[계층 2 — 코드에서 확정 불가능한 전제]

경계 연속성의 핵심은 블록 내 샘플 인덱스 방향이다. HCT_A0_0[0..15]에서 [0]이 최신 샘플인지 오래된 샘플인지는 E8300 ADC FIFO 하드웨어 스펙에 의존하며, lib_audio_in.c/h 어디에도 명시적 주석, assert, static_assert가 없다.

[0]=최신 가정이 참이면: 현재 블록의 [15]가 블록 내 가장 오래된 샘플(블록 시작)이고, 직전 블록의 [0]이 직전 블록의 가장 최신 샘플(직전 블록 끝)이 된다. 이 경우 시간 순서는 ...직전블록[0], 현재블록[15], 현재블록[14]... 가 되어 블록 경계에서 갭·중복 없이 연속된다.

[0]=오래됨 가정이 참이면: 현재 블록의 [15]는 블록의 가장 최신 샘플, 직전 블록의 [0]은 직전 블록의 가장 오래된 샘플이 된다. 이 경우 p_delay_buf0[14]→[15] 다음에 p_delay_buf1[0]이 오는 구조는 최신 블록 끝에서 직전 블록 시작(가장 오래됨)으로 15샘플을 뛰어넘는 갭이 된다.

[결론]

코드 분석만으로 검증 가능한 블록 축 방향은 정합하다. 그러나 경계 연속성의 성립 여부는 FIFO 내 샘플 인덱스 방향([0]=최신 vs 오래됨)에 완전히 종속되며, 이는 현재 코드에 명시적 근거가 없어 HW 레퍼런스 매뉴얼 확인 또는 RTT 임펄스 측정으로만 확정 가능하다. 따라서 주장의 진위를 코드 근거만으로 확인 또는 반증할 수 없다.


## 추가 근거

- audioMixer.c:74: p_mix[15] = ((p_no_delay_buf0[15] >> AUDIO_INPUT_RSHIFT) + (p_delay_buf1[0] >> AUDIO_INPUT_RSHIFT)) >> 1
- main.c:330: g_lib_dmic_in_buffers[0][(BUF_MAX_CNT-1)-j][i] = g_lib_dmic_in_buffers[0][(BUF_MAX_CNT-2)-j][i] (j=0: [1]=이전[0]) 후 [0]=FIFO 신규 → [0]=최신 블록 확정
- main.c:454: p_delay_buf0=buffers[1][0](최신), p_delay_buf1=buffers[1][1](직전) → 블록 간 인수 정합
- lib_audio_in.h:31: LIB_AUDIO_IN_BUF_MAX_CNT=2
- lib_audio_in.c:9-17: 버퍼 인덱싱 주석에 buffer0/buffer1 구분만 있고 샘플 인덱스 방향 미명시
- 블록 내 HCT_A0_0[0..15] 샘플 순서([0]=최신 vs 오래됨)를 직접 명시하는 코드·주석 없음


