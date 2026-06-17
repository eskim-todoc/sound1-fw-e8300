# 노드 01 — DMIC 입력 분석가

## 핵심 발견

### DMIC 데이터 비트폭 및 타입
- 데이터 타입: `volatile int _XMEM` (24-bit 오디오, 32-bit int에 부호 확장 저장)
- FIFO 배열: `HCT_A0_0[32]` (MIC0/EZ), `HCT_A0_1[32]` (MIC1/QCC) — 32샘플 단위

### >>5 시프트 발생 위치
- 매크로: `AUDIO_INPUT_RSHIFT = 5` (definitionsForAlgorithm.h L103)
- 적용 함수: `audio_mix_internal_mic_only()`, `audio_mix_external_mic_only()`, `audio_mix_1_buffer()`, `audio_mix_2_buffers()` (audioMixer.c)
- 코드: `p_mix[i] = p_internal_mic[i] >> AUDIO_INPUT_RSHIFT;`
- **FIFO에서 읽은 후 오디오 믹서 단계에서 >>5 적용** → Q1.23 → Q6.18 변환

### FIFO 이벤트 트리거
- `FIFO_0_ISR()` → `g_interrupt_flags.mic0 = 1` (interrupt_service_routine.c L81-84)
- 메인 루프: `mic0 == 1 && pcm_out == 1` 동시 만족 시 신호처리 시작 (main.c L182-186)
- FIFO 인터럽트 설정: `SYS_FIFO_CFXINTCONFIG(0, FIFO_INT_A0_0)` (lib_audio_in.c L49)

### INT24 상수
- `INT24_MAX = 0x7FFFFF` (+8,388,607)
- `INT24_MIN = -0x800000` (-8,388,608)
- (definitionsForAlgorithm.h L38-40)

## 경로 요약
```
DMIC(DIO23) → Decimation Filter → IOC → FIFO_A0_0[32]
→ FIFO_0_ISR(mic0=1) → main loop 조건 충족
→ audio_mix() : >>5 적용 → HEAR_ADDR_AUDIO_MIX
→ AGC 처리
```
