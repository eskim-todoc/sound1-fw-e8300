# C. 유령 선언 · 미사용 include · 파일 단위 잔여

**총 25건** (제거 권고 25건 · 보류 권고 0건)

## 유령 함수 선언 (선언만 있고 정의가 코드베이스 어디에도 없음)

**`main.h`**

- [x] `reset_global_variables_in_main` — `main.h:43` · 정의·호출 전체 0건(main.c 포함 전수 확인) · **안전**
- [x] `debugging_for_AGC_and_LogMapping` — `main.h:50` · 정의·호출 전체 0건 · **안전**
- [x] `checkCharList` — `main.h:59` · 정의·호출 전체 0건 · **안전**
- [x] `checkChar` — `main.h:60` · 정의·호출 전체 0건 · **안전**
- [x] `debugMode` — `main.h:61` · 정의·호출 전체 0건 · **안전**
- [x] `calculate_48bit_QInFn` — `main.h:62` · 정의·호출 전체 0건 · **안전**
- [x] `calculate_24bit_QInFn` — `main.h:63` · 정의·호출 전체 0건 · **안전**
- [x] `debug_printer_for_mcuErrorCode` — `main.h:65` · 정의·호출 전체 0건 · **안전**
- [x] `debugPrint_ADCRegs` — `main.h:67` · 정의·호출 전체 0건 · **안전**
- [x] `debugPrint_ADCInput` — `main.h:68` · 정의·호출 전체 0건 · **안전**

10건 전부 `main.c`가 1차 리팩토링에서 "호출부만 갱신, 내용은 제외" 대상이었던 탓에 남은 1세대(Gen1) 잔재로 추정(노드07).

**`isd/tdc_isd.h`**

- [x] `tdc_isd_clear_command_start_flag` — `tdc_isd.h:32` · 정의·호출 전체 0건 · **안전**
- [x] `tdc_isd_is_connected` — `tdc_isd.h:33` · 정의·호출 전체 0건 · **안전**
- [x] `tdc_isd_update_link_connected` — `tdc_isd.h:34` · 정의 0건 - 유일한 호출(`tdc_isd.c:308`)도 영구 미정의 매크로 `conneded_ISDCheck_byForwardPath` 분기 안(processorDirective.h 구조상 절대 정의 안 됨) · **안전**
- [x] `tdc_isd_update_link_disconnected` — `tdc_isd.h:35` · 정의 0건 - 유일한 호출(`tdc_isd.c:319`)도 같은 죽은 분기 안 · **안전**

**`ble/tdc_ble_communication.h`**

- [x] `tdc_ble_communication_reset_globals` — `tdc_ble_communication.h:20` · 정의·호출 전체 0건(`tdc_ble_communication.c` 502줄 전체 확인) · **안전**

**`ble/tdc_ble_mapping.h`**

- [x] `tdc_ble_mapping_is_program_connected` — `tdc_ble_mapping.h:135` · 정의·호출 전체 0건. 동일 역할은 함수 스코프 `static mappingProgramConnected`(`tdc_ble_mapping.c:1880`)가 대체 수행 중 · **안전**

**`hal/tdc_hal_i2c.h`**

- [x] `tdc_hal_i2c_comm` — `tdc_hal_i2c.h:171` · 실제 정의(`tdc_hal_i2c.c:165`)가 `#else`(`TDC_HAL_I2C_USING_ISR` 상시 정의로 인한 죽은 분기) 안에 있어 전처리 후에는 정의 자체가 사라짐 - 호출 시 링크 에러 확정 · **안전**

**`cfx_link/tdc_shm.h`** (오케스트레이터 지정 17건 외 추가 발견 - 동일 패턴이라 포함, §비고 참고)

- [x] `tdc_shm_update_system_op_mode_to_cfx` — `tdc_shm.h:410` · 정의·호출 전체 0건(선언만 존재, `.c`에 구현 없음) · **안전**
- [x] `tdc_shm_read_current_map_data` — `tdc_shm.h:357` · 정의·호출 전체 0건(선언만 존재, `.c`에 구현 없음) · **안전**

## 무의미한 include (전이 의존 없음 - 삭제해도 빌드 영향 없음)

**`ble/tdc_ble_remote.c`**

- [x] `#include <board.h>` — `tdc_ble_remote.c:12` · `board.h`/`Board_OTE_ver1_5.h` 매크로 36개 전수 대조 결과 이 파일 실코드에서 사용 0건("디버깅용" 주석만 존재, 실제로는 주석 처리된 GPIO 호출 2곳뿐). board.h는 매크로만 정의(타입 없음)해 비전이적 - 삭제해도 다른 심볼 획득 경로 없음 · **안전**

**`ble/tdc_ble_remote_sp_para.c`**

- [x] `#include <board.h>` — `tdc_ble_remote_sp_para.c:8` · 동일 검증 방식으로 매크로 36개 전수 미사용 확인(주석조차 없음), 비전이적 · **안전**

## 빈 몸통 함수 (호출은 되지만 실질 동작 없음)

**`sys/tdc_sys_control.c`**

- [x] `tdc_sys_control_nrf_off_command` — `tdc_sys_control.c:38` · 본문이 주석(`// Sys_GPIO_Set_Low(DIO_NUM_NRF_ON_OFF_COMMAND);`) 하나뿐인 빈 몸통. `DIO_NUM_NRF_ON_OFF_COMMAND`는 `board/board.h`에서 값 없는 빈 매크로("NOT USED ANY MORE"). 호출부 3곳(`main.c:874,1106`, `sys/tdc_sys_init.c:356`)도 함께 정리 필요 · **안전**

**`sys/tdc_sys_init.c`**

- [x] `tdc_sys_reset_nrf` — `tdc_sys_init.c:64` · 실질 동작은 `__NOP()` 지연 400회뿐, GPIO 제어 3줄 전부 주석 처리(`DIO_NUM_NRF_SWDIO_NRESET`도 값 없는 빈 매크로). 호출부 3곳(`main.c:873,1105`, `sys/tdc_sys_init.c:352`)도 함께 정리 필요 · **안전**

## 미사용 구조체 필드 · 미사용 typedef

**`sys/tdc_sys_error.h`**

- [x] `FPGA_OR_ISD_ErrorFlag` — `tdc_sys_error.h:136` · `tdc_sys_error_code_t` 구조체 필드, 읽기/쓰기 전체 코드베이스에서 0건. CM3 로컬 전용 구조체라 `cfx_cm3_sharedMemoryAll` ABI와 무관 · **안전**

**`dfu/tdc_dfu_ble_ota.h`**

- <s>`ST__OTA_CONTROL_PACKET` — `tdc_dfu_ble_ota.h:115-119`</s> → **[제거목록-A-매크로.md](제거목록-A-매크로.md) 에 동일 항목이 있습니다.** 중복이라 이 목록에서 뺐습니다 (여기서 체크하지 마세요)

## 파일 단위 잔여 (아무도 include하지 않는 `.h` / 컴파일 대상에서 빠진 `.c`)

**조사 결과 0건.** `.cproject`의 `sourceEntries`가 단일 항목이고 `excluding` 속성이 없어 `source/` 하위 68개 `.c` 파일 전부가 컴파일 대상이며(직접 재확인), 85개 `.h` 헤더 각각이 최소 1곳 이상에서 `#include` 되고 있음을 스크립트로 재검증했다(노드08의 결론과 일치). 단, 모듈 통째 제거 대상 5건(`tdc_dfu_ota` 등)이 실제로 제거되면 그 결과로 새로 고립되는 헤더가 생길 수 있으나, 이는 해당 모듈 제거 작업의 후속 확인 사항이라 이 목록에는 넣지 않았다.

## 비고

- **isd `tdc_isd_set_stim_para_common_ground`(노드05 후보_5, `tdc_isd_stim_para_setting.h:12`) 제외**: "선언만 있고 정의 없음"이라는 점에서 유령 선언 패턴과 완전히 일치하지만, 오케스트레이터가 isd 도메인에 명시적으로 지정한 4건(`tdc_isd_update_link_connected/disconnected`, `tdc_isd_clear_command_start_flag`, `tdc_isd_is_connected`)에는 포함되지 않았다. 노드05가 위험도를 **위험**(자극 파라미터 인접)으로 매긴 항목이라 의도적 제외로 판단해 이번 목록에 넣지 않았음 - 은수님 별도 확인 권장.
- **`ble/tdc_ble_communication.h:4`의 `#include <tdc_hal_uart.h>`(노드06 후보_9) 및 `fs/tdc_fs.h`·`util/tdc_util.h`·`main.c`의 동일 패턴(노드07 5장 특이사항)**: "무의미한 include"라는 성격은 같지만 대상이 `tdc_hal_uart.h`라 지침 4항의 "UART 관련 전 항목(별도 게이트)"에 해당해 이 목록에서 제외했다.
- **`dfu/tdc_dfu_ota.c`+`.h` 전체(노드06 후보_12, 노드08 후보_1)와 그 안의 미사용 상수 31개(노드06 후보_11 일부)**: "모듈 통째 제거 5건" 중 `tdc_dfu_ota`에 해당해 제외. 단 `tdc_dfu_ble_ota.h`(별개 파일, BLE OTA 실사용 중)의 `ST__OTA_CONTROL_PACKET`은 제외 대상이 아니라 위에 포함시켰다.
- **`hal/tdc_hal_i2c_cfx.c/h` + `hal/tdc_hal_i2c_state.h` 전체(노드03 후보_1)**: 지정 5건 중 `tdc_hal_i2c_cfx` 모듈 번들로 판단해 제외(node03이 세 파일을 하나의 후보로 묶어 보고함).
- **중복 선언 3건은 "사장 아님"으로 확인되어 제외**: `tdc_isd_fpga.h:42,74`(`get_backtel_configuration_written_value`, 실은 함수 자체가 무호출이라 후보_8 소관), `tdc_shm.h:307,329`(`tdc_shm_read_connected_isd_map_stamp`, 실사용 함수의 중복 프로토타입), `cfx_link/tdc_cfx_eeprom_erase.h:26-27` ↔ `tdc_cfx_eeprom_recover.h:21-22`(실사용 함수의 복제 선언, 헤더 계층 우회 이슈). 셋 다 삭제 대상 심볼이 아니라 "정의는 하나, 선언이 중복"인 헤더 위생 문제라 이 목록(사장 코드 제거)의 범위 밖으로 판단했다. 참고용으로만 기록.
- **노드03 후보_15(`bootloader_CRC_calc()` + `tdc_pwr_clock_backup_t`/`bootloader_boot_information` 미사용 타입)**: 함수+전역+타입+매크로가 뒤섞인 묶음이며 핵심은 무호출 함수(링크 레벨 확정 사장 함수)라 판단해 함수/변수 담당 섹션 소관으로 보고 이 목록에는 넣지 않았다.
- **불가침 제외 기록**: `cfx_cm3_sharedMemoryAll`, `*_IRQHandler`, SDK `Sys_*`/`SYS_*`, `isd/tdc_isd_map_*` 5대 FSM(및 `tdc_isd_map_data`), `lib/` 원본 API - 05·07·08번 로그가 이미 조사 대상에서 배제 처리한 것을 그대로 따랐고, 이번 목록에도 후보로 올리지 않았다.
- 라인 번호는 전 항목 `Grep`으로 직접 재확인했다(로그에 기재된 라인과 실제 소스 라인이 모두 일치함을 확인).
