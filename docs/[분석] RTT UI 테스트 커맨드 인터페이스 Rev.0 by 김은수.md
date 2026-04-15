# [분석] RTT UI 테스트 커맨드 인터페이스 Rev.0

작성자: 김은수
대상: E8300 / 2__cm3 / Cortex-M3
목적: SEGGER RTT 채널을 통해 LED Arbiter 및 시스템 상태(배터리/ISD/mapping/error)를
      런타임에 주입/조회하여 LED 운용 Rev.3 구현을 검증한다.

---

## 1. 확정 사항

| 항목 | 값 |
|---|---|
| prefix | `ui_` |
| 파일 위치 | `src/2__cm3/Gen1_5/ui/` |
| 명령 sentinel | `:` (콜론으로 시작) |
| 조건부 컴파일 매크로 | `ENABLE_UI_CMD` (in `processorDirective.h`) |
| 빈 라인 동작 | `:led show` 자동 실행 |
| 단축 alias | 미사용 |
| 레거시 단일 문자 | 명명형으로 완전 이관 |
| 입력 소스 | `SEGGER_RTT_Read(0, ...)` (기존과 동일 채널) |
| 라인 종결 | `\r`, `\n`, `\r\n` 모두 허용 |
| 라인 버퍼 | 128 byte |
| 인자 구분 | 공백 (`' '`, `'\t'`) |
| 에코 | on (입력 문자 echo + 프롬프트 `> `) |

---

## 2. 파일 구성

```
src/2__cm3/Gen1_5/ui/
├── ui_cmd.h        디스패처 public API
├── ui_cmd.c        라인 버퍼 + 토크나이저 + 핸들러 테이블
├── ui_led_cmd.h    LED 테스트 커맨드 등록 API
├── ui_led_cmd.c    :led *, :batt *, :pair 등 LED 직접 주입
├── ui_sys_cmd.h    시스템 주입 커맨드 등록 API
└── ui_sys_cmd.c    :isd, :map, :err, :vol, :init_all_map, :dump_log, :write_integrity_err
```

### 2.1 ui_cmd.h (핵심 API)

```c
#ifndef UI_CMD_H__
#define UI_CMD_H__

#include <stdbool.h>
#include <stdint.h>

#define UI_CMD_SENTINEL      ':'
#define UI_CMD_MAX_LINE      128
#define UI_CMD_MAX_ARGC      8
#define UI_CMD_MAX_HANDLERS  32

typedef int (*ui_cmd_fn_t)(int argc, char *argv[]);

typedef struct {
    const char *name;        /* sentinel 제외. 예: "led", "batt" */
    ui_cmd_fn_t  fn;
    const char *help;        /* 한 줄 도움말 */
} ui_cmd_entry_t;

void ui_cmd_init(void);
bool ui_cmd_register(const ui_cmd_entry_t *entry);
void ui_cmd_poll(void);      /* 기존 do-while 대체 */

/* 핸들러 내부에서 사용 */
void ui_cmd_printf(const char *fmt, ...);
void ui_cmd_print_help(void);

#endif
```

### 2.2 ui_led_cmd / ui_sys_cmd 등록 함수

```c
void ui_led_cmd_register(void);
void ui_sys_cmd_register(void);
```

각 모듈 내부에서 자신의 핸들러들을 `ui_cmd_register()`로 등록.

---

## 3. 명령어 스펙 (확정)

### 3.1 레거시 이관 (5건)

| 레거시 | 신규 | 동작 |
|---|---|---|
| `0`~`9` | `:vol <n>` (n=1~10) | `changeAudioVolume(n)` |
| `e` | `:init_all_map` | `ci_map_init_map_data_all(true)` |
| `l` | `:dump_log` | `ci_event_log_read()` |
| `x` | `:inject_err data_logging` | `errorCodeUpdate(en__MAJOR_ERRORCODE_DATA_LOGGING_ERROR, en__data_logging_error_open, __LINE__)` |
| `t` | `:write_integrity_err` | `ci_event_log_write(CI_EVENT_LOG_TYPE_INTEGRITY_ERROR)` |

### 3.2 LED 커맨드 (`ui_led_cmd.c`)

```
:help
:led show                        Arbiter 현재 상태/활성 소스/패턴 출력
:led req <src> <state>           특정 소스 상태 요청. 예: :led req BATTERY BATT_CRITICAL
:led clr <src>                   해당 소스 요청 해제 (LED_ST_NONE)
:led user <0|1>                  사용자 LED Off 플래그 (1=Off, 0=On)
:led pair                        PAIR 500ms latch 펄스 주입
:led burst <on|off>              Power On/Off 버스트 진행 여부 조회(on) / 강제 해제(off)
:batt <pct>                      배터리 % 주입(테스트 override). 0~100
:batt show                       현재 주입값 및 실제 %
```

**src 토큰**: `POWER`, `ERROR`, `BLE_IND`, `MAPPING`, `BATTERY`, `ISD` (대/소문자 무관)
**state 토큰**: `NONE`, `READY`, `IN_USE`, `BATT_MID`, `BATT_CRITICAL`, `MAPPING_NO_ISD`, `MAPPING_ISD`, `PAIR`, `OTA_QCC`, `OTA_EZAIRO`, `ERROR_MAP`, `ERROR_MCU`, `ERROR_ACCEL`, `ERROR_FPGA`, `ERROR_PMIC`, `POWER_ON`, `POWER_OFF`

### 3.3 시스템 주입 커맨드 (`ui_sys_cmd.c`)

```
:isd <0|1>                       ISD 연결 상태 override
:map <0|1>                       mapping 연결 상태 override
:err <type>                      type: data_logging | fpga | acc | pmic | mcu | map
:err clr                         에러 플래그 전체 해제
:vol <n>                         볼륨 1~10
:init_all_map                    맵 전체 초기화
:dump_log                        이벤트 로그 덤프
:write_integrity_err             무결성 오류 로그 기록
```

### 3.4 테스트 override 훅

`systemControl.c` / `main.c`에서 상태 읽기 지점에 다음 훅 추가:

```c
#ifdef ENABLE_UI_CMD
bool ui_sys_ovr_isd_active(void);     /* true면 ovr 값 사용 */
bool ui_sys_ovr_isd_value(void);
bool ui_sys_ovr_map_active(void);
bool ui_sys_ovr_map_value(void);
bool ui_sys_ovr_batt_active(void);
uint8_t ui_sys_ovr_batt_percent(void);
#endif
```

호출측 예시:
```c
bool isd_conn = ui_sys_ovr_isd_active() ? ui_sys_ovr_isd_value()
                                        : isd_state.conneded_ISD;
```

---

## 4. 라인 버퍼 · 토크나이저 설계

### 4.1 수신 루프 (ui_cmd_poll)

```c
void ui_cmd_poll(void)
{
    char ch;
    while (SEGGER_RTT_Read(0, &ch, 1) > 0) {
        if (ch == '\r' || ch == '\n') {
            if (line_len_ == 0) {
                /* 빈 라인 → :led show 자동 */
                ui_cmd_dispatch_literal("led show");
            } else {
                line_[line_len_] = '\0';
                ui_cmd_dispatch(line_);
                line_len_ = 0;
            }
            ui_cmd_prompt();
        } else if (ch == 0x08 || ch == 0x7F) {   /* BS / DEL */
            if (line_len_ > 0) { line_len_--; ui_cmd_echo("\b \b"); }
        } else if (line_len_ < UI_CMD_MAX_LINE - 1) {
            line_[line_len_++] = ch;
            ui_cmd_echo_ch(ch);
        }
    }
}
```

### 4.2 디스패치

1. 첫 문자가 `:`가 아니면 무시 + `unknown, type ':help'` 안내
2. `:` 제거 후 공백 기준 토큰 분해 (최대 `UI_CMD_MAX_ARGC`)
3. `argv[0]` 이름으로 핸들러 테이블 선형 검색
4. 매칭 시 `fn(argc, argv)` 호출
5. 반환값은 0=OK, 음수=err (에러 시 help 1줄 출력)

### 4.3 등록 테이블

```c
static ui_cmd_entry_t s_table_[UI_CMD_MAX_HANDLERS];
static uint8_t        s_table_n_;
```

선형 검색으로 충분 (명령 수 <30).

---

## 5. main.c 변경

### 5.1 include / 초기화

```c
#ifdef ENABLE_UI_CMD
#include "ui_cmd.h"
#include "ui_led_cmd.h"
#include "ui_sys_cmd.h"
#endif
```

`main()` 초기화 구간 말미:
```c
#ifdef ENABLE_UI_CMD
    ui_cmd_init();
    ui_led_cmd_register();
    ui_sys_cmd_register();
#endif
```

### 5.2 메인 루프 교체

**기존 (L538~578 do-while 블록)** 제거, 한 줄로 교체:
```c
#ifdef ENABLE_UI_CMD
    ui_cmd_poll();
#endif
```

---

## 6. processorDirective.h

```c
/* RTT 기반 UI 테스트 커맨드 (개발/검증 전용) */
#define ENABLE_UI_CMD
```

릴리즈 빌드에서는 주석 처리.

---

## 7. 검증 체크리스트

Rev.3 구현 검증 시나리오:

1. `:led show` → 초기 상태 표시 (POWER_ON burst 진행 중 확인)
2. POWER_ON burst 종료 후 `:batt 50` → LED_ST_READY(녹색 지속) 확인
3. `:batt 8` → BATT_CRITICAL(빨강 180/180) 전환 확인
4. `:batt 12` (히스테리시스 ±2%, 10%+2=12 경계) → CRITICAL 유지 확인
5. `:batt 13` → READY 복귀 확인
6. `:isd 1` → IN_USE(녹색 지속)로 전환, CRITICAL 상황에서는 여전히 CRITICAL 우선
7. `:map 1` + `:isd 0` → MAPPING_NO_ISD(파랑 300/300)
8. `:map 1` + `:isd 1` → MAPPING_ISD(파랑 지속)
9. `:led pair` → 500ms PAIR latch 펄스
10. `:err fpga` → ERROR_FPGA가 모든 것을 덮는지 확인, `:err clr`로 복귀
11. `:led user 1` → 사용자 LED Off 상태에서 BATT_CRITICAL/ERROR/POWER만 살아있는지 확인
12. `:led req BLE_IND OTA_QCC` → 녹색 1100/1100
13. 빈 Enter → `:led show` 자동 실행

---

## 8. 구현 순서

1. `ui_cmd.{c,h}` — 디스패처 + 토크나이저 + 등록 API
2. `ui_led_cmd.{c,h}` — LED Arbiter 주입
3. `ui_sys_cmd.{c,h}` — 시스템 override 훅 + 레거시 이관
4. `main.c` 수정 (include/init/poll)
5. `processorDirective.h` 매크로 추가
6. `systemControl.c` / `main.c` 내 상태 읽기 지점에 override 훅 연결
7. 빌드 확인 후 검증 체크리스트 수행

---

[검증]
- 명령 sentinel `:` 로 기존 단일문자 입력과 구분됨 → 충돌 없음
- `ENABLE_UI_CMD` 미정의 시 전체 no-op → 릴리즈 영향 0
- 토큰 수 32 상한, 라인 128 byte → 스택/ROM 사용 제한적
- override 훅은 `ui_sys_ovr_*_active()` false 시 실제 값 그대로 → 프로덕션 안전
