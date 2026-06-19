---
name: 10_rdy-window-timing
purpose: 명제_G 검증 — 통신 윈도우 폐쇄·재개방(force_window_open) 비용 정량화
type: 에이전트-로그
maturity: stable
tags: [touch, iqs323, rdy, force-window-open, timing, 명제_G]
---

# 명제_G 검증 — RDY 통신 윈도우 폐쇄·재개방 비용 (은수님 의문 2-1)

> **TL;DR**: `force_window_open`은 레지스터 지정(주소 write → STOP)이 윈도우를 닫은 뒤 IC가 다음 측정 사이클을 끝내고 RDY를 내릴 때까지 busy-wait하는 구조다. 대기 시간(t_wait)은 데이터시트 §8.13 기준 **0.1 ms ~ 45 ms** (typical)이며, 펌웨어 타임아웃 상한도 **45 ms**로 일치한다. 측정 사이클 주기(T_주기)가 실측되지 않아 기대값은 미확정이지만, **비용은 무시할 수 없는 수준(수 ms 이상)**이다.

---

## 1. 질문 정의

**은수님 의문 2-1**: 터치 read를 위해 레지스터 주소를 write할 때, STOP으로 윈도우가 닫히고 다음 윈도우가 열릴 때까지 기다리는 구조가 시간을 크게 소모하는가?

---

## 2. 메커니즘 확인

### 2.1 STOP → 윈도우 닫힘 (근거: 데이터시트 §8.9)

원문: "A standard I²C STOP will close the current communication window."

레지스터 주소를 write하면 3-byte 트랜잭션(START + addr_7bit + W + reg_addr + lsb + msb + STOP) 또는 주소-only write(1-byte: addr + reg_addr + STOP) 후 STOP이 발행된다. **STOP 수신 즉시 현재 comm window가 닫힌다.**

### 2.2 force_window_open 구조 (근거: `tdc_drv_iqs323.c` L143~172)

```c
static bool force_window_open(void)
{
    // 이미 열려 있으면 즉시 반환
    if (Sys_GPIO_Read(TDC_DRV_IQS323_RDY_PIN) == TDC_DRV_IQS323_RDY_WINDOW_OPENED) {
        return true;
    }
    // Force Comm 시퀀스: 0xFF 바이트 write (SCL 없이 SDA 토글로 웨이크업 요청)
    if (!i2c_write(&force_comm, 1)) { return false; }

    // RDY LOW 대기 — busy-wait 폴링
    while (1) {
        if (TDC_DRV_IQS323_MAX_WAIT_MS_FOR_WINDOW_OPEN <= (ci_timer_get_tick() - tick_old)) {
            // 타임아웃 = 45 ms
            return false;
        }
        if (GPIO_RDY == LOW) { return true; }
    }
}
```

**`read_register` / `write_register`는 모두 첫 단계에서 `force_window_open()`을 호출한다** (L182, L207, L232, L246). 즉, 매 레지스터 접근 직전에 반드시 이 대기가 실행된다.

### 2.3 t_wait 수치 (근거: 데이터시트 §8.13, Figure 8.2)

원문: "The minimum and maximum time between the communication request and the opening of a RDY window (t_wait) is application specific. The typical values of t_wait are **0.1 ms ≤ t_wait ≤ 45 ms**."

- 펌웨어 타임아웃 상한(`TDC_DRV_IQS323_MAX_WAIT_MS_FOR_WINDOW_OPEN`) = **45 ms** (`tdc_drv_iqs323.h:31`)
- 데이터시트 typical 상한과 정확히 일치.

---

## 3. 비용 정량화

### 3.1 이미 윈도우가 열려 있는 경우 (비용 = 0)

`force_window_open` 첫 줄에서 GPIO를 읽어 이미 LOW이면 **즉시 반환**한다. 대기 없음.

**그러나 Sound1의 현재 구조에서 이 경로는 드물다.** 이유: 상위 폴링 주기(200 ms)와 IQS323 Streaming 주기(T_주기, Report Rate=0 → 수~수십 ms 수준 [추정])가 크게 불일치한다. IC는 빠르게 윈도우를 열고 스스로 닫는(Transaction Timeout 200 ms)데, 200 ms 후에 펌웨어가 왔을 때 윈도우가 마침 열려 있을 확률은 낮다.

### 3.2 윈도우가 닫혀 있는 경우 — force_window_open 비용

| 단계 | 소요 시간 | 근거 |
|---|---|---|
| 0xFF Force Comm write | ~수십 µs [추정] | SCL=128 kHz, 1-byte I²C 트랜잭션 |
| IC 웨이크업 + 측정 완료까지 대기 | **0.1 ms ~ 45 ms** (typical) | 데이터시트 §8.13 t_wait |
| 합계 (전형 범위) | **0.1 ms ~ 45 ms** | — |

t_wait 기대값은 T_주기(IQS323 RDY 주기)의 절반 수준 [추정]. T_주기는 Report Rate=0 조건에서 측정 사이클 시간에 의존하며, §3.4 예시(NP self-cap 3ch)에서 16 ms가 참고 상한으로 언급됨 (2_검증_핵심수치.md 미해결_1). 기대 대기 시간: **수 ms ~ 10 ms 수준** [추정].

### 3.3 read_status 1회 총 비용

`tdc_drv_iqs323_read_status()` 내부에서 `read_register()`가 1회 호출된다 (L207). 순서:

1. `force_window_open()` — **0.1~45 ms** 대기 (윈도우 닫혀 있을 때)
2. I²C write (reg addr 지정) — **~0.5 ms** (SCL=128 kHz, 1-byte)
3. STOP → 윈도우 닫힘 → re-open 대기 (`force_window_open` 2차)
   - 코드 상 `read_register`는 write 후 re-start 방식이므로 두 번의 force_window_open 가능성 (L207: 호출 1회만 확인됨, 전체 흐름은 `read_register` 구현 참조 필요)
4. I²C read (2-byte) — **~0.4 ms**

**총 비용 추정: 대기(0.1~45 ms) + I²C(~1 ms) ≈ 수십 ms 이상 가능** [추정]

---

## 4. 핵심 판정 — 명제_G

| 항목 | 판정 | 근거 |
|---|---|---|
| **STOP 후 윈도우 닫힘 여부** | **예** — STOP 즉시 닫힘 | DS §8.9 원문 |
| **다음 윈도우 대기 발생 여부** | **예** — force_window_open busy-wait | `tdc_drv_iqs323.c` L159~171 |
| **대기 시간 범위** | **0.1 ms ~ 45 ms** (typical) | DS §8.13, `tdc_drv_iqs323.h:31` |
| **비용이 크게 소모하는가?** | **예 — 무시할 수 없음** | 200 ms 폴링 예산 내 수십 ms가 force_window_open에 소비될 수 있음 |
| **기대 대기 시간** | T_주기 절반 [추정] — 실측 필요 | DS §8.13: t_wait는 report rate 의존 |

---

## 5. 구조적 원인

현재 아키텍처의 비용 발생 원인은 크게 두 가지다.

**원인_1**: Streaming 모드에서 IC는 T_주기마다 RDY를 열지만, 펌웨어는 200 ms 폴링으로만 접근한다. 폴링 시점에 윈도우가 열려 있을 가능성이 낮아 force_window_open이 항상 대기를 수행하게 된다.

**원인_2**: `read_register`는 "주소 write → STOP → re-open → data read" 패턴이다. 레지스터 주소 write의 STOP이 윈도우를 닫아, data read를 위한 force_window_open이 추가로 필요하다 [추정 — read_register 내부 구현 전체 확인 권고].

---

## 6. 미확정 / 실측 필요

| 미확정 항목 | 현재 상태 | 해소 방법 |
|---|---|---|
| T_주기 (IQS323 RDY 주기 실제값) | 데이터시트 미명시, 실측 필요 | 오실로스코프 RDY 핀 / RTT 타임스탬프 |
| t_wait 기대값 | §8.13에 0.1~45 ms typical만 명시 | T_주기 실측 후 산출 |
| STOP 후 RDY HIGH 복귀 지연 | 원문 미명시 (open-drain RC 의존) | RDY rising edge 캡처 |
| read_register 내 force_window_open 호출 횟수 | 코드 일부 확인, 전체 흐름 미확인 | `tdc_drv_iqs323.c` read_register 전체 검토 |

---

## 7. 참조 원문 위치

| 섹션 | 내용 |
|---|---|
| DS §8.9 | STOP closes comm window |
| DS §8.13 Figure 8.2 | t_wait = 0.1~45 ms (typical) |
| DS §8.8 | Transaction Timeout 200 ms (마스터 무응답 시 상한) |
| `tdc_drv_iqs323.h:31` | `TDC_DRV_IQS323_MAX_WAIT_MS_FOR_WINDOW_OPEN = 45` |
| `tdc_drv_iqs323.c:143~172` | `force_window_open()` 구현 |
| `tdc_touch.h:25` | `TDC_TOUCH_POLL_INTERVAL = 200 ms` |
| 2_검증_핵심수치.md 미해결_1 | T_주기 실측 미완료 |
