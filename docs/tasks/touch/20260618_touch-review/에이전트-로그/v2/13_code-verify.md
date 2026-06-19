---
name: code-verify
purpose: IQS323 통신 윈도우·스트리밍·read 경로·force_window_open 실제 동작을 grep ground truth로 확정
type: 에이전트-로그
maturity: completed
tags: [touch, iqs323, i2c, rdy, force_window_open, streaming, read-sequence, ground-truth]
---

# 에이전트 로그 13 — 코드 검증 (ground truth)

**TL;DR**: `force_window_open()`은 0xFF write 후 RDY LOW를 **최대 45ms** 대기한다. `read_register()`는 매 호출마다 두 차례 윈도우를 연다(주소 write용 + 데이터 read용). IQS323 스트리밍 모드 설정/사용 코드는 전혀 없다. 터치 read 1회는 force_window_open → i2c_write(주소) → wait_close → force_window_open → i2c_read(2바이트) → wait_close 순서의 **윈도우 2회 open 시퀀스**다.

---

## 1. force_window_open() 동작 확정

**근거**: `tdc_drv_iqs323.c` L143~172

```c
static bool force_window_open(void)
{
    uint8_t force_comm = 0xFF;

    if (Sys_GPIO_Read(TDC_DRV_IQS323_RDY_PIN) == TDC_DRV_IQS323_RDY_WINDOW_OPENED)
        return true;                          // 이미 열려 있으면 즉시 반환

    if (!i2c_write(&force_comm, 1))           // Force Communication: 0xFF 1바이트 write
        return false;

    int tick_old = ci_timer_get_tick();
    while (1)
    {
        if (TDC_DRV_IQS323_MAX_WAIT_MS_FOR_WINDOW_OPEN <= (ci_timer_get_tick() - tick_old))
        {
            ci_printe("[TOUCH] TIMEOUT (%d MS) FORCE WINDOW OPEN \r\n", ...);
            return false;
        }
        if (Sys_GPIO_Read(TDC_DRV_IQS323_RDY_PIN) == TDC_DRV_IQS323_RDY_WINDOW_OPENED)
            return true;
    }
}
```

**확정 결과**:

| 항목 | 값 | 근거 |
|---|---|---|
| 대기 조건 | RDY 핀이 LOW(window 열림 상태) 될 때까지 폴링 | L167 `Sys_GPIO_Read` 비교 |
| 타임아웃 | **45ms** (`TDC_DRV_IQS323_MAX_WAIT_MS_FOR_WINDOW_OPEN = 45`) | `tdc_drv_iqs323.h` L31 |
| 윈도우 강제 트리거 방법 | I2C로 0xFF 1바이트 write (Force Communication) | L152 |
| 이미 열려 있으면 | 0xFF write 없이 즉시 `true` 반환 | L147~149 |
| 타임아웃 시 | `ci_printe` 출력 후 `false` 반환 (에러 경로) | L162~165 |

---

## 2. read_register() — 매 호출마다 윈도우 2회 open 확정

**근거**: `tdc_drv_iqs323.c` L227~263

```c
static bool read_register(uint8_t addr, uint8_t *p_lsb, uint8_t *p_msb)
{
    // --- 1차 윈도우: 주소(레지스터 포인터) write ---
    if (!force_window_open())  { return false; }    // L233: 윈도우 열기 #1
    if (!i2c_write(&reg, 1))   { return false; }    // L238: 주소 1바이트 write
    wait_rdy_window_closed(TDC_DRV_IQS323_MAX_WAIT_MS_FOR_WINDOW_CLOSE);  // L244: 닫힘 대기

    // --- 2차 윈도우: 데이터 read ---
    if (!force_window_open())  { return false; }    // L246: 윈도우 열기 #2
    if (!i2c_read(buf, 2))     { return false; }    // L252: 2바이트 read
    wait_rdy_window_closed(TDC_DRV_IQS323_MAX_WAIT_MS_FOR_WINDOW_CLOSE);  // L261: 닫힘 대기

    return true;
}
```

**확정 결과**: `read_register()` 1회 호출 = `force_window_open()` **2회** 호출. 매번 새로 window를 열고 닫는다. 캐싱·지속 open 없음.

- 윈도우 닫힘 대기 타임아웃: **20ms** (`TDC_DRV_IQS323_MAX_WAIT_MS_FOR_WINDOW_CLOSE = 20`, `tdc_drv_iqs323.h` L32)
- `wait_rdy_window_closed()` 타임아웃은 soft(반환값 미사용) — 코드 내 주석 L128~133 명시

---

## 3. 스트리밍 모드 설정/사용 코드 존재 여부

**grep 결과** (검색 범위: `src/2__cm3/Cortex-M3-src/systemControl/`):

```
grep -i "stream" → IQS323/touch 관련 결과 0건
grep "STREAMING\|streaming\|stream_mode\|STREAM_MODE\|IQS323_STREAM" → 0건
```

**확정 결과**: IQS323 스트리밍 모드(Stream mode — IQS323이 RDY를 주기적으로 자발 assert해 연속 데이터 push) 관련 설정·사용 코드가 **전혀 존재하지 않는다**. 현 펌웨어는 순수 Event/Comm Window 기반 polling 방식만 사용한다.

---

## 4. 터치 read 1회의 실제 통신 시퀀스

`tdc_touch_get_state()` → `tdc_drv_iqs323_read_status()` → `read_register(SYSTEM_STATUS_REG)` 호출 기준:

```
[1] tdc_touch_get_state()                         tdc_touch.c L379
  └─ tdc_drv_iqs323_read_status()                 tdc_drv_iqs323.c L1166
       └─ read_register(SYSTEM_STATUS, ...)        L1171
            │
            ├─ [step 1] force_window_open()        L233
            │    ├─ RDY 이미 LOW? → 즉시 통과
            │    └─ RDY HIGH → i2c_write(0xFF) → RDY LOW 대기(최대 45ms)
            │
            ├─ [step 2] i2c_write(&reg_addr, 1)    L238  ← 레지스터 주소 1바이트 write
            │
            ├─ [step 3] wait_rdy_window_closed(20ms) L244 ← 윈도우 닫힘(RDY HIGH) 대기
            │
            ├─ [step 4] force_window_open()        L246  ← 2차 윈도우 open
            │    └─ (동일: RDY HIGH 이면 0xFF write → 대기 45ms)
            │
            ├─ [step 5] i2c_read(buf, 2)           L252  ← LSB+MSB 2바이트 read
            │
            └─ [step 6] wait_rdy_window_closed(20ms) L261 ← 최종 윈도우 닫힘 대기
```

**시간 상한**: 최악의 경우 `45ms + 20ms + 45ms + 20ms = 130ms` (모두 타임아웃 시). 정상 시 각 대기는 수ms 이내 완료.

**폴링 주기와의 관계**: `tdc_touch_process()`는 100ms 간격으로 `tdc_touch_get_state()`를 호출한다(tdc_touch.c L337 `TDC_TOUCH_POLL_INTERVAL`). read 1회 정상 소요 << 100ms 이므로 구조적 충돌 없음.

---

## 5. 요약 — 4개 질문 확정 답변

| 질문 | 확정 답변 | 근거 라인 |
|---|---|---|
| ① force_window_open() 대기 조건·타임아웃 | RDY LOW 폴링, 타임아웃 **45ms** | .c L157~165, .h L31 |
| ② read_register가 매 read마다 윈도우를 새로 여는가 | **YES, 매 호출마다 2회** (주소 write용·데이터 read용 각 1회) | .c L233, L246 |
| ③ 스트리밍 모드 설정/사용 코드 존재 여부 | **전혀 없음** — IQS323 스트리밍 관련 심볼 0건 | grep 결과 |
| ④ 터치 read 1회 실제 통신 시퀀스 | force_open → write(addr) → wait_close → force_open → read(2B) → wait_close (6단계) | .c L233~261 |
