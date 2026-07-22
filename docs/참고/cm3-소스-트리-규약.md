---
name: cm3 소스 트리 규약
purpose: 2__cm3 의 소스 트리 배치·네이밍·include·FSM·파일처리 규약 (루트 코딩 지침의 프로젝트 구체화)
type: 참고
applies_to: [Sound1]
tags: [cm3, convention, naming, include, source-tree, fsm]
---

# 2__cm3 코딩 규약 (tdc_convention)

**TL;DR**: 루트 지침 `E:\workspace\rules\지침\코딩\`(네이밍 컨벤션 · 구현 패턴 · 밸리데이션 계층화)을 2__cm3 에 구체화한 규약. 전면 리팩토링(2026-07-20 승인, `docs/tasks/cm3/20260720_cm3-full-refactor/`)의 기준 문서다. 상세 근거는 해당 작업의 `계획.md` 참조.

## 1. 네이밍 (루트 `네이밍 컨벤션.md` 준수)

| 종류 | 규칙 | 예 |
|---|---|---|
| public 함수 | `tdc_<모듈>_<동작>()` | `tdc_isd_process()` |
| static 함수 | **무접두어** (짧은 이름, 오타·모호 약어만 정정) | `handle_touch_debug()` |
| extern 전역 | `g_tdc_<모듈>_<이름>` | `g_tdc_isd_connection_history` |
| static 전역(파일) | `s_tdc_<이름>` | `s_tdc_battery_state` |
| static 지역(함수) | `s_<이름>` | `s_prev_value` |
| 타입 | `tdc_<이름>_t` | `tdc_isd_status_t` |
| 매크로/enum 값 | `TDC_<모듈>_<이름>` | `TDC_PWR_LOW_BATT_PCT` |
| 파일 | `tdc_<모듈>[_<역할>].c/.h` | `tdc_pwr_battery.c` |

## 2. 도메인(모듈) 약어 (2026-07-20 승인)

`sys`(시스템) · `pwr`(전원/배터리/충전) · `led` · `touch` · `isd`(내부기) · `ble` · `qcc` · `fs`(파일시스템) · `boot` · `dfu` · `ui` · `stim`(자극 파라미터) · `shm`(공유메모리) · `cfx`(CFX 원격 명령) · `util`/`crc`/`fft`/`printf`(범용)

**역할 표식**: `tdc_hal_<periph>_`(MCU 내부 페리페럴: SPI/I2C/타이머/UART/DIO) · `tdc_drv_<chip>_`(외부 칩: MIS2DH/MAX17262/REN_ISL*/IQS323)

## 3. rename 금지 (불가침)

1. **공유 ABI**: `cfx_link/tdc_shm.h` 의 타입·필드 전부 (헤더 상단 경고 참조)
2. **복제 헤더 교차 심볼**: `board/processorDirective.h` · `stim/tdc_stim_definitions.h` 중 동결 70종 — `docs/tasks/cm3/20260720_cm3-full-refactor/분석-데이터/08_보류-동결-목록.md`
3. **외부 라이브러리**: `lib/SEGGER_RTT/` · `lib/tiny-AES-c/` — **원본 API 심볼만** 유지. 라이브러리 폴더 안이라도 자작 래퍼는 rename 대상(예: `ci_aes_*` → `tdc_aes_*`)
4. **SDK 심볼**: `Sys_*` / `SYS_*` / `hw.h` 계열 (ON Semi 제공)

## 4. FSM 패턴 (루트 `구현 패턴.md` 준수)

명시 state enum(`tdc_<mod>_state_t`) + ctx 구조체(designated initializer 로 초기값 명시) + 단일 `step()`(매 tick 1회, 즉시 반환) + 상태별 static 핸들러 + 부수효과는 전이 시점만 + 상태 조회 인터페이스. **1-shot·순수 계산은 FSM 화하지 않는다.**

## 5. 파일 처리 패턴

`tdc_fs_record`: `magic_begin + version + size + payload + magic_end + crc16`, all-or-nothing, 실패 시 기본값 캐시 + 즉시 재기록. (`tdc_fs_stim_mute` 패턴 승격)

## 6. 소스 인코딩

주석에 CP949 비호환 문자 금지 (`—` `≈` `µ` `►` 등) — 저장소 루트 `CLAUDE.md` §소스 인코딩 규칙, pre-commit 훅이 차단.


## 7. 소스 트리 구조 (2026-07-22 확정)

```
2__cm3/
├── sections.ld      링커 -T 스크립트 - 루트 불가침
├── Debug/           빌드 산출물 (CDT 생성, git 미추적)
└── source/          모든 소스는 이 아래에만
    ├── main.c  main.h
    ├── board/       보드·프로세서 정의 (Board_*.h, processorDirective.h, 99_eeprom_address.h)
    ├── lib/         외부 라이브러리 (SEGGER_RTT, tiny-AES-c)
    └── <도메인>/    hal drv led touch ui sys pwr fs ble qcc dfu isd stim cfx_link boot util
```

**모듈 co-location**: `.c` 와 `.h` 는 같은 도메인 폴더에 둔다. 헤더·소스를 물리 분리하지 않는다.

## 8. include 규약 (2026-07-22 확정)

| 규칙 | 내용 |
|---|---|
| 표기 | **`<파일명.h>`** 한 가지. `"..."` 금지 |
| 경로 | **붙이지 않는다.** `<tdc_hal_i2c.h>` (O) / `<hal/tdc_hal_i2c.h>` (X) |
| 전제 | **헤더 파일명은 전역 유일**해야 한다. 새 헤더 추가 시 동명 존재 여부를 확인한다 |
| 구분 | 내것/외부는 `tdc_` 접두어로 구분한다 (`<tdc_shm.h>` vs `<stdint.h>`) |

> [!CAUTION]
> **새 폴더를 만들면 `.cproject` 의 `-I` 에 반드시 등록한다.** flat include 방식이라 `-I` 누락은 곧 빌드 실패다. 경로는 `${workspace_loc:/${ProjName}/source/<폴더>}` 형식.
>
> **등록 위치는 도구마다 독립이다.** Eclipse CDT 는 include 경로를 컴파일러·어셈블러·링커가 **각각 따로** 관리한다.
>
> | 파일 | 등록할 도구 |
> |---|---|
> | `.c` / `.h` | Cross ARM C Compiler → Includes |
> | `.S` / `.s` | **Cross ARM GCC (Assembler) → Includes** |
>
> `<>` 표기는 `-I` 목록만 탐색하므로, `""` 시절 암묵적으로 동작하던 **"자기 디렉터리 우선" 폴백이 없다**. 실제로 소스 루트 분리(2026-07-22) 때 어셈블러 목록이 비어 있어 `SEGGER_RTT_ASM_ARMv7M.S` 가 `<SEGGER_RTT.h>` 를 못 찾아 빌드가 깨졌다.

`-I` 현황: 20건 (`source` + `source/board` + `source/lib/{SEGGER_RTT,tiny-AES-c}` + 도메인 16). 근거: `docs/tasks/cm3/20260722_cm3-source-root/`.
