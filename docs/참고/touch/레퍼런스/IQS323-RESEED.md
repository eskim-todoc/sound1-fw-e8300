---
name: IQS323-RESEED
purpose: IQS323 터치 감지 핵심 개념(counts·LTA·delta·count drift·RESEED) 상세 설명 — Sound1 동작 원리 이해용 1차 참고
type: 참고
maturity: stable
tags: [iqs323, touch, reseed, lta, calibration]
---

# IQS323 RESEED

**TL;DR**: counts = 정전용량 측정값, LTA = 비터치 기준선(지수 이동 평균), delta = counts − LTA. delta > THRESHOLD이면 터치 판정. Count drift는 ESD 등으로 counts가 기준값을 벗어나는 현상. RESEED는 LTA ← current_counts로 강제 초기화하는 트리거 — 반드시 비터치 상태에서 실행.

---

## 1. 배경 — 핵심 개념 정의

### 1.1 Counts (카운트)

IQS323이 터치 전극의 정전용량을 측정해 **숫자로 변환한 값**이다. 단위는 없으며, 정전용량이 클수록 counts가 크다.

```
물리적 정전용량 (pF)  →  [IQS323 내부 변환]  →  counts (정수)
```

- 비터치 상태에서도 전극 고유의 기생 정전용량 때문에 counts = 0이 아닌 어떤 값을 가진다.
- 손가락을 대면 전극과 손가락 사이 정전용량 증가 → counts 증가.
- ATI 캘리브레이션 후 비터치 기준 counts ≈ **ATI_TARGET(512)**로 맞춰진다.
- MULT/COMP 보상값이 counts 스케일을 결정한다. MULT가 크면 같은 물리 정전용량도 더 큰 counts로 읽힌다.

```
counts 예시 (Sound1, 절전 MULT=0x5C82):
  비터치 상태: ~0x5C82 (약 400~500 범위)
  터치 상태:   ~0x6282~0x6682 (약 100~200 더 높음)
```

---

### 1.2 LTA (Long-Term Average, 장기 이동 평균)

**counts의 장기 평균값**으로, 터치 판정의 기준선 역할을 한다.

IQS323 내부에서 지수 이동 평균(exponential moving average)으로 계산된다:

```
LTA_new = LTA_old + (counts - LTA_old) × (1/2^BETA)
```

- `BETA`가 클수록 LTA가 느리게 움직인다 (환경 변화에 둔감, 드리프트에 강함).
- **비터치 상태**: counts ≈ LTA이므로 LTA는 천천히 counts를 따라 움직인다.
- **터치 상태**: LTA 업데이트가 일시 정지된다. 손가락을 댄 동안의 높은 counts가 LTA에 반영되지 않는다.

> [!NOTE]
> LTA가 느린 환경 변화(온도·습도·노화)를 자동으로 흡수하는 것이 핵심 역할이다. 계절이 바뀌거나 습도가 달라져도 기준선이 자동 이동하므로 재캘리브레이션이 불필요하다.

---

### 1.3 Delta (델타)

**delta = current_counts − LTA**

현재 counts와 LTA의 차이. 이 값으로 터치 여부를 판정한다.

```
비터치 상태:  counts ≈ LTA  →  delta ≈ 0
터치 상태:    counts > LTA  →  delta > 0
터치 판정:    delta > THRESHOLD          (손 댐)
터치 해제:    delta < −HYSTERESIS        (손 뗌)
```

Sound1 설정값:
- 노말 모드: THRESHOLD = 80, HYSTERESIS = 80
- 절전 모드: THRESHOLD = 30, HYSTERESIS = 30

HYSTERESIS를 두는 이유는 판정 경계선에서의 떨림(채터링)을 방지하기 위해서다.

```
delta 값 흐름 예시:
  비터치  →   0
  터치 시작 →  THRESHOLD 초과 → 터치 판정
  손 뗌  →   0 방향으로 감소
  −HYSTERESIS 미만 → 터치 해제
```

---

### 1.4 Count Drift (카운트 드리프트)

**실제 터치 없이 counts가 기준값에서 벗어나는 현상.** LTA가 따라오지 못하면 delta 오류가 생긴다.

#### 느린 드리프트 (Slow Drift) — 정상 처리됨

원인: 온도·습도 변화, 제품 노화
```
counts가 서서히 상승 → LTA도 천천히 따라 상승 → delta ≈ 0 유지
```
LTA의 설계 목적 자체가 이 드리프트를 흡수하는 것이므로 정상 동작이다.

#### 급격한 드리프트 (Fast Drift) — 문제 발생

원인: ESD, 전원 순환, 클럭/MULT 변경
```
counts가 갑자기 급등 → LTA가 즉시 따라오지 못함 → delta 급증 → 오판정
```

경미한 경우: LTA가 수십 ms~수백 ms 이내에 수렴 → 일시적 오판 후 자연 회복.

**심각한 경우 (Sound1 ESD 문제)**:
```
ESD 누적 → 전극 기생 정전용량 실질 증가 → counts가 항상 높게 유지
         → delta 항상 > THRESHOLD → 항시 터치 판정 → 먹통
```

| 드리프트 종류 | 속도 | LTA 추적 | 결과 |
|---|---|---|---|
| 온도·습도 | 분~시간 단위 | 충분히 따라옴 | ✅ 자동 보정 |
| MULT 변경 | 즉시 | 따라오지 못함 | ⚠️ RESEED 필요 |
| 경미한 ESD | 빠름 | 수백 ms 내 수렴 | ⚠️ 일시 오판 |
| ESD 누적 (심각) | 지속 | 영원히 따라오지 못함 | ❌ 먹통 → SW 해결 불가 |

> [!IMPORTANT]
> **ESD 누적 먹통 상태에서 RESEED가 무효한 이유**: RESEED는 LTA ← current_counts로 즉시 초기화한다. 그러나 ESD로 인한 counts 상승이 지속되면 RESEED 직후에도 counts = LTA로 시작하지만, 즉시 다시 counts > LTA가 된다. 비터치 구간이 없으므로 어떤 타이밍에 RESEED를 해도 근본 문제(전극 정전기 누적)를 해결하지 못한다.

---

## 2. 터치 감지 전체 흐름

```
전극 정전용량 측정
       ↓
   counts 계산 (MULT/COMP 적용)
       ↓
   delta = counts - LTA
       ↓
   delta > THRESHOLD ?
     YES → 터치 판정
     NO  → 비터치
       ↓
   비터치 상태이면: LTA를 counts 방향으로 서서히 이동 (느린 드리프트 흡수)
```

---

## 3. RESEED란

> **현재 counts 값을 LTA로 강제 덮어쓰는 트리거.**

```
RESEED 실행 순간: LTA ← current_counts
결과: delta = current_counts - LTA = 0
```

하드웨어 동작이며, SW는 트리거만 한다.

```c
/* 트리거 방법: SYSTEM_CONTROL(0xC0).LSB.reseed = 1 */
write_register(0xC0, 0x08, 0x00);
```

레지스터 쓰기 후 IC가 1클럭 내에 처리하며, 비트는 자동 클리어된다.

---

## 4. RESEED가 필요한 상황

### 3.1 ATI 보상값(MULT/COMP) 변경 후

MULT는 counts의 스케일 배율이다.

```
processed_counts = f(raw_counts, MULT, COMP)
```

MULT를 바꾸면 같은 물리적 정전용량에서도 counts 값이 달라진다. 기존 LTA는 이전 MULT 기준이므로 **delta가 의도치 않게 크거나 작아진다**.

RESEED로 LTA를 새 MULT 기준 counts로 재초기화해야 delta = 0에서 시작한다.

**Sound1 예시:**
- 노말 모드 MULT = 0x5E82 → RESEED → LTA 고정
- 절전 모드 MULT = 0x5C82 (값 변경) → **RESEED 필요**

### 3.2 하드웨어 환경 급변 후

절전 진입 시 발생하는 변화:
- 주변 소자 OFF (자석, 배터리 IC, 충전 IC)
- SYSCLK 30.72 MHz → 2.56 MHz (클럭 다운)
- I2C 속도 변경

이런 변화는 IQS323 인근 정전용량 환경을 바꾸므로 새 환경 기준으로 LTA 재설정이 필요하다.

---

## 5. 터치 중 RESEED 시 발생하는 문제

```
[잘못된 순서]

1. 터치 상태 (counts = 600, LTA = 500, delta = 100 > THRESHOLD → 터치)
2. RESEED 실행 → LTA ← 600
3. 손 뗌 → counts = 500, LTA = 600
   delta = 500 - 600 = -100 (음수)
4. 이후 다시 터치 → counts = 600, LTA = 600
   delta = 0 → 터치 미감지 ← 문제
```

**결과**: RESEED 이후 손을 뗐다 다시 터치해도 delta가 0에 가까워 감지 불가. LTA가 서서히 500으로 돌아와야 다시 정상 동작하는데, 이 수렴 시간 동안 터치가 무시된다.

---

## 6. RESEED vs ATI

| 항목 | RESEED | ATI |
|---|---|---|
| 역할 | LTA를 현재 counts로 즉시 고정 | MULT/COMP를 조정해 counts를 ATI_TARGET(≈512)으로 수렴 |
| 트리거 | SW 명시적 트리거 (0x08 쓰기) | SW 명시적 또는 Power-On 자동 |
| 소요 시간 | 즉시 (1클럭) | 수십~수백 ms (하드웨어 알고리즘 수렴) |
| 변경 대상 | LTA | MULT, COMP |
| 사용 목적 | 환경 기준선 재설정 | 민감도 자동 캘리브레이션 |

> **Sound1 현재 방침**: autoATI 사용 안 함. MULT/COMP 고정 값 사용 + RESEED로 LTA만 재설정.

---

## 7. Sound1 코드에서의 RESEED

### 6.1 노말 모드 (`apply_settings()`)

```c
write_ati_compensation();  // MULT=0x5E82, COMP=0x63E4 고정 쓰기
// ...
write_register(0xC0, 0x08, 0x00);  // RESEED
```

MCLR 리셋 + Auto-ATI 완료 후 호출하므로, 이 시점에 터치 상태일 수 있다.
→ READY 전이 직후 `s_boot_touch_ignore` 플래그로 부팅 터치 무시 처리.

### 6.2 절전 모드 — 2단계 분리 처리

절전 진입 흐름은 다음 두 단계로 분리한다.

**단계 1 — `tdc_drv_iqs323_apply_sleep_settings()` (ci_power_sleep() 전)**

```c
/* 절전용 THRESHOLD/HYSTERESIS, ATI_MULT/COMP, CH_TIMEOUT 기록.
 * 정상 클럭(30.72 MHz) + 정상속도 I2C에서 확실히 쓴다.
 * RESEED는 아직 하지 않는다. */
tdc_drv_iqs323_apply_sleep_settings();
```

**단계 2 — ci_power_sleep() 이후 (main.c)**

```c
ci_power_sleep();               /* SYSCLK 30.72M → 2.56M, 주변기기 OFF */
i2c_set_master_prescale(...);   /* 저속 클럭 맞게 I2C 속도 조정 */

/* 절전 환경(주변기기 OFF·클럭 다운)이 확정된 후:
 * 1. 실제 터치 해제 대기 (사용자 손가락이 아직 닿아 있으면 기다림)
 * 2. RESEED — 이 환경의 counts를 LTA 기준으로 확정 */
{
    bool pressed, ati_error;
    while (tdc_drv_iqs323_read_status(&pressed, &ati_error) && pressed)
    {
        SYS_WATCHDOG_REFRESH();
    }
}
tdc_drv_iqs323_reseed();        /* LTA ← 절전 환경 counts, delta = 0 */
```

**분리 이유**: MULT/COMP를 ci_power_sleep() 전에 적용하면 정상 환경(빠른 클럭, 정상 I2C)에서 레지스터 기록이 보장된다. 이후 ci_power_sleep()으로 주변기기가 꺼지면서 전극 환경(기생 정전용량)이 바뀌므로, RESEED는 이 변화 후에 해야 올바른 LTA 기준이 설정된다.

---

## 8. 요약

```
RESEED를 안 하면  → 환경 변화 후 LTA가 틀어져 delta 오류
RESEED를 터치 중에 하면 → LTA가 터치 값으로 고정되어 이후 터치 미감지
RESEED를 비터치 상태에서 하면 → delta = 0 기준 재설정, 정상 동작
```
