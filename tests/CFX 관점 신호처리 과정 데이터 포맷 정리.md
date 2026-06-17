---
name: CFX 관점 신호처리 Q-포맷 변환 흐름
purpose: MIC 1채널 기준 DMIC 입력→vMag 출력까지 각 단계의 Q-포맷·비트폭·크기 관계 정리 (개발진 공유용)
type: 참고
maturity: stable
tags: [signal-processing, q-format, cfx, hear, fft, agc, dmic]
---

**TL;DR**: DMIC 24비트 Q1.23 입력이 오디오믹서(→Q6.18), AGC(→Q24.0), FFT FIFO 삽입, 해닝 윈도잉+DFT(→22비트 복소수), vMag(→Q2.22 256개)를 거쳐 출력된다. 총 누적 게인은 FFT_SIZE/32 = 16배이며, vMag 결과를 시간영역 크기로 환원하려면 `>> 4`를 적용한다.

> [!NOTE]
> 본 문서는 `1__cfx` + `3__hear` 소스 코드를 직접 분석해 작성됐다. 주요 근거 파일: `audioMixer.c`, `agc.c`, `FrequencyAnalysis.c`, `definitionsForAlgorithm.h`, `microcode.hct`.

---

## 전체 흐름

```
  DMIC          믹서(>>5)         AGC            FFT FIFO      Win_DFT_R           vMag         결과
   |                |                |               |               |               |            |
───+────────────────+────────────────+───────────────+───────────────+───────────────+────────────▶
   |                |                |               |               |               |
   |         >>5    |  Gain 산출:    |  역순삽입     |  해닝 Q1.23   |  √(R²+I²)     | >>4
   |         ×1/32  |   Q1.47        |  Top→Bottom   |  BFP ×(1/4)   |  SINGLE_P     | (WINDOW 모드)
   |                |   →Q8.16       |  D_FIFO_A1_0  |  22비트 출력  |  ×(1/2)       | 시간영역 크기
   |                |   →Q1.23       |               |               |               |
   |                |   →Q12.12      |  ×Gain  >>12  |               |               |
   |                |  INT24 클리핑  |               |               |               |
   |                |                |               |               |               |
   └─ Q1.23         └─ Q6.18         └─ Q24.0        └─ Q24.0        └─ 22비트복소수 └─ Q2.22
      24비트           24비트           24비트          24비트          (32비트 int)   24비트
                                                                   512개 Re/Im      [0]=DC
                                                                    [Re0,Im0,...]   [1]=Nyquist
                                                                                   [2..255] 유효빈
                                                                                   총 256개
```

---

## 단계별 상세

### 1단계 — DMIC 입력 (Q1.23 → 24비트 int)

| 항목 | 내용 |
|---|---|
| 입력 포맷 | 24비트 **Q1.23** (Digital Decimation Filter 출력) |
| 저장 타입 | `volatile int _XMEM` (32비트 int에 부호 확장) |
| FIFO 배열 | `HCT_A0_0[32]` (MIC0/EZ), `HCT_A0_1[32]` (MIC1/QCC) |
| 트리거 | `FIFO_0_ISR()` → `g_interrupt_flags.mic0 = 1` |
| 처리 시작 조건 | `mic0 == 1 && pcm_out == 1` 동시 충족 (main.c) |

### 2단계 — 오디오 믹서 (Q1.23 → Q6.18, 유효비트 기준)

```c
// audioMixer.c
p_mix[i] = p_internal_mic[i] >> AUDIO_INPUT_RSHIFT;  // AUDIO_INPUT_RSHIFT = 5
```

| 항목 | 내용 |
|---|---|
| 연산 | `>> 5` (1/32 스케일 다운) |
| 출력 포맷 | **Q6.18** (유효비트 기준, 저장은 24비트 int) |
| 출력 대상 | `HEAR_ADDR_AUDIO_MIX` |
| 목적 | AGC·HEAR 연산 전 오버플로우 여유 확보 |

### 3단계 — AGC 처리 (Q6.18 → Q24.0)

AGC는 입력 최대값을 기준으로 선형 게인을 산출하고 오디오 데이터에 곱한다.

#### AGC Gain 산출 Q 포맷 변환 체인

```
MaxElement (HEAR HW 자동 계산)
  → cfx_pwr_to_dB()  : 입력 Q1.47 (frac48), 출력 Q8.16
     ※ 호출 전 << 5 (NORMALIZE_SHIFT_BIT_NUM_FOR_10DB_LIBRARY)
     ※ 입력값 최소 23 이상이어야 정상 동작
  → Attack/Release 필터 (Q8.16 도메인)
  → cfx_dB_to_pwr()  : 입력 Q8.16, 출력 Q1.47 (frac48)
  → >> 24             : Q1.47 → Q1.23
  → × m_mathlib_gain_linear_Q12_12 : Q1.23 × Q12.12 = Q13.35
  → >> 23             : Q13.35 → Q12.12
  → 최종 Linear Gain  : Q12.12 (24비트 int)
```

#### Gain 적용 (`apply_agc_gain`)

```c
// agc.c
gained_value = ((long) agc_gain_linear) * p_mix[i];  // Q12.12 × Q24.0 = Q36.12
gained_value = gained_value >> 12;                    // Q36.12 → Q36.0
// INT24 클리핑
if (gained_value > INT24_MAX) gained_value = INT24_MAX;   // +8,388,607
if (gained_value < INT24_MIN) gained_value = INT24_MIN;   // -8,388,608
p_agc_output[i] = (int) gained_value;                     // Q24.0
```

| 단계 | 포맷 | 비트 |
|---|---|---|
| cfx_pwr_to_dB 입력 | Q1.47 | 48 |
| cfx_pwr_to_dB 출력 | Q8.16 | 24 |
| cfx_dB_to_pwr 출력 (>>24) | Q1.23 | 24 |
| Linear Gain 최종 | **Q12.12** | 24 |
| AGC 출력 오디오 | **Q24.0** | 24 |

### 4단계 — FFT FIFO 삽입 (Q24.0 유지)

```c
// FrequencyAnalysis.c
void update_FFT_inputData(void) {
    register int _XMEM* p_agcOutput_Top = m_agc_output_buffer + (df_inputADC_DataBuffLength - 1);
    for (register int i = 0; i < df_inputADC_DataBuffLength; i++)
        D_FIFO_A1_0->ACCESS = *p_agcOutput_Top--;  // 역순 삽입
}
```

| 항목 | 내용 |
|---|---|
| 소스 | `m_agc_output_buffer[16]` (Q24.0) |
| 대상 FIFO | `D_FIFO_A1_0` (HEAR FFT 전용 FIFO) |
| 삽입 순서 | 역순 (Top → Bottom, 포인터 감소) |
| FFT FIFO 크기 | `HCT_A1_0[512]` (512샘플 누적 후 처리) |

### 5단계 — Win_DFT_R_Forward (해닝 윈도잉 + 512pt FFT)

```c
// microcode.hct
Win_DFT_R_Forward(
    HEAR_FC_fft_vmag_win_fft,   // 이름
    HEAR_FFT_SIZE,              // K = 512
    HEAR_ADDR_FFT_INPUT,        // 입력: FFT FIFO
    HEAR_ADDR_FFT_OUTPUT,       // 출력: C0MEM (Re/Im 512개)
    HEAR_ADDR_FFT_TEMP01,       // 스크래치패드 1
    HEAR_ADDR_FFT_TEMP23,       // 스크래치패드 2
    HEAR_ADDR_FFT_WINDOW,       // 해닝 윈도우 (Q1.23, 512개)
    1, 1                        // 주소 모드: incremental
)
```

#### 해닝 윈도우 구성

- 계수: Q1.23, 256개 (0.0 ~ ≈1.0), 512개로 전반부+대칭 후반부 확장
- 저장 위치: `HEAR_ADDR_FFT_WINDOW = HEAR_ADDR_FFT_OUTPUT + 512`

#### BFP(Block Floating Point) 게인

Ezario FFT 라이브러리는 내부적으로 BFP 연산을 수행해 출력을 22비트로 정규화한다.  
이 과정에서 **1/4 게인**이 발생한다 (실측 확인, FrequencyAnalysis.c 주석 "정우님 테스트").

| 항목 | 내용 |
|---|---|
| 입력 포맷 | Q24.0 |
| 윈도우 포맷 | Q1.23 |
| 출력 포맷 | 22비트 유효값 (32비트 int 저장) |
| 출력 구조 | [Re0, Im0, Re1, Im1, … Re255, Im255] = 512개 |
| BFP 게인 | **×(1/4)** |

### 6단계 — vMag (복소수 크기 → Q2.22)

```c
// microcode.hct
vMag(
    HEAR_FC_fft_vmag_vmag,
    HEAR_HALF_FFT_SIZE,          // K = 256
    HEAR_ADDR_FFT_OUTPUT,        // 입력: Re/Im pairs
    HEAR_ADDR_VMAG_OUTPUT,       // 출력: magnitude 256개
    1, 1,
    VMAG_SINGLE_PRECISION        // 현재 설정
)
```

| 항목 | 내용 |
|---|---|
| 계산 | **√(Re² + Im²)** (에너지가 아닌 크기) |
| DOUBLE_PRECISION | 48비트, Q2.46 |
| **SINGLE_PRECISION (현재)** | **24비트, Q2.22** (DOUBLE 상위 추출) |
| SINGLE_PRECISION 게인 | **×(1/2)** (DOUBLE 대비) |
| 출력 배열 | 256개 magnitude |
| 인덱스 주의 | **[0] = DC (0 Hz), [1] = Nyquist (fs/2)** |

---

## 전체 게인 분석

```
입력 신호 크기: A  (시간 영역, 사인파 기준)

① FFT 변환         : × FFT_SIZE (= × 512)
② Real/Imag 분산   : × 1/2  (사인파 에너지가 양쪽 피크로 분산)
③ 해닝 윈도우      : × 1/2  (윈도우 함수에 의한 에너지 감소)
④ HEAR BFP         : × 1/4  (Ezario FFT 라이브러리 BFP 특성, 실측)
⑤ vMag SINGLE      : × 1/2  (DOUBLE 대비 상위 24비트만 사용)
─────────────────────────────────────────────
총 게인 = 512 × (1/2) × (1/2) × (1/4) × (1/2)
        = 512 / 32
        = 16
        = 2⁴
```

> [!IMPORTANT]
> **vMag 결과를 시간영역 크기로 환산**: `result >> 4`
>
> ```c
> // definitionsForAlgorithm.h
> #if (HEAR_FC_DFT_TYPE == HEAR_FC_DFT_TYPE_WINDOW)
> #define RIGHT_SHIFT_MAX_MAG_FREQ_SCALE 4   // 현재 설정 (윈도우 모드)
> #elif (HEAR_FC_DFT_TYPE == HEAR_FC_DFT_TYPE_NO_WINDOW)
> #define RIGHT_SHIFT_MAX_MAG_FREQ_SCALE 5
> #endif
>
> // FrequencyAnalysis.c — 채널 매핑 후 스케일 적용
> *scaled_ptr++ = (*freq_rep_ptr++) >> RIGHT_SHIFT_MAX_MAG_FREQ_SCALE;
> ```

---

## 단계별 Q 포맷 요약

| 단계 | 데이터 | Q 포맷 | 비트폭 | 비고 |
|---|---|---|---|---|
| DMIC 원본 | FIFO `HCT_A0_0[]` | **Q1.23** | 24 | Decimation Filter 출력 |
| 오디오 믹서 후 | `HEAR_ADDR_AUDIO_MIX` | **Q6.18** (유효) | 24 | `>> 5` 적용 |
| AGC Gain | `agc_gain_linear` | **Q12.12** | 24 | dB→선형 변환 후 |
| AGC 출력 | `m_agc_output_buffer[]` | **Q24.0** | 24 | INT24 클리핑 포함 |
| FFT 입력 | `HCT_A1_0[512]` | **Q24.0** | 24 | 역순 삽입 |
| 해닝 윈도우 | `HEAR_ADDR_FFT_WINDOW` | **Q1.23** | 24 | 256개 대칭 확장 |
| FFT 출력 (Re/Im) | `D_HEAR_C0MEM_BASE` | 22비트 유효 | 32 | BFP 정규화 |
| vMag 출력 | `HEAR_ADDR_VMAG_OUTPUT` | **Q2.22** | 24 | 256개, [0]=DC |
| 스케일 보정 후 | `g_freq_rep_values_scaled[]` | Q 미지정 | 24 | `>> 4` 적용, 시간영역 크기 |

---

## 최종 결과 해석

- vMag 256개 출력 중 **[0]과 [1]은 DC/Nyquist** — 가청 주파수 빈 아님
- 유효 주파수 빈: **[2] ~ [255]** (254개)
- `>> 4` 보정 후 값 ≈ 시간영역 원본 신호 크기 A에 근사
- 실제 사용 시: 256빈 → `g_pass_bin_index[]` 매핑으로 **32채널** 축소 → 로그 변환 → 자극값

> [!NOTE]
> **AGC가 적용된 경우**: 오디오 믹서 출력(Q6.18) 기준으로 AGC gain이 결정되므로, 입력 신호의 실제 레벨과 vMag 출력 크기의 절대 관계는 AGC 게인값에 따라 달라진다. 위 게인 분석은 AGC gain = 1 (0 dB)인 경우의 수식이다.
