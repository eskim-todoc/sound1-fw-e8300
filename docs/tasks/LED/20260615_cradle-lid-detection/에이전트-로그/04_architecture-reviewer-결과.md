---
name: 아키텍처 & 통합 설계가 결과
purpose: 크래들 뚜껑 상태 → LED 비활성화 최적 데이터 흐름 설계 결과
type: tasks/에이전트-로그
maturity: done
tags: [agent-result, architecture, data-flow, tdc-pattern, setter]
---

# 아키텍처 & 통합 설계가 결과

**TL;DR**: 권장 설계 — `ble_communication.c` 0x34 블록에서 `tdc_led_set_cradle_inhibit(bool)` 호출, `LedOutput.c`의 `compute_best_state()` 상단 가드로 IDLE 반환. 기존 `tdc_ui_command_set_mapping_connected()` 단방향 setter 패턴 동일 적용.

---

## 1. tdc_ 단방향 setter 패턴 (기존 사례)

**파일**: `tdc_ui_command.h:21`, `tdc_ui_command.c:82-85`
```c
// 헤더 선언
void tdc_ui_command_set_mapping_connected(bool connected);

// 구현
void tdc_ui_command_set_mapping_connected(bool connected)
{
    s_tdc_actual_mapping_connected = connected;
}
```
- main.c에서 BLE 상태 수신 후 매 cycle 호출
- 내부 static 변수에 캐싱, 외부 getter 없음 (단방향)

## 2. main.c 메인 루프 실행 순서 (func_normal)

```
tdc_touch_process()         → 터치 폴링
systemControl()             → LED 패턴 결정 (라인 488-495)
isd_interface()             → ISD 상태 갱신 (라인 502-505)
bleCommunication(isd_state) → BLE 패킷 처리 (라인 511) ← 여기서 0x34 수신
  └─ tdc_led_set_cradle_inhibit() 호출 (새로 추가)
LED source request 블록     → 배터리/ISD/매핑 LED 요청 (라인 537-650)
```

## 3. 권장 데이터 흐름

```
QCC (SPI)
  ↓
bleCommunication()          ble_communication.c:511
  ↓
0x34 패킷 파싱
  data[0] → 배터리%
  data[1] → charger_connected (0=끊김, 1=연결)
  data[2] → lid_state (1=열림, 2=닫힘, else=열림)
  ↓
bool inhibit = (charger_connected == 1) && (lid_state == 2)
tdc_led_set_cradle_inhibit(inhibit)
  ↓
LedOutput.c: s_tdc_led_cradle_inhibit = inhibit
  ↓ (매 1ms Timer3 ISR)
led_arbiter_tick()
  ↓
compute_best_state()
  if (s_tdc_led_cradle_inhibit) return LED_ST_IDLE;  ← 가드
  ↓
LED_OUT() → tdc_led_write_gpio(false, false, false) → GPIO LOW
```

## 4. 신규 심볼 목록

| 파일 | 심볼 | 형태 |
|---|---|---|
| `LedOutput.c` | `static volatile bool s_tdc_led_cradle_inhibit` | 내부 상태 변수 |
| `LedOutput.c` | `void tdc_led_set_cradle_inhibit(bool inhibit)` | setter 구현 |
| `LedOutput.h` | `void tdc_led_set_cradle_inhibit(bool inhibit);` | 공개 선언 |

## 5. 대안 비교

| 방안 | 삽입 위치 | 장점 | 단점 |
|---|---|---|---|
| **권장 A** | `compute_best_state()` 상단 | 기존 패턴 일치, 소스 순회 전 차단 | 패턴 엔진은 여전히 구동 (미미) |
| B | `LED_OUT()` 내부 | GPIO 직전 완전 차단 | ISR 1ms마다 조건 검사 오버헤드 |
| C | `led_request()` 소스 레벨 | 특정 SRC만 억제 가능 | 모든 SRC 억제 시 복잡 |
