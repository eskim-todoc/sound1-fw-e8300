# 노드 03 — FFT 입력준비 분석가

## 핵심 발견

### 상수값
- `FFT_SIZE = 512`, `HALF_FFT_SIZE = 256` (definitionsForAlgorithm.h L97-98)
- `HEAR_FFT_SIZE = 512`, `HEAR_HALF_FFT_SIZE = 256` (microcode.h L13-14)

### FFT FIFO 삽입 함수
- 함수: `update_FFT_inputData()` (FrequencyAnalysis.c L32-43)
- 소스: `m_agc_output_buffer[16]` (Q24.0, 24비트 정수)
- 대상: `D_FIFO_A1_0->ACCESS` (HEAR FFT FIFO A1_0)
- 방향: Top→Bottom 역순 복사 (포인터 감소)
- 삽입 직전 데이터 포맷: **Q24.0** (AGC 출력 그대로)

### Win_DFT_R_Forward 호출
- `CALL_FUNCTION_CHAIN(HEAR_FC_fft_vmag)` (main.c L411)
- FFT+vMag가 하나의 Function Chain으로 통합 실행
- 입력: `HEAR_ADDR_FFT_INPUT`, 출력: `HEAR_ADDR_FFT_OUTPUT`
- 윈도우: `HEAR_ADDR_FFT_WINDOW = HEAR_ADDR_FFT_OUTPUT + 512`

### MIC 채널 분리
- MIC0(내부/EZ): `HCT_A0_0` → `audio_mix_internal_mic_only()`
- MIC1(외부/QCC): `HCT_A0_1` → `audio_mix_external_mic_only()`
- 기본 경로: `audio_mix_internal_mic_only()` (main.c L388-392)

### 게인 주석 (FrequencyAnalysis.c L89-98)
```
총 게인: FFT_SIZE * 1/2(분산) * 1/2(윈도우) * 1/4(BFP) * 1/2(vMag)
       = FFT_SIZE / 32 = 512 / 32 = 16 = 2^4
→ RIGHT_SHIFT_MAX_MAG_FREQ_SCALE = 4 (>>4)
```
