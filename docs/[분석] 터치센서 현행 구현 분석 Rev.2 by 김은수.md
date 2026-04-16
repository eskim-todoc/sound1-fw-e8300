# 터치센서 현행 구현 분석

IQS323 터치센서 드라이버의 현재 구현 상태를 분석한 문서.
Rev.2 — 드라이버 리팩토링 및 Auto-ATI 대기 수정 반영.

---

## 1. 파일 구성

| 파일 | 위치 | 역할 |
|---|---|---|
| `tdc_iqs323.h` | `src/2__cm3/.../systemControl/` | 레지스터 정의, 비트필드 구조체, 공개 API 선언 |
| `tdc_iqs323.c` | `src/2__cm3/.../systemControl/` | 드라이버 전체 (I2C, RDY, 설정, 초기화, 폴링, 롱터치) |
| `initialize.c` | `src/2__cm3/.../systemControl/` | `tdc_iqs323_init()` 호출만 |
| `main.c` | `src/2__cm3/Cortex-M3-src/` | `tdc_iqs323_process()` 호출만 |

### 공개 API (3개)

| 함수 | 설명 |
|---|---|
| `tdc_iqs323_init()` | 초기화 시퀀스 전체 (MCLR → 설정 → ATI) |
| `tdc_iqs323_process()` | 100ms 폴링 + 롱터치 판정. 반환 true = 롱터치 |
| `tdc_iqs323_get_touch_state()` | System Status(0x10) 읽어 터치 상태 반환 |

### 내부 구조 (static)

| 계층 | 함수 |
|---|---|
| 레지스터 헬퍼 | `write_register()`, `read_register()`, `write_and_verify()` |
| RDY 윈도우 | `force_window_open()`, `wait_rdy_window_closed()`, `is_rdy_window_opened()` |
| I2C 저수준 | `i2c_write()`, `i2c_read()` |
| 설정 단계 | `mclr_reset()`, `wait_auto_ati_done()`, `ack_reset_event()`, `confirm_reset_event()`, `events_enable()`, `sensor_setup()`, `touch_settings()`, `re_ati_trigger()`, `wait_re_ati_done()` |
| 롱터치 판정 | `proc_touch()` |

---

## 2. 초기화 시퀀스 (`tdc_iqs323_init`)

`tdc_iqs323.c`에 위치. `Initialize()`에서 인터럽트 활성화 후 호출됨.

```
1) mclr_reset()              — DIO16 LOW → 1ms → INPUT 복원 → 50ms 부팅 대기
2) wait_auto_ati_done()      — System Status(0x10)의 ATI Active=0 될 때까지 폴링 (최대 20회, 50ms 간격)
3) ack_reset_event()         — System Control(0xC0)에 ACK Reset 쓰기
4) confirm_reset_event()     — System Status(0x10) 읽어 Reset Event 해제 확인 (최대 10회)
5) events_enable()           — Events Enable(0xD3) 설정 + 검증 읽기
6) sensor_setup()            — Sensor 2, 1 비활성 → Sensor 0만 활성화 + 검증 읽기
7) touch_settings()          — CH0 Touch Threshold/Hysteresis 설정 + 검증 읽기
8) re_ati_trigger()          — Re-ATI 트리거
   (50ms 대기)
9) wait_re_ati_done()        — ATI Event 플래그 확인 (최대 10회, 100ms 간격)
```

각 단계의 반환값을 검증하며, 실패 시 `ci_printe`로 에러 로그 출력.

---

## 3. 현재 레지스터 설정 값

### 3.1 Events Enable (`0xD3`) — `0x52`

| 이벤트 | 설정 |
|---|---|
| ATI Error | **활성** |
| ATI Event | **활성** |
| Touch Event | **활성** |
| Power / Slider / Prox Event | 비활성 |

### 3.2 Sensor Setup (`0x30` / `0x40` / `0x50`)

- **Sensor 0** (`0x30`): `0x0101` — CTx0 활성, Channel Enable
- **Sensor 1** (`0x40`): `0x0000` — 비활성
- **Sensor 2** (`0x50`): `0x0000` — 비활성

### 3.3 Touch Settings (`0x62`) — `0x5050`

| 항목 | 값 |
|---|---|
| Touch Threshold | 80 (0x50) |
| Touch Hysteresis | 80 (0x50) |

### 3.4 System Control (`0xC0`)

ACK Reset 전송 후 별도 System Control 설정 없음.
→ Power Mode = Normal, Interface = Streaming.

### 미설정 항목

ATI Setup/Base(`0x36`~`0x38`), Prox Threshold(`0x61`), Event Timeout(`0xD2`), Filter Beta(`0xB0`~`0xB4`), Report Rate(`0xC1`~`0xC4`), Conversion Frequency(`0x31`), PXS Mode(`0x32`) — 모두 IC 기본값 사용.

---

## 4. 런타임 동작

### 4.1 폴링 (`tdc_iqs323_process`)

100ms 간격으로 System Status(0x10) 읽기. CH0 Touch 비트로 TOUCH/NOT_TOUCH 판정. 상태 변경 시 로그 출력.

### 4.2 롱터치 판정 (`proc_touch`)

| 전이 | 동작 |
|---|---|
| RESET/NOT_TOUCH → TOUCH | 첫 터치 시각 기록 |
| TOUCH → TOUCH (3초 경과) | 롱터치 이벤트 1회 보고 (`true` 반환) |
| TOUCH → NOT_TOUCH | 롱터치 플래그 리셋 |

### 4.3 절전 모드

`func_sleep()` 루프에서 `tdc_iqs323_process()` 호출:
1. 롱터치 감지 → 터치 해제 대기
2. 해제 확인 → `SYS_WATCHDOG_RESET()` → MCU 전체 리셋
3. 재부팅 → `tdc_iqs323_init()` → MCLR 리셋으로 IQS323도 초기화

---

## 5. 해결된 이슈

### 5.1 절전 후 워치독 리셋 시 RE-ATI 타임아웃 (Rev.1에서 분석)

**증상**: MCU만 리셋되고 IQS323 전원 유지 → I2C 불일치 → 설정 타임아웃

**해결**: MCLR 하드 리셋으로 IQS323을 항상 POR 상태에서 시작.

### 5.2 완전 전원 차단 후에도 간헐적 RE-ATI 실패

**증상**: 파워서플라이 차단 후 재공급 시에도 RE-ATI 타임아웃이 간헐적 발생.

**원인**: MCLR 리셋 후 IQS323의 Auto-ATI가 아직 실행 중인 상태에서 설정 및 RE-ATI 트리거 진행. Auto-ATI 실행 중에 RE-ATI를 트리거하면 무시됨.

로그 비교:

```
[성공] RE-ATI CHECK TRY 1: LSB=0x32 → ATI Event=1, ATI Active=1 → 즉시 성공
[실패] RE-ATI CHECK TRY 1: LSB=0x03 → ATI Event=0 → 10회 전부 Event 미발생 → 타임아웃
```

양쪽 모두 CONFIRM RESET 단계에서 `LSB=0x20` (ATI Active=1)으로 Auto-ATI가 미완료 상태.

**해결**: ACK Reset 전에 `wait_auto_ati_done()` 추가. ATI Active=0이 될 때까지 폴링(최대 20회, 50ms 간격) 후 설정 진행.

```
[수정 전]  MCLR → 50ms → ACK Reset → 설정 → RE-ATI (Auto-ATI 미완료 시 무시됨)
[수정 후]  MCLR → 50ms → ATI Active=0 폴링 → ACK Reset → 설정 → RE-ATI
```

---

## 6. 하드웨어 설정

| 항목 | 값 |
|---|---|
| I2C Slave Address | `0x44` |
| RDY/MCLR 핀 | `DIO16` |
| MCLR LOW 유지 | 1ms (데이터시트: ≥250ns) |
| MCLR 후 부팅 대기 | 50ms |
| Auto-ATI 완료 대기 | 최대 1000ms (20회 × 50ms) |
| 윈도우 열기 대기 | 최대 45ms |
| 윈도우 닫기 대기 | 최대 20ms |
| RE-ATI 완료 대기 | 최대 1000ms (10회 × 100ms) |
| 폴링 주기 | 100ms |
| 롱터치 임계값 | 3000ms |

---

## 7. 코드 변경 이력 (Rev.0 → Rev.2)

| Rev | 변경 |
|---|---|
| Rev.0 | 최초 현행 분석 (driver_IQS323.c/h 기준) |
| Rev.1 | 절전 후 워치독 리셋 시 RE-ATI 타임아웃 원인 분석 추가 |
| Rev.2 | 드라이버 리팩토링 반영 (tdc_iqs323.c/h), Auto-ATI 대기 수정 반영, Rev.0/1의 구 코드 참조 제거 |
