---
name: 사장 조사 - hal drv sys pwr (35파일)
purpose: hal/drv/sys/pwr 35파일 전수 조사로 사장 코드 후보 16건(안전 11 · 주의 5 · 위험 0) 도출
type: tasks/에이전트로그
applies_to: [Sound1]
tags: [cm3, dead-code, 조사]
---

# 03_사장조사-hal-drv-sys-pwr

**TL;DR**: hal(19)·drv(2)·sys(8)·pwr(6) 총 35파일의 public 함수·전역·매크로·타입을 전수 대조했다. 사장 후보 16건(안전 11 · 주의 5 · 위험 0)을 찾았다. 핵심은 CFX 경유 I2C 제어 경로(hal_i2c_cfx 계열) 전체, sys_earpiece.c 전체, pwr_lsad.c 전체(LSAD 배터리 측정)가 컴파일 조건/호출부 주석 처리로 완전히 도달 불가능하다는 것이다.

## 1. 조사 범위와 방법

- 대상: `hal`(19파일) `drv`(2파일) `sys`(8파일) `pwr`(6파일) = 35파일, `source/` 기준 상대경로.
- `uart` 계열(`hal/tdc_hal_uart.c/h`)은 노드1 담당이라 본 조사에서 제외했다.
- 각 파일의 모든 public 함수·전역변수·매크로·타입에 대해 `Grep`으로 참조처를 찾고, 호출부가 **살아있는 전처리 분기 안**에 있는지 파일을 직접 열어 확인했다(`#ifdef`/`#if 0`/`#else` 추적).
- 특히 아래 두 매크로가 상시 활성(고정값)임을 `board/processorDirective.h`에서 확인한 뒤, 이 매크로들이 게이팅하는 `#else` 분기 전체를 죽은 코드로 판정했다:
  - `CM3_I2C_controls_FPAG` (board/processorDirective.h:105, `#ifndef` 없이 상시 정의) → FPGA I2C 제어는 항상 `tdc_hal_i2c_isd_*` 경로, `tdc_hal_i2c_cfx_*` 경로는 항상 죽음.
  - `TDC_HAL_I2C_USING_ISR` (hal/tdc_hal_i2c.h:9, 상시 정의) → I2C 통신은 항상 ISR 방식, 폴링 방식 코드는 항상 죽음.
- 함수가 "호출은 되는데 몸통이 비어있는" 패턴(NRF on/off·reset 계열)은 board.h의 `DIO_NUM_NRF_*` 매크로가 빈 매크로(`#define X` 값 없음, "NOT USED ANY MORE" 주석)로 정의된 것과 교차 확인했다.
- IRQ가 실제로 NVIC에서 활성화되는지(`NVIC_EnableIRQ`)도 같이 확인해 "핸들러는 있지만 절대 안 걸리는" 사례(CFX_1, LSAD)를 찾았다. 단, `*_IRQHandler` 자체는 지침에 따라 후보에서 제외했다(2장 표에는 등장하지 않음, 3장에 근거만 기록).

## 2. 사장 후보 목록

| # | 대상 | 위치 | 종류 | 근거 | 위험도 |
|---|---|---|---|---|---|
| 후보_1 | CFX 경유 I2C 제어 경로 전체: `tdc_hal_i2c_cfx_write/read`, `tdc_hal_i2c_cfx.h`, `tdc_hal_i2c_state.h` 전체 매크로 | hal/tdc_hal_i2c_cfx.c:23-86, hal/tdc_hal_i2c_cfx.h:9,11, hal/tdc_hal_i2c_state.h(전체) | 함수+헤더+매크로 세트 | `CM3_I2C_controls_FPAG`가 board/processorDirective.h:105에서 상시 정의 → isd/tdc_isd_fpga.c의 23개 `#ifdef CM3_I2C_controls_FPAG / #else tdc_hal_i2c_cfx_* / #endif` 블록 모두 `#else`(죽음)만 해당. sys/tdc_sys_earpiece.c:62-106의 호출은 그마저 바깥쪽 `#if 0`(58행)에 이중으로 갇혀 있음. `CFX_1_IRQHandler`(hal/tdc_hal_i2c_cfx.c:17)가 쓰는 `cfx_i2c_done`도 `NVIC_EnableIRQ(CFX_1_IRQn)`가 sys/tdc_sys_init.c:133에서 주석 처리돼 있어 인터럽트 자체가 안 걸림 | 주의 |
| 후보_2 | I2C 폴링(non-ISR) 대체 구현: `tdc_hal_i2c_comm()` 선언부 + switch-case 죽은 분기 2개 | hal/tdc_hal_i2c.h:9(매크로),171(선언), hal/tdc_hal_i2c.c:162-166(#ifdef 분기),231-237,295-301(dead case) | 함수+분기 | `TDC_HAL_I2C_USING_ISR`가 hal/tdc_hal_i2c.h:9에서 상시 정의 → `tdc_hal_i2c_comm()`이라는 이름의 함수는 실제로 정의된 적이 없음(항상 `I2C_0_IRQHandler`로 컴파일). 헤더 선언만 존재하는 유령 심볼. `i2c_state_WritingDone`/`ReadingDone` case(231-237, 295-301)도 `#ifndef TDC_HAL_I2C_USING_ISR`라 항상 제외 - 이 안에는 `tdc_hal_i2c_driver_t.i2c_diver_state`(타입명을 변수처럼 오용)라는 버그도 있으나 죽은 코드라 무해 | 안전 |
| 후보_3 | 미정의 IRQHandler 선언 3종: `SPI_1_RX_IRQHandler`, `SPI_1_TX_IRQHandler`, `SPI_1_COM_IRQHandler` | hal/tdc_hal_spi.h:39-41 | 헤더 선언 | 코드베이스 전체에서 정의도 호출도 0건. 실제 살아있는 핸들러는 언더스코어 없는 `SPI1_COM_IRQHandler`(hal/tdc_hal_spi.c:52, `SPI1_COM_IRQn`과 명명 일치). 헤더에 남은 typo성 유령 선언 - `*_IRQHandler` 불가침 규칙은 "실제 정의돼 벡터테이블에 결속된 핸들러"에 적용되는 것이라 이 3개(정의 자체가 없음)는 대상 아님 | 안전 |
| 후보_4 | 미사용 DIO CFG 매크로 9종: `OTE_1_5_GEN_DIO_CFG_NORMAL_EARPIECE_DET_N/_CHG_DET_N/_CASE_DET/_CASE_OPEN`, `_LP_EARPIECE_DET_N/_CHG_DET_N/_CASE_DET/_CASE_OPEN/_ACCEL_INT` | hal/tdc_hal_dio.h:26-37 | 매크로 9개 | NORMAL 4종은 dio.c:70에서 주석(`//`) 속에서만, 또는 아예 미참조. LP 5종은 전부 tdc_hal_dio_configure_sleep()의 `#if 0`(dio.c:84-105) 안에서만 참조 - 10개 CFG 매크로 중 `OTE_1_5_GEN_DIO_CFG_NORMAL_ACCEL_INT`(dio.c:73) 1개만 생존 | 안전 |
| 후보_5 | write-only 인터럽트 플래그: `s_int_flag_acc_sensor`, `s_int_flag_case_lid_open` 및 setter 2종 | hal/tdc_hal_dio.c:7-8(변수),10-17(setter),138,146(IRQHandler에서 호출) | 전역변수+함수 | `tdc_hal_dio_set_int_flag_acc_sensor/case_lid_open()`은 DIO_0/1_IRQHandler에서 호출되어 플래그를 1로 세팅하지만, 두 플래그를 **읽는 코드가 전체 코드베이스에 0건**. 순수 write-only 상태 - 다만 호출부가 불가침 대상인 IRQHandler 본문 안이라 제거하려면 그 본문도 같이 손대야 함 | 주의 |
| 후보_6 | NRF on/off 제어 잔재 3종: `tdc_sys_control_nrf_off_command()`(빈 몸통, 호출 3곳), `tdc_sys_control_nrf_on_command()`(무호출), `NRF_adv_powerMode()`(#if 0) | sys/tdc_sys_control.c:38-46(두 함수),105,109(주석 처리된 호출),113-121(#if 0) | 함수 3개 | `tdc_sys_control_nrf_off_command()`는 main.c:874,1106 / sys/tdc_sys_init.c:356 에서 호출되지만 몸통은 `// Sys_GPIO_Set_Low(DIO_NUM_NRF_ON_OFF_COMMAND);` 뿐. `DIO_NUM_NRF_ON_OFF_COMMAND`는 board/board.h:28에서 값 없는 빈 매크로("NOT USED ANY MODE"). `tdc_sys_control_nrf_on_command()`는 유일한 호출 후보(sys_control.c:109)마저 주석. `NRF_adv_powerMode()`는 이미 `#if 0`에 갇혀 있고 존재하지 않는 매크로 `ENABLE_NRF_ADV_LowPower`를 참조(정의 0건) | 안전 |
| 후보_7 | `tdc_sys_reset_nrf()` - 몸통이 사실상 빈 함수(NOP 지연만) | sys/tdc_sys_init.c:64-81(정의), 352(호출),  main.c:873,1105(호출) | 함수 | 3곳에서 호출되지만 실제 GPIO 조작 3줄이 전부 주석 처리(`// Sys_GPIO_Set_High/Low(DIO_NUM_NRF_SWDIO_NRESET)`). `DIO_NUM_NRF_SWDIO_NRESET`은 board/board.h:22에서 값 없는 빈 매크로("NOT USED ANY MORE"). 실질 동작은 `__NOP()` 400회 두 묶음뿐 | 안전 |
| 후보_8 | `reset_interrupt_Disable_PRIMASK()` - 무호출 함수 | sys/tdc_sys_init.c:83-97 | 함수 | 헤더(tdc_sys_init.h)에 선언 없음. 코드베이스 전체에서 유일한 참조가 같은 파일의 주석 처리된 호출(sys_init.c:360, `// reset_interrupt_Disable_PRIMASK();`) 뿐 | 안전 |
| 후보_9 | `error_toggler()`, `error_blink()` - 무호출 진단 루틴 | sys/tdc_sys_init.c:199-230 | 함수 2개 | 헤더에 선언 없음. `error_blink()`가 내부에서 `error_toggler()`를 호출하는 것 외에 두 함수 모두 외부 호출 0건. LED를 무한 토글하는 에러 표시용 루틴으로 보이나 아무도 부르지 않음 | 안전 |
| 후보_10 | `FPGA_OR_ISD_ErrorFlag` - 미사용 구조체 필드 | sys/tdc_sys_error.h:136 | 구조체 필드 | `tdc_sys_error_code_t.FPGA_OR_ISD_ErrorFlag`를 읽거나 쓰는 코드가 전체 코드베이스에 0건(선언 줄만 매치). 이 구조체는 CM3 로컬 전용이며 `cfx_cm3_sharedMemoryAll`에 속하지 않아 ABI 문제 없음 | 안전 |
| 후보_11 | `sys/tdc_sys_earpiece.c` 전체 파일(112줄) - `tdc_sys_earpiece_update_status()` | sys/tdc_sys_earpiece.c(전체), sys/tdc_sys_earpiece.h:10, main.c:695(유일 호출부, 주석 처리) | 파일 전체 | 유일한 호출부가 `main.c:695`에서 `// tdc_sys_earpiece_update_status();`로 주석 처리되어 전체 파일이 도달 불가능. 내부적으로 `tdc_shm_update_earpiece_detection_value_to_cfx()`(cfx_link/tdc_shm.c:161)를 호출하는데 이 함수의 호출부도 전체 코드베이스에서 이 죽은 파일뿐 - `earpieceDetecion` 공유메모리 필드가 CM3 쪽에서 사실상 갱신되지 않는 상태 | 주의 |
| 후보_12 | `pwr/tdc_pwr_lsad.c` 전체 모듈(LSAD 기반 배터리 측정) - `tdc_pwr_lsad_init/uninit/get_count/update` | pwr/tdc_pwr_lsad.c(전체), sys/tdc_sys_init.c:363(주석 처리된 init 호출),480,488,491(#if 0 블록), cfx_link/tdc_shm.c:155-159(유일한 update 호출부, 그 자체도 무호출) | 파일 전체 | `tdc_pwr_lsad_init()`의 유일한 호출 후보(sys_init.c:363)가 주석 처리(`// tdc_pwr_lsad_init();  // 배터리 측정을 위한 초기화`). `tdc_pwr_lsad_uninit()`은 전체 코드베이스 무호출. `tdc_pwr_lsad_get_count()`는 sys_init.c의 `#if 0`(476-493) 안에서만 참조. `tdc_pwr_lsad_update()`는 `tdc_shm_read_battery_level_from_cfx()`(cfx_link/tdc_shm.c:155)가 유일한 호출자인데, 이 함수 자체도 전체 코드베이스에서 호출 0건. 배터리는 이제 QCC 0x34로 공급(main.c:596,721,790-801, ble/tdc_ble_communication.c:100-152에서 `tdc_pwr_battery_get/set_percent/state` 확인) - LSAD 경로는 그 이전 세대 유산. `LSAD_IRQHandler`는 불가침이라 후보에서 제외하지만, `tdc_pwr_lsad_init()`이 죽어있어 `NVIC_EnableIRQ(LSAD_IRQn)`도 실행되지 않아 이 핸들러는 현재 런타임에 걸리지 않음(참고용 근거) | 주의 |
| 후보_13 | `tdc_pwr_battery_calculate_boundary()` 및 관련 매크로/전역 (`batteryBoundary`, `calculated_3V_value`, `df_battery_boundary_*` 12개 매크로) | pwr/tdc_pwr_battery.c:130-279 | 함수+전역+매크로 | 유일한 호출자가 `tdc_pwr_lsad_init()`(pwr/tdc_pwr_lsad.c:59, 후보_12로 사장 판정됨)뿐이라 연쇄적으로 죽음. `calculated_3V_value`(battery.c:178)는 선언만 되고 대입/참조 0건 | 주의 |
| 후보_14 | `battery_percentage` - 미사용 전역 | pwr/tdc_pwr_battery.c:281 | 전역변수 | 코드베이스 전체에서 유일한 참조가 같은 파일의 주석 처리된 줄(285행, `// return battery_percentage;`) 뿐. 실제 반환값은 `s_tdc_pwr_battery_percent`(286행) | 안전 |
| 후보_15 | `bootloader_CRC_calc()` + `s_boot_info` + 보관용 타입/매크로 (`tdc_pwr_clock_backup_t`, `bootloader_boot_information`, `TDC_PWR_CLOCK_NORMAL/_STANDBY/_PREHEAT`, `NVM_BOOT_INFO_OFFSET[_OCTETS]`, `NVM_BOOT_INFO_SIZE_OCTETS`) | pwr/tdc_pwr_clock.c:7-11,16,19-36, pwr/tdc_pwr_clock.h:27-29,33-45,52-95 | 함수+전역+타입+매크로 | `bootloader_CRC_calc()`는 헤더에 선언 없고 코드베이스 전체 호출 0건(SDK CRC 레지스터를 쓰는 완결된 함수지만 아무도 안 부름). `s_boot_info` 배열은 선언만 되고 파일 내에서도 참조 0건. `tdc_pwr_clock_backup_t`/`bootloader_boot_information` 타입으로 선언된 변수가 코드베이스에 0건. `TDC_PWR_CLOCK_NORMAL/_STANDBY/_PREHEAT`(값 1/2/3)도 정의 자체 외 참조 0건 | 안전 |
| 후보_16 | ISL9122 PMIC 레지스터 매크로 6종: `TDC_DRV_ISL9122_REG_INTFLAG_MASK`, `TDC_DRV_PMIC_DEFAULTVALUE_INTFLAG_MAS`, `TDC_DRV_PMIC_BITPOSITION_OC_FAULT_MODE`, `TDC_DRV_PMIC_BITLENGTH_OC_FAULT_MODE`, `TDC_DRV_PMIC_BITPOSITION_TYPE`, `TDC_DRV_PMIC_BITLENGTH__TYPE` | drv/tdc_drv_isl9122.h:11,14,16-17,19-20 | 매크로 6개 | 앞 4개는 `tdc_drv_isl9122_reset()`의 `#if 0`(drv/tdc_drv_isl9122.c:92-101) 안에서만 참조. `BITPOSITION_TYPE`/`BITLENGTH__TYPE` 2개는 죽은 코드 안에서조차 참조 0건(완전 고아 매크로) | 안전 |

## 3. 사장 아님으로 판정한 것 (오탐 방지 기록)

| 대상 | 위치 | 판정 이유 |
|---|---|---|
| `cfx_cm3_sharedMemoryAll` 구조체 | cfx_link/tdc_shm.h 등 | 지침 4항 불가침 - CFX·calibration ABI. 후보_1·11·12에서 이 구조체를 쓰는 CM3측 "로직 함수"만 사장으로 판정했고, 구조체 자체·필드 레이아웃은 그대로 유지 대상 |
| `*_IRQHandler` 전체: `CFX_0`,`FIFO_5`(hal/tdc_hal_cfx_tick.c), `DIO_0`,`DIO_1`(hal/tdc_hal_dio.c), `I2C_0`(hal/tdc_hal_i2c.c), `SPI1_COM`,`DMA0`,`DMA1`(hal/tdc_hal_spi.c), `TIMER_3`(hal/tdc_hal_timer.c), `CFX_1`(hal/tdc_hal_i2c_cfx.c), `LSAD`(pwr/tdc_pwr_lsad.c) | 각 파일 | 지침 4항 불가침. `CFX_1_IRQHandler`와 `LSAD_IRQHandler`는 실제로 NVIC에서 활성화되지 않는 것으로 확인했지만(후보_1, 후보_12 근거), 핸들러 자체는 규칙대로 후보에서 제외했다 |
| `hal/tdc_hal_trims.c` 전체 (12개 함수) | hal/tdc_hal_trims.c/h | ON Semi SDK 포팅 스타일(`chess_storage`, `MANU_TABLE_Type` 등)이라 사장처럼 보이지만, `pwr/tdc_pwr_clock.c:86-148`의 `tdc_pwr_clock_normal()`이 12개 함수 전부를 순서대로 호출한다. 전량 생존 |
| `hal/tdc_hal_i2c_isd.c`의 `tdc_hal_i2c_isd_write/read` | hal/tdc_hal_i2c_isd.c | 표면적으로 후보_1(cfx 계열)과 대칭 구조라 함께 죽었을 것으로 오판하기 쉬우나, `CM3_I2C_controls_FPAG`가 상시 정의라 이쪽이 **살아있는 분기**다. isd/tdc_isd_fpga.c 23곳, sys/tdc_sys_earpiece.c(비록 그 파일 자체는 죽음)에서 `#ifdef` 쪽으로 호출됨 - 그렙 함정 사례 그대로 |
| `tdc_drv_isl9122_reset()` | drv/tdc_drv_isl9122.c:86 | sys/tdc_sys_init.c:379의 호출은 `#if 0` 안(죽음)이지만, isd/tdc_isd_init_fpga.c:64 (`case 10:` 라이브 상태머신 분기)에서 살아있는 호출이 있음 - 같은 심볼이 파일에 따라 죽고 사는 사례. 전체적으로 생존 판정 |
| `hal/tdc_hal_dma.h`의 `TDC_HAL_DMA0/1_CFG0/CTRL/STATUS` 6개 매크로 | hal/tdc_hal_dma.h | hal/tdc_hal_spi.c:233-244(`tdc_hal_spi_enable_dma()`)에서 실사용. 헤더만 봐서는 고아 매크로처럼 보이나 유일한 소비처가 있고 그 소비처가 살아있음 |
| `tdc_pwr_battery_read_percentage()` | pwr/tdc_pwr_battery.c:283 | `tdc_pwr_battery_get_percent()`와 기능이 완전히 중복되지만 isd/tdc_isd_map_live.c:460, ble/tdc_ble_remote.c:967, ble/tdc_ble_mapping.c:2076 3곳에서 실제로 호출된다. 사장은 아니고 "중복 구현" 이슈로 5장에 기록만 함 |

## 4. 판단 보류 · 추가 확인 필요

- 후보_1(CFX 경유 I2C 제어): FPGA I2C 제어를 CM3 직접 방식에서 CFX 경유 방식으로 되돌릴 계획이 향후에도 없는지 확인 필요. 없다면 `tdc_hal_i2c_cfx.*` + `tdc_hal_i2c_state.h` + isd_fpga.c/earpiece.c의 `#else` 분기까지 함께 정리하는 게 좋다.
- 후보_5(DIO 인터럽트 플래그): 가속도계·케이스 뚜껑 오픈 감지를 향후 다시 쓸 계획(TODO)인지, 완전 폐기된 기능인지 확인 필요. "가속도 센서(MIS2DH)는 더 이상 사용하지 않는다 - 드라이버 제거(2026-07-22)"(sys/tdc_sys_init.c:469 주석)로 미루어 가속도 쪽은 폐기 확정으로 보이나, 케이스 뚜껑 오픈(`case_lid_open`)은 별개 기능이라 별도 확인 필요.
- 후보_11(sys_earpiece.c): 이어피스 감지가 QCC 쪽으로 완전 이관된 것인지, 아직 이관 예정(미완성 TODO)인지 코드만으로는 판단 불가. 코드베이스 안에서 QCC발 이어피스 감지 대체 경로를 찾지 못했다(배터리·충전기와 달리 "QCC 0x34로 대체" 같은 명시 주석이 없음).
- 후보_12·13(LSAD 배터리 측정): 완전 제거할지, QCC 통신 두절 시의 비상 폴백으로 남겨둘지는 은수님 판단이 필요해 보인다. 현재는 "더 이상 EZ에서 배터리 측정하지 않음"(sys/tdc_sys_init.c:362,473 주석)이라는 명시적 폐기 의도가 있어 안전 제거 쪽에 무게가 실린다.

## 5. 특이사항

- **그렙 함정 재확인**: 지침에서 예고한 대로, `tdc_hal_i2c_cfx_*`와 `tdc_hal_i2c_isd_*`는 호출부 문자열만 보면 둘 다 "쓰이고 있다"로 보인다(isd/tdc_isd_fpga.c에 각 12쌍). 실제로는 `#ifdef CM3_I2C_controls_FPAG`가 매 호출부를 가르는데, 이 매크로가 board/processorDirective.h:105에서 조건 없이 상시 정의되어 있어 `isd_*` 쪽만 생존한다. 표면적 호출 횟수(둘 다 12회 내외)가 비슷해서 더욱 오판하기 쉬운 구조였다.
- **버그가 죽은 코드 속에 숨어있던 사례**: hal/tdc_hal_i2c.c:234,298의 `tdc_hal_i2c_driver_t.i2c_diver_state = ...`는 타입명(`tdc_hal_i2c_driver_t`)을 변수처럼 대입하는 코드로, 살아있었다면 컴파일 에러였을 코드다. 다행히 `#ifndef TDC_HAL_I2C_USING_ISR` 안이라 항상 제외되어 무해하다.
- **"호출되지만 빈 몸통" 패턴이 하나가 아니라 세트**: 은수님이 예시로 든 UART(`TDC_PRINTF`) 패턴과 동일한 구조가 NRF 제어 쪽에 두 벌 있었다 - `tdc_sys_control_nrf_off_command()`(후보_6)와 `tdc_sys_reset_nrf()`(후보_7). 둘 다 `board/board.h`의 `DIO_NUM_NRF_*` 매크로가 "NOT USED ANY MORE/MODE" 주석과 함께 빈 매크로로 격하되어 있어, NRF 관련 GPIO 제어 자체가 하드웨어 리비전에서 통째로 빠진 것으로 보인다.
- **배터리 측정 경로 세대 교체**: pwr 도메인 사장 후보(12,13,14)는 전부 "EZ(CM3)가 직접 LSAD로 배터리를 재는 방식"에서 "QCC가 0x34로 배터리 정보를 주는 방식"으로 세대 교체되며 남은 잔재라는 공통 원인을 갖는다. main.c 여러 곳의 "QCC 제공" 주석과 ble/tdc_ble_communication.c의 실 사용처가 이를 뒷받침한다.
- **이어피스 감지도 같은 계열일 가능성**: sys_earpiece.c(후보_11)와 hal_dio.c의 EARPIECE/CASE 계열 DIO 설정(후보_4)이 동시에 죽어있는 것은 우연이 아니라, 배터리·충전기와 마찬가지로 이어피스 감지도 GPIO 직접 감지에서 다른 경로(QCC 또는 미이관)로 넘어가는 중일 가능성을 시사한다. 다만 이쪽은 대체 경로를 코드에서 확인하지 못해 4장에 보류로 남겼다.
- **파일 커버리지**: hal 19파일(cfx_tick, dio.c/h, dma.h, i2c.c/h, i2c_cfx.c/h, i2c_isd.c/h, i2c_state.h, spi.c/h, timer.c/h, trims.c/h, uart.c/h) 중 uart.c/h 2파일은 노드1 담당이라 제외하고 17파일을 조사했고, drv 2·sys 8·pwr 6은 전량 조사했다.
