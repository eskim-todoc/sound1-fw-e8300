---
name: i2c-mechanism-analysis
purpose: IQS323 I2C 통신 메커니즘 분석 — auto-ATI 중 I2C 실패 형태, RDY 핸드셰이크 구조, 현 코드의 통신 실패 감지 여부
type: 에이전트-로그
maturity: in-progress
tags: [touch, iqs323, i2c, rdy, ati, communication, failure-mode]
---

# 에이전트 로그 02 — IQS323 I2C 메커니즘 분석

**TL;DR**: auto-ATI 중 I2C 실패는 슬레이브 무응답/NACK이 아니라 **RDY 핀 미assert → force_window_open() 타임아웃**으로 나타난다. `write_and_verify`는 read-back 값 비교를 하지 않아 검증이 무의미하고, 현 코드는 실패를 `bool false` 반환으로 감지하지만 상위에서 묵살한다. "I2C 통신 실패를 auto-ATI 신호로 쓴다"는 전략의 핵심 전제는 `force_window_open()` 타임아웃을 신호로 쓴다는 뜻으로 해석 가능하나, 현재 상위 코드는 이 false 반환을 대부분 무시한다.

---

## 1. RDY 핸드셰이크 구조 (데이터시트 §8.4~§8.7 근거)

### 핵심 정의

IQS323은 **Comm Window 기반** 통신 구조다.

- RDY 핀은 open-drain active-low. IQS323이 LOW로 당기면 window **열림**, release(HIGH)하면 window **닫힘**.
- 마스터는 **RDY가 LOW(window 열림) 상태에서만** I2C 트랜잭션을 시작해야 한다.
- window 밖(RDY=HIGH)에서 read 시도 시 → IQS323이 **`0xEE`** 반환 (데이터시트 §8.10).
- window 안이지만 미존재 레지스터 주소 접근 시 → 동일하게 **`0xEE`** 반환.

### ATI 중 I2C 차단 규칙 (데이터시트 §8.4)

> "System Status의 Reset Event bit가 set이 아니면, ATI 동안 I²C 통신 disabled"

**해석**: `Reset Event` bit가 SET인 동안(= 부팅 직후 reset 이벤트 처리 전)에는 ATI 중에도 통신 window가 제공된다. 그러나 `Reset Event`를 ACK(클리어)한 이후 re-ATI가 실행되면 **I2C 통신 window 자체가 열리지 않는다** — RDY가 LOW로 떨어지지 않는다.

---

## 2. auto-ATI 중 I2C "실패"의 실제 형태

### 형태 ②가 정답: RDY window 미열림 → force_window_open() 타임아웃

데이터시트 §8.4에 따르면, auto-ATI 진행 중에는 IQS323이 RDY를 LOW로 당기지 않는다(= comm window를 열지 않는다). 따라서:

| 실패 형태 | 발생 여부 | 근거 |
|---|---|---|
| ① 슬레이브 NACK / 무응답 | **발생하지 않음** | NACK은 I2C START 이후 슬레이브가 address에 응답하지 않을 때 발생. RDY가 HIGH이면 I2C 트랜잭션 자체를 시작하지 않아야 하므로 이 경로 도달 불가 (정상 흐름) |
| ② RDY LOW 미assert → window 미열림 | **정확한 실패 형태** | §8.4: ATI 중 comm window 제공 안 됨. `force_window_open()` 내 `TDC_DRV_IQS323_MAX_WAIT_MS_FOR_WINDOW_OPEN` 타임아웃으로 나타남 |
| ③ Clock stretching | 무관 | Clock stretching은 트랜잭션 내부 메커니즘. ATI 중 window 미열림과는 별개. window가 안 열리면 START 자체가 불가 |
| ④ 정상 ACK + 0xEE stale 데이터 | **부분 발생 가능** | window 밖에서 강제 read를 시도하면 0xEE 반환. 코드에서 `(lsb == 0xEE) && (msb == 0xEE)` 체크가 존재하나, 이는 window 미열림의 증거가 아니라 "window가 닫혔거나 잘못된 주소" 신호임 |

### 현 코드의 실패 경로 (tdc_drv_iqs323.c 기준)

```
read_register() 호출
  └─ force_window_open()          (L232 / L246)
       ├─ RDY가 이미 HIGH → 0xFF write (Force Comm 시도)
       ├─ i2c_write(0xFF) → I2C 드라이버가 i2c_state_Error 반환 가능
       └─ TDC_DRV_IQS323_MAX_WAIT_MS_FOR_WINDOW_OPEN 대기
            └─ ATI 중이면 RDY never LOW → TIMEOUT → return false
                 → read_register() → return false
                      → 상위 함수(is_auto_ati_done_single_read 등) → return false
```

**결론**: ATI 중 `read_register()` 호출 시 실패는 `force_window_open()` 내 타임아웃으로 나타나고, `false`가 반환된다. NACK이 발생하지 않으며 0xEE도 오지 않는다 — 그냥 RDY가 뜨지 않아 대기 후 타임아웃이다.

---

## 3. 현 코드의 통신 실패 감지 여부

### 3-1. write_and_verify (L265~280)

```c
static bool write_and_verify(uint8_t addr, uint8_t lsb, uint8_t msb)
{
    uint8_t read_lsb, read_msb;

    if (!write_register(addr, lsb, msb))  { return false; }
    if (!read_register(addr, &read_lsb, &read_msb)) { return false; }

    return true;  // ← 값 비교 없음! read_lsb/read_msb 미사용
}
```

**문제**: 함수명과 달리 **read-back 값 비교를 하지 않는다**. write 성공 + read 성공이면 항상 `true` 반환. 쓴 값이 레지스터에 실제로 반영됐는지 검증 불가. (분석.md §4-8 이미 확인됨)

### 3-2. read_register (L227~263)

`force_window_open()` 실패 또는 `i2c_read()` 실패 시 `false` 반환. 감지는 한다.

### 3-3. i2c_read / i2c_write (L39~109)

- `i2c_state_Error` 감지 → `init_I2c()` (I2C 드라이버 재초기화) → `return false`.
- **감지 O, 복구 O** (init_I2c 호출), 그러나 반환값이 상위에 전파될 뿐 재시도 없음.

### 3-4. is_auto_ati_done_single_read (L312~331)

```c
static bool is_auto_ati_done_single_read(void)
{
    if (!read_register(..., &lsb, &msb)) { return false; }     // 실패 → false
    if ((lsb == 0xEE) && (msb == 0xEE)) { return false; }    // 0xEE → false
    return (status.elements.lsb.ati_active == 0);
}
```

통신 실패와 ATI 진행 중(ati_active==1)을 모두 `false`로 처리한다. 따라서 **두 상황을 구분할 수 없다**. 이것이 핵심 전략 문제다.

### 3-5. 상위 레이어(apply_settings) 묵살 패턴

```c
if (!sensor_setup()) { ci_printe("[TOUCH] FAIL: SENSOR SETUP \r\n"); }
// 반환값 사용 안 함 — 실패해도 다음 단계로 진행
```

`apply_settings()` (L1036~1164)에서 대부분의 설정 write 실패는 로그만 출력하고 **계속 진행**한다. 통신 실패를 감지하지만 **상위에서 묵살**한다.

---

## 4. 전략 전제 조건 검토

은수님 가정: "I2C 통신 실패를 auto-ATI 신호로 쓴다"

### 현재 구조에서 가능한가?

| 조건 | 현 상태 |
|---|---|
| `read_register()`가 ATI 중 false 반환하는가? | **YES** — force_window_open() 타임아웃으로 false |
| `is_auto_ati_done_single_read()`가 이를 false로 반환하는가? | **YES** — 감지됨 |
| 상위 상태머신이 이 false를 의미 있게 처리하는가? | **NO** — "아직 완료 아님"으로 단순 처리, 다음 tick 재시도 (이게 의도된 동작) |
| 통신 실패와 ATI 진행 중을 **구분**하는가? | **NO** — 둘 다 false, 구분 불가 |

### 결론

현재 `is_auto_ati_done_single_read()`는 **"통신 실패 OR ATI 진행 중 → false"** 로 동작하며, 이 false는 상태머신에서 "아직 ATI 완료 아님"으로 해석되어 다음 tick에서 재시도한다. 즉 I2C 통신 실패가 묵시적으로 "ATI 아직 중" 신호로 쓰이고 있지만, 코드는 이를 **명시적으로 구분하거나 감지하지 않는다**. 이 묵시적 동작을 명시적 설계로 바꾸려면:

1. `force_window_open()` 타임아웃을 **ATI 중 window 미열림**으로 해석하는 로직 추가 필요
2. 또는 `is_auto_ati_done_single_read()` 반환값에 3-상태(true/false/error) 구분 필요
3. 현재 `write_and_verify`의 값 비교 부재는 별개 문제 — 설정 단계의 신뢰성을 무너뜨림

---

## 5. 요약표

| 질문 | 답 |
|---|---|
| ATI 중 I2C 실패 형태 | **② RDY window 미열림** → `force_window_open()` 타임아웃 (NACK/STOP 없음) |
| RDY 핸드셰이크 구조 | **Window 기반**: RDY LOW = window 열림, HIGH = 닫힘. 마스터는 LOW 시에만 통신 시작 |
| RDY 안 뜨면 read를 막는가? | **막는다** — `force_window_open()` 내 폴링 타임아웃으로 차단, `false` 반환 |
| 강제 read 시 무효값이 오는가? | window 밖 강제 read 시 **0xEE 반환** (데이터시트 §8.10). 현 코드는 0xEE 체크 있음 |
| `write_and_verify`가 값 비교 하는가? | **NO** — read-back 후 비교 없이 항상 true (분석.md §4-8) |
| `read_register`가 실패 시 반환 | **false** 반환, 버퍼 불변 (stale 값 없음 — 쓰기 전 종료) |
| 현 코드가 실패를 감지하는가? | **부분적 YES** — 드라이버 레벨은 감지하나 상위(apply_settings)가 묵살. is_auto_ati_done은 "false = ATI 중 OR 통신 실패" 구분 불가 |

---

## 참고 위치

- 데이터시트 §8.4 "Communication During ATI": `docs/참고/touch/데이터시트/05_i2c인터페이스.md` L112~117
- 데이터시트 §8.10 "Invalid Communications Return (0xEE)": 동 파일 L274~290
- `force_window_open()`: `tdc_drv_iqs323.c` L143~172
- `write_and_verify()`: `tdc_drv_iqs323.c` L265~280
- `is_auto_ati_done_single_read()`: `tdc_drv_iqs323.c` L312~331
- `i2c_write()` / `i2c_read()` 에러 처리: `tdc_drv_iqs323.c` L39~109
- 분석.md §4-8 (write_and_verify 무검증 ground truth): `docs/tasks/touch/20260618_touch-review/분석.md`
