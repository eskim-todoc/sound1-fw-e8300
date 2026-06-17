# 노드 04 — Win_DFT_R_Forward + BFP 분석가

## 핵심 발견

### Win_DFT_R_Forward 시그니처 (microcode.hct L174-184)
```c
Win_DFT_R_Forward(
    HEAR_FC_fft_vmag_win_fft,   // name
    HEAR_FFT_SIZE,              // K=512
    HEAR_ADDR_FFT_INPUT,        // 입력 (FIFO)
    HEAR_ADDR_FFT_OUTPUT,       // 출력 (C0MEM)
    HEAR_ADDR_FFT_TEMP01,       // scratch (H01MEM)
    HEAR_ADDR_FFT_TEMP23,       // scratch (H23MEM)
    HEAR_ADDR_FFT_WINDOW,       // 윈도우 (OUTPUT+512)
    1, 1                        // addr mode: incremental
)
```
- 입력 포맷: **Q24.0** (24비트 정수)
- 출력 포맷: Real/Imag 복소수, **22비트 유효값** (BFP 자동 정규화)

### BFP 1/4 게인 근거
- Ezario FFT 라이브러리의 Block Floating Point가 자동 스케일 → 22비트 제한 → 2비트 손실 = 1/4 게인
- 실측값 (FrequencyAnalysis.c L92): "실제 동작 테스트로, HEAR의 BFP 연산은 1/4 게인 적용 확인 (정우님 테스트)"

### 해닝 윈도우 저장 방식 (ci_fft.c)
- 전반부 256개: `p_memory[i] = hanning_window_coeff[i]`
- 후반부 256개: `p_memory[256+i] = hanning_window_coeff[255-i]` (대칭)
- 512개로 확장 후 `HEAR_ADDR_FFT_WINDOW`에 저장

### DFT 출력 구조
- 출력: [Re0, Im0, Re1, Im1, ... Re255, Im255] = 512개 요소
- 메모리: `D_HEAR_C0MEM_BASE` (C0 메모리)
- 유효 비트폭: 22비트 (32비트 int에 저장)
