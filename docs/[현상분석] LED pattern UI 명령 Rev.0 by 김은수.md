# LED 패턴 디버깅용 UI 명령 현상 분석

작성자: 김은수
작성일: 2026-04-23
상태: 리뷰 대기
근거 요구사항: [`[요구사항] LED pattern UI 명령 Rev.0`]([요구사항]%20LED%20pattern%20UI%20명령%20Rev.0%20by%20김은수.md)

---

## 1. 현행 `--led` 명령 구조

`tdc_ui_command.c` 의 `handle_led()` ([240~360라인](../src/2__cm3/Gen1_5/ui/tdc_ui_command.c#L240)) 에 다음 서브커맨드가 이미 구현됨:

| 서브 | 동작 |
|---|---|
| `--led show` | 모든 src 의 현재 state + override / burst / user_off 출력 |
| `--led req <src> <state>` | 해당 src 에 state 강제 요청 + override 활성 |
| `--led clr <src>` | override 해제 + state NONE 으로 |
| `--led user on|off` | 사용자 LED off 플래그 |
| `--led pair` | LED_SRC_BLE_IND 에 PAIR 요청 (latch 효과 확인용) |
| `--led burst on|off` | POWER burst 진행 상태 / 강제 클리어 |

→ 새 서브 `pattern N` 은 이 핸들러 안에 추가하면 됨.

## 2. override 메커니즘

`s_tdc_led_override[LED_SRC__MAX]` 가 src 별 boolean 으로 활성화 되어 있으면, main.c 의 LED 요청 로직이 그 src 를 건너뜀:

```c
#ifdef ENABLE_UI_CMD
if (!tdc_ui_command_is_led_override(LED_SRC_BATTERY))
#endif
    led_request(LED_SRC_BATTERY, batt_st);
```

따라서 `--led pattern N` 도 단순히 `s_tdc_led_override[*] = true` + `led_request(*, LED_ST_NONE)` 모두 적용 후, 해당 src 에만 state 요청 → 자동 갱신 무시되고 그 패턴 유지.

## 3. 사진의 디버깅 칼럼 ↔ 코드 enum 매핑

이미지의 "패턴 (디버깅)" 칼럼 N 값 → `(led_src_t, led_state_t)` 매핑:

| N | src | state |
|--:|---|---|
| 0 | (none) | (모든 src NONE — IDLE) |
| 1 | LED_SRC_POWER | LED_ST_POWER_ON |
| 2 | LED_SRC_POWER | LED_ST_POWER_OFF |
| 3 | LED_SRC_MAPPING | LED_ST_MAPPING_ISD_BATT_READY |
| 4 | LED_SRC_MAPPING | LED_ST_MAPPING_NO_ISD_BATT_READY |
| 5 | LED_SRC_MAPPING | LED_ST_MAPPING_ISD_BATT_LOW |
| 6 | LED_SRC_MAPPING | LED_ST_MAPPING_NO_ISD_BATT_LOW |
| 7 | LED_SRC_ERROR | LED_ST_ERROR_MCU (대표) |
| 8 | LED_SRC_BATTERY | LED_ST_BATT_CRITICAL |
| 9 | LED_SRC_BATTERY | LED_ST_BATT_MID |
| 10 | LED_SRC_BATTERY | LED_ST_BATT_READY |
| 11 | LED_SRC_ISD | LED_ST_IN_USE |
| 12 | LED_SRC_BLE_IND | LED_ST_PAIR |
| 13 | LED_SRC_BLE_IND | LED_ST_OTA_QCC |
| 14 | LED_SRC_BLE_IND | LED_ST_OTA_EZAIRO |

테이블로 코드에 표현하면 switch 분기보다 가독성 좋음.

## 4. 영향 범위

| 항목 | 변경 |
|---|---|
| `handle_led()` 안에 `pattern` 서브 추가 | `tdc_ui_command.c` |
| 패턴 매핑 const 테이블 신설 | `tdc_ui_command.c` (static, file-local) |
| `--led` help 문자열 갱신 | `tdc_ui_command.c` 의 `s_tdc_commands[]` |
| 외부 헤더 / 다른 모듈 | 변경 없음 |

→ 단일 파일 수정.

## 5. 결정 필요 사항

| # | 항목 | 옵션 | 권장 |
|---|---|---|---|
| Q1 | ERROR 패턴 (#7) 어떤 enum | LED_ST_ERROR_MCU 등 5종 중 | **LED_ST_ERROR_MCU** (대표) |
| Q2 | N=0 동작 | (A) 모든 src NONE + override 유지 (B) override 해제 | **(A)** — 이전 패턴 차단 명확, 해제는 `--led clr` 사용 |
| Q3 | 잘못된 N 처리 | (A) error msg + return -1 (B) 무시 | **(A)** 표준 |
| Q4 | help 문자열 길이 | (A) 한 줄 (B) 여러 줄 | (A) 기존 컨벤션 따름 |

## 6. 검증

### 6.1 정적

- 빌드 성공
- `Grep "led pattern"` 호출 매크로 출력 1 건 (handle_led 안)

### 6.2 실기

| # | 명령 | 기대 |
|---|---|---|
| a | `--led pattern 0` | LED off |
| b | `--led pattern 7` | 빨강 ON 180 / OFF 180 |
| c | `--led pattern 12` | 파랑 ON 500 / OFF 500 |
| d | `--led pattern 5` | 보라 ON 100 / OFF 900 |
| e | `--led pattern 99` | error 메시지 + LED 변화 없음 |
| f | 패턴 진입 후 자동 요청 (배터리 갱신 등) 와도 패턴 유지 | override 동작 |
| g | `--led clr battery` 로 override 해제 후 자동 갱신 복귀 | 정상 |
