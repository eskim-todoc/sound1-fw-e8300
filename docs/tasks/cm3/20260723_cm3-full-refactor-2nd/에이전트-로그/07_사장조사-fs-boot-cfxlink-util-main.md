---
name: 사장 조사 - fs boot cfx_link util main (33파일)
purpose: fs·boot·cfx_link·util·main 33파일 전수 조사로 사장 코드 후보와 위험도를 기록하고, G8 이월분 19건을 재판정
type: tasks/에이전트로그
applies_to: [Sound1]
tags: [cm3, dead-code, 조사, cfx-link, fs, main]
---

# 07_사장조사-fs-boot-cfxlink-util-main

**TL;DR**: 33파일 전수 조사로 사장 후보 14그룹(고유 심볼 107건) 확인 - 안전 104 · 주의 3 · 위험 0. **G8 이월 19건 중 EEPROM 원격 9건(및 재조사로 드러난 동종 4건 포함 총 13건)은 재조사 결과 전부 생존**(내부 호출 체인을 grep이 놓친 오탐)이었고, 나머지 공유메모리 접근자 쪽에서 실제 사장 7건을 새로 확정했다. `tdc_shm_addr.h`의 오프셋 매크로 81개, `main.h`의 고아 함수 선언 10개(정의 자체가 없음)가 최대 규모 발견이다.

## 1. 조사 범위와 방법

- 대상: `fs`(12) `boot`(2) `cfx_link`(11) `util`(6) `main.c`/`main.h` = 33파일
- 1단계: `docs/tasks/cm3/20260720_cm3-full-refactor/게이트-노트/G8-cfx-link.md`를 먼저 읽고 §9 "제거 보류 19건"(EEPROM 원격 9 + 공유메모리 접근자 10)을 재판정 우선순위로 잡았다.
- 2단계: 각 헤더의 public 함수·매크로·타입·전역변수를 추출 → `source/` 전체(153파일)에서 `Grep`으로 호출부 검색.
- 3단계(그 함정 회피): 호출부가 "같은 모듈의 다른 .c 파일 안"에 있는 경우를 놓치지 않으려고, 1차 조사에서 실수로 대상 파일 자신을 grep 결과에서 제외해버린 필터링 오류를 발견 → 필터를 제거하고 전체 재검색해 EEPROM 원격 함수 체인이 실제로는 살아있음을 확인했다(§3 참고).
- 4단계: 호출이 발견되면 그 호출을 감싸는 `#if`/`#else` 조건이 실제로 컴파일되는지 값까지 추적(`TDC_PRINTF_INTERFACE`, `TDC_TOUCH_DEBUG_PRINT_ENABLE` 등).

## 2. 사장 후보 목록

| # | 대상 | 위치 | 종류 | 근거 | 위험도 |
|---|---|---|---|---|---|
| 후보_1 | `tdc_shm_is_cfx_eeprom_data_loaded` | `cfx_link/tdc_shm.c:42`, `tdc_shm.h:271` | 함수 | 정의됨(순수 getter), 레포 전체 호출 0건 | 안전 |
| 후보_2 | `tdc_shm_update_system_op_mode_to_cfx` | `cfx_link/tdc_shm.h:410` | 함수(선언만) | **`.c`에 정의 자체가 없다.** 헤더 선언뿐이고 호출도 0건(고아 프로토타입) | 안전 |
| 후보_3 | `tdc_shm_read_current_map_data` | `cfx_link/tdc_shm.h:357` | 함수(선언만) | **`.c`에 정의 자체가 없다.** 호출 0건(고아 프로토타입) | 안전 |
| 후보_4 | `tdc_shm_read_cfx_error_code` | `cfx_link/tdc_shm.c:658`, `tdc_shm.h:420` | 함수 | 정의됨(순수 getter, `cfx_cm3_sharedMemoryAll.CFX_ErrorCode` 반환), 호출 0건 | 안전 |
| 후보_5 | `tdc_shm_enter_low_power_mode_cm3_to_cfx` | `cfx_link/tdc_shm.c:144`, `tdc_shm.h:407` | 함수 | 정의됨, 호출 0건. 내부에서 `tdc_shm_on_off_3_v_pmic_cm3_to_cfx(false)` + ULP 플래그 설정 - PMIC/전원 제어에 인접 | 주의 |
| 후보_6 | `tdc_shm_read_battery_level_from_cfx` | `cfx_link/tdc_shm.c:155`, `tdc_shm.h:412` | 함수 | 정의됨, 호출 0건. 내부에서 `tdc_pwr_lsad_update()`를 트리거(ADC 갱신 부수효과 포함) | 주의 |
| 후보_7 | `tdc_shm_is_power_button_pushed` | `cfx_link/tdc_shm.c:87`, `tdc_shm.h:406` | 함수 | 정의됨, 호출 0건. `main.c:597` 주석에 "`tdc_touch_process()` 대체"라고 명시. `Sys_GPIO_Read` HW 레지스터 읽기 + ABI 필드(`powerButton_pushed_CFX_to_CM3`) 기록 겸용이라 주의 등급 | 주의 |
| 후보_8 | `tdc_shm_addr.h` 오프셋 매크로 81개 | `cfx_link/tdc_shm_addr.h:27~127` | 매크로(81건) | `Addr_SharedMem_*`/`addr_SharedMem_*`/`Addr_sharedMem_*` 계열 매크로 81개 전부 정의부(같은 헤더) 외 레포 전체 참조 0건. 실사용은 `StartAddressCM3_sharedVarialbe`·`CM3_DataMemoryBaseAddr`·`CFX_AccessAddrForCM3DataMem`·`BaseAddr_CM3CFX_SharedVariable` 4개뿐(`tdc_shm.c:59`에서 주소 검증에 사용) | 안전 |
| 후보_9 | `_set_boot_alt_try_success` | `boot/tdc_boot.c:125` | static 함수 | 유일한 호출 흔적이 193행 `//_set_boot_alt_try_success();` 주석뿐. 활성 호출 0건 | 안전 |
| 후보_10 | `TDC_PRINTF`의 도달불가 전처리 분기 4곳 | `util/tdc_printf.h:34-35`(UART 분기), `:72-99`(구버전 verbose-header 분기), `:100-115`(VT100 분기), `:117-118`(no-op else 분기) | 매크로(전처리 분기 4건) | `TDC_PRINTF_INTERFACE`가 `TDC_PRINTF_INTERFACE_SEGGER_RTT`로 고정(`tdc_printf.h:21`)이라 UART 분기(`tdc_hal_uart_printf` 호출)·구버전 verbose 포맷 분기·VT100 이스케이프 분기·no-op else 분기가 전부 컴파일에서 제외됨. **은수님이 예시로 든 `tdc_hal_uart_printf()` 사장의 실체가 바로 이 34-35행** | 안전 |
| 후보_11 | `SND_FATFS_MOUNT_OPTION` / `TDC_FS_LOGICAL_DRIVE_NUM_FOR_BOOT_STATUS` | `fs/tdc_fs.h:42`, `:34` | 매크로(2건) | 정의 후 레포 전체 참조 0건. 실사용은 `TDC_FS_MOUNT_OPTION`·`TDC_FS_LOGICAL_DRIVE_NUM`(형제 매크로) | 안전 |
| 후보_12 | `main.h` 고아 함수 선언 10건 | `main.h:43,50,59,60,61,62,63,65,67,68` | 함수(선언만, 10건) | `reset_global_variables_in_main` / `debugging_for_AGC_and_LogMapping` / `checkCharList` / `checkChar` / `debugMode` / `calculate_48bit_QInFn` / `calculate_24bit_QInFn` / `debug_printer_for_mcuErrorCode` / `debugPrint_ADCRegs` / `debugPrint_ADCInput` - **10개 전부 `.c` 구현이 코드베이스 어디에도 없고, 호출도 0건**. `main.c`가 1차 리팩토링 제외 대상이었기 때문에 남아있던 것으로 추정 | 안전 |
| 후보_13 | `aes128_test()` | `main.c:277` (정의), `:335` (유일한 호출 흔적) | 함수 | 유일한 호출부가 `// aes128_test();`로 주석 처리(335행). 그 외 호출 0건. AES 라이브러리 자체 검증용 데모 함수로, 호출돼도 부작용 없이 지역 변수만 조작 | 안전 |
| 후보_14 | `OTE_1_5_GEN_TEST_WITHOUT_CFX` | `main.h:37` | 매크로 | 정의 후 `#if` 등 어디에도 참조되지 않음(레포 전체 0건) | 안전 |

**요약 카운트**: 안전 104 · 주의 3 · 위험 0 (합계 107건, 14그룹)

## 3. 사장 아님으로 판정한 것 (오탐 방지 기록)

| 대상 | 위치 | 최초 인상 | 실제 판정 · 근거 |
|---|---|---|---|
| `tdc_cfx_eeprom_copy_isd_info_to_repository` / `_copy_mapping_data_to_repository` / `_copy_user_setting_parameters_to_repository` / `_copy_map_stamp_to_repository`(4) + `tdc_cfx_eeprom_write_map_data_by_mapping` / `_write_isd_info_by_mapping` / `_write_user_setting_parameters_by_mapping` / `_write_map_stamp_parameters_by_mapping`(4) + `tdc_cfx_eeprom_erase_map_stamp_by_mapping` / `_erase_map_data_by_mapping` / `_erase_isd_info_by_mapping` / `_erase_user_setting_parameters_by_mapping`(4) + `tdc_cfx_eeprom_recover_map_data_by_mapping`(1) = **13건** | `cfx_link/tdc_cfx_eeprom_{read,write,erase,recover}.c` | G8 게이트노트(§9)가 "EEPROM 원격 9건, 호출 0"이라 기록. 개별 함수만 grep하면 실제로 외부 호출 0건으로 보인다 | **전부 생존.** `tdc_shm_set_read_write_map_data_flash_command()`(`cfx_link/tdc_shm.c:589`, 외부 호출처는 `isd/tdc_isd_map_data.c` 10곳)가 `tdc_cfx_{read,write,erase,recover}_mapdata_mapping_app()`을 호출하고, 그 각 `_mapping_app` 함수가 다시 `_by_mapping`/`_to_repository` 함수들을 호출하는 3단 체인이 살아있다(`tdc_cfx_eeprom_write.c:79-98`, `erase.c:184-204`, `recover.c:109-132`, `read.c:125-138`). **G8 시점 grep이 "같은 파일 안에서의 호출"을 놓친 사례** - 과제 3항이 경고한 grep 함정이 그대로 재현됨 |
| `FS_MEM_UART` / `FS_MEM_UART_T` | `hal/tdc_hal_uart.h:133,138` | 이름이 UART라 사장으로 오인 가능 | **불가침**(과제 지정). `cfx_link/tdc_shm.c:283`에서 CM3→CFX 공유메모리로 실사용 확인(`tdc_shm_set_flag_audio_parameters_calculation_done_cm3_to_cfx()` 내부, `#if 1` 활성 분기) |
| `cfx_cm3_sharedMemoryAll` 구조체·필드·인스턴스 | `cfx_link/tdc_shm.h` | - | **불가침**(과제 지정, CFX·calibration ABI) |
| `g_cm3_manu_reserved` | `main.c:100` | 레포 내 심볼 참조자 0건 | `__attribute__((section(".cm3_manu_reserved"),used,aligned(4)))`로 **명시적으로 링커 보존 지정**. 생산/검사 장비가 고정 주소로 직접 접근하는 제조 예약 영역으로 추정 - `used` 속성 자체가 저자의 의도적 보존 신호 |
| `tdc_shm_set_command_map_change_cm3_to_cfx` | `cfx_link/tdc_shm.c:220` | 외부 호출처 0건이라 사장으로 보임 | 같은 파일의 `tdc_shm_change_program_map_num()`(외부에서 `ble/`·`isd/`·`ui/`·`main.c`가 다수 호출)이 내부적으로 호출(`tdc_shm.c:482`) |
| `tdc_fs_fft_read_window_coeff` / `tdc_fs_fft_write_window_coeff` | `fs/tdc_fs_fft.c:579,590` | 외부 호출처 0건 | 같은 파일의 `tdc_fs_fft_init_window_coeff()`(`sys/tdc_sys_init.c`에서 호출)가 내부적으로 순서대로 호출(`tdc_fs_fft.c:558,562`) |
| `IRQHandler` 8종(`CFX_0_IRQHandler` 포함) | `main.h:41` 등 | - | **불가침**(과제 지정, SDK 벡터테이블 이름 결속) |

## 4. 판단 보류 · 추가 확인 필요

| 대상 | 위치 | 보류 사유 |
|---|---|---|
| `DEV_FW_VER_BETA` / `DEV_FW_VER_RND` / `DEV_FW_VER_HW_TEST` / `DEV_FW_VER_SW_TEST` | `main.h:29,31-33` | `devFwVer_type = DEV_FW_VER_RELEASE`(`main.c:119`)만 실사용, 나머지 3개는 참조 0건. 다만 `TDC_PRINTF_INTERFACE_UART`처럼 개발자가 손으로 골라 쓰는 빌드 라벨 상수 성격이라 "사장"과 "미사용 옵션"의 경계가 애매하다. 완전 삭제 여부는 은수님 확인 필요 |
| `tdc_shm_addr.h` 오프셋 매크로 81개(후보_8) | `cfx_link/tdc_shm_addr.h` | 기능적으로는 안전 확정이나, G8 §4에 예고된 "3-프로젝트(2__cm3/1__cfx/5__calibration) ABI 동기화" 별도 작업에서 레이아웃 대조용 참고자료로 쓰일 가능성이 있다. 삭제 타이밍(이번 사장정리 vs ABI 동기화 작업 이후)은 확인 필요 |
| `main.h` 고아 선언 10건(후보_12) | `main.h` | 기능적 위험은 0(정의 자체가 없어 링크도 안 됨)이나, 왜 이렇게 오래 방치됐는지(1세대 Gen1 잔재로 추정) 히스토리 확인은 은수님 판단 영역 |
| `tdc_shm_enter_low_power_mode_cm3_to_cfx` / `tdc_shm_read_battery_level_from_cfx` / `tdc_shm_is_power_button_pushed`(후보_5,6,7) | `cfx_link/tdc_shm.c` | HW(PMIC/ADC/GPIO) 인접 함수라 삭제 전 실기 검증 권장(과제 §4 "주의" 기준 그대로 적용) |

## 5. 특이사항

- **`tdc_fs.h`가 `tdc_hal_uart.h`를 include하는 이유(과제 지정 질문 답변)**: `fs/*.c` 어디에도 `FS_MEM_UART`·`tdc_hal_uart_*`·`TDC_HAL_UART_*` 심볼을 실제로 쓰는 곳이 없다(grep 0건). 동일 패턴이 `util/tdc_util.h`·`main.c`에도 있다. 세 파일 모두 이미 `tdc_printf.h`를 include하고 있고, `tdc_printf.h` 자신이 `tdc_hal_uart.h`를 include(`util/tdc_printf.h:15`, UART 분기용 `tdc_hal_uart_printf` 선언 목적)하므로 각 파일의 직접 include는 사실상 중복(전이적으로 이미 보임)으로 추정된다. 은수님이 예시로 든 "`tdc_hal_uart_printf()`가 정의돼 있지만 호출 0건" 메커니즘의 실체를 `util/tdc_printf.h`에서 직접 확인했다: `TDC_PRINTF_INTERFACE = TDC_PRINTF_INTERFACE_SEGGER_RTT`(21행) 고정 때문에 `TDC_PRINTF_INTERFACE_UART` 분기(34-35행, `tdc_hal_uart_printf` 호출 매크로)가 컴파일에서 영구 제외된다. `hal/tdc_hal_uart.c`의 `tdc_hal_uart_printf()` 자체는 hal 담당 에이전트 범위라 이 로그에서는 참고로만 기재.
- `CFX_0_IRQHandler`가 `main.h:41`에 선언돼 있지만 실제 정의는 `hal/tdc_hal_cfx_tick.c`에 있다(그 파일 전용 헤더 `tdc_hal_cfx_tick.h` 자체가 없음). 불가침 항목이라 제거 후보는 아니지만, 도메인 분리 관점에서 향후 hal 쪽 헤더로 선언을 이관할 여지가 있는 구조적 잔재다.
- `tdc_shm.h` 안에서 `tdc_shm_read_connected_isd_map_stamp`가 307행과 329행에 동일 시그니처로 중복 선언돼 있다(컴파일은 통과, 무해). 사장은 아니고 정리 대상.
- `tdc_cfx_eeprom_recover_map_data_by_mapping` / `tdc_cfx_eeprom_recover_mapdata_mapping_app`이 `tdc_cfx_eeprom_erase.h`(26-27행)와 `tdc_cfx_eeprom_recover.h`(21-22행) 양쪽에 중복 선언돼 있다. 사장은 아니고 정리 대상.
- `main.c`는 1차 리팩토링에서 "호출부만 갱신, 내용 자체는 제외" 대상이었는데, 이번 조사로 그 "제외"의 실체가 확인됐다 - `main.h`에 1세대(Gen1) 잔재로 추정되는 고아 함수 선언 10건(후보_12)이 그대로 남아 있었다.
- `main.c:930-966`에 `tdc_touch_sleep_log_debug()`가 `#if (TDC_TOUCH_DEBUG_PRINT_ENABLE) ... #else ... #endif`로 두 벌 정의돼 있어 처음엔 중복 정의 버그로 의심했으나, 상호 배타적 조건부 컴파일로 정상 패턴임을 확인(`TDC_TOUCH_DEBUG_PRINT_ENABLE`은 `touch/tdc_touch_config.h:22`에서 1로 정의돼 있어 실제로는 위쪽 `#if` 분기가 산다). 사장 아님.
