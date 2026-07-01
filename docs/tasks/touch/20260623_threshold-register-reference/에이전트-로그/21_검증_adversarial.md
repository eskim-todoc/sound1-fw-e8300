---
name: 21_검증_adversarial
purpose: 조사 4종(코드매핑·DS공식·계수출처·데이터흐름) 전수 적대적 검증 — 인용 파일:줄·공식·코드 상수 직접 대조
type: 검증
maturity: experimental
tags: [touch, iqs323, adversarial-verify, threshold, ati, beta, register]
---

# 적대적 검증 — IQS323 임계·계수 레지스터 조사 4종 교차 대조

> **TL;DR**: 조사 4종의 파일:줄·DS 섹션·공식·코드 상수를 직접 Read로 대조. 핵심 기술 내용은 대부분 정확 확인. 치명 오류 1건(함수 주석 stale — 전 조사 미감지), 정정 필요 주석 2건 추가 발굴.

**검증 대상 파일 (직접 Read)**
- `src/2__cm3/Cortex-M3-src/systemControl/tdc_touch_iqs323.c`
- `src/2__cm3/Cortex-M3-src/systemControl/tdc_touch_iqs323.h`
- `src/2__cm3/Cortex-M3-src/systemControl/tdc_touch.c` (L.270~303)
- `src/2__cm3/Cortex-M3-src/main.c` (L.920~935)
- `docs/참고/touch/데이터시트/06_레지스터레퍼런스.md`
- `docs/참고/touch/데이터시트/02_proxfusion동작.md`

---

## 1. 코드 상수 값 대조

| 인용 | 조사 주장 | 실제 코드 | 판정 |
|---|---|---|---|
| h:46 `THRESHOLD` | `#define ... 80` | `#define TDC_TOUCH_IQS323_THRESHOLD 80` (L.46) | ✓ REAL |
| h:49 `HYSTERESIS` | `#define ... 8` | `#define TDC_TOUCH_IQS323_HYSTERESIS 8` (L.49) | ✓ REAL |
| h:52 `PROX_THRESHOLD` | `#define ... 255` | `#define TDC_TOUCH_IQS323_PROX_THRESHOLD 255` (L.52) | ✓ REAL |
| c:304 `REG_BETA_COUNTS` | `write_register(0xB0, 0x02, 0x02)` | L.304 그대로 | ✓ REAL |
| c:305 `REG_BETA_LTA_NORMAL` | `write_register(0xB1, 0x06, 0x06)` | L.305 `write_register(REG_BETA_LTA_NORMAL, 0x06, 0x06)` | ✓ REAL |
| c:306 `REG_BETA_LTA_FAST` | `write_register(0xB2, 0x02, 0x02)` | L.306 그대로 | ✓ REAL |
| c:307 `REG_FAST_FILTER_BAND` | `write_register(0xB4, 0x0A, 0x00)` | L.307 그대로 | ✓ REAL |
| c:308 `REG_CONV_FREQ` | `write_register(0x31, 0x7F, 0x05)` | L.308 `write_register(REG_CONV_FREQ, 0x7F, 0x05)` | ✓ REAL |
| c:310 `REG_SYSTEM_CONTROL` 초기 | `write_register(0xC0, 0x50, 0x07)` | L.310 그대로 | ✓ REAL |
| c:439 Re-ATI 트리거 | `write_register(0xC0, 0x54, 0x07)` | L.439 그대로 | ✓ REAL |
| c:445 RESEED 트리거 | `write_register(0xC0, 0x58, 0x07)` | L.445 그대로 | ✓ REAL |
| c:415 `REG_CH0_PROX` | `write_register(0x61, 255, 0x00)` | L.415 `write_register(REG_CH0_PROX, TDC_TOUCH_IQS323_PROX_THRESHOLD, 0x00)` → 255, 0x00 | ✓ REAL |
| c:429 `REG_SENSOR0_ATI_SETUP` | `write_register(0x36, 0x0C, 0x04)` | L.429 `write_register(REG_SENSOR0_ATI_SETUP, 0x0C, 0x04)` | ✓ REAL |

### 1.1 write_register(0x62, 0x50, 0x80) 비트 분해 검증

`touch_settings(80, 8)` 호출 경로 (c:284~289):
```
write_register(REG_CH0_TOUCH, threshold, (uint8_t)((hysteresis & 0x0F) << 4))
= write_register(0x62, 80, (8 & 0x0F) << 4)
= write_register(0x62, 0x50, 0x80)
```
- LSB=0x50=80 → DS A.17 bits[7:0]=Touch Threshold ✓
- MSB=0x80=1000_0000b → bits[7:4]=8 → register bits[15:12]=Touch Hysteresis=8 ✓
- DS A.17 확인: "bits[15:12] = Touch Hysteresis" ✓

**판정**: REAL — 조사 전 항목 정확.

---

## 2. 데이터시트 공식 대조

### 2.1 DS A.17 Touch Settings (0x62)

| 조사 인용 공식 | DS 실제 원문 (06_레지스터레퍼런스.md A.17) | 판정 |
|---|---|---|
| Touch Threshold = (Threshold × LTA) / 256 | "Touch Threshold = (Threshold × LTA) / 256" | ✓ REAL |
| Touch Hysteresis = (H/256) × Touch Threshold | "Touch Hysteresis = (Touch Hysteresis / 256) × Touch Threshold" | ✓ REAL |
| bits[7:0]=TOUCH_THRESHOLD, bits[15:12]=TOUCH_HYSTERESIS | A.17 테이블 동일 | ✓ REAL |

### 2.2 DS A.16 Prox Settings (0x61)

| 조사 인용 | DS 실제 (A.16) | 판정 |
|---|---|---|
| Prox Threshold "8-bit value" 뿐, 공식 없음 | bits[7:0]=Prox Threshold "8-bit value" (계수 공식 미기재) | ✓ REAL |
| Prox 진입식 `(LTA−Counts) > Prox Threshold` | DS §5.7: `(LTA - Counts) > Prox Threshold` | ✓ REAL |

### 2.3 DS A.12 ATI Setup (0x36)

| 조사 인용 | DS 실제 (A.12) | 판정 |
|---|---|---|
| 0x040C: bits[15:4]=64, bit3=1(Large Band), bits[2:0]=100=4(Full) | `bits[15:4]=ATI Resolution Factor → 0x040=64`, `bit3=1→Large(1/8)`, `bits[2:0]=100=Full` | ✓ REAL |
| ATI TARGET = ATI BASE × (Resolution Factor / 16) = 100 × 4 = 400 | "ATI TARGET = ACTUAL ATI BASE × (ATI Resolution Factor / 16)" | ✓ REAL |
| ATI Base 기본값 0x0064=100 (0x37 미기록) | §9 Memory Map: `0x37 ATI Base 기본값 0x0064` | ✓ REAL |
| Large ATI Band = 1/8 × ATI Target = 50 → Re-ATI boundary [350, 450] | A.12 "Large ATI Band = (1/8 × ATI TARGET)" | ✓ REAL |
| Re-ATI = LTA 기준, Counts/delta 무관 | DS §5.10 "LTA가 ATI Band 밖으로 drift할 때 실행" | ✓ REAL |

### 2.4 DS §5.6 Filter Betas

| 조사 인용 | DS 실제 (§5.6, 02_proxfusion동작.md) | 판정 |
|---|---|---|
| `Damping factor = Beta/256` | DS §5.6 수식: `Damping factor = Beta/256` | ✓ REAL |
| Fast Filter Band: sensing 반대방향 drift 시 fast beta 전환 | "counts가 LTA로부터 sensing 반대 방향으로 Fast Filter Band 이상 drift하면 fast filtering 적용" | ✓ REAL |
| self-cap 반대방향 = Counts > LTA (Fast 발동) | §5.7 self-cap: 터치 시 Counts 감소 → 반대 = Counts 증가 | ✓ REAL |

### 2.5 DS §5.5 LTA freeze

| 조사 인용 | DS 실제 (§5.5) | 판정 |
|---|---|---|
| touch·prox 이벤트 중 LTA 동결 | "touch·proximity 이벤트 중에는 frozen" | ✓ REAL |

### 2.6 DS §5.11 ATI Error

| 조사 인용 | DS 실제 (§5.11) | 판정 |
|---|---|---|
| ATI Error 후 자동 re-ATI 미발동, master가 수동 트리거 | "ATI Error 발생 시 re-ATI가 자동 트리거되지 않는다. master가 Re-ATI bit set으로 수동 트리거해야 함" | ✓ REAL |

---

## 3. System Status 비트 배치 대조

| 조사 인용 | 실제 코드 | DS A.2 | 판정 |
|---|---|---|---|
| CH0 Touch = bit9 = MSB bit1 | c:356 `(msb & (1u << 1))` 주석 "bit9 = MSB bit1" | bit9 = CH0 Touch ✓ | ✓ REAL |
| CH0 Prox = bit8 = MSB bit0 | c:357 `(msb & (1u << 0))` 주석 "bit8 = MSB bit0" | bit8 = CH0 Prox ✓ | ✓ REAL |
| ATI Error = bit6 = LSB bit6 | c:355 `(lsb & (1u << 6))` | bit6 = ATI Error ✓ | ✓ REAL |
| ATI Active = bit5 = LSB bit5 | c:354 `(lsb & (1u << 5))` | bit5 = ATI Active ✓ | ✓ REAL |

---

## 4. SW 계측 공식 대조

| 조사 인용 | 실제 코드 | 판정 |
|---|---|---|
| `abs_thr = (THRESHOLD × lta) / 256u` at tdc_touch.c:286 | L.286 `(uint32_t)TDC_TOUCH_IQS323_THRESHOLD * dbg.lta) / 256u` | ✓ REAL |
| 동일 공식 main.c:930 | L.930 동일 패턴 | ✓ REAL |
| `pabs_thr = (PROX_THRESHOLD × lta) / 256u` at tdc_touch.c:287 | L.287 `(uint32_t)TDC_TOUCH_IQS323_PROX_THRESHOLD * dbg.lta) / 256u` | ✓ REAL |
| 동일 공식 main.c:931 | L.931 동일 패턴 | ✓ REAL |

> [!NOTE]
> SW abs_thr/pabs_thr는 DS A.17 공식의 SW 미러링이나 **디버그 출력 전용** — 실제 터치 판정은 IC 하드웨어(System Status 비트). 조사 모두 이 점 명시. ✓

---

## 5. 치명 오류 및 미감지 사항

### 5.1 [치명] tdc_touch_iqs323.c:300 stale 주석 — 조사 4종 전부 미감지

```c
/* Beta(필터)·Power·Conversion write. ...
 * 현재 값 0xB1=0x0808·0xB2=0x0202 는 안전 기본값(99 §6 #1). */  ← L.300
static bool beta_power_settings(void) {
    ...
    ok &= write_register(REG_BETA_LTA_NORMAL, 0x06, 0x06);  ← L.305 실제 write
```

- **주석(L.300)**: "현재 값 0xB1=0x0808" → NP=8, LP=8을 주장
- **실제 write(L.305)**: `write_register(0xB1, 0x06, 0x06)` → NP=6, LP=6 = 0x0606
- **불일치**: 0x0808 ≠ 0x0606. 주석이 이전 값(Beta=8) 그대로 남은 stale 상태.
- **파급**: 주석을 신뢰하는 개발자가 "현재 NP/LP=8"로 오해. 조사 4종 모두 현재 write값=6은 정확히 추적했으나 이 모순은 아무도 지적하지 않음.
- **정정 필요**: L.300 주석 → "현재 값 0xB1=0x0606·0xB2=0x0202"

### 5.2 [주석 오류] tdc_touch_iqs323.h:49 "≈5cnt" — 조사 1이 불확실로 처리, 정확히는 오류

```c
#define TDC_TOUCH_IQS323_HYSTERESIS 8
/* Hysteresis 필드값(4비트 0~15, bits[15:12]).
 * 실제 hyst = (H/256) x Threshold ≈ 5cnt */  ← "5cnt" 주장
```

- 실계산: (8/256) × (80×400/256) = (8/256) × 125 = 3.906cnt ≈ **3.9cnt**
- "≈5cnt"는 ~28% 과대 기재. LTA=400 기준.
- 조사 1 불확실 섹션에 "헤더 주석 '≈5cnt' vs 실계산 ≈ 3.9cnt" 언급 ✓ (올바른 감지)
- 하지만 단순 "불확실"이 아닌 **주석 오류**: 계산이 명확히 3.9cnt를 지시.
- **정정 필요**: h:49 주석 → "실제 hyst ≈ 3.9cnt @ LTA=400"

### 5.3 [주석 오류] tdc_touch_iqs323.c:412 "동일 계수" — 조사 3이 명시, 나머지 미감지

```c
/* Prox Threshold = Touch Threshold 동일 계수 → prox 진입점을 touch 진입점과 일치시킨다.  ← L.412
 * ...*/
if (!write_register(REG_CH0_PROX, TDC_TOUCH_IQS323_PROX_THRESHOLD, 0x00))  ← L.415 실제 write
```

- 주석: "동일 계수" → PROX=THRESHOLD 동일값 암시
- 실제: PROX_THRESHOLD=255, THRESHOLD=80 — **다른 값**
- 설계 의도가 변경(prox 무력화)됐으나 주석이 구 의도(동일 계수) 그대로 잔존.
- 조사 3이 "[실측 게이트_4]"로 언급. 조사 1·2·4는 미감지.
- **정정 필요**: L.412 주석 → "Prox Threshold = 255 (prox 사실상 무력화, touch만 LTA freeze 담당)"

---

## 6. 불확실 항목 평가 (조사의 [실측 게이트] 표기 적절성)

| 불확실 항목 | 조사 처리 | 검증자 평가 |
|---|---|---|
| Beta IIR 공식: DS Beta/256 vs AZD004 1/2^β | 4종 모두 [실측 게이트] 명시 | ✓ 적절 — DS §5.6 원문은 Beta/256 공식을 명시하나 코드 주석이 AZD004 기준 택했음을 명시. IC 실제 구현 확인 필요. Beta=1 실측 시 두 공식 차이 극명(0.39% vs 50%/tick)으로 1회 측정으로 판별 가능. |
| Prox Threshold 단위 (절대 vs ×LTA/256) | 4종 모두 [실측 게이트] 명시 | ✓ 적절 — DS A.16이 "8-bit value"만 기재, §5.7 진입식도 LTA/256 스케일 없음. 절대값이 유력하나 DS 미명시. 255 설정은 어느 해석에서도 무력화 동등. |
| ATI Base 기본값(0x37 미기록) | 2·3종이 명시 | ✓ 적절 — DS §9 기본값 0x0064 확인되나 POR 후 실제 보존 여부는 레지스터 덤프 필요. |
| IC 측정 주기(0xC1=0) | 3종이 언급 | ✓ 적절 — τ 계산의 "IC 주기=200ms" 가정이 실측 미확인. |

---

## 7. 종합 판정

| 카테고리 | 건수 | 결과 |
|---|---|---|
| 코드 상수·라인 번호 정확 | 14건 | 전부 REAL — 오류 0 |
| DS 공식 정확 인용 | 9건 | 전부 REAL — 오류 0 |
| System Status 비트 배치 | 4건 | 전부 REAL — 오류 0 |
| SW 계측 공식 | 4건 | 전부 REAL — 오류 0 |
| [치명] stale 주석 (조사 전체 미감지) | 1건 | c:300 0x0808≠실제 0x0606 |
| [주석 오류] 조사 일부 미감지 | 2건 | h:49 ≈5cnt→3.9cnt, c:412 동일계수→개별값 |
| [실측 게이트] 적절 표기 | 4건 | Beta공식·Prox단위·ATI Base·IC주기 |

> [!IMPORTANT]
> **조사 4종의 기술적 핵심(레지스터 값·공식·DS 인용)은 모두 정확**. 단 `tdc_touch_iqs323.c:300`의 함수 선두 주석이 "0xB1=0x0808"을 주장하나 실제 write는 0x0606 — 조사 4종 **전부 미감지**한 치명 stale 주석. 코드 리뷰 시 이 주석을 신뢰하면 Beta NP/LP=8로 오인할 수 있다.

---

## 8. 정정 목록 (코드 반영 필요)

1. **`tdc_touch_iqs323.c:300`** — "현재 값 0xB1=0x0808" → "현재 값 0xB1=0x0606"으로 수정
2. **`tdc_touch_iqs323.h:49`** — "실제 hyst ≈ 5cnt" → "실제 hyst ≈ 3.9cnt @ LTA=400"으로 수정
3. **`tdc_touch_iqs323.c:412`** — "Prox Threshold = Touch Threshold 동일 계수 → prox 진입점을 touch 진입점과 일치시킨다" → "Prox Threshold = 255(prox 사실상 무력화) → LTA freeze는 touch 이벤트만 담당"으로 수정
