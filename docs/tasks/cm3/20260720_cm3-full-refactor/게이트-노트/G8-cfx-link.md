---
name: G8 cfx_link 게이트 노트
purpose: G8(cfx_cm3_sharedMemory·fn_fromCFX·ci_boot·ci_ota) 이동·rename + 3프로젝트 ABI 동기화 방침
type: tasks/게이트노트
applies_to: [Sound1]
tags: [cm3, refactoring, gate, g8, cfx-link, shared-memory, abi, eeprom]
---

# G8 — cfx_link (CFX 공유 결합부) · 마지막 게이트

**TL;DR**: 공유 메모리 결합부. **핵심 원칙 = 구조체 타입/필드(ABI)는 동결, 접근자 함수는 rename**. `cfx_cm3_sharedMemory` → `cfx_link/tdc_shm_*`(접근자), `fn_fromCFX` → `cfx_link/tdc_cfx_*`, `ci_boot`/`ci_ota` → `boot/`·`dfu/`. 3프로젝트 ABI 동기화(§4)는 별도 작업으로 분리 - 이번엔 방침만 확정.

## 1. 파일 매핑

| 현행 | 신규 | 도메인 |
|---|---|---|
| `Cortex-M3-src/systemControl/cfx_cm3_sharedMemory.c/.h` | `cfx_link/tdc_shm.c/.h` | cfx_link (접근자만 rename, 구조체 동결) |
| `systemControl/cfx_cm3_shared_Memory_Addr.h`(루트 잔여?) | `cfx_link/tdc_shm_addr.h` | (존재 시) |
| `Gen1_5/fn_fromCFX/fn_from_cfx_eeprom_{read,write,erase,recover}.c/.h` | `cfx_link/tdc_cfx_eeprom_*.c/.h` | cfx_link |
| `Cortex-M3-src/ci_boot.c/.h` | `boot/tdc_boot.c/.h` | boot (bootloader 사본과 내용 상이 - 독립 rename) |
| `Gen1_5/dfu/ci_ota.c/.h` | `dfu/tdc_dfu_ota.c/.h` | dfu (G6 dfu 합류) |
| `Gen1_5/dfu/sdk_ci_boot.h` | `dfu/tdc_dfu_sdk_boot.h` | dfu (sdk_ci_ 자작 - 은수님 rename 허용) |

`Gen1_5/fn_fromCFX/` · `Gen1_5/dfu/` · `systemControl/` 소멸 → **`Cortex-M3-src/systemControl/` 완전 소멸**.

## 2. 공유 메모리 — 동결 vs rename (핵심)

| 대상 | 방침 | 근거 |
|---|---|---|
| **구조체 타입** `ST__CFX_CM3_SharedMemory_*` (16종) | **동결** | CFX·calibration 복제본 - ABI |
| **구조체 필드** (105종, `CM3_status` 등) | **동결** | 동 |
| **전역 인스턴스** `cfx_cm3_sharedMemoryAll` | **동결** | `.shared_memory` 섹션 배치, CFX 와 주소 정합 |
| **접근자 함수** (`changeSystemModeFlag` · `readBatteryLevel_FromCFX` 등) | **rename** → `tdc_shm_*` | 2__cm3 전용, 복제본 없음 |
| **파일명** `cfx_cm3_sharedMemory` | **rename** → `tdc_shm` | 2__cm3 파일 |
| 오타 `fillSepcificCommndBuffer` · `isMapdateLoaded` · `sharedMemoryAddresError` | 정정 | Sepcific/Mapdate/Addres |

> [!CAUTION]
> 헤더 상단 "공유 ABI 경고 주석"(G0)은 유지. 접근자 rename 후에도 구조체/필드/인스턴스는 diff 0 이어야 한다.

## 3. 도메인 접두어

| 도메인 | 접두어 | 예 |
|---|---|---|
| cfx_link | `tdc_shm_`(공유메모리 접근) · `tdc_cfx_`(CFX 명령) | `changeSystemModeFlag` → `tdc_shm_change_system_mode`, `fn_erase_mapData_byMapping` → `tdc_cfx_eeprom_erase_map_data` |
| boot | `tdc_boot_` | `ci_boot_handle_fsm` → `tdc_boot_handle_fsm` |
| dfu | `tdc_dfu_` | `ci_ota_*` → `tdc_dfu_ota_*` |

## 4. 3프로젝트 ABI 동기화 방침 (§부수 발견 반영)

heartbeat 작업(4a0f272) 이후 확인된 **calibration 사본 drift**: `5__calibration/include/cfx_cm3_sharedMemory.h` 가 자극 묵음 필드(2026-01-20)부터 미동기. 구조체 앞부분 prefix 동일이라 레이아웃은 안 깨지나 소스는 불일치.

**결정**: 3프로젝트(2__cm3 · 1__cfx · 5__calibration) 헤더 동기화는 **본 리팩토링 범위 밖 별도 작업**. 이유:
- calibration/CFX 는 지시 범위(2__cm3) 밖
- ABI 레이아웃은 현재 깨지지 않음(신규 breakage 아님)
- 동기화하려면 3프로젝트 동시 빌드·검증 필요

G8 은 **2__cm3 쪽 접근자·파일명만** 정리하고, 공유 구조체는 원형 유지 → 3헤더 레이아웃 정합 보존.

## 5. 불가침

| 항목 | 방침 |
|---|---|
| 공유 구조체/필드/인스턴스 | 동결 (§2) |
| 복제 헤더 교차 심볼 (동결 목록) | G0 `08_보류-동결-목록.md` 대조 |
| `PcmBitStream_Mode_*` | 값 불변 |
| SEGGER_RTT · tiny-AES-c | 라이브러리 (Gen1_5 잔류) |
| `main.c` | 리팩토링 제외 (호출부 갱신만) |

## 6. 유닛/모듈 맵

| 모듈 | 유닛 | 검증 라벨 |
|---|---|---|
| cfx_link | `tdc_shm`(공유메모리 접근) · `tdc_cfx_eeprom_*`(EEPROM 원격) | **[로직설명]** — 접근자 rename, 구조체 동결 |
| boot | `tdc_boot`(부트 상태 FSM) | **[로직설명]** |
| dfu(확장) | `tdc_dfu_ota` | **[로직설명]** |

## 7. 검증

| 라벨 | 결과 |
|---|---|
| 검증_공통_1 (구 심볼 잔존 0) | ✅ 7패턴 0 (`cfx_cm3_sharedMemory`·`fn_*`·`ci_boot_`·`ci_ota_`·`changeSystemModeFlag`·`cfx_interuupt`·`fn_from_cfx`). 잔존 2건은 주석 내 타 프로젝트 파일 경로(정상), main.c 주석 1건 갱신 |
| **검증_G8_특화 (구조체 ABI diff 0)** | ✅ **구조체 필드 변경 라인 0** — 접근자 함수 선언만 rename. 공유 필드(`CM3_status`·`CM3_tempValue1`·`chargerConnectorPluggedIn`·`isd_userName` 등) 전부 존치. 인스턴스 `cfx_cm3_sharedMemoryAll` 동결 |
| 검증_공통_3 (균형·훅) | ✅ cfx_link 11 + boot 2 + dfu 3 파일 균형 OK · 깨진 include 0 · 훅 통과 |
| 검증_공통_4 (사장 실증) | ⚠️ **제거 보류** — §9 |
| 검증_공통_5 (빌드·실기) | ✅ PASS — 은수님 확인 (2026-07-22): "빌드 후 정상동작 확인했어" → **G8 폐쇄 · 전면 리팩토링 ⑦ 구현 종료** |

> [!NOTE]
> G8 커밋(`01c5162`) 이후 `.cproject` 정리(`201cd46`)를 함께 실기 확인했다. 소멸 폴더 include 경로 13건·`excluding` 3건·`sourcePath` 1건 제거로 Eclipse 인덱서 이중 경로(상대/절대 동시 노출) 잔재를 해소. `sections.ld`(링커 `-T` 스크립트)·`linked/include`·`${eclipse_home}` SDK 경로는 원형 보존.

## 9. 사장 판정 — 제거 보류 (19건)

cfx_link 접근자 81개 중 호출 0: `tdc_cfx_eeprom_*_by_mapping`/`_to_repository`(EEPROM 원격 9) + `tdc_shm_read_battery_level_from_cfx`/`read_cfx_error_code`/`read_current_map_data` 등(공유메모리 접근 10).

**이번 게이트에서 제거하지 않는다**:
- `fn_*` EEPROM 함수는 **CFX 명령 프로토콜 완결성 자산** — mapping 앱 원격 트리거 가능성(호출 0이어도 프로토콜 일부)
- 매핑 데이터 read/write 경로라 실기 검증 없는 제거는 위험
- 자동 검출이 G1~G7 반복 오탐 — 마지막 게이트이므로 **전체 통과 후 일괄 실증**이 안전

→ 별도 사장 정리 작업(전 도메인 census 재실증)에서 판단. G5·G7 보류분과 함께.

## 8. 완결

G8 통과 시 **2__cm3 도메인 폴더 재편 완료**: `hal·drv·led·touch·ui·sys·pwr·fs·ble·qcc·dfu·isd·stim·cfx_link·boot` + `Cortex-M3-src/main.c`(제외) + `Gen1_5/{SEGGER_RTT,tiny-AES-c}`(lib) + `99_includeBoard`(보드) + `config`(루트 헤더). 전면 리팩토링 ⑦ 구현 종료 → ⑧ 구현-검증.
