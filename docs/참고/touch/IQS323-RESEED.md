---
name: IQS323-RESEED
purpose: IQS323 RESEED 동작 원리 — LTA 강제 초기화 메커니즘과 올바른 사용 조건 설명
type: 참고
maturity: stable
tags: [iqs323, touch, reseed, lta, calibration]
---

# IQS323 RESEED

**TL;DR**: RESEED는 현재 counts를 LTA(터치 기준선)로 강제 고정하는 트리거. 반드시 비터치 상태에서 실행해야 하며, ATI 보상값(MULT/COMP) 변경 후 환경 재기준화에 필수.

---

## 1. 배경 — 터치 감지 구조

IQS323은 정전용량 변화를 **counts** 값으로 읽는다.

```
터치 판정: delta > THRESHOLD
터치 해제: delta < -HYSTERESIS

여기서 delta = current_counts - LTA
```

**LTA(Long-Term Average)**: 비터치 상태의 정전용량 기준선. 온도·습도 같은 느린 환경 변화를 자동 추적한다.

터치가 없는 평상시 `delta ≈ 0`, 손을 대면 counts가 오르며 `delta > THRESHOLD`에서 터치로 판정한다.

---

## 2. RESEED란

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

## 3. RESEED가 필요한 상황

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

## 4. 터치 중 RESEED 시 발생하는 문제

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

## 5. RESEED vs ATI

| 항목 | RESEED | ATI |
|---|---|---|
| 역할 | LTA를 현재 counts로 즉시 고정 | MULT/COMP를 조정해 counts를 ATI_TARGET(≈512)으로 수렴 |
| 트리거 | SW 명시적 트리거 (0x08 쓰기) | SW 명시적 또는 Power-On 자동 |
| 소요 시간 | 즉시 (1클럭) | 수십~수백 ms (하드웨어 알고리즘 수렴) |
| 변경 대상 | LTA | MULT, COMP |
| 사용 목적 | 환경 기준선 재설정 | 민감도 자동 캘리브레이션 |

> **Sound1 현재 방침**: autoATI 사용 안 함. MULT/COMP 고정 값 사용 + RESEED로 LTA만 재설정.

---

## 6. Sound1 코드에서의 RESEED

### 6.1 노말 모드 (`apply_settings()`)

```c
write_ati_compensation();  // MULT=0x5E82, COMP=0x63E4 고정 쓰기
// ...
write_register(0xC0, 0x08, 0x00);  // RESEED
```

MCLR 리셋 + Auto-ATI 완료 후 호출하므로, 이 시점에 터치 상태일 수 있다.
→ READY 전이 직후 `s_boot_touch_ignore` 플래그로 부팅 터치 무시 처리.

### 6.2 절전 모드 (`apply_sleep_settings()`)

```c
/* 1. RESEED 먼저 — MULT 변경으로 인한 counts 스케일 불일치 즉시 해소 */
write_register(0xC0, 0x08, 0x00);  // LTA ← 현재 counts, delta = 0

/* 2. RESEED 후 실제 터치 해제 대기 */
{
    bool pressed, ati_error;
    while (tdc_drv_iqs323_read_status(&pressed, &ati_error) && pressed)
    {
        SYS_WATCHDOG_REFRESH();
    }
}
```

**순서가 중요한 이유**: MULT(0x5C82)로 바꾼 직후 counts 스케일이 달라져 기존 LTA 대비 delta > THRESHOLD → 실제 터치가 없어도 계속 터치로 판정된다. 터치 해제 대기를 RESEED 전에 두면 오판으로 인해 영원히 해제되지 않는 무한 루프가 된다.

RESEED를 먼저 실행해 delta = 0으로 초기화한 뒤 터치 해제를 기다려야 실제 터치만 정확히 반영된다.

---

## 7. 요약

```
RESEED를 안 하면  → 환경 변화 후 LTA가 틀어져 delta 오류
RESEED를 터치 중에 하면 → LTA가 터치 값으로 고정되어 이후 터치 미감지
RESEED를 비터치 상태에서 하면 → delta = 0 기준 재설정, 정상 동작
```
