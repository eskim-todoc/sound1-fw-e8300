---
name: 판정-RDY-timeout
purpose: read_debug 영구 FORCE WINDOW OPEN TIMEOUT의 root cause 수렴 및 해법 적대적 검증
type: 판정
maturity: stable
tags: [touch, iqs323, rdy, force_window_open, root-cause, debug-read]
---

# read_debug 영구 RDY Timeout — 가설 판정 · Root Cause · 해법 권고

> **TL;DR**: root cause는 단일하다. `read_status` 정상 종료 후 RDY가 High인 상태에서 `read_debug`의 `force_window_open`이 0xFF를 I2C 쓰기 트랜잭션으로 발행하는데, IC는 이 0xFF를 undefined 동작으로 처리해 45ms 이내에 RDY Low를 구동하지 않는다. IC 자체는 정상 Streaming 측정을 계속하므로 `read_status`는 자연 RDY Low를 잡아 매 tick 성공하지만, `read_debug`만 동일 조건을 반복해 영구 timeout에 빠진다. 권고 해법: **0x10~0x14 한 윈도우 연속 read 통합** — read_status의 2-윈도우를 확장해 0x10·0x13·0x14를 단일 I2C 트랜잭션으로 읽어, `read_debug` 별도 윈도우를 원천 제거한다.

---

## 1. Ground Truth — 실기 로그 요약

| 구분 | 관측 |
|---|---|
| 정상 구간 | 21회. `[T] LTA=402 CNT=400~402 D=0~3 THR=25 band_in` 출력. read_status + read_debug 모두 성공 |
| 전환점 | 22회째 즉시. 이후 `FORCE WINDOW OPEN TIMEOUT` + `DEBUG READ FAIL (addr)` 영구 반복 |
| read_status 실패 | **없음** — `READ 0x10 WINDOW OPEN FAIL` 로그 전혀 없음 |
| 실패 위치 | `tdc_touch_iqs323.c force_window_open() 153` — read_debug 첫 번째 force_window_open의 폴링 루프 내 timeout 분기 |

---

## 2. 1-tick 내 호출 시퀀스 (코드 라인 기준)

```
[read_status — tdc_touch_iqs323.c:321~340, read_register:185~216]
  A. force_window_open()     : RDY High 시 0xFF i2c_write → 45ms 폴링 RDY Low 대기
  B. i2c_write(&reg_0x10)   : 레지스터 주소 송신 (STOP → RDY High)
  C. wait_window_closed(20ms): RDY High 복귀 대기 [반환값 무시]
  D. force_window_open()     : 데이터 윈도우 획득
  E. i2c_read(buf, 2)        : 2바이트 데이터 read (STOP → RDY High)
  F. wait_window_closed(20ms): RDY High 복귀 대기 [반환값 무시] ← 이 시점에서 RDY High 확인

[read_debug — tdc_touch_iqs323.c:342~371]
  G. force_window_open()     : ← RDY High 상태 진입 → 0xFF i2c_write → 45ms timeout ← 실패 지점
  (이후 라인 353 early return → [H][I][J][K][L] 실행 안 됨)
```

**결정적 사실**: 단계 F(`wait_window_closed`)가 RDY High를 확인하고 true 반환하는 순간, 단계 G는 반드시 RDY High 상태에서 시작한다. IC는 STOP 수신 직후 다음 측정 사이클을 시작하지만, 다음 RDY Low가 내려오기 전(0.5~16ms 범위) G가 먼저 0xFF를 전송한다.

---

## 3. 가설 판정

### 가설_A: 폴링당 윈도우 2회 강제 충돌 (STOP 직후 read_debug force_comm이 IC 측정 중에 걸림)

**판정: real (주 원인)**

**근거**:
1. **코드 구조상 필연적**: 단계 F 완료 직후 단계 G 진입은 동일 함수 호출 흐름에서 코드 분기 없이 연속 실행된다. IC가 STOP을 받은 후 다음 RDY Low까지는 최소 0.5ms(1MHz 변환 완료 시간) 이상 소요되며, 이 구간에서 G가 0xFF를 발행하는 것은 Report Rate=0 Streaming 환경에서 구조적으로 발생한다.
2. **로그 정합**: `FORCE WINDOW OPEN TIMEOUT`이 항상 read_debug 경로(라인 153)에서만 발생한다. read_status 경로는 동일한 `force_window_open`을 사용함에도 실패 없음 → 양 경로의 차이는 진입 시점의 RDY 상태뿐이다. read_status는 200ms 폴링 게이트로 IC의 자연 RDY Low를 잡고, read_debug는 read_status 종료 직후 RDY High에서 강제 진입한다.
3. **21회 성공 설명**: ATI 완료 직후 IC 과도기(apply_settings 내 wait_ati_done_blocking, 최대 1.25s + RESEED)에서 IC 측정 리듬이 완전 Streaming NP 안정 상태와 다르다. 이 과도기 약 21회(~4.2s = 200ms × 21) 동안 단계 G 진입 시 RDY가 이미 Low인 경우가 충분히 존재해 force_window_open이 즉시 true 반환(0xFF 미전송)된다. IC가 Streaming NP 정상 리듬으로 안정화되면 read_status 단계 F가 RDY High 정상 확인 패턴으로 고착되고, G는 매 tick RDY High에서 0xFF를 발행해 영구 timeout이 된다.
4. **영구화 메커니즘**: read_debug 실패 후 early return(라인 356)으로 복구 없음. 다음 tick에도 read_status가 자연 RDY Low로 성공(IC 정상) → 단계 F RDY High 확인 → 단계 G RDY High + 0xFF 동일 실패. 탈출 조건 없음.

---

### 가설_B: conversion은 빠른데 force_comm 부작용 / IC 상태 latch / 통신 절반 서비스 → 영구 꼬임

**판정: uncertain (부분 실재, 가설_A에 종속)**

**근거**:
- 은수님 모델의 전제("1MHz conversion → STOP 후 45ms 이전에 다음 RDY Low 도착")는 **데이터시트 기준 정확하다**. §6.5 Table A.1(Period=5 → Fxfer=1MHz) + §5.4(LTA=402 → ~0.4ms/사이클)에서 RDY 주기는 0.5~2ms이며, 이는 45ms timeout보다 훨씬 짧다.
- 따라서 은수님 모델이 예상한 대로 "STOP 직후 45ms 이내에 다음 RDY Low가 와야 한다"는 것은 **IC 정상 동작 시 맞다**. 21회 성공이 이를 실증한다.
- 그러나 0xFF I2C 쓰기(START + 슬레이브주소 + 0xFF + STOP)는 데이터시트 §8.13의 Force Communication(SDA 4회 토글, SCL 없음)이 아닌 일반 I2C 트랜잭션이다. Stop Bit Disable=0 환경에서 이 0xFF 쓰기는 데이터시트에 정의된 동작이 없다(§8.9: 0xFF end-comm은 Stop Bit Disable=1 전용). IC가 이 undefined 트랜잭션을 수신했을 때 RDY Low를 구동하지 않는 것은 IC 동작 결과이지 IC 자체 버그가 아니다.
- "IC 상태 latch" 가설은 로그에서 기각된다. read_status가 계속 성공한다는 것은 IC가 정상 RDY Low를 구동하고 있다는 의미이므로 IC가 완전히 잠긴 상태는 아니다. IC는 정상이고 read_debug 경로만 잘못된 시점에 잘못된 명령을 보내고 있다.

**결론**: 가설_B의 "force_comm 부작용" 부분은 가설_A와 동일 원인이다. "IC 상태 latch / 통신 절반 서비스" 부분은 로그로 기각된다. 가설_B는 독립 원인이 아니라 가설_A의 메커니즘 일부다.

---

### 가설_C: 코드 버그 (RDY GPIO 오판 · wait_window_closed 무시 · i2c 상태)

**판정: real (부 원인 — 가설_A를 구성하는 코드 결함들)**

**근거**:
1. **wait_window_closed 반환값 미사용** (라인 181, 200, 213, 358, 365 전부): 단계 F가 20ms 내에 RDY High를 확인하지 못하고 soft timeout으로 false 반환할 때, 호출처는 이를 알지 못하고 진행한다. 그러나 로그에서 가장 빈번한 실패는 단계 G(RDY High 확인 후 0xFF 발행)이고 단계 F soft timeout이 직접 관찰되지는 않는다. 따라서 부 기여 요인으로 분류한다.
2. **force_window_open의 0xFF 오용** (라인 143): 이것이 가설_A의 핵심 코드 결함이다. RDY가 High일 때 IC를 깨우려고 0xFF를 I2C 트랜잭션으로 보내지만, IC는 이를 undefined 명령으로 받아 RDY Low를 구동하지 않는다. 데이터시트 §8.13의 Force Comm은 SDA 토글 기반이므로 현재 구현은 근본적으로 틀렸다.
3. **RDY GPIO 오판**: `TDC_TOUCH_IQS323_WIN_OPEN = 0` (h 파일 30번줄)으로 RDY Low=0=WIN_OPEN 매핑은 pull-up + open-drain 구성과 일치한다. GPIO 방향·핀 설정(INPUT, DIO_NO_PULL)은 mclr에서 정상 복원된다. GPIO 오판 가능성은 낮다.
4. **i2c 상태 오염**: 실패 후 early return 시 wait_window_closed를 건너뛰어 IC I2C 상태가 완전히 닫히지 않은 채로 다음 tick이 시작될 수 있다. 그러나 read_status가 매 tick 성공한다는 것은 i2c 드라이버 자체는 정상 복구됨을 시사한다.

---

## 4. Root Cause 수렴

**단일 root cause**:

> `read_debug`의 `force_window_open`(tdc_touch_iqs323.c:353)은 `read_status` 정상 종료 직후 RDY High 상태에서 진입하여 0xFF를 I2C 쓰기 트랜잭션으로 발행한다. 이 0xFF는 데이터시트 §8.13 Force Communication이 아닌 undefined I2C 명령이며, IC는 45ms 이내에 RDY Low를 구동하지 않는다. 결과로 매 tick read_status는 성공(IC 자연 RDY Low), read_debug는 영구 timeout이 된다.

**주 원인**: `read_status` 종료 직후 RDY High 상태에서 `read_debug`의 별도 `force_window_open` + 0xFF 발행
**부 원인**: `force_window_open` 내 0xFF I2C 쓰기가 데이터시트 Force Comm 프로토콜을 구현하지 않음

---

## 5. 해법 후보 적대적 평가

### (가) 0x10~0x14 한 윈도우 연속 read 통합 [권고]

**구현 개요**: `read_status`의 2-윈도우 read_register 시퀀스를 확장하여, 두 번째 윈도우(데이터 read)에서 0x10을 레지스터 포인터로 지정 후 6바이트(0x10 2바이트 + 0x13 2바이트 + 0x14 2바이트)를 IQS323 auto-increment로 단일 i2c_read로 획득한다. `read_debug`를 별도 함수로 호출하지 않는다.

**Root cause 제거 여부**: **완전 제거.** read_debug 호출 자체가 사라지므로 force_window_open RDY High 진입 문제가 구조적으로 제거된다. 별도 윈도우 1개 절약이 추가 이점이다.

**부작용·회귀 평가**:
- IQS323 auto-increment가 0x10→0x11→...→0x15 순서로 동작하는지 데이터시트 §8.x 확인 필요. 0x10(System Status 2B) 다음 0x13(CH0 Counts 2B) + 0x14(CH0 LTA 2B)는 연속이 아니라 0x11·0x12 gap이 있다. **따라서 단순 6바이트 연속 read는 불가 — 0x13을 포인터로 지정한 별도 2-윈도우 시퀀스를 유지하되, read_status 완료 후 즉시 동일 윈도우(RDY Low 상태)에서 0x13 write → RDY 재획득 → 4바이트 read 구조로 통합해야 한다.**
- 수정 범위: `read_status`(또는 새 combined read 함수) + `tdc_touch.c`의 호출부. FSM 입력 구조체 `tdc_touch_in_t` 변경 없음(디버그 데이터는 side-path).
- 회귀: 없음. read_status 동작은 동일하고 read_debug가 제거되는 구조.

---

### (나) 계측 저빈도 (read_debug를 N tick마다 1회 호출)

**Root cause 제거 여부**: **미제거.** 호출 빈도를 낮춰도 호출될 때마다 동일한 RDY High 진입 + 0xFF 발행 → timeout 발생. N tick마다 1회 timeout이 될 뿐이다.

**추가 부작용**: 계측 빈도 감소로 튜닝 정보 손실. 그러나 실패 자체는 없어지지 않는다.

**판정**: 부적합.

---

### (나') 계측 타이밍 분리 (read_debug를 read_status와 분리된 tick에서 호출)

**Root cause 제거 여부**: **불확실.** read_debug를 독립 tick에서 호출해 자연 RDY Low를 만날 가능성을 높이지만, force_window_open의 0xFF 오용 자체는 남는다. 운 좋게 RDY Low 구간에 진입하면 force_window_open이 즉시 true 반환해 성공하지만, 진입 타이밍이 보장되지 않아 간헐적 실패가 잔존한다.

**판정**: 부적합(불완전 해결).

---

### (다) read_status 결과 캐시 재사용 (별도 read 안 함)

**Root cause 제거 여부**: **완전 제거(단, 계측 포기).** read_debug 호출을 완전히 없애므로 문제가 사라진다.

**부작용**: LTA/Counts 실시간 계측 불가. 현재 디버그 계측(`#if TDC_TOUCH_DEBUG_PRINT_ENABLE` 블록)의 목적인 튜닝 데이터를 잃는다. 운용 펌웨어에서 디버그 블록을 OFF하는 것과 동일하며, 디버그 필요 기간에는 쓸 수 없다.

**판정**: 계측이 필요한 현 시점에는 부적합. 운용 릴리즈용으로는 가장 단순한 해결(디버그 블록 비활성화로 이미 달성).

---

### (라) Event Mode 전환

**Root cause 제거 여부**: **완전 제거(통신 구조 전환).** Event Mode에서 IC는 이벤트(터치/ATI)가 있을 때만 RDY Low를 구동한다. 폴링 게이팅 없이 falling-edge 인터럽트로 전환하면, force_window_open 호출 자체가 불필요해진다.

**부작용·회귀 평가**:
- 아키텍처 변경이 크다: GPIO 인터럽트 설정, ISR 진입, 폴링 구조 전체 재작성, 절전 로직 변경 필요.
- REG_EVENTS_ENABLE(0xD3)의 이벤트 종류(ATI 이벤트·터치·ATI 에러)와 Event Mode 전환 레지스터(REG_SYSTEM_CONTROL 또는 Report Rate 설정) 확인 필요.
- 현재 코드베이스의 FSM·연결층과 인터페이스 재정의 수반.
- 계측(LTA/Counts)은 이벤트 외 시점에 별도 폴링이 필요해 Event Mode의 이점이 반감된다.
- 장기적으로 올바른 방향이나 즉시 적용 비용이 높다.

**판정**: 장기 개선으로 적합. 현재 이슈 즉시 해결에는 과도한 변경.

---

## 6. 최종 권고

**권고: (가) 변형 — combined_read 함수로 0x10 + 0x13 연속 획득**

`read_debug`를 폐지하고 `read_status`와 통합한 `tdc_touch_iqs323_read_combined` 함수를 작성한다.

**시퀀스**:
1. 윈도우_1: force_window_open → i2c_write(0x10) → wait_window_closed
2. 윈도우_2: force_window_open → i2c_read(2바이트 = 0x10 Status) → wait_window_closed
3. 윈도우_3: force_window_open → i2c_write(0x13) → wait_window_closed
4. 윈도우_4: force_window_open → i2c_read(4바이트 = 0x13 Counts + 0x14 LTA) → wait_window_closed

이 구조에서 모든 force_window_open은 IC의 자연 RDY Low를 기다린다. `read_debug`의 별도 호출이 없으므로 "read_status 직후 RDY High 상태 진입" 경로가 사라진다.

**근거**:
- Root cause를 구조적으로 제거한다 — 0xFF 발행 트리거 조건이 원천 소멸
- `read_debug` + 별도 `force_window_open` 2개가 제거되어 통신 부하 감소
- FSM 입력(`tdc_touch_in_t`)·출력 구조 변경 없음 → 회귀 없음
- `tdc_touch.c`의 디버그 블록 호출부만 수정 (범위 최소)
- `force_window_open` 내 0xFF 오용 자체도 향후 완전 제거 권고 (Issue: 정상 통신 흐름에서 이미 자연 RDY Low를 기다리므로 force_comm 자체가 불필요, 제거 후 단순화)

---

## 7. 실기 판별 방법

combined read 적용 후 아래 항목으로 검증한다.

| 판별_번호 | 조건 | 기대 결과 | 실패 시 의미 |
|---|---|---|---|
| 판별_1 | 부팅 후 RTT 로그 200회 이상 관찰 | `[T] LTA=... CNT=... band_in` 연속 출력, `FORCE WINDOW OPEN TIMEOUT` 0건 | 수정이 root cause를 제거하지 못함 — 다른 경로 존재 |
| 판별_2 | 21회 전환점 없이 안정 유지 | ATI 과도기(~21회) 이후에도 동일 정상 출력 지속 | IC 과도기 의존성이 root cause의 일부였음 확인 |
| 판별_3 | read_status 단독 실패 로그 | `READ 0x10 WINDOW OPEN FAIL` 여전히 0건 | 정상 — combined read가 동일 read_status 성공률 유지 |
| 판별_4 | 터치 눌렀을 때 CNT 감소, D 증가 | delta > abs_thr 시 `**BAND**` 출력, pressed=P | combined read에서 Counts/LTA 값 정합성 확인 |

---

## 8. 은수님 모델 최종 판정

| 구성 요소 | 판정 | 근거 |
|---|---|---|
| "1MHz conversion → STOP 후 45ms 이내에 RDY Low 도착" 전제 | **맞음** | 데이터시트 §6.5 + §5.4 기반 ~0.4~2ms 사이클, 45ms < 이 값. 21회 성공이 실증 |
| "45ms timeout이면 conversion 속도 문제" 진단 | **틀림** | IC는 정상적으로 RDY Low를 구동하고 있음(read_status 매 tick 성공). Timeout은 IC 동작 문제가 아니라 read_debug가 RDY High 상태에서 undefined 0xFF 명령을 발행해 IC가 응답하지 않는 것 |
| 종합 | 전제는 정확하나 고장 원인 진단이 틀림 | root cause는 conversion 속도가 아니라 read_debug의 RDY High 진입 + 0xFF 오용 |
