---
name: streaming-window-h2-2-verification
purpose: 명제_H(2-2) 검증 — 스트리밍 모드 통신 윈도우 200ms 주장 사실 여부 및 force_window_open 필요성 판정
type: 분석
maturity: stable
tags: [touch, iqs323, streaming, rdy, communication-window, force-window, firmware, 명제검증]
---

# 명제_H(2-2) 검증: 스트리밍 모드 통신 윈도우 200ms — 사실 여부 및 force_window_open 필요성

> **TL;DR**: 명제_H(2-2) "스트리밍 모드에서 통신 윈도우가 약 200ms 열려 있다"는 **거짓**이다. 200ms는 마스터 무응답 시 강제 닫힘 상한(Transaction Timeout, §8.8)이지 정상 통신 시 t_Low가 아니다. 정상 서비스 시 t_Low ≈ 0.4~0.5ms(트랜잭션 길이). 현 펌웨어는 스트리밍 모드를 실제로 사용 중이며, force_window_open은 Streaming 환경에서 폴링 방식 때문에 필수적으로 남아 있어야 한다.

---

## 1. 검증 대상 명제

| 명제 | 내용 |
|---|---|
| **명제_H(2-2)-A** | 스트리밍 모드에서 통신 윈도우가 약 200ms 열려 있다 |
| **명제_H(2-2)-B** | 따라서 force_window_open이 불필요하다 |
| **명제_H(2-2)-C** | 스트리밍 read만으로 터치 상태 취득이 충분하다 |

---

## 2. 현 펌웨어 스트리밍 모드 사용 여부

### 2.1 레지스터 설정 확인

`tdc_drv_iqs323.c` 내 모든 System Control(0xC0) write를 전수 확인한다.

| 코드 위치 | LSB | MSB | Interface bit[7] | Power Mode bits[6:4] |
|---|---|---|---|---|
| `ack_reset_event()` — 라인 382 | 0x01 | 0x00 | **0 = Streaming** | 000 = NP |
| `tdc_drv_iqs323_reseed()` — 라인 945 | 0x08 | 0x00 | **0 = Streaming** | 000 = NP |
| `apply_sleep_settings()` CH_TIMEOUT — 라인 1024 | 0x00 | 0x07 | **0 = Streaming** | 000 = NP |
| `apply_settings()` Reseed — 라인 1147 | 0x08 | 0x00 | **0 = Streaming** | 000 = NP |
| `apply_settings()` CH_TIMEOUT — 라인 1155 | 0x00 | 0x07 | **0 = Streaming** | 000 = NP |

**판정**: 모든 write에서 bit[7] = 0. IQS323은 **Streaming 모드로 운용** 중이다. Report Rate 레지스터(0xC1~0xC4)는 write 없으므로 칩 기본값 0x0000(0ms) = 측정 완료 즉시 RDY 토글. Transaction Timeout(0xD1)도 write 없으므로 기본값 **200ms**.

근거: `1_분석_펌웨어적용값.md §2, §3, §4` — 펌웨어 ground truth 교차검증 완료.

---

## 3. 명제_H(2-2)-A 판정: "통신 윈도우 약 200ms"

### 3.1 200ms의 실제 의미

데이터시트 §8.8 원문:

> *"If the communication window is not serviced within the time specified in milliseconds by the I²C Transaction Timeout register, the communications window is closed (RDY goes high) and processing continues as normal."*

200ms(0x00C8 = Transaction Timeout 기본값)는 **마스터가 응답하지 않을 때 IC가 강제로 윈도우를 닫는 watchdog 상한값**이다. 마스터가 정상 서비스하면 STOP 수신 즉시 윈도우가 닫힌다(§8.9).

### 3.2 시나리오별 t_Low

| 시나리오 | t_Low | 근거 |
|---|---|---|
| **정상 서비스** (마스터 START→R/W→STOP) | **≈ 0.4~0.5ms** (트랜잭션 길이, SCL=128kHz 기준) | §8.9, `1_분석_스트리밍동작.md §3.2 시나리오_1` |
| **마스터 무응답** | **최대 200ms** (Timeout 경과 후 강제 닫힘) | §8.8, `1_분석_스트리밍동작.md §3.2 시나리오_2` |

**판정**: **명제_H(2-2)-A 거짓.** 200ms는 t_Low의 정상값이 아니라 무응답 시의 상한이다.

---

## 4. 명제_H(2-2)-B 판정: "force_window_open이 불필요"

### 4.1 force_window_open의 역할

`force_window_open()` (라인 143~172):

```c
static bool force_window_open(void)
{
    if (Sys_GPIO_Read(TDC_DRV_IQS323_RDY_PIN) == TDC_DRV_IQS323_RDY_WINDOW_OPENED)
        return true;                    // 이미 열려 있으면 즉시 반환

    uint8_t force_comm = 0xFF;
    if (!i2c_write(&force_comm, 1))     // 닫혀 있으면 Force Comm(0xFF) 전송
        return false;
    // ... 최대 TDC_DRV_IQS323_MAX_WAIT_MS_FOR_WINDOW_OPEN(45ms) 대기
}
```

`write_register()` (라인 178~198), `read_register()` (라인 227~263) 에서 각각 `force_window_open()` 호출. 모든 레지스터 접근의 진입점이다.

### 4.2 Streaming + 200ms 폴링의 구조적 불일치

| 항목 | 값 |
|---|---|
| IQS323 RDY 토글 주기 | Report Rate = 0 → 측정 완료 즉시, 추정 수ms~16ms 이내 |
| t_Low (정상 서비스) | ≈ 0.4~0.5ms |
| 상위 폴링 간격 (`TDC_TOUCH_POLL_INTERVAL`) | **200ms** (tdc_touch.h 라인 25) |

IQS323은 수ms마다 RDY를 토글하지만 CM3는 200ms 간격으로만 읽으러 간다. MCU가 폴링을 시작할 시점에 IC의 윈도우는 이미 닫혀 있을 확률이 압도적으로 높다. `force_window_open()`이 없으면 ICL이 닫힌 상태에서 I2C 트랜잭션을 시도하게 되어 통신 실패 또는 쓰레기 데이터 취득이 발생한다.

> [!IMPORTANT]
> 스트리밍 모드에서 200ms 폴링을 쓰는 이상 `force_window_open()`은 **구조적 필수 요소**다. Streaming이 200ms 윈도우를 "열어두는" 것이 아니라, Force Comm(0xFF)으로 새 윈도우를 강제 요청하는 것이다.

**판정**: **명제_H(2-2)-B 거짓.** force_window_open은 현 폴링 구조에서 삭제 불가하다.

---

## 5. 명제_H(2-2)-C 판정: "스트리밍 read만으로 충분"

### 5.1 스트리밍 read의 의미

"스트리밍 read"가 "RDY LOW가 되길 기다렸다가 read"라면, 이는 인터럽트 기반 아키텍처를 전제한다. 현 펌웨어는 RDY에 인터럽트를 연결하지 않고 GPIO 폴링(busy-wait)을 사용한다(`is_rdy_window_opened()` 라인 115~118).

200ms 폴링 사이클에서 "자연스럽게 열려 있는 윈도우를 찾아 읽기"는 IC RDY 주기가 200ms보다 훨씬 짧아 거의 불가능하다. [추정] RDY 주기가 예를 들어 5~16ms라면, 200ms 중 ICL이 열려 있는 구간은 전체의 약 2.5~8%(0.5ms/주기 기준)에 불과해 우연히 폴링이 윈도우를 찾을 확률은 매우 낮다.

### 5.2 Event Mode 전환 없이는 불충분

단순 스트리밍 read로 충분하려면 인터럽트 기반 RDY 감지 또는 Report Rate를 폴링 주기(200ms)에 맞춰 설정해야 한다. 현 구조에서 대안은:

- **현상 유지**: force_window_open + 200ms 폴링 (작동은 하나 대다수 윈도우 손실)
- **권장 개선**: Event Mode + falling-edge 인터럽트 (데이터시트 §8.11.2 권장 패턴)

**판정**: **명제_H(2-2)-C 거짓.** 현 폴링 방식에서 force 없는 단순 read는 통신 실패 위험이 높다.

---

## 6. 종합 결론

| 명제 | 판정 | 핵심 근거 |
|---|---|---|
| **H(2-2)-A**: 통신 윈도우 200ms 개방 | **거짓** | 200ms = 무응답 시 watchdog 상한(§8.8). 정상 t_Low ≈ 0.5ms |
| **H(2-2)-B**: force_window_open 불필요 | **거짓** | 200ms 폴링 vs 수ms RDY 주기 → force 없이는 구조적 통신 실패 |
| **H(2-2)-C**: 스트리밍 read만으로 충분 | **거짓** | 인터럽트 미사용, Report Rate 불일치 → 우연한 윈도우 포착 확률 극히 낮음 |

**현 펌웨어 확인**: Streaming 모드 사용 확인. Report Rate 0ms(기본값), Transaction Timeout 200ms(기본값). force_window_open은 Force Comm(0xFF)을 통해 새 통신 윈도우를 요청하는 방식으로 현 폴링 구조를 유지시키는 핵심 메커니즘이다.

**개선 방향** (본 검증 범위 외, 참고): Event Mode 전환 + 인터럽트 도입 시 force_window_open 불필요해지며 불필요한 통신 부하 제거 가능. 상세는 `../제어 재설계 분석·권고.md` 참조.

---

## 참조 문서

| 문서 | 근거 항목 |
|---|---|
| `RDY 윈도우 타이밍 분석/1_분석_스트리밍동작.md §3.2` | 시나리오별 t_Low 분류 |
| `RDY 윈도우 타이밍 분석/1_분석_검증반론.md 명제_1` | t_Low = 200ms 오해 반박 |
| `RDY 윈도우 타이밍 분석/1_분석_펌웨어적용값.md §2~4` | 펌웨어 레지스터 ground truth |
| `RDY 윈도우 타이밍 분석/3_종합결론.md §3.3` | Sound1 실무 함정 — 폴링 vs 고속 RDY |
| `tdc_drv_iqs323.c` 라인 143~172, 182, 232, 246 | force_window_open 구현 및 호출 |
| 데이터시트 §8.8, §8.9, §8.11.1, §8.11.2 | 원문 근거 |
