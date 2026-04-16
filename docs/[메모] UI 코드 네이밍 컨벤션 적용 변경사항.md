# UI 코드 네이밍 컨벤션 적용 변경사항

브랜치: `claude_feature_ui-naming-convention`

---

## 1. 구조 변경

### 파일 통합 (6개 → 2개)

| 기존 | 변경 후 |
|---|---|
| `ui_cmd.h` + `ui_led_cmd.h` + `ui_sys_cmd.h` | `tdc_ui_command.h` |
| `ui_cmd.c` + `ui_led_cmd.c` + `ui_sys_cmd.c` | `tdc_ui_command.c` |

프레임워크, LED 명령어, 시스템 명령어를 하나의 모듈로 통합.
별도 `register()` 함수 불필요 → `tdc_ui_command_init()`에서 일괄 등록.

### 센티넬 변경

| 기존 | 변경 후 | 예시 |
|---|---|---|
| `:` (1글자) | `--` (2글자) | `:led show` → `--led show` |

---

## 2. Public API (헤더에 노출되는 심볼)

### 함수

| 기존 | 변경 후 |
|---|---|
| `ui_cmd_init()` | `tdc_ui_command_init()` |
| `ui_cmd_poll()` | `tdc_ui_command_poll()` |
| `ui_sys_ovr_isd_active()` | `tdc_ui_command_override_isd_active()` |
| `ui_sys_ovr_isd_value()` | `tdc_ui_command_override_isd_value()` |
| `ui_sys_ovr_map_active()` | `tdc_ui_command_override_map_active()` |
| `ui_sys_ovr_map_value()` | `tdc_ui_command_override_map_value()` |
| `ui_sys_ovr_batt_active()` | `tdc_ui_command_override_battery_active()` |
| `ui_sys_ovr_batt_percent()` | `tdc_ui_command_override_battery_percent()` |

### 헤더에서 제거 (내부로 이동)

통합으로 외부 노출 불필요해진 심볼:

- `ui_cmd_register()`, `ui_cmd_printf()`, `ui_cmd_print_help()` → static
- `ui_led_cmd_register()`, `ui_sys_cmd_register()` → 삭제 (init에서 일괄 등록)
- `ui_sys_ovr_set_batt()`, `ui_sys_ovr_clear_batt()` → static 변수 직접 접근
- `ui_cmd_fn_t`, `ui_cmd_entry_t` 타입 → .c 파일 내부 전용

---

## 3. 내부 심볼 네이밍 변경

### 약어 → 전체 단어

| 약어 | 변경 | 적용 위치 |
|---|---|---|
| `cmd` | `command` | 파일명, 함수명, 매크로명 |
| `sys` | `system` | 제거 (모듈 통합) |
| `ovr` | `override` | 함수명, 변수명 |
| `batt` | `battery` | 함수명, 변수명 |
| `pct` | `percent` | 변수명 |
| `vol` | `volume` | 변수명 |
| `st` | `state` | 변수명 |

### Static 변수 (`s_tdc_` prefix)

| 기존 | 변경 후 |
|---|---|
| `s_table_[]` | `s_tdc_table[]` |
| `s_table_n_` | `s_tdc_table_cnt` |
| `line_[]` | `s_tdc_line[]` |
| `line_len_` | `s_tdc_line_len` |
| `k_src_map[]` | `s_tdc_source_map[]` |
| `k_state_map[]` | `s_tdc_state_map[]` |
| `s_ovr_isd_active` | `s_tdc_override_isd_active` |
| `s_ovr_batt_pct` | `s_tdc_override_battery_percent` |
| (기타 override 변수 동일 패턴) | |

### Static 함수 (prefix 제거, 역할 기반)

| 기존 | 변경 후 |
|---|---|
| `ui_cmd_echo_ch()` | `echo_char()` |
| `ui_cmd_echo()` | `echo_string()` |
| `ui_cmd_prompt()` | `print_prompt()` |
| `ui_cmd_printf()` | `output_printf()` |
| `ui_cmd_tokenize()` | `tokenize()` |
| `ui_cmd_dispatch()` | `dispatch()` |
| `cmd_led()` | `handle_led()` |
| `cmd_batt()` | `handle_battery()` |
| `cmd_isd()` | `handle_isd()` |
| `cmd_err()` | `handle_error()` |
| `cmd_vol()` | `handle_volume()` |
| (기타 핸들러 동일 패턴) | |

---

## 4. main.c 변경

| 위치 | 변경 내용 |
|---|---|
| `#include` | 3개 → 1개 (`tdc_ui_command.h`만) |
| 초기화 | 3줄 → 1줄 (`tdc_ui_command_init()`) |
| override 호출 | `tdc_ui_system_override_*` → `tdc_ui_command_override_*` |
| poll | `tdc_ui_command_poll()` |

`ENABLE_UI_CMD` 매크로는 기존 코드이므로 변경 없음.

---

## 5. CLI 사용성 개선

### 명령어 인자 소문자화 + 소스 prefix 제거

| 기존 (대문자, prefix 중복) | 변경 후 (소문자, 간결) |
|---|---|
| `--led req BATTERY BATT_CRITICAL` | `--led req battery critical` |
| `--led req ERROR ERROR_FPGA` | `--led req error fpga` |
| `--led req MAPPING MAPPING_ISD` | `--led req mapping with_isd` |
| `--led req BLE_IND PAIR` | `--led req ble pair` |
| `--led req POWER POWER_ON` | `--led req power on` |

### on/off 도입 (기존 0/1 제거)

| 기존 | 변경 후 |
|---|---|
| `--isd 1` / `--isd 0` | `--isd on` / `--isd off` |
| `--map 1` / `--map 0` | `--map on` / `--map off` |
| `--led user 0` / `--led user 1` | `--led user on` / `--led user off` |

---

## 6. `--led req` override 가드 추가

`--led req`로 직접 설정한 LED 상태가 main loop에 덮어쓰이는 문제 수정.

- `tdc_ui_command.c`: per-source override 배열 `s_tdc_led_override[LED_SRC__MAX]` 추가
- `tdc_ui_command.h`: `tdc_ui_command_is_led_override(int source)` 공개 함수 추가
- `--led req`: override 플래그 설정, `--led clr`: override 플래그 해제
- `--led show`: override 소스에 `(override)` 표시

가드 적용 위치:

| 파일 | 소스 | 기존 동작 |
|---|---|---|
| `main.c` | `LED_SRC_BATTERY` | 매 틱 배터리 % 기반 갱신 |
| `main.c` | `LED_SRC_ISD` | 매 틱 ISD 연결 기반 갱신 |
| `main.c` | `LED_SRC_MAPPING` | 매 틱 매핑+ISD 연결 기반 갱신 |
| `systemControl.c` | `LED_SRC_ERROR` | 매 틱 에러 플래그 기반 갱신 |

모두 `#ifdef ENABLE_UI_CMD` 으로 감싸서 릴리즈 빌드에 영향 없음.

---

## 7. 변경하지 않은 항목

| 항목 | 사유 |
|---|---|
| `ENABLE_UI_CMD` | 기존 코드 소급 불가 |
| CLI 명령어 이름 (`led`, `batt` 등) | 사용자 인터페이스 문자열 |
| `str_map_t` 등 내부 타입 | public이 아님 |
| `ci_strcasecmp` 등 내부 유틸 | static, 역할 명확 |

---

## 7. 검증

- 구 심볼 잔여 참조: grep 결과 **0건**
- main.c 호출부: 헤더/함수 모두 신규 이름과 일치 확인
