---
name: touch-review-에이전트로그-firmware-impl
purpose: 은수님 retry-and-count 전략의 실제 코드화 모습과 ATI_Active 폴링 대비 복잡도·충분성 비교 (명제_E 주력)
type: 에이전트-로그
maturity: in-progress
tags: [touch, iqs323, i2c, retry-and-count, ati-active-polling, firmware-impl, 명제_E]
---

# 05 펌웨어 구현·대안 비교 분석가 — 명제_E

**TL;DR**: 은수님 retry-and-count 전략을 현 코드에 구현하면 `tdc_touch.c` READY 폴링 루프와 `tdc_touch_config.h`에 ~20줄 추가 수준(카운터 1개·조건 2곳)으로 끝난다. 실패 감지 인프라(`read_register` → `read_status` → `tdc_touch_get_state`)는 이미 갖춰져 있어 신설 불필요. ATI_Active 폴링 안은 드라이버 레이어에 별도 대기 루프·에러 핸들러·복구 경로를 추가해야 하므로 코드량이 은수님 안의 3~4배 추정. 단 두 방식은 상호 배타가 아니라 결합 가능하며, 결합 시 상보적 이득이 있다.

---

## 1. 현 코드 사전 조건 확인

### 1-1. 폴링 주기 (코드 라인 근거)

- `tdc_touch.h:25` — `#define TDC_TOUCH_POLL_INTERVAL 200` (ms)
- `tdc_touch.c:337` — `if (TDC_TOUCH_POLL_INTERVAL <= (curr_tick - s_touch_tick_old))`
- 은수님 안은 100ms 제안. 즉 `TDC_TOUCH_POLL_INTERVAL` 값을 200 → 100으로 변경해야 한다.
- **단, `touch.c:334` 주석이 이미 "100ms 폴링"으로 오기재**(`분석.md §5-2` 발견). 실제값은 200ms이며 주석 오류는 Phase 0 정정 대상.

> **영향 분석**: `POLL_INTERVAL` 200→100으로 낮추면 매 100ms마다 `read_status()` → `force_window_open()` → I2C 트랜잭션 2회(주소 write + 2바이트 read)가 추가 발생. 롱터치 누적 카운트 타이밍, `s_boot_5s_warned` 기준 등 시간 계산에는 영향 없음(절대 tick 비교이므로). 방전 주기도 `get_state` 호출 단위이므로 함께 100ms로 빨라짐 — 방전 열화·측정 cycle 교란 여부는 별도 실측 필요(실측1 게이트).

### 1-2. 실패 감지 경로 (신설 필요 여부)

현재 실패 감지 경로는 완전히 구축되어 있음. 경로 추적:

```
tdc_touch.c:341  tdc_touch_get_state(&curr_state)
  → tdc_touch.c:384  tdc_drv_iqs323_read_status(&pressed, &ati_error)
    → tdc_drv_iqs323.c:1171  read_register(0x10, &lsb, &msb)
      → force_window_open() — 실패 시 false 반환
      → i2c_write() — 실패 시 init_I2c() + false 반환
      → i2c_read() — 실패 시 init_I2c() + false 반환
```

- `read_register()` (L227~263): `force_window_open` 실패·I2C write 실패·I2C read 실패 → 각각 `false` 반환. `ci_printe` 로그 출력 포함.
- `tdc_drv_iqs323_read_status()` (L1166~1183): 내부 `read_register` 실패 → `false` 반환.
- `tdc_touch_get_state()` (L379~426): `read_status` 실패 → `false` 반환.
- `tdc_touch.c:341~343`: `got_state = tdc_touch_get_state(...)` → `if (got_state)` 분기로 실패는 이미 감지 중.

**결론: 실패 감지 코드는 신설 불필요. 이미 bool 반환값으로 호출자에게 전달된다.**

### 1-3. `ati_error` 무시 경로와의 관계

- `tdc_touch.c:389~393`: `ati_error` 값은 수신하지만 `(void) ati_error`로 즉시 무시.
- 이유: ATI Mode=Disabled(현 운용 모드)에서 CH1 더미 채널의 autoATI 잔재 비트가 전역 합산되어 실제 터치 판정과 무관. 상세: `docs/참고/touch/이슈해결/2026-06_CRX1-ESD-더미채널.md`
- **Phase 1a (ATI Full 재활성) 이후**: ATI_Active 폴링 방안은 `ati_error`를 다시 활용하는 경로를 열게 되므로, 현재 무시 경로와 충돌이 생긴다. 은수님 안은 `ati_error`를 계속 무시한 채 단순 I2C 실패 카운팅만 사용하므로 이 충돌이 없다.

---

## 2. 은수님 안 — 실제 구현 코드 스케치

### 변경 위치 (3곳)

**위치 1: `tdc_touch_config.h`** — 상수 신설 (2줄)

```c
/* Phase 1a — I2C 통신관리: retry-and-count 전략 */
#ifndef TDC_TOUCH_I2C_FAIL_MAX
#  define TDC_TOUCH_I2C_FAIL_MAX   10   /* 연속 실패 N회 → 고장 판정 */
#endif
```

**위치 2: `tdc_touch.h`** — POLL_INTERVAL 변경 (1줄)

```c
/* 은수님 안: 200 → 100 변경 */
#define TDC_TOUCH_POLL_INTERVAL     100   /* 폴링 간격 (ms) */
```

**위치 3: `tdc_touch.c`** — 카운터 변수 + 판정 로직 (총 ~15줄)

```c
/* 파일 상단 정적 변수 추가 (~3줄) */
static int s_i2c_fail_count     = 0;   /* 연속 I2C 실패 카운터 */
static bool s_touch_hw_fault    = false;  /* 고장 판정 플래그 */

/* READY 폴링 블록 내 — 기존 코드 */
/* tdc_touch.c:337 if (TDC_TOUCH_POLL_INTERVAL <= ...) { */
    bool got_state = tdc_touch_get_state(&curr_state);
    if (got_state)
    {
        s_i2c_fail_count = 0;                 /* 성공 시 카운터 리셋 */
        /* 기존 상태 변화 로그 로직 */
        if (s_touch_state_old != curr_state) { ... }
    }
    else
    {
        s_i2c_fail_count++;
        if (s_i2c_fail_count >= TDC_TOUCH_I2C_FAIL_MAX)
        {
            if (!s_touch_hw_fault)
            {
                s_touch_hw_fault = true;
                ci_printe("[TOUCH] I2C FAIL x%d → HW FAULT \r\n", TDC_TOUCH_I2C_FAIL_MAX);
                /* 고장 판정 후 처리: 상위에 알림 or 리셋 경로 — 별도 설계 필요 */
            }
        }
        else
        {
            ci_printv("[TOUCH] I2C FAIL %d/%d, retry next tick \r\n",
                      s_i2c_fail_count, TDC_TOUCH_I2C_FAIL_MAX);
        }
        /* got_state=false이면 curr_state는 쓰레기값 — 기존 주석(touch.c:354)과 동일하게 무시 */
    }
/* } */
```

### 변경 규모 요약

| 파일 | 변경 유형 | 추가 라인 수 |
|---|---|---|
| `tdc_touch_config.h` | 상수 2개 신설 | ~4줄 |
| `tdc_touch.h` | 상수값 변경 (200→100) | 0줄 순증(수정) |
| `tdc_touch.c` | 정적 변수 2개 + 분기 로직 | ~15줄 |
| **합계** | | **~19줄** |

> 추정 표기: 위 코드 스케치는 개념 수준. 실제 고장 판정 후 처리(MCLR 재시도? 상위 에러 코드 전달? WDT 리셋?)는 별도 설계 필요하며 10~20줄 추가 가능성 있음. 핵심 감지·카운트 로직 자체는 ~19줄이 타당한 추정.

---

## 3. ATI_Active 폴링 안 — 동등 구현 추정

`계획.md §3 Phase 1a` 기술 기준으로 필요한 구현 목록:

### 추가 필요 코드

**드라이버 레이어 (`tdc_drv_iqs323.c`)**:

1. **ATI_Active 폴링 함수**: `is_auto_ati_done_single_read()` (L312~331)는 이미 존재. 단 호출 타이밍·대기 루프를 `read_status` 직전에 삽입해야 함. 블로킹 대기(wait loop)는 `read_register` 경로 내부에 추가해야 하므로 단일 함수가 아닌 `read_status` 진입 직전 대기 래퍼가 필요 (~15줄).
2. **ATI_Error 핸들러**: 에러 감지 → 노터치 게이트 확인 → 수동 Re-ATI 재트리거(`re_ati_trigger()` L569~579 기존 존재) → `wait_re_ati_done()` (L581~623 기존 존재). 연결 로직 신규 약 20줄.
3. **I2C 타임아웃 복구 경로**: 무응답 N회 시 `mclr_reset()` 재시도 또는 `init_I2c()` 재진입 상태머신. 복구 성공 판정, 실패 시 상위 에러 전달. 약 20~30줄.
4. **ATI_Active 폴링 중 터치 상태 보관**: 폴링 대기 동안 stale 상태를 어떻게 처리할지 상태 변수 신설 필요. 약 5줄.

**기능 레이어 (`tdc_touch.c`)**: `got_state` false 처리 분기 일부 변경 약 5줄.

**설정** (`tdc_touch_config.h`): 타임아웃 임계 상수 3~4개 신설 약 8줄.

### 변경 규모 추정

| 항목 | 추정 라인 |
|---|---|
| ATI_Active 대기 래퍼 | ~15줄 |
| ATI_Error → Re-ATI 연결 로직 | ~20줄 |
| I2C 타임아웃 복구 상태머신 | ~25줄 |
| 상태 변수·설정 상수 | ~13줄 |
| **합계** | **~73줄** |

> 추정 표기: 기존 `wait_auto_ati_done`(L343~378), `wait_re_ati_done`(L581~623) 재사용을 전제한 최소 추정. 재사용 없이 풀 구현 시 100줄 이상 가능.

---

## 4. 두 방식 비교표

| 항목 | 은수님 retry-and-count | ATI_Active 폴링 |
|---|---|---|
| **순증 코드량** | ~19줄 | ~73줄 (추정) |
| **상태 수** | 2 (카운터 int, fault bool) | 4+ (폴링 대기·에러·복구·stale) |
| **드라이버 변경** | 없음 | 있음 (read_status 경로 내 대기 삽입) |
| **실패 감지 신설** | 없음 (기존 bool 반환 활용) | 없음 (기존 활용하나 연결 로직 신설) |
| **`ati_error` 활용** | 없음 (기존 무시 유지) | 있음 (재활성, 현 무시 경로와 충돌) |
| **엣지케이스 노출** | 고장·ATI 오판(명제_D·F 참조) | ATI 대기 중 블로킹·stale read·Re-ATI 루프 |
| **ATI Full 이후 적합성** | ATI Full 재활성 여부와 무관하게 동작 | ATI Full 재활성 시 필수 선행 요구(계획 §3) |
| **롤백 용이성** | 빌드 가드 or 상수 OFF, 즉시 | 드라이버 변경 수반, 롤백 더 복잡 |
| **단순성 실재** | **실재** — 변경 파일 1개(touch.c), 드라이버 불변 | 복잡 — 드라이버 + 기능 + 설정 3파일 동시 |

### 엣지케이스 비교

| 상황 | 은수님 안 거동 | ATI_Active 폴링 거동 |
|---|---|---|
| auto-ATI 1회가 100ms 초과 (지속 통신 불가) | 카운터 누적, 10회면 오판 고장 | 대기 루프가 ATI 완료까지 블로킹 |
| ATI 완료 직후 즉시 read 성공 | 자연 복구, 카운터 리셋 | 폴링이 이미 완료 감지 후 read |
| 진짜 I2C 하드웨어 고장 | 10회 연속 실패 → 정확 고장 판정 | 타임아웃 복구 루프 → 결국 복구 불가 → 별도 에스컬레이션 필요 |
| 터치 중 ATI 미동작 (가정_2) | 실패 없음, 정상 동작 | ATI_Active bit=0 → 폴링 바로 통과 |
| 단일 I2C 버스 노이즈 | 카운터 1 증가, 다음 tick 복구 | force_window_open 실패 → 동일 경로(이미 존재) |

---

## 5. 결합 가능성 검토

두 방식은 상호 배타가 아니다. 계층을 달리하여 결합 가능:

```
[상위] ATI_Active 폴링 (드라이버 레이어) — ATI 중 대기·에러 핸들러
         ↓ (ATI_Active 폴링이 있어도 예외 I2C 오류는 발생 가능)
[하위] retry-and-count (기능 레이어) — 예외 실패 누적·최종 고장 판정
```

- ATI_Active 폴링이 정상적으로 ATI 중 통신을 막아주면 retry-and-count 카운터는 거의 오르지 않는다.
- 하드웨어 고장·예상치 못한 I2C 버스 오류가 발생하면 retry-and-count가 안전망으로 동작한다.
- 결합 시 은수님 안 추가 코드(~19줄)는 ATI_Active 폴링이 있어도 그대로 유지 가능 — 오버헤드 최소.

**권고**: Phase 1a(I2C 통신관리)를 ATI_Active 폴링으로 구현하더라도 은수님 retry-and-count를 최후 안전망으로 병행 채택하는 것이 가장 견고하다.

---

## 6. 현 코드 전제 조건 정리

| 전제 | 현재 상태 | 구현 영향 |
|---|---|---|
| POLL_INTERVAL = 200ms | `tdc_touch.h:25` — 200 | 은수님 100ms 제안대로 변경 시 방전 주기도 100ms로 빨라짐(실측1 게이트) |
| read_status → bool 반환 | `iqs323.c:1166` — 완비 | 신설 불필요 |
| write_and_verify 실검증 부재 | `iqs323.c:265~280` — read-back 비교 없음, 항상 true | Phase 0 정정 선행 권고(기존 분석) |
| ati_error 무시 | `touch.c:389~393` | 은수님 안은 영향 없음, ATI_Active 폴링 안은 충돌 처리 필요 |
| is_auto_ati_done_single_read | `iqs323.c:312~331` — 이미 존재 | ATI_Active 폴링 구현 시 재사용 가능 |

---

## 7. 종합 판정

1. **은수님 안 구현 모습·규모**: `tdc_touch.c`에 정적 변수 2개 + 분기 ~15줄, `tdc_touch_config.h`에 상수 ~4줄. 드라이버 변경 없음. 실패 감지 경로 신설 없음(기존 bool 반환 활용). 총 ~19줄 순증.

2. **ATI_Active 폴링 대비 단순성 실재 여부**: **실재한다.** 은수님 안 ~19줄 vs ATI_Active 폴링 ~73줄. 상태 수 2 vs 4+. 드라이버 불변 vs 드라이버 수정. 은수님 안이 명백히 더 단순하다.

3. **실패 감지 신설 필요성**: **없다.** `read_register` → `read_status` → `tdc_touch_get_state` 경로에서 각 단계가 이미 bool을 반환하며, `tdc_touch.c:341~343`의 `got_state` 분기가 이미 실패를 처리 중이다. 카운터와 판정 로직만 추가하면 된다.

4. **결합 가능성**: **가능하고 권고됨.** ATI_Active 폴링(1차 방어) + retry-and-count(최후 안전망) 계층화 결합이 단독 채택보다 견고하다. 추가 비용은 은수님 안 ~19줄이 전부이며, ATI_Active 폴링이 있어도 제거할 이유가 없다.
