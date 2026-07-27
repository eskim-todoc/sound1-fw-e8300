---
name: 횡단 조사 - 미참조 파일·무의미 include·-I 정합
purpose: CM3 source 전역(153파일)에서 파일 단위 사장 코드·무의미 include·-I 정합·헤더 중복 정의를 조사한다
type: tasks/에이전트로그
applies_to: [Sound1]
tags: [cm3, dead-code, 조사]
---

# 08_사장파일-헤더-include

**TL;DR**: 153파일 전수 조사 결과 파일 단위 사장 후보 9건 (안전 6 · 주의 3 · 위험 0). 최대 발견은 `dfu/tdc_dfu_ota.c`+`.h` 전체 모듈(470줄)이 외부에서 전혀 호출되지 않는 점, 그리고 `TDC_HAL_I2C_USING_ISR` 매크로가 `processorDirective.h`와 `tdc_hal_i2c.h` 양쪽에 무조건 정의되어 `tdc_hal_i2c.c`의 `#ifndef` 분기(2곳)를 영구 사장시키는 점이다. `.cproject` 컴파일 대상·`-I` 경로·헤더명 유일성은 이상 없음.

## 1. 조사 범위와 방법

- 범위: `E:/workspace/projects/sound1-fw-e8300/src/2__cm3/source` 전역 153파일(.c 68 / .h 85), 폴더 구분 없이 전수.
- `.cproject`: `sourceEntries`(`name="source"`, exclude 없음) 및 Cross ARM C Compiler / Cross ARM GNU Assembler 양쪽 `-I` 목록을 `Read`로 직접 확인.
- include 전수: `Grep`으로 `^\s*#include\s*[<"]...` (라인 시작 앵커, 주석 제외) 패턴으로 918건의 살아있는 include 지시문을 추출 → 헤더별 참조 횟수 집계.
- **grep 함정 방어**: 1차 시도에서 앵커 없는 패턴(`#include ...`)을 썼더니 `tdc_shm.h:31-33`의 주석 처리(`// #include <...>`)까지 "포함됨"으로 잡혔다. 라인 시작 앵커(`^\s*#include`)로 재실행해 주석을 제거한 뒤 918건을 확정했다. 이후 모든 매크로/함수 사용 여부 확인은 `grep -v '^\s*[0-9]*:\s*//'` 로 주석 라인을 배제하고 실제 코드 라인만 봤다.
- 헤더별 참조 0건 여부, 동일 파일 내 중복 include, 매크로 중복 정의(같은 이름이 서로 다른 파일에 `#define`), 헤더 basename 전역 유일성을 각각 스크립트로 대조.
- "무의미한 include" 후보는 헤더가 제공하는 모든 `#define`/타입 이름을 추출해 해당 include 파일 전체에서 실제 사용(주석 제외) 여부를 대조하는 방식으로 검증했다(추측이 아니라 심볼 목록 기반 전수 대조).

## 2. 사장 후보 목록

| # | 대상 | 위치 | 종류 | 근거 | 위험도 |
|---|---|---|---|---|---|
| 후보_1 | `dfu/tdc_dfu_ota.c` + `dfu/tdc_dfu_ota.h` (전체 모듈, 470줄) | `dfu/tdc_dfu_ota.c:1-417`, `dfu/tdc_dfu_ota.h:1-53` | 미참조 파일(모듈 단위) | 헤더가 선언하는 유일한 공개 API `tdc_dfu_ota_command_parsing()`(`.h:51`)를 포함해 `tdc_dfu_ota_prepare_file`·`tdc_dfu_ota_write_file`·`ota_update_file` 전부, 자기 자신(`tdc_dfu_ota.c:5`) 외에는 어디서도 호출·include 되지 않음(`grep -rn "tdc_dfu_ota"` 결과 전량 자기 파일). 같은 도메인의 `tdc_dfu_ble_ota.h`(참조 5건, main.c/ble_communication.c 등에서 실사용)로 이미 대체된 구세대 OTA 구현으로 추정. `.cproject`상 컴파일은 되지만(`sourceEntries` 미제외) `--gc-sections`로 최종 바이너리에서는 제거될 가능성이 높음 | 주의 |
| 후보_2 | `ble/tdc_ble_remote.c:12` `#include <board.h> // 디버깅용` | `ble/tdc_ble_remote.c:12` | 무의미한 include | `board.h`+`Board_OTE_ver1_5.h`가 정의하는 매크로 36개 전부를 대조했으나 이 파일의 실제 코드 라인에서 단 하나도 사용되지 않음(주석 처리된 `Sys_GPIO_Set_Low(DIO19)` 등만 존재, `:720`·`:1312`). board.h는 타입/구조체 없이 매크로만 정의하므로 삭제해도 다른 파일에 영향 없음(비전이적) | 안전 |
| 후보_3 | `ble/tdc_ble_remote_sp_para.c:8` `#include <board.h> // 디버깅용` | `ble/tdc_ble_remote_sp_para.c:8` | 무의미한 include | 동일 검증 방식으로 board.h/Board_OTE_ver1_5.h 매크로 36개 전부 미사용 확인(주석조차 없음). 후보_2와 동일 성격 | 안전 |
| 후보_4 | `df_True` / `df_False` 중복 정의 | `board/processorDirective.h:173-174` ↔ `stim/tdc_stim_definitions.h:6-7` | 중복 정의(매크로) | 두 헤더 모두 `#define df_True 1` / `#define df_False 0` 동일 값으로 정의. 1차 리팩토링 로그에서 이미 기록된 항목을 재확인. 값이 동일해 컴파일 경고는 없으나 "정의의 단일 출처" 원칙 위반 | 안전 |
| 후보_5 | `TDC_HAL_I2C_USING_ISR` 중복 정의 + 그로 인한 사장 분기 | `board/processorDirective.h:155` ↔ `hal/tdc_hal_i2c.h:9`; 영향받는 사장 분기: `hal/tdc_hal_i2c.c:164-166`(`tdc_hal_i2c_comm` 정의부), `:231-237`, `:295-301` | 중복 정의(매크로) + 그 결과 발생한 전처리 사장 코드 | 두 헤더 모두 값 없는 `#define TDC_HAL_I2C_USING_ISR`(feature flag)를 무조건(가드 없이) 정의. `tdc_hal_i2c.c`는 자신의 헤더(`tdc_hal_i2c.h`)를 1번째 줄에서 include하므로 이 매크로는 파일 전체에서 항상 정의된 상태 → `:162`의 `#ifdef TDC_HAL_I2C_USING_ISR` 분기(`I2C_0_IRQHandler`)만 항상 살고, `:164`의 `#else`(`tdc_hal_i2c_comm`)와 `:231`·`:295`의 `#ifndef` 분기(`case i2c_state_WritingDone`/`case i2c_state_ReadingDone` 처리)는 전처리 단계에서 영구 배제됨. `tdc_hal_i2c_comm()`은 헤더(`tdc_hal_i2c.h:171`)에 선언만 있고 실제로 정의될 수 없는 유령 함수(호출자도 없음, `grep` 결과 선언 1건+미완성 정의 1건뿐). `I2C_0_IRQHandler`는 SDK 벡터테이블 결속 이름이라 불가침(제거 대상 아님) - 이 항목은 매크로 정리만 대상 | 주의 |
| 후보_6 | `cfx_link/tdc_cfx_eeprom_erase.h:26-27`가 `tdc_cfx_eeprom_recover.h:21-22` 선언을 복제 | `cfx_link/tdc_cfx_eeprom_erase.h:26-27` (원본: `tdc_cfx_eeprom_recover.h:21-22`) | 중복 정의(함수 프로토타입) + 헤더 계층 우회 | `tdc_cfx_eeprom_erase.h`에 `tdc_cfx_eeprom_recover_map_data_by_mapping()`/`tdc_cfx_eeprom_recover_mapdata_mapping_app()` 선언이 손으로 복제되어 있음. 실제 호출부 `cfx_link/tdc_shm.c:621`는 `tdc_cfx_eeprom_recover.h`를 직접 include하지 않고, `tdc_cfx_eeprom_write.h`→`tdc_cfx_eeprom_erase.h` 전이 include로 얻은 복제 선언에 의존해 컴파일됨. 두 헤더의 서명이 어긋나면(수정 시 한쪽만 고치는 실수) 조용히 깨질 수 있는 구조 | 주의 |
| 후보_7 | `sys/tdc_sys_init.c`가 자신의 헤더 `tdc_sys_init.h`를 include하지 않음 | `sys/tdc_sys_init.c` (include 목록 전체, `tdc_sys_init.h` 없음) | 헤더 정합성 결여 | `tdc_sys_init.h`가 선언한 `tdc_sys_init()`/`tdc_sys_reset_nrf()`/`tdc_sys_uninit()`/`tdc_sys_memory_setup_completed()` 4개 함수의 정의부(`tdc_sys_init.c`)가 정작 그 헤더를 include하지 않아 컴파일러가 선언-정의 시그니처 일치를 검증하지 못함. 현재는 시그니처가 일치해 문제 없지만, `main.c:130`에 `tdc_sys_init()`에 대한 별도 forward-declaration이 중복 존재하는 것도 같은 맥락(헤더 include 이전에 작성된 잔재로 추정) | 안전 |
| 후보_8 | 동일 파일 내 동일 헤더 중복 include (18건) | 예: `main.c:37,39`(`tdc_pwr_clock.h`), `cfx_link/tdc_shm.c:11-12`(`tdc_pwr_lsad.h`), `isd/*.c` 다수(`tdc_isd.h` 자기 자신을 2회, 9개 파일), `ble/tdc_ble_mapping.c`(`tdc_ble_mapping.h`·`tdc_isd_map_live.h` 각 2회), `pwr/tdc_pwr_battery.c:2,5`(`stdbool.h`), `sys/tdc_sys_init.c:46,55`(`tdc_pwr_clock.h`) 등 총 18개 파일 | 중복 include(단순) | include guard(`#ifndef __xxx_h__`)로 보호되어 컴파일에는 영향 없음. 가독성/유지보수 관점의 청소 대상 | 안전 |
| 후보_9 | `cfx_link/tdc_shm.h:31-33` 주석 처리된 죽은 include | `cfx_link/tdc_shm.h:31-33` | 이미 비활성화된 include | `// #include <OTE_1_5_gen_CFX_EEPROM_erase.h>` 등 3줄이 주석으로 죽어있음(전처리 대상 아님). 파일 자체가 프로젝트에 존재하지 않는 구세대 파일명이라 grep으로 "포함되는 것"처럼 오판하기 쉬운 대표 사례(§1 grep 함정) | 안전 |

## 3. 사장 아님으로 판정한 것 (오탐 방지 기록)

| 대상 | 왜 사장처럼 보였는가 | 실제로 살아있는 근거 |
|---|---|---|
| `hal/tdc_hal_spi.c:11` `#include <board.h> // 디버깅용` | 후보_2·3과 동일하게 "디버깅용" 주석이 붙어 있어 무의미 include로 의심 | `board.h`가 정의하는 `GPIO_PIN_ReadCommandForSPI_Master`(`:29,36,319`), `NRF_SPI_CLK_PIN`·`NRF_SPI_CS_PIN`·`NRF_SPI_MOSI_PIN`·`NRF_SPI_MISO_PIN`(`:256`)이 모두 실제 SPI 통신 코드에서 사용됨. 주석 라벨만으로 판단하면 오탐 |
| `hal/tdc_hal_dma.h` (참조 1건, `hal/tdc_hal_spi.c`만 include) | 참조 카운트 1이라 "고립 헤더" 의심 | `TDC_HAL_DMA0_CFG0/CTRL/STATUS`, `TDC_HAL_DMA1_CFG0/CTRL/STATUS` 6개 매크로 전부 `tdc_hal_spi.c:233-303`에서 실사용. 참조 1건 = "자기 모듈 전용 헤더"일 뿐 사장이 아님 |
| `hal/tdc_hal_i2c_state.h` (참조 1건, `hal/tdc_hal_i2c_cfx.c`만 include) | 위와 동일 이유로 의심 | `TDC_HAL_I2C_STATE_IDLE`이 `tdc_hal_i2c_cfx.c:45,75`에서 실사용. 단, 이 헤더 내 `TDC_HAL_I2C_ERR_*`(4개)·`TDC_HAL_I2C_STATE_TX/RX_*`(4개)·`TDC_HAL_I2C_WAITING_TIME_MS`는 전체 코드베이스에서 정의 외 참조가 0건 — 헤더 자체는 살아있으나 매크로 9/11개가 죽어있음(개별 심볼 사장이라 본 조사의 "파일 단위" 범위에서는 후보 표에 올리지 않고 여기 기록만 남김. 별도 심볼 단위 조사 노드가 있다면 참고 바람) |
| `cfx_link/tdc_cfx_eeprom_recover.h` (직접 참조 1건, 자기 자신뿐) | 참조 카운트 1 + `tdc_shm.c`가 이 헤더를 직접 include 안 함 → 미사용 의심 | 실제로는 후보_6에서 밝힌 대로 `tdc_cfx_eeprom_erase.h`의 복제 선언을 통해 `tdc_cfx_eeprom_recover_mapdata_mapping_app()`가 `tdc_shm.c:621`에서 호출됨. 헤더 자체는 죽지 않았고, include 경로가 우회되어 있을 뿐 |
| `isd/tdc_isd_map_*` 5종 FSM 관련 전 파일·헤더 | 참조 카운트가 낮거나 헤더가 서로만 include하는 구조라 사장 의심 가능 | 작업 지시서 불가침 목록에 명시(의료 자극 제어 FSM) — 조사 대상에서 원천 제외. 별도 검증 없이 후보에서 배제 |
| `cfx_cm3_sharedMemoryAll`, `FS_MEM_UART`/`FS_MEM_UART_T`, `*_IRQHandler` 8종, `Sys_*`/`SYS_*`/`hw.h`, `lib/` 원본 API, `sections.ld` | 이름·사용 패턴만 보면 일부 사장처럼 보일 수 있음 | 작업 지시서 불가침 목록 — 검증 없이 제외. `I2C_0_IRQHandler`(후보_5의 영향받는 함수)도 이 규칙으로 제거 대상에서 제외(매크로 정리만 대상이지 이 함수 자체는 아님) |

## 4. 판단 보류 · 추가 확인 필요

- **`-I` 경로의 SDK 쪽 절반(`${eclipse_home}..\include\cm3`, `${eclipse_home}..\include\shared`)**: Eclipse 워크스페이스 변수라 리포지토리 내에서 실제 경로를 해석할 수 없었다(로컬 Eclipse 설치 위치 의존). 이 두 경로 자체의 존재 여부, 그리고 그 안에 `board.h`·`FPGA.h`처럼 범용적인 이름의 SDK 헤더가 있어 flat include 전제(파일명 전역 유일성)를 깨뜨리는지는 은수님 개발 환경에서 직접 확인이 필요하다. 프로젝트 소스 트리(`source/` 전체) 내부의 85개 헤더 basename은 전부 유일함을 확인했다(중복 0건).
- **`tdc_hal_i2c_state.h`의 죽은 매크로 9개**(§3 표 참고): 파일 단위 조사 범위를 벗어나 후보 표에 올리지 않았으나, 심볼 단위 사장 조사 노드가 있다면 교차 확인 바람.
- **`dfu/tdc_dfu_ota.c`/`.h`가 왜 남아있는지**: git 이력(파일 자체의 히스토리)을 조회하면 `tdc_dfu_ble_ota`로 전환된 시점과 사유를 알 수 있을 것으로 보이나, 본 조사는 "현재 소스 상태"만 대상으로 하여 별도 `git log --follow` 조회는 수행하지 않았다.

## 5. 특이사항

- `.cproject`의 `sourceEntries`는 `<entry kind="sourcePath" name="source"/>` 단일 항목이며 `excluding` 속성이 없다 → **source/ 하위 68개 .c 파일 전부가 빠짐없이 컴파일 대상**이다(1차 리팩토링에서 이미 단일 entry로 정리된 상태 재확인, 신규 제외 없음).
- C 컴파일러 `-I` 목록 20개(도메인 16개 폴더 + `board` + `lib/SEGGER_RTT` + `lib/tiny-AES-c` + `source` 루트) + SDK 2개(`include/cm3`, `include/shared`) = 총 22건, 모두 실제 존재하는 폴더와 1:1 대응됨(폴더 스캔으로 확인). 어셈블러(`Cross ARM GNU Assembler`) `-I`는 `lib/SEGGER_RTT` 하나뿐인데, 프로젝트 내 유일한 `.S` 파일(`SEGGER_RTT_ASM_ARMv7M.S`)이 그 위치의 `SEGGER_RTT.h` 하나만 include하므로 정합함.
- grep 함정 관련: 이번 조사에서 실제로 "주석 처리된 include"(`tdc_shm.h:31-33`)와 "매크로 무조건 정의로 인한 `#ifndef` 사장 분기"(`tdc_hal_i2c.c`, 후보_5)를 둘 다 실물로 확인했다. 특히 후보_5는 지시서의 "드라이버 5종 중 1종만 생존" 사례와 동일 패턴(전처리 분기가 눈에는 보이지만 실제로는 죽어있음)이라 대표 사례로 기록해 둔다.
- 후보_1(`tdc_dfu_ota`)은 파일 단위 조사에서 나온 가장 규모가 큰 발견이나, OTA/펌웨어 업데이트 도메인이라 **제거 전 은수님의 별도 확인을 권장**한다(불가침 목록에는 없으나 도메인 민감도가 있어 자동으로 안전 등급을 매기지 않았다).
