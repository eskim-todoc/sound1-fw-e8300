# 노드 05 — vMag 분석가

## 핵심 발견

### vMag 시그니처 (microcode.hct)
```c
vMag(
    HEAR_FC_fft_vmag_vmag,     // name
    HEAR_HALF_FFT_SIZE,        // K=256
    HEAR_ADDR_FFT_OUTPUT,      // 입력 (Re/Im pairs)
    HEAR_ADDR_VMAG_OUTPUT,     // 출력
    1, 1,                      // addr mode
    VMAG_SINGLE_PRECISION      // 현재 설정
)
```

### DOUBLE vs SINGLE PRECISION
| 모드 | 비트 | Q 포맷 |
|---|---|---|
| VMAG_DOUBLE_PRECISION | 48 | Q2.46 |
| VMAG_SINGLE_PRECISION (현재) | 24 | **Q2.22** (상위 추출) |
- SINGLE_PRECISION은 DOUBLE 결과의 상위 24비트만 사용 → **1/2 게인 발생**

### 계산 방식
- **sqrt(R² + I²)** — 복소수 크기(magnitude), 에너지(R²+I²) 아님
- FrequencyAnalysis.c L80: "vMag 계산은 sqrt(Re^2 + Im^2) 이므로 항상 0 이상"

### 출력 배열
- 256개 (HEAR_HALF_FFT_SIZE)
- **[0] = DC 성분 (0 Hz)**
- **[1] = Nyquist 성분 (fs/2)**
- [2]~[255] = 중간 주파수 빈
- 출력 메모리: `HEAR_ADDR_VMAG_OUTPUT = HEAR_ADDR_FFT_INPUT + 512`

### 다음 단계
1. 256빈 → 사용자 맵(`g_pass_bin_index[]`) 기반 32채널 매핑 (최대값 추출)
2. `>> RIGHT_SHIFT_MAX_MAG_FREQ_SCALE(=4)` 스케일 조정
3. `logarithmMapping()` 로그 변환
4. 전극 자극 신호 생성
