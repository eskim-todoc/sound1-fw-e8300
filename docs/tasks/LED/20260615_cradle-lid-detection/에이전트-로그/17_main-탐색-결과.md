---
name: 에이전트-17-결과
purpose: main.c func_normal() 삽입 위치·isd_state·func_sleep() 위치 탐색 결과
type: agent-log
maturity: draft
tags: [cradle, main, 구현, 탐색]
---

# 에이전트 17 결과 요약

**TL;DR**: isd_state 335라인. systemControl() 488라인. bleCommunication() 511라인. systemOff 체크 737라인. SYS_WAIT_FOR_INTERRUPT 777라인. func_sleep() 797라인. 모든 필요 함수 include 경로 확인됨.

## 주요 라인

| 항목 | 라인 |
|---|---|
| `isd_state` 선언 (`ST__ISD_STATUS`) | 335 |
| `systemControl()` 호출 | 488 |
| `bleCommunication()` 호출 | 511 |
| LED 업데이트 | 574, 594, 612 |
| `systemState.systemOff` 체크 | 737 |
| `SYS_WATCHDOG_REFRESH()` | 776 |
| `SYS_WAIT_FOR_INTERRUPT` | 777 |
| `func_sleep()` 시작 | 797 |

## include 경로 확인

| 함수/상수 | 확인 결과 |
|---|---|
| `write_FPGA_reset()` | ✓ `isd_interface_FPGA.h` |
| `ResetNRF()`, `NRF_Off_Command()` | ✓ `systemControl.h` |
| `snd_qcc_set_mode()`, `SND_QCC_MODE_SHUTDOWN` | ✓ `snd_qcc.h` (main.c:50) |
| `Sys_GPIO_Set_Low()` | ✓ 포함됨 |
| `OnOff_3V_PMIC_CM3_to_CFX()` | ✓ `cfx_cm3_sharedMemory.h` |
| `turnOffLED()` | ✓ `LedOutput.h` |
| `tdc_cradle_get_cover_state()` | ✓ `batteryNPowerControl.h` (main.c:12) |
| `bleCommunication()` | ✓ `ble_communication.h` |
