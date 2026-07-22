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

1. **공유 ABI**: `cfx_cm3_sharedMemory.h` 의 타입·필드 전부 (헤더 상단 경고 참조)
2. **복제 헤더 교차 심볼**: `processorDirective.h` · `definitionsForAlgorithm.h` 중 동결 70종 — `docs/tasks/cm3/20260720_cm3-full-refactor/분석-데이터/08_보류-동결-목록.md`
3. **외부 라이브러리**: `SEGGER_RTT/` · `tiny-AES-c/` (원형 유지)
4. **SDK 심볼**: `Sys_*` / `SYS_*` / `hw.h` 계열 (ON Semi 제공)

## 4. FSM 패턴 (루트 `구현 패턴.md` 준수)

명시 state enum(`tdc_<mod>_state_t`) + ctx 구조체(designated initializer 로 초기값 명시) + 단일 `step()`(매 tick 1회, 즉시 반환) + 상태별 static 핸들러 + 부수효과는 전이 시점만 + 상태 조회 인터페이스. **1-shot·순수 계산은 FSM 화하지 않는다.**

## 5. 파일 처리 패턴

`tdc_fs_record`: `magic_begin + version + size + payload + magic_end + crc16`, all-or-nothing, 실패 시 기본값 캐시 + 즉시 재기록. (`ci_stim_mute` 패턴 승격)

## 6. 소스 인코딩

주석에 CP949 비호환 문자 금지 (`—` `≈` `µ` `►` 등) — 저장소 루트 `CLAUDE.md` §소스 인코딩 규칙, pre-commit 훅이 차단.
