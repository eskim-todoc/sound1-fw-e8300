---
name: 검증 — adversarial 정합 (페르소나 21)
purpose: 분석 4종(01~04) + 계획(11)의 할루시네이션·논리 구멍을 데이터시트·코드 대조로 색출
type: tasks/에이전트-로그
applies_to: [Sound1]
maturity: stable
tags: [touch, iqs323, adversarial-verify, register, bit-mapping, sleep-ati, 상태:완료]
---

# 검증 — adversarial 정합 (페르소나 21)

**TL;DR**: 분석/계획 주장 총 13종 검증. real 8 · refuted 2 · uncertain 3. 치명 이슈 2건 확인: (1) 분석 04의 `over_band` 수식 오류 → 항상 OVER 표시(분석 02 수식이 정합), (2) 계획 11의 `grep -c ci_printe` 검증 임계값 오류(13이 아닌 18). 절전 ATI 정합 핵심 로직·레지스터 비트·디바운스 설계는 전반적으로 정합.

---

## 검증 방법

모든 claim을 코드 Read·grep·Python 계산으로 대조. 데이터시트: `06_레지스터레퍼런스.md`, `02_proxfusion동작.md`. 코드: `tdc_touch_iqs323.c/.h`, `tdc_touch_logic.c/.h`, `tdc_touch.c`, `main.c`, `tdc_touch_time.h`.

---

## Claim별 검증 결과

### C1 — threshold=255 → 절전 중 터치 감지 실질 불가

**주장 (분석 01)**: `apply_sleep_settings()` 내 `touch_settings(255, 255)`가 절대임계를 LTA 수준(≈LTA×255/256)으로 끌어올려 손가락 delta로는 절대 초과 불가. ULP 루프 `tdc_touch_get_state()` → 항상 NOT_TOUCH.

**판정: real**

**근거**:
- 코드 `tdc_touch_iqs323.c:413` 확인: `touch_settings(255, 255)` 호출.
- `write_register(0x62, lsb=255, msb=255)` → 16-bit `0xFFFF` → bits[7:0]=255(Threshold), bits[15:12]=15(Hyst).
- ATI Target ≈ 400 counts일 때: `abs_threshold = 255×400/256 = 398 counts`. Touch 조건 `(LTA - Counts) > 398` → counts가 2 이하여야 충족. 실기 불가.
- `tdc_touch_get_state()` → `tdc_touch_iqs323_read_status()` → System Status bit9(CH0 Touch)를 읽음. IC가 Touch bit를 set하지 않으면 항상 NOT_TOUCH 반환. `06_레지스터레퍼런스.md:397`: `Touch Threshold = (Threshold × LTA) / 256` 공식 일치.
- ULP 루프 `main.c:922-930` 롱터치 경로는 `tdc_touch_get_state()` 반환 state == TOUCH 에 의존 → threshold=255 상태에서 동작 불가 모순 확인.

---

### C2 — apply_sleep_settings()가 0xC0=0x0700(Power=Normal)을 쓴다

**주장 (분석 01)**: `0xC0=0x0700은 Power Mode를 Normal로 고정`.

**판정: real**

**근거**:
- 코드 `tdc_touch_iqs323.c:418`: `write_register(REG_SYSTEM_CONTROL, 0x00, 0x07)` → LSB=0x00, MSB=0x07.
- `A.30 System Control (0xC0)` — `06_레지스터레퍼런스.md:516`: LSB bits[6:4] = Power Mode. `0x00 bits[6:4]=000` = Normal.
- MSB=0x07 → bits[10:8]=111 → CH0/CH1/CH2 Timeout Disable.
- 분석 01 표현 `0x0700`은 16-bit 표기(MSB=0x07, LSB=0x00)로 일치.

---

### C3 — reseed()가 0xC0에 AutoNoULP(bits[6:4]=101)을 쓰며 apply_sleep_settings() Normal 설정을 덮는다

**주장 (분석 01)**: `reseed()` = `write_register(REG_SYSTEM_CONTROL, 0x58, 0x07)` → LSB=0x58=bits[6:4]=101(AutoNoULP)+bit3=1(Reseed). apply_sleep_settings()의 Normal(000)을 덮음.

**판정: real**

**근거**:
- 코드 `tdc_touch_iqs323.c:383`: `write_register(REG_SYSTEM_CONTROL, 0x58, 0x07)` 확인.
- `0x58 = 0b01011000`: bits[6:4]=101=Automatic No ULP, bit3=1(Reseed), bit[7]=0(Streaming 유지).
- `main.c:881` `apply_sleep_settings()` 호출 후 `main.c:889` `reseed()` 호출 순서 확인.
- 분석 01 표기 `0xC0←0x0758`는 16-bit 표기 `(0x07<<8)|0x58 = 0x0758` 로 일치.
- Power Mode 덮임: apply_sleep_settings Normal(000) → reseed AutoNoULP(101). 최종 IQS323 Power Mode = AutoNoULP. 분석 01 결론 correct.

---

### C4 — 0x36(ATI Setup)은 apply_sleep_settings()에서 변경 없어 Full ATI 모드 보존

**주장 (분석 01)**: `apply_sleep_settings()`가 0x36을 건드리지 않아 Full ATI 모드(bits[2:0]=100) 그대로 보존.

**판정: real**

**근거**:
- `tdc_touch_iqs323.c:409~425` 전체 `apply_sleep_settings()` 함수: `REG_CH0_TOUCH(0x62)`와 `REG_SYSTEM_CONTROL(0xC0)` 두 레지스터만 기록. 0x36 write 없음.
- `tdc_touch_iqs323.c:349`: `write_register(REG_SENSOR0_ATI_SETUP, 0x0C, 0x04)` → LSB=0x0C=0b00001100: bits[2:0]=100=Full, bit3=1=Large ATI Band(1/8). `06_레지스터레퍼런스.md:346` A.12 일치.
- Full ATI 모드 = IC가 MULT/COMP 자동 산출. 절전 후에도 ATI Target·Band 기준 유효.

---

### C5 — threshold=255 제거 1줄이면 운용 임계(16) 그대로 절전 감지 가능, 추가 Re-ATI 불필요

**주장 (분석 01)**: `apply_sleep_settings()`의 `touch_settings(255,255)` 1줄 제거 시 `apply_settings()`에서 기록한 0x62=0x0810(threshold=16, hysteresis=8)이 절전 중에도 유지.

**판정: real**

**근거**:
- `apply_settings()` → `touch_settings(TDC_TOUCH_IQS323_THRESHOLD, TDC_TOUCH_IQS323_HYSTERESIS)` = `touch_settings(16, 8)` (`tdc_touch_iqs323.c:341-344`, `tdc_touch_iqs323.h:46-49`).
- `apply_sleep_settings()`가 이를 255,255로 덮어쓰는 것이 유일한 임계 변경.
- 제거 시 threshold=16 유지. ATI 결과(MULT/COMP) 또한 유지.
- 추가 Full ATI 재실행 불필요: IC는 절전 진입 시 기존 ATI 결과를 그대로 사용함. 데이터시트 §5.10: ATI는 LTA가 ATI Band 밖으로 drift할 때만 재발동.

> [!NOTE]
> 오탐 증가 위험은 별도 실측으로 확인 필요. 이는 분석 01이 명시한 위험이고, 이번 검증 범위 내에서 반론 불가.

---

### C6 — 0x13=Channel 0 Filtered Counts, 0x14=Channel 0 LTA (read-only)

**주장 (분석 02)**: 레지스터 주소 0x13 = CH0 Filtered Counts, 0x14 = CH0 LTA.

**판정: real**

**근거**:
- `06_레지스터레퍼런스.md:27-30` Memory Map 주소표:
  - `0x13 | Channel 0 Filtered Counts | - | 16-bit value`
  - `0x14 | Channel 0 LTA | - | 16-bit value`
- Read-only 섹션(System Information)에 위치. 기존 코드에 REG_CH0_COUNTS·REG_CH0_LTA #define 없음(`tdc_touch_iqs323.c:17-32` 전체 #define 목록 확인).

---

### C7 — 절대임계 공식 threshold×LTA/256

**주장 (분석 01, 02)**: 절대 터치임계 = `threshold × LTA / 256`.

**판정: real**

**근거**:
- `06_레지스터레퍼런스.md:398` A.17: `Touch Threshold = (Threshold × LTA) / 256`. 공식 일치.
- `tdc_touch_iqs323.h:42`: 주석 `절대 = 계수 x LTA / 256`. 코드 자체 주석도 일치.

---

### C8 — 분석 02의 over_band 수식: `(counts > abs_threshold)` — 오류

**주장 (분석 04)**: `bool over_band = (counts > abs_threshold)`. 분석 02의 BAND 마커 기준으로 통합 제안.

**판정: refuted (치명 버그)**

**근거**:
- `abs_threshold`는 counts-difference 단위 (LTA - Counts 공간). typical abs_threshold = 25 counts-difference.
- 실기 counts는 LTA ≈ 400 주변 → `counts ≈ 395` (noTouch). `395 > 25 = true` → 항상 BAND 표시 (오탐).
- 터치 조건(`02_proxfusion동작.md:406`): `(LTA - Counts) > Touch_Threshold(abs)`.
- 올바른 over_band: `(delta > abs_threshold)` where `delta = (lta > counts) ? (lta - counts) : 0`.
- **분석 02의 over_band 공식**: `(delta > abs_thr)` → 정합.
- **분석 04의 over_band 공식**: `(counts > abs_threshold)` → 오류. 두 분석이 서로 다른 공식 제안.

> [!IMPORTANT]
> **치명 이슈**: 계획 구현 시 분석 04의 공식을 사용하면 `[TOUCH] LTA=... !OVER!`가 항상 표시되어 계측 로그가 무의미. 반드시 분석 02의 `delta > abs_thr` 공식을 사용해야 한다.

---

### C9 — 분석 04의 ci_printe 총 13건 보존 주장

**주장 (분석 04)**: `tdc_touch_iqs323.c`의 `ci_printe` 총 13건 전부 유지.

**판정: refuted (수량 오류, 보존 원칙은 유효)**

**근거**:
- `grep -c ci_printe tdc_touch_iqs323.c` = **18건** (코드 직접 확인).
- 분류: 저수준 I2C 통신 실패 7건(`:145,164,169,184,189,196,201`) + `apply_settings()` 실패 9건(`:338~367`) + `apply_sleep_settings()` 실패 2건(`:415,420`) = 18.
- 분석 04는 18개 항목을 목록으로 나열하면서 표제에 "13건"이라 기재 → **내부 불일치**.
- 보존 원칙(기존 치명 printe 전부 유지) 자체는 요구사항과 일치하며 올바름.

> [!IMPORTANT]
> **치명 이슈**: 계획 11 verify_items 항목: `grep -c 'ci_printe' tdc_touch_iqs323.c → 기존 13건 + READ_DEBUG 2건 추가`. 실제 기준값은 18이므로 이 검증이 실패한다. 올바른 검증: **18 + 신규 2 = 20**.

---

### C10 — 부팅 디바운스 boot_release_cnt 추가 로직 정합성

**주장 (분석 03)**: `logic.h` 상태구조체에 `uint8_t boot_release_cnt` 추가, step() boot_ignore 블록 3분기 교체. logic.c 순수성 유지.

**판정: real**

**근거**:
- `tdc_touch_logic.h:64-75` 상태구조체 현황 확인. 신규 필드 추가 공간 있음.
- `TDC_TIME_TO_CNT(400, 200) = ceil(400/200) = 2` (Python 계산 일치).
- boot_ignore 블록 early return(`logic.c:116-137`) → gate/stuck/롱터치에 영향 없음. 분기 교체는 해당 블록 내부에만 국한.
- logic.c include: `tdc_touch_logic.h + tdc_touch_time.h` 뿐 (`logic.c:12-13`). ci_printf.h 미포함 → 순수성 유지.
- `logic.h:6-9` 순수성 게이트 주석 명시: `hw.h/ci_timer.h/ci_printf.h/LedOutput.h/tdc_touch_iqs323.h 절대 include 금지`.

---

### C11 — tdc_touch.c:148 ATI printw 삽입 위치 정합성

**주장 (분석 04)**: `tdc_touch.c:148 in.ati_active 대입 직후·logic_step 호출 전`에 `if (in.read_ok && in.ati_error && !in.ati_active)` printw 삽입.

**판정: real**

**근거**:
- `tdc_touch.c:144-148` 순서: `read_ok(144) → now_ms(145) → pressed(146) → ati_error(147) → ati_active(148)`. 148 이후 in 구조체 완성.
- `tdc_touch.c:152`: `tdc_touch_logic_step()` 호출. 148→152 사이 삽입 가능.
- 조건 `in.read_ok && in.ati_error && !in.ati_active`는 `logic.c:140-142` Re-ATI 게이트 조건(`!ati_active && ati_error`)과 동일. 자연스러운 동반 출력.

---

### C12 — Re-ATI 원인 구분(curr_state 기반) 정합성

**주장 (분석 04)**: `out.curr_state == NOT_TOUCH` → 드리프트 게이트, `TOUCH` → stuck stage1. 두 경로 상호 배타.

**판정: real**

**근거**:
- `logic.c:139-146`: 게이트 Re-ATI 조건 `curr_state == NOT_TOUCH`.
- `stuck_eval() logic.c:20-25`: `curr_state != TOUCH → 리셋, ACT_NONE 반환`. stage 진입 자체가 TOUCH 전용.
- 동일 step에서 curr_state는 단일 값 → 두 경로 동시 발동 불가. 상호 배타 확인.
- `out.action` 덮어쓰기 충돌도 없음: gate(NOT_TOUCH) 이후 stuck_eval 실행 시 NOT_TOUCH → stuck_eval returns NONE → action 유지.

---

### C13 — Beta 해석 (DS vs AZD004) 불일치

**주장 (오케스트레이터 입력 §레지스터/Beta)**: DS: `Beta/256 = 클수록 빠름`. AZD004: `2^β = 클수록 느림`. 정반대.

**판정: uncertain (실측 게이트)**

**근거**:
- `02_proxfusion동작.md:358`: `Damping factor = Beta/256`. 해설(§5.6): "Beta가 클수록 빠르게 변화를 추적한다" → DS 해석 확인.
- AZD004 해석은 본 검증 범위의 데이터시트에 없음 → 직접 대조 불가.
- 현재 Beta=8: DS 해석 기준 alpha=3.1%(중간), AZD004 기준 2^8 감쇠(매우 느림). 두 해석 방향이 반대이므로 실측 필수.
- 이 불일치는 기존 [실측 게이트]로 명시된 사항. 이번 계획 범위 외.

---

## 치명 이슈 (critical_issues)

### 이슈 1 — over_band 수식 오류 (분석 04 제안 코드)

- **위치**: 계획 11 P3, `tdc_touch.c` 계측 printd 블록
- **내용**: 분석 04의 `bool over_band = (counts > abs_threshold)` 는 counts(≈400)를 abs_threshold(≈25)와 비교 → 항상 true → BAND 마커가 항상 `!OVER!`
- **수정**: 반드시 분석 02의 `(delta > abs_thr)` 공식 사용. `delta = (lta > counts) ? (lta - counts) : 0u`

### 이슈 2 — 계획 11 ci_printe 검증 임계값 오류

- **위치**: 계획 11 verify_items 항목: `grep -c 'ci_printe' tdc_touch_iqs323.c → 기존 13건 + READ_DEBUG 2건 추가`
- **내용**: 실제 기준 18건(grep 직접 확인). 13으로 체크하면 검증 항목이 실패 또는 의미 없음.
- **수정**: `기존 18건 + 신규 2건 = 20건` 으로 정정.

---

## 부록 — 주요 레지스터 비트 대조표

| 레지스터 | 필드 | 코드값 | DS 근거 | 판정 |
|---|---|---|---|---|
| 0x10 System Status bit9 | CH0 Touch | `msb & (1u<<1)` | A.2: bit9=CH0 Touch, MSB=byte1 | 일치 |
| 0x10 bit6 | ATI Error | `lsb & (1u<<6)` | A.2: bit6=ATI Error, LSB=byte0 | 일치 |
| 0x10 bit5 | ATI Active | `lsb & (1u<<5)` | A.2: bit5=ATI Active | 일치 |
| 0x36 bits[2:0] | ATI Mode | 0b100=4=Full | A.12: 100=Full | 일치 |
| 0x36 bit3 | ATI Band | 1=Large(1/8) | A.12: 1=Large | 일치 |
| 0x62 bits[7:0] | Touch Threshold | 16 | A.17: bits[7:0]=Threshold | 일치 |
| 0x62 bits[15:12] | Touch Hysteresis | 0 (intended 8, bits[11:8]에 배치됨) | A.17: bits[15:12]=Hyst | 비트 배치 불일치 — [실측 게이트] |
| 0xC0 bits[6:4] | Power Mode | 000(Normal) 후 101(AutoNoULP) 덮임 | A.30: 101=Automatic No ULP | 일치 |
| 0xC0 bit3 | Reseed | 1 (reseed 시) | A.30: bit3=Reseed | 일치 |
| 0xC0 bit2 | Re-ATI | 1 (re_ati 시) | A.30: bit2=Re-ATI | 일치 |

---

*페르소나 21 (검증_adversarial정합) — 2026-06-22*
