---
name: Sullivan1.5 ↔ Sound1 internalDevice 비교 분석
purpose: 베이스 프로젝트 Sullivan1_5__CM3 와 Sound1 2__cm3 의 internalDevice 폴더 의미적 diff 정리
type: 분석
revision: Rev.0
status: 완료(internalDevice 영역)
applies_to: [Sound1, sync, internalDevice]
tags: [sullivan, baseline, diff, internal-device, comparison]
---

# Sullivan1.5 ↔ Sound1 internalDevice 비교 분석 (Rev.0)

**TL;DR**: `src/Sullivan1_5__CM3/Cortex-M3-src/internalDevice/` ↔ `src/2__cm3/Cortex-M3-src/internalDevice/` 32 파일 의미적 비교. **27 파일 동등, 5 파일 차이** (`isd_interface.c/.h`, `isd_interface_init_FPGA.c/.h`, `isd_interface_init_ISD.c`). 차이의 대부분은 Sound1 측 정리·통합(① QCC ISD 연결 상태 통보 `snd_qcc_set_isd()` 추가, ② 신규 getter `snd_isd_interface_get_state()` + 정적 변수 `s_` prefix 적용, ③ 로컬 로그 매크로 `ci_link_print*` / `ci_ifc_init_fpga_print*` 전체 폐기 → 표준 `ci_print*` 일원화). **Sullivan에만 있고 Sound1에 미반영**된 항목은 I²C 통신 실패 에러 진단 로그 5 줄(`isd_interface_init_FPGA.c` 4 줄, `isd_interface_init_ISD.c` 1 줄) — 진단 가치 측면에서 부활 여부 검토 필요.

> [!IMPORTANT]
> 비교 기준은 [`분석 BleCommunication.md §7`](분석%20BleCommunication.md) 와 동일 — `git diff --no-index -w --ignore-blank-lines` + hunk 직접 검토.

## 1. 비교 범위

- Base (좌측 / `a`): `src/Sullivan1_5__CM3/Cortex-M3-src/internalDevice/`
- Target (우측 / `b`): `src/2__cm3/Cortex-M3-src/internalDevice/`
- 파일 32 개 — Renesas PMIC 드라이버 3 종, FPGA/ISD 인터페이스, 매핑(Live/스펙·임피던스·eCAP/Specific/TestStim), 자극 파라미터/StandAlone 등.

## 2. 요약표 (파일 단위)

| # | 파일 | 동등? | 변경 방향 | 비고 |
|---|---|---|---|---|
| 1 | `driver_REN_ISL91128.c` | **동등** | - | 공백/주석만 차이 |
| 2 | `driver_REN_ISL91128.h` | **동등** | - | 공백/주석만 차이 |
| 3 | `driver_REN_ISL9122.c` | **동등** | - | 공백/주석만 차이 |
| 4 | `driver_REN_ISL9122.h` | **동등** | - | 공백/주석만 차이 |
| 5 | `driver_REN_ISL98608.c` | **동등** | - | 공백/주석만 차이 |
| 6 | `driver_REN_ISL98608.h` | **동등** | - | 공백/주석만 차이 |
| 7 | `indicatorByStimul.c` | **동등** | - | 공백/주석만 차이 |
| 8 | `indicatorByStimul.h` | **동등** | - | 공백/주석만 차이 |
| 9 | `isd_interface.c` | 차이 | Sound1 → 추가·정리 | QCC ISD 통보, getter 신설, `s_` prefix, 로그 매크로 일원화 |
| 10 | `isd_interface.h` | 차이 | Sound1 → 정리 | `ci_link_print*` 매크로 전부 제거, getter 선언 추가 |
| 11 | `isd_interface_FPGA.c` | **동등** | - | 공백/주석만 차이 |
| 12 | `isd_interface_FPGA.h` | **동등** | - | 공백/주석만 차이 |
| 13 | `isd_interface_init_FPGA.c` | 차이 | **양방향 분기** | Sound1: `ci_ifc_init_fpga_print*` → `ci_print*` 일원화 + 메시지 표현 정리. **Sullivan: I²C 실패 진단 로그 4 줄 유지** |
| 14 | `isd_interface_init_FPGA.h` | 차이 | Sound1 → 정리 | `ci_ifc_init_fpga_print*` 매크로 전부 제거 |
| 15 | `isd_interface_init_ISD.c` | 차이 | **양방향 분기** | Sound1: 일부 주석 제거 + 로그 레벨 조정(`ci_printd` → `ci_printv`/`ci_printi`). **Sullivan: `[ISD] I2C READ FAIL : FIFO COUNT` 에러 로그 유지** |
| 16 | `isd_interface_init_ISD.h` | **동등** | - | 공백/주석만 차이 |
| 17~20 | `isd_interface_mapping_Live.{c,h}`, `..._SepcificStimulation.{c,h}` | **동등** | - | 공백/주석만 차이 |
| 21~22 | `isd_interface_mapping_eCAP_Measurement.{c,h}` | **동등** | - | 공백/주석만 차이 |
| 23~24 | `isd_interface_mapping_impedanceMeasurement.{c,h}` | **동등** | - | 공백/주석만 차이 |
| 25~26 | `isd_interface_mapping_readWrtieMapData.{c,h}` | **동등** | - | 공백/주석만 차이 |
| 27~28 | `isd_interface_mapping_testStimulation.{c,h}` | **동등** | - | 공백/주석만 차이 |
| 29~30 | `isd_interface_stimulationParaSetting.{c,h}` | **동등** | - | 공백/주석만 차이 |
| 31~32 | `isd_interface_stimulationStandAlone.{c,h}` | **동등** | - | 공백/주석만 차이 |

→ **차이 있는 파일: 5 개** (#9, 10, 13, 14, 15) / **동등 파일: 27 개**

## 3. Sullivan에만 있는 변경 (Sound1 미반영 — 검토 필요)

본 폴더에서는 Sullivan 측에 후속 패치된 신규 로직은 **없음**. 단, **I²C 통신 실패 시 에러 진단 로그** 가 Sullivan 에는 유지되고 Sound1 에서 제거된 사례가 있어 검토 대상.

### 3.1 `isd_interface_init_FPGA.c` — I²C 실패 에러 로그 4 줄

| # | 위치 (함수, 상황) | Sullivan | Sound1 |
|---|---|---|---|
| 1 | `init_FPGA()` — `read_FPGA_version()` 실패 시 | `ci_ifc_init_fpga_printe("[FPGA] I2C READ FAIL : VERSION")` | (else 블록 제거, 로그 없음) |
| 2 | `init_FPGA()` — `read_FPGA_systemResgister_1st()` 실패 시 (reset 검증 단계) | `ci_ifc_init_fpga_printe("[FPGA] I2C READ FAIL : SYSTEM REGISTER 1ST")` | (else 블록 제거, 로그 없음) |
| 3 | `init_FPGA()` — `read_FPGA_systemResgister_1st()` 실패 시 (init 검증 단계) | `ci_ifc_init_fpga_printe("[FPGA] I2C READ FAIL : SYSTEM REGISTER 1ST")` | (else 블록 제거, 로그 없음) |
| 4 | `init_FPGA()` — FPGA 리셋 I²C 쓰기 실패 시 | `ci_ifc_init_fpga_printe("[FPGA] I2C WRITE FAIL : FPGA RESET")` | (else 블록 제거, 로그 없음) |

**해석**: Sound1 측에서 코드 가독성을 위해 깊이 중첩된 `if/else` 의 else 블록을 정리하면서 진단 로그까지 함께 제거된 것으로 추정. I²C 통신 실패는 ISD 연결 불가의 직접 원인이므로 진단 가치가 높음.

또한 Sullivan 측 에러 로그 한 줄에는 `EXPECT` 비교값까지 인자로 포함되어 있는데(`SYS_STAT1, MASKER, RESULT, EXPECT`), Sound1 측은 `EXPECT` 인자를 제거함 — 두 위치 (RESET 검증 / INIT 검증) 의 비교 메시지에서 동일 패턴.

### 3.2 `isd_interface_init_ISD.c` — `[ISD] I2C READ FAIL : FIFO COUNT` 1 줄

`isd_path_Open()` 의 FIFO 카운트 읽기 분기:

```c
// Sullivan:
else
{
    ci_printe("[ISD] I2C READ FAIL : FIFO COUNT \r\n");
}

// Sound1:
else
{
}
```

빈 else 블록만 남아있는 상태. Sullivan 의 에러 로그 부활 권고.

## 4. Sound1에만 있는 변경 (Sullivan 미반영 — 단순 기록)

본 절은 Sound1 측 후속 작업 성과로 본 task 범위 밖이지만 비교를 위해 기록.

### 4.1 `isd_interface.c/.h` — QCC ISD 연결 상태 통보 + getter 신설

| 변경 | 상세 |
|---|---|
| include | `#include <snd_qcc.h>` 추가 |
| 변수 네이밍 | `isd_state` → `s_isd_state`, `isdControlStateChagedFlag` → `s_isd_control_state_chaged_flag` (정적 전역에 `s_` prefix 적용) |
| 신규 함수 | `ST__ISD_STATUS snd_isd_interface_get_state(void)` — `s_isd_state` 반환 getter. `.h` 에 선언, `.c` 에 구현. |
| QCC 통보 | `isd_interface()` 본체 ISD 연결 상태 변경 4 곳에 `snd_qcc_set_isd(SND_QCC_ISD_CONNECTED/DISCONNECTED)` 호출 추가 |
| 함수 헤더 주석 | `// 이 함수가 평소 StandAlone 모드에서 링크 체크하는 함수다.` 한 줄 제거 (`update_isd_LinkConnection_byBacktel_withLiveStimulation()` 위) |

### 4.2 `isd_interface.h` — 로컬 로그 매크로 6 종 폐기

Sullivan 측에 정의되어 있던 `ci_link_printe/w/i/d/v` 5 종 매크로 + `CI_LINK_PRINT_ENABLE_*` 5 종 빌드 가드 매크로 **전부 제거**. `ci_printf.h` include 까지 제거 (로컬 매크로용 의존 정리). Sound1 측 `.c` 에서는 표준 `ci_print*` 직접 사용.

### 4.3 `isd_interface.c` — 로그 매크로 정리 + 디버그 출력 형식 변경

| 변경 | Sullivan | Sound1 |
|---|---|---|
| 로그 함수 일원화 | `ci_link_printe/w/i/d/v(...)` 다수 위치 | `ci_printe/w/i/d/v(...)` 또는 코멘트 처리 |
| 링크 디버그 상태 출력 | `static char *lsp_string_debug[]` 7 원소 문자열 테이블 + 변경 시 "STATE: 'OLD' -> 'NEW'" 출력 | inline 다단 조건문(`? :` 체인)으로 7 케이스 분기, 변경 시 "STATE: %s" 1 줄 출력 + `debug_isd_power_state` 변수명으로 변경 |
| TX 파워 변경 verbose 로그 | `ci_link_printv("[LINK] HIGH/LOW, STABLE/UNSTABLE : ...")` 다수 활성 | 모두 코멘트(`//`) 처리, 비활성 |
| Backtel 디스커넥트 로그 | `ci_link_printw("[LINK] DISCONNECTED, NO BACKTEL ...")`, `ci_link_printw("[LINK] DISCONNECTED, TOO MUCH BACKTEL ...")` | `ci_printw(...)` 동일 메시지 (매크로만 일원화) |

### 4.4 `isd_interface_init_FPGA.h` — 로컬 로그 매크로 6 종 폐기

Sullivan 측에 정의되어 있던 `ci_ifc_init_fpga_printe/w/i/d/v` 5 종 매크로 + `CI_IFC_INIT_FPGA_PRINT_ENABLE_*` 5 종 빌드 가드 매크로 **전부 제거**. `ci_printf.h` include 는 유지 (이미 표준 매크로 사용 의존이 있음).

### 4.5 `isd_interface_init_FPGA.c` — 로그 매크로 일원화 + 메시지 형식 변경

| 변경 | Sullivan | Sound1 |
|---|---|---|
| 로그 함수 | `ci_ifc_init_fpga_printe/w/i/d/v(...)` 다수 | `ci_printe/w/i/d/v(...)` 또는 코멘트 처리 |
| TX PMIC 메시지 | `"[TX PMIC] RESET FAILED"`, `"[TX PMIC] INIT SUCCESS"` | `"[LINK] POWER PMIC RESET FAILED"`, `"[LINK] POWER PMIC INIT SUCCESS"` (태그 통일) |
| FPGA 리셋 실패 메시지 | `"[FPGA] RESET FAILED, SYS_STAT1=..., MASKER=..., RESULT=..., EXPECT=..."` (4 인자) | `"[FPGA] NOT INITIALIZED AFTER RESET, SYS_STAT1=..., MASKER=..., RESULT=..."` (3 인자, EXPECT 제거) |
| FPGA INIT 실패 메시지 | `"[FPGA] INIT FAILED, ..., EXPECT=..."` (4 인자) | `"[FPGA] INIT FAILED, ..."` (3 인자, EXPECT 제거) |
| FPGA 버전 출력 | `"[FPGA] VERSION : %u.%u, ADDR MODE EN : %u"` (MAJOR/MINOR/ADDR_MODE) — `MAJOR` 비트 mask `0x07` | `"[FPGA] VERSION : %u.%u"` (MAJOR/MINOR) — `MAJOR` 비트 mask `0x0F`. ADDRESSING_MODE_EN 표시 제거 |
| FPGA RF disable/enable verbose | `ci_ifc_init_fpga_printv("[FPGA] TRY TO DISABLE (RF) XFR")`, `"[FPGA] SUCCESS TO DISABLE (RF) XFR ..."`, `"[FPGA] NO POWER LEVEL BACKTEL RESPONSE ..."` 활성 | 모두 코멘트(`//`) 처리, 비활성 |
| FPGA 에러 한 줄 | `"[FPGA] ERROR OCCURRED (03h, SYS_ERR_CHK)"` | `"[FPGA] ERROR OCCURRED"` (레지스터 식별자 제거) |

> ⚠ **FPGA 버전 MAJOR 비트 mask 변경 — 잠재적 의미 변경**
> Sullivan: `((FPGA_version >> 4) & 0x07)` (3-bit)
> Sound1: `((FPGA_version >> 4) & 0x0F)` (4-bit)
> 표시값이 다를 수 있음. FPGA 버전 4-bit MAJOR 인지 3-bit MAJOR + 1-bit ADDR_MODE 분리인지 사양 확인 필요. **본 항목은 표시 디버깅 로그라 동작에는 영향 없지만 디스플레이값 의미가 달라짐 — 사양 확인 권고.**

### 4.6 `isd_interface_init_ISD.c` — 주석 정리 + 로그 레벨 조정

| 변경 | Sullivan | Sound1 |
|---|---|---|
| `else` 분기 주석 | `else // 읽어온 내부기 시얼 번호가 0일 때 (뭔가 이상이 생긴 것으로 간주)` | `else` (주석 제거) |
| 블록 종료 주석 | `// 끝, 백텔 FIFO 수가 4개 (시리얼 길이)가 맞을 때`, `// 시작, 백텔 FIFO 수가 4개 (시리얼 길이)가 아닐 때`, `// 끝, ...`, `// 끝, FIFO 카운트 읽기` | 모두 제거 |
| inline 주석 위치 | `change_isd_state(en__isdStatus_FPGA_Ok);` + 다음 줄 `// 내부기 전송 파워 설정 부터 다시.` | 같은 줄에 trailing 주석 |
| ISD 매핑 정보 로그 | `ci_printd(...)` 다수 (MAP NUM, STIM INDICATOR, LED INDICATOR, AUDIO VOLUME, STIM VOLUME, ISD ID MATCH NUM, USER SETTING: MAP NUM) | 모두 `ci_printv(...)` 로 레벨 강등 |
| ISD path open 성공 로그 | `ci_printd("[ISD] SUCCESS TO OPEN ISD PATH")` | `ci_printi("[ISD] SUCCESS TO OPEN ISD PATH")` (DEBUG → INFO 승격) |

## 5. 양방향 분기 (양쪽 다 다른 부분)

본 폴더에서는 명확한 양방향 분기 없음. 모든 차이가 ① Sound1 측 정리·통합·재구성, 또는 ② Sullivan 측 진단 로그 유지 / Sound1 측 진단 로그 삭제 — 두 가지 패턴으로 일원화 해석 가능.

## 6. 결론 및 후속 권고

| 우선순위 | 항목 | 권고 |
|---|---|---|
| 🟡 검토 | §3.1 `isd_interface_init_FPGA.c` 의 I²C READ/WRITE 실패 에러 로그 4 줄 | I²C 통신 실패는 ISD 연결 불가의 직접 원인. 진단 가치 높음 — `ci_printe` 또는 동급 채널로 부활 권고 |
| 🟡 검토 | §3.2 `isd_interface_init_ISD.c` 의 `[ISD] I2C READ FAIL : FIFO COUNT` 1 줄 | 빈 else 블록만 남아있어 진단 누락 — `ci_printe` 부활 권고 |
| 🟡 확인 | §4.5 FPGA_version MAJOR 비트 mask 변경 (`0x07` → `0x0F`) | FPGA 버전 사양 확인 후 비트 mask 정합성 검증. 표시 디버깅 로그라 동작 영향은 없으나 의미는 다름 |
| ⚪ 정보 | §4 Sound1 측 정리 항목 (QCC ISD 통보, getter 신설, `s_` prefix, 로그 매크로 일원화) | 본 task 범위 밖. 단순 기록 |
| 🟢 진행 | `signalProcessing/` 비교 | 동일 방법으로 진행 |

본 폴더는 BleCommunication 폴더와 달리 **신규 로직성 Sullivan 후속 패치는 없고**, Sound1 측 정리 작업이 주류. Sullivan 측에 남은 가치는 **에러 진단 로그 5 줄** 부활 검토 중심.

## 7. 갱신 이력

| 날짜 | 변경 |
|---|---|
| 2026-05-14 | Rev.0 작성. 32 파일 중 5 파일 차이, 27 파일 동등. Sullivan에만 있는 후속 패치 후보 = I²C 실패 에러 로그 5 줄 (FPGA 4 + ISD 1). Sound1 측 정리는 §4 — QCC ISD 통보·getter·`s_` prefix·로그 매크로 일원화·메시지 표현 정비. 잠재 의미 변경 1 건 (FPGA_version MAJOR 비트 mask `0x07` → `0x0F`). 다음 단계 = `signalProcessing/`. |
