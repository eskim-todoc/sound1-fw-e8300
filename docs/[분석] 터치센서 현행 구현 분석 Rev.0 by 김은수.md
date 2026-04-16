# 터치센서 현행 구현 분석

IQS323 터치센서 드라이버의 현재 구현 상태를 분석한 문서.

---

## 1. 파일 구성

| 파일 | 위치 | 역할 |
|---|---|---|
| `driver_IQS323.h` | `src/2__cm3/.../systemControl/` | 레지스터 정의, 비트필드 구조체, 함수 선언 |
| `driver_IQS323.c` | `src/2__cm3/.../systemControl/` | I2C 통신, RDY 윈도우 관리, 레지스터 읽기/쓰기 |
| `initialize.c` | `src/2__cm3/.../systemControl/` | `iqs323_init()` 초기화 시퀀스, `proc_touch()` 롱터치 판정 |
| `main.c` | `src/2__cm3/Cortex-M3-src/` | `iqs323_proc()` 메인 루프 폴링, 절전 모드 깨우기 |

---

## 2. 초기화 시퀀스 (`iqs323_init`)

`initialize.c`에 위치. 인터럽트 활성화 후 호출됨.

```
1) iqs323_ack_reset_event()      — Reset Event 클리어 (0xC0에 0x01 쓰기)
2) iqs323_confirm_reset_event()  — System Status(0x10) 읽어 Reset Event 해제 확인 (최대 10회 재시도)
3) iqs323_events_enable()        — Events Enable(0xD3) 설정
4) iqs323_sensor_setup()         — Sensor 2, 1 비활성 → Sensor 0만 활성화
5) iqs323_touch_settings()       — CH0 Touch Threshold/Hysteresis 설정
6) iqs323_re_ati_trigger()       — Re-ATI 트리거
   (300ms 대기)
7) iqs323_wait_re_ati_done()     — ATI 완료 확인 (최대 10회, 회당 100ms 대기)
```

### 미설정 항목

| 항목 | 설명 |
|---|---|
| I2C Event 모드 | 설정 안 함 → 기본값 Streaming 모드로 동작 |
| I2C Stop Bit Disable | 주석 처리됨, 미적용 |
| Prox Threshold/Debounce (`0x61`) | 설정 안 함 → 기본값(0x0000) |
| ATI Setup/Base/Multiplier (`0x36`, `0x37`, `0x38`) | 설정 안 함 → IC 기본값 사용 |
| Power Mode (`0xC0` Bit6:4) | 설정 안 함 → Normal Power, Streaming 유지 |
| Report Rate (`0xC1`~`0xC4`) | 설정 안 함 → IC 기본값 |
| Event Timeout (`0xD2`) | 설정 안 함 → IC 기본 롱터치 타임아웃 |
| Filter Beta (`0xB0`~`0xB4`) | 설정 안 함 → IC 기본값 |
| Conversion Frequency (`0x31`) | 설정 안 함 → IC 기본값 |
| Prox Control / PXS Mode (`0x32`) | 설정 안 함 → IC 기본값 (Self-Capacitance) |

---

## 3. 현재 레지스터 설정 값

### 3.1 Events Enable (`0xD3`)

| 이벤트 | 설정 |
|---|---|
| ATI Error | **활성** |
| ATI Event | **활성** |
| Power Event | 비활성 |
| Slider Event | 비활성 |
| Touch Event | **활성** |
| Prox Event | 비활성 |

### 3.2 Sensor Setup (`0x30` / `0x40` / `0x50`)

**Sensor 0** (`0x30`): 활성

| 비트 | 설정 |
|---|---|
| Enable Channel | **1** (활성) |
| CTx0 | **1** (활성) |
| 나머지 (CTx1, CTx2, TxA, CalCap 등) | 0 (비활성) |
| Linearise, Invert, Dual Direct, Vbias | 0 (비활성) |
| Release/Movement UI | 0 (비활성) |

**Sensor 1** (`0x40`), **Sensor 2** (`0x50`): 모두 비활성 (전체 0x0000)

### 3.3 Touch Settings (`0x62`) — CH0만

| 항목 | 값 | 의미 |
|---|---|---|
| Touch Threshold | 80 (0x50) | Touch Threshold = (80 × LTA) / 256 |
| Touch Hysteresis | 80 (0x50) | Hysteresis = (80/256) × Touch Threshold |

### 3.4 System Control (`0xC0`) — 초기화 시 ACK Reset 전송용

ACK Reset(Bit0=1)만 쓰고, 이후 별도 System Control 설정 없음.
→ Power Mode = Normal(기본), Interface = Streaming(기본).

---

## 4. 런타임 동작

### 4.1 메인 루프 폴링 (`iqs323_proc` in main.c)

```
100ms 간격으로:
  1. iqs323_get_touch_state() → System Status(0x10) 읽기
     - ATI Error → ATI_ERROR 상태
     - CH0 Touch 비트 → TOUCH / NOT_TOUCH
  2. 상태 변경 시 로그 출력
  3. proc_touch()로 롱터치 판정
```

### 4.2 롱터치 판정 (`proc_touch` in initialize.c)

소프트웨어 기반 롱터치 감지. IQS323의 Event Timeout(0xD2)과는 별개.

```
상태 머신 (4가지 상태):
  RESET → TOUCH 전환 시: 첫 터치 시각 기록
  TOUCH → TOUCH 유지 시: 경과시간 ≥ 3000ms이면 롱터치 1회 보고
  TOUCH → NOT_TOUCH: 롱터치 플래그 리셋
  NOT_TOUCH → TOUCH: 첫 터치 시각 다시 기록
```

**롱터치 조건**: CH0 TOUCH 상태가 **3초(3000ms)** 연속 유지
**보고**: 최초 1회만 `true` 반환, 해제 전까지 재보고 없음

### 4.3 절전 모드에서의 사용

절전(HALT) 루프에서도 `iqs323_proc()` 호출:
1. 롱터치 감지 → "LONG TOUCH DETECTED, WAIT RELEASE" 로그
2. 터치 해제 대기 → NOT_TOUCH 확인 시 절전 모드 탈출

---

## 5. 헤더에 정의되었으나 사용되지 않는 항목

### 5.1 10-상태 FSM

헤더에 정의된 FSM 상태:

```c
#define IQS323_FSM_RESET           0
#define IQS323_FSM_INIT            1
#define IQS323_FSM_WAIT_ANY_EVENT  2
#define IQS323_FSM_TOUCH_TRIGGERED 3
#define IQS323_FSM_KEEP_TOUCHING   4
#define IQS323_FSM_CONFIRM_TOUCH   5
#define IQS323_FSM_WAIT_RELEASE    6
#define IQS323_FSM_KEEP_RELEASING  7
#define IQS323_FSM_CONFIRM_RELEASE 8
#define IQS323_FSM_ATI_ERROR       9
```

**실제 사용**: 없음. `proc_touch()`에서 4-상태(RESET/TOUCH/NOT_TOUCH/ATI_ERROR)로 대체됨.

### 5.2 `iqs323_process()` 함수

```c
bool iqs323_process(void) { return true; }  // 스텁
```

항상 `true`만 반환하는 빈 함수. 실제 로직은 `main.c`의 `iqs323_proc()`에 구현됨.

---

## 6. I2C 통신 패턴

모든 레지스터 접근이 동일한 패턴을 따름:

```
[쓰기]
  1. rdy_window_open() — Force Comm(0xFF) 또는 RDY GPIO 확인
  2. i2c_write(addr + data) — 레지스터 주소 + 데이터 (3바이트)
  3. wait_rdy_window_closed() — RDY HIGH 대기

[읽기 (Write-then-Read)]
  1. rdy_window_open()
  2. i2c_write(addr) — 레지스터 주소만 (1바이트)
  3. wait_rdy_window_closed()
  4. rdy_window_open()
  5. i2c_read(2바이트) — LSB first
  6. wait_rdy_window_closed()
```

### 특이사항

- I2C가 **blocking** 방식 (`while(1)` 루프로 완료 대기)
- `0xEEEE` 응답 = 유효하지 않은 통신 (윈도우 밖에서 읽기 시도)
- Write 후 Read-back으로 설정 값 검증 수행 (모든 설정 함수에서)

---

## 7. 하드웨어 설정

| 항목 | 값 |
|---|---|
| I2C Slave Address | `0x44` |
| RDY 핀 | `DIO16` |
| RDY Active | LOW (윈도우 열림) |
| 윈도우 열기 대기 | 최대 45ms |
| 윈도우 닫기 대기 | 최대 20ms |
| ATI 완료 대기 | 최대 500ms (10회 × 100ms) |
| 폴링 주기 | 100ms |
| 롱터치 임계값 | 3000ms (소프트웨어) |

---

## 8. 코드 구조 관찰 사항

### 8.1 코드 분산

| 기능 | 위치 | 비고 |
|---|---|---|
| 드라이버 (I2C, 레지스터) | `driver_IQS323.c` | 적절 |
| 초기화 | `initialize.c` (`iqs323_init`) | 드라이버 외부 |
| 메인 루프 폴링 | `main.c` (`iqs323_proc`) | 드라이버 외부 |
| 롱터치 판정 | `initialize.c` (`proc_touch`) | 드라이버 외부 |

→ 터치 감지 로직(`iqs323_proc`, `proc_touch`)이 드라이버가 아닌 `main.c`/`initialize.c`에 산재.

### 8.2 디버그 로그 메시지 불일치

대부분의 함수에서 로그 prefix가 `"[TOUCH] ACK RESET EVENT:"`로 되어 있으나, 실제로는 해당 함수의 동작과 무관한 경우가 많음 (복사-붙여넣기 흔적).

### 8.3 미사용 코드

- 10-상태 FSM 정의 (`IQS323_FSM_*`) — 코드 어디에서도 참조 안 됨
- `iqs323_process()` 스텁 — 빈 함수
- `iqs323_update_tick()` / `iqs323_get_tick()` / `iqs323_update_state()` / `iqs323_get_state()` — 드라이버에 있지만 `main.c`에서만 호출하는 글로벌 상태 관리

---

## 9. 요약

| 구분 | 상태 |
|---|---|
| I2C 통신 | 동작함 (blocking) |
| RDY 윈도우 관리 | 구현됨 (Force Comm, 타임아웃) |
| 초기화 시퀀스 | 구현됨 (Reset ACK → Events → Sensor → Touch → ATI) |
| 싱글 터치 감지 | 동작함 (CH0, 100ms 폴링) |
| 롱터치 감지 | 동작함 (소프트웨어 3초 타이머) |
| ATI 캘리브레이션 | 초기화 시 1회 수행, 런타임 Re-ATI 없음 |
| ATI Error 처리 | 상태 보고만, 자동 복구 없음 |
| 프록시미티 감지 | 미구현 (이벤트 비활성, Threshold 미설정) |
| Event 모드 | 미사용 (Streaming 모드) |
| 저전력 모드 전환 | 미설정 (Normal Power 고정) |
| 다채널(CH1/CH2) | 비활성 |
| I2C 에러 복구 | `init_I2c()` 호출로 I2C 재초기화 |
