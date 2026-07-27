---
name: 사장 조사 - isd (25파일, 최대 도메인)
purpose: isd(내부기 자극 제어) 도메인 25파일 전수 조사로 사장 코드 후보와 위험도를 기록
type: tasks/에이전트로그
applies_to: [Sound1]
tags: [cm3, dead-code, 조사, isd]
---

# 05_사장조사-isd

**TL;DR**: isd 25파일 전수 조사 결과 사장 후보 18건(안전 11 · 주의 6 · 위험 1). `tdc_isd_update_link_connected/disconnected`·`tdc_isd_clear_command_start_flag`·`tdc_isd_is_connected`는 **선언만 있고 구현체 자체가 코드베이스 어디에도 없는** 유령 함수. `tdc_isd_map_*` 6파일(5대 FSM + 데이터 I/O)은 불가침으로 전량 제외. `DisalbedBackTel`은 트리 전체에서 단 한 번도 정의되지 않아 관련 `#else` 분기가 영구 사장 상태.

## 1. 조사 범위와 방법

- 대상: `E:/workspace/projects/sound1-fw-e8300/src/2__cm3/source/isd` 25파일 (10,487줄)
- 방법: 각 헤더의 public 선언을 추출 → `Grep` 으로 `E:/.../source` 전체(153파일)에서 호출부 검색 → 호출이 발견되면 해당 호출을 감싸는 `#if`/`#ifdef`/`#ifndef` 조건을 `board/processorDirective.h`(매크로 정의 원천)와 대조해 **실제로 살아있는 분기인지** 재확인.
- `board/processorDirective.h` 를 전수 확인해 이 도메인에 영향을 주는 조건부 매크로의 실제 정의 상태를 먼저 고정했다 (아래 표). 이후 모든 `#if defined(...)` 판단은 이 표를 기준으로 했다.

| 매크로 | 실제 상태 (processorDirective.h) | isd 도메인 영향 |
|---|---|---|
| `CM3_I2C_controls_FPAG` | line 105, 무조건 정의 | `#ifdef` 분기(tdc_hal_i2c_isd_*) 항상 살아있음, `#else`(tdc_hal_i2c_cfx_*) 항상 죽음 |
| `conneded_ISDCheck_byISDPower` / `conneded_ISDCheck_byForwardPath` | line 99: `#if 1`→ByISDPower 정의, `#else`(byForwardPath)는 절대 정의 안 됨 | `#if defined(conneded_ISDCheck_byForwardPath)` 분기 영구 사장 |
| `disable_Tx_PowerControl` | line 95: `#if 0` → 미정의 | `#if !defined(...)` 이므로 TX 파워 제어 분기는 **살아있음** |
| `EEPROM_LSK_Error` | line 91: `#if 0` → 미정의 | `#if defined(EEPROM_LSK_Error)` 분기 영구 사장 |
| `DisalbedBackTel` | **소스 트리 전체(153파일)에서 `#define` 자체가 단 한 번도 없음** | `#ifndef DisalbedBackTel` 은 항상 참(활성), 관련 `#else`/`#ifdef DisalbedBackTel` 은 영구 사장 |

## 2. 사장 후보 목록

| # | 대상 | 위치 | 종류 | 근거 | 위험도 |
|---|---|---|---|---|---|
| 후보_1 | `tdc_isd_fpga_get_written_value` / `tdc_isd_fpga_update_written_value` | `tdc_isd_fpga.h:34-37`, `tdc_isd_fpga.c:34-107` | 함수 2개 | 선언·구현 모두 `#if 0` 안에 있어 이미 자체적으로 비활성. 활성 코드 어디서도 참조 없음 | 안전 |
| 후보_2 | `tdc_isd_update_link_connected` / `tdc_isd_update_link_disconnected` | 선언 `tdc_isd.h:34-35`, 호출 `tdc_isd.c:308,319` | 함수 선언(구현 없음) | **구현부가 코드베이스 어디에도 없음**(grep 전수 확인). 유일한 호출은 `tdc_isd.c:304 #if defined(conneded_ISDCheck_byForwardPath)` 안인데 이 매크로는 processorDirective.h:99-103 구조상 영구 미정의. 즉 "정의는 없는데 호출부도 죽어있어" 지금은 컴파일 되지만, 분기가 살아나는 순간 링크 에러 확정 | 안전 |
| 후보_3 | `tdc_isd_clear_command_start_flag` | `tdc_isd.h:32` | 함수 선언 | 선언만 있고 정의·호출 전무(grep 0건) | 안전 |
| 후보_4 | `tdc_isd_is_connected` | `tdc_isd.h:33` | 함수 선언 | 선언만 있고 정의·호출 전무(grep 0건) | 안전 |
| 후보_5 | `tdc_isd_set_stim_para_common_ground` | `tdc_isd_stim_para_setting.h:12` | 함수 선언 | 선언만 있고 정의 없음. `tdc_isd_stim_setting_step()`(`tdc_isd_stim_para_setting.c:1119-1164`)의 `en__commonground` 케이스는 실제로 `tdc_isd_set_stim_para_monopolar()`를 호출함(1155행) — common_ground 전용 함수는 만들어지다 만 것으로 추정 | 위험 (자극 파라미터 관련 — §4 보류) |
| 후보_6 | `tdc_isd_fpga_read_io_mux` | `tdc_isd_fpga.h:56`, `tdc_isd_fpga.c:352-369` | 함수 | 정의는 있으나 전체 트리에서 호출 0건 | 안전 |
| 후보_7 | `tdc_isd_fpga_read_optional_config` | `tdc_isd_fpga.h:58`, `tdc_isd_fpga.c:393-410` | 함수 | 정의는 있으나 전체 트리에서 호출 0건 | 안전 |
| 후보_8 | `tdc_isd_fpga_get_systemregister_1st_written_value` / `_2nd_written_value` / `get_pulse_phase_width_written_value` / `get_backtel_configuration_written_value` | `tdc_isd_fpga.h:39-42,74`, `tdc_isd_fpga.c:109-127` | 함수 4개(getter) | 4개 모두 호출 0건. `get_backtel_configuration_written_value`는 헤더에 74행에서 **중복 선언**까지 있음 | 안전 |
| 후보_9 | `tdc_isd_fpga_write_systemregister_1st` / `write_systemregister_2nd` / `write_backtel_config` | `tdc_isd_fpga.h:65,81-82`, `tdc_isd_fpga.c:517-578` | 함수 3개(raw writer) | 3개 모두 호출 0건. 단, FPGA 레지스터에 직접 쓰는 액추에이터 성격이라 삭제 전 재확인 권장(같은 레지스터를 다루는 `write_enable_rf_tx`/`write_disable_rf_tx`/`write_reset` 등 전용 함수가 이미 실사용 중) | 주의 |
| 후보_10 | `tdc_isd_fpga_change_backtel_cal` | `tdc_isd_fpga.h:75`, `tdc_isd_fpga.c:840-863` | 함수 | 8bit/12bit/disable 백텔 모드 변경 함수의 형제 함수인데 호출 0건. PCM 버퍼에 실제로 쓰는 함수라 주의 등급 | 주의 |
| 후보_11 | `ResetCounter` / `DelayResetCounter` | `tdc_isd.h:6-7` | 매크로 | 정의된 파일(tdc_isd.h) 포함 전체 트리에서 사용처 0건 | 안전 |
| 후보_12 | `PcmBitStream_Mode_notApplicable` (99) | `tdc_isd_pcm.h:13` | 매크로 | 사용처 0건. 나머지 `PcmBitStream_Mode_*` 5종은 모두 실사용 확인 | 안전 |
| 후보_13 | `pcm_Mold_connectionCheck_ForwardPath` / `pcm_Mold_connectionCheck_ReadISDPower` | `tdc_isd_pcm.h:27-28` | 매크로 | 사용처 0건. 현재 path-check 로직은 `df_forwardPathCheck_arbitraryValue`/`df_duplicateZeroValue` 값을 쓰는 별도 방식(`tdc_isd.c:57-98`)으로 대체된 것으로 추정 | 안전 |
| 후보_14 | `BackelCircuitEnabled_duringLiveStimulation` (1) | `tdc_isd_pcm.h:35` | 매크로 | 사용처 0건. 같은 그룹의 값 0·2(`BackelCircuitDisabled_FpagFifoCleared...`, `BackelCircuitDisabled_readPcmFired...`)는 `tdc_isd.c`/`tdc_shm.c`/`tdc_ble_remote.c`에서 실사용 중 | 안전 |
| 후보_15 | `#if defined(conneded_ISDCheck_byForwardPath)` 분기 | `tdc_isd.c:304-323` | 죽은 전처리 분기 | processorDirective.h 구조상 `byForwardPath`는 영구 미정의(§1 표). 이 분기 안의 `tdc_isd_update_link_connected/disconnected` 호출이 후보_2의 근거이기도 함 | 주의 |
| 후보_16 | `#else`/`#ifdef DisalbedBackTel` 분기 계열 | `tdc_isd.c:525-529,712-716`, `tdc_isd_init.c:471-476,541-593`, `tdc_isd_init_fpga.c:665-669` | 죽은 전처리 분기(5곳) | `DisalbedBackTel`이 트리 전체에서 정의된 적이 없어(§1) 전부 영구 사장. `tdc_isd_init.c:475` 는 심지어 미선언 변수 `connected_ISD_id`, 653행 부근은 `update_Connected_ISD_id()`를 참조해 이 분기가 살아나면 즉시 컴파일 실패 — 오래전부터 손대지 않았다는 방증 | 주의 |
| 후보_17 | `#if defined(EEPROM_LSK_Error)` 분기 | `tdc_isd_init.c:204-223`, `tdc_isd_init_fpga.c:554-573` | 죽은 전처리 분기(2곳) | processorDirective.h:91 에서 `#if 0`으로 감싸여 영구 미정의(§1) | 주의 |
| 후보_18 | `#else`(비-`CM3_I2C_controls_FPAG`, `tdc_hal_i2c_cfx_*` 경유) 분기 전역 | `tdc_isd_fpga.c` 전역(대략 15개소), `tdc_isd_init.c:11-13`, `tdc_isd_init_fpga.c:11-13` | 죽은 전처리 분기(다수) | `CM3_I2C_controls_FPAG`가 무조건 정의(§1)라 `#else` 분기는 전부 영구 사장. `tdc_isd_fpga_check_fpga_fifo_empty`(237행)·`read_pulse_width`(313행)·`read_fifo_counter`(336행)·`is_rf_tx_enable`(648행)의 `#else` 는 함수 시그니처에 없는 변수 `p_readValue`를 참조해 **정의를 되살리면 즉시 컴파일 실패**. `tdc_hal_i2c_cfx_*` 자체는 hal 도메인 소관이라 09번(죽은 전처리)·03번(hal) 노드와 교차 확인 필요 | 주의 |

**요약 카운트**: 안전 11 · 주의 6 · 위험 1 (합계 18건)

## 3. 사장 아님으로 판정한 것 (오탐 방지 기록)

| 대상 | 위치 | 제외 사유 |
|---|---|---|
| `tdc_isd_map_ecap.c/h`, `tdc_isd_map_impedance.c/h`, `tdc_isd_map_live.c/h`, `tdc_isd_map_specific_stim.c/h`, `tdc_isd_map_test_stim.c/h` (5대 FSM) | `isd/tdc_isd_map_*` | 지시사항 불가침 목록 명시. 5개 FSM 모두 `tdc_ble_mapping.c`에서 `_step()` 호출 확인(예: `tdc_isd_map_live_step` → `tdc_ble_mapping.c:2055`). test_stim만 진입점 이름이 `testStimulation()`(tdc_ 접두 없음, `tdc_isd_map_test_stim.c:30`)이고 `tdc_ble_mapping.c:2221`에서 호출 확인 — 살아있음 |
| `tdc_isd_map_data.c/h` | `isd/tdc_isd_map_data.*` | 파일명 패턴상 `tdc_isd_map_*`에 해당하고 5대 FSM의 맵 데이터 읽기/쓰기를 직접 담당하는 자극 제어 인접 모듈이라, "5대 FSM 불가침" 지시의 취지에 맞춰 함께 제외(보수적 판단). 참고: 이름상으론 6번째 `map_*` 파일이라 "5대"와 파일 수가 안 맞는데, map_data는 FSM이 아니라 FSM들의 공용 데이터 I/O 유틸리티로 보임 — 오케스트레이터 확인 권장(§4) |
| `tdc_isd_fpga_read_version` / `tdc_isd_fpga_is_rf_tx_enable` | `tdc_isd_fpga.c:129,640` | `tdc_isd_init_fpga.c:168,324,333`에서 실사용 확인 |
| `tdc_isd_fpga_change_12_bit_backtel_mode` | `tdc_isd_fpga.c:787` | `tdc_isd_map_ecap.c:642`, `tdc_isd_map_impedance.c:269`(불가침 FSM)에서 실사용 확인 — 호출부가 불가침 파일 안에 있을 뿐 함수 자체는 사장 아님 |
| `fill_pcmBuff_writeData_checkPathOpen_normalValue` 등 3개 내부 헬퍼 | `tdc_isd.c:51,67,83` | 같은 파일의 `tdc_isd_fill_pcm_path_open_normal/dup_zero`가 실호출. 다만 `static` 누락(헤더에 없는데 외부 링키지) — 사장은 아니고 린트성 이슈로만 §5 기록 |
| `#if !defined(disable_Tx_PowerControl)` 블록 (TX 파워 증감 switch, `tdc_isd.c:380-463,627-665`) | `tdc_isd.c` | processorDirective.h:95 에서 `disable_Tx_PowerControl`가 `#if 0`로 미정의 → `!defined`가 참이라 **활성 코드**. 얼핏 죽어 보이지만 실제로 살아있음(1차 조사에서 드라이버 5종 중 1종만 살아있었다는 사례와 유사한 함정) |
| `tdc_isd_update_link_by_backtel_live` / `_mapping`, `tdc_isd_is_connection_check_with_mapping`, `tdc_isd_fill_pcm_path_open_*` | `tdc_isd.h` 전역 | 전부 `tdc_isd_map_live.c`(FSM) 또는 `tdc_ble_mapping.c`에서 실사용 확인 |

## 4. 판단 보류 · 추가 확인 필요

- **`tdc_isd_set_stim_para_common_ground`**(후보_5): 구현·호출 모두 없다는 grep 근거는 명확하지만, 이름 자체가 "자극 파라미터 설정"이라 지시사항상 최소 위험 등급 대상. 공통접지(common ground) 자극 모드가 실제 제품에서 지원되는지, 아니면 애초에 미구현 상태로 방치된 것인지 은수님 확인 필요.
- **`tdc_isd_map_data.c/h`의 6번째 `map_*` 파일 포함 여부**(§3): "5대 매핑 FSM"과 실제 파일 수(6개) 불일치. map_data를 불가침 목록에 포함할지 여부를 오케스트레이터/은수님이 명시적으로 확정해주면 좋겠음. 이번 조사에서는 보수적으로 제외 처리함.
- **후보_18 (`CM3_I2C_controls_FPAG` else 분기)**: `tdc_hal_i2c_cfx_*` 함수 자체의 실사용 여부는 hal 도메인(03번 노드) 소관. 이 분기를 삭제하려면 hal 쪽 `tdc_hal_i2c_cfx_read/write`가 다른 곳(CFX 링크 등)에서도 안 쓰이는지 교차 확인 후 결정 권장.
- **후보_15~18 (전처리 분기 계열)**: 09번 노드(`09_죽은전처리-블록.md`)가 전사 횡단으로 동일 주제를 다루므로, 최종 정리 시 중복 보고 병합 필요.

## 5. 특이사항

- **유령 함수 3개의 공통 패턴**: `tdc_isd_update_link_connected`/`_disconnected`/`tdc_isd_clear_command_start_flag`/`tdc_isd_is_connected`는 전부 "헤더에 선언 → 본문 어디에도 구현 없음"인데, 지금은 우연히 호출부까지 죽어있어(또는 호출 자체가 없어) 링크 에러가 안 난다. 은수님이 예로 든 `tdc_hal_uart_printf()` 사례와 본질적으로 동일한 패턴("선언은 있는데 실제 경로는 전처리에서 소멸")이 isd 도메인에도 4건 있었다.
- **`DisalbedBackTel` 오탈자 매크로**: 정확한 스펠링이면 `DisabledBackTel`이어야 할 텐데 `DisalbedBackTel`로 되어 있고, 이 오탈자 매크로가 `#define` 된 적이 없다. 오탈자 때문에 우연히 항상 미정의 상태가 된 것인지, 의도적으로 "백텔 비활성화 실험 모드"를 만들다 만 것인지는 불명. 관련 `#else` 블록들이 이미 컴파일 불가능한 상태(§2 후보_16)라는 것은, 적어도 최근 리팩토링 기간 동안 이 매크로를 정의해 빌드해 본 적이 없다는 강한 방증이다.
- **`tdc_isd_map_test_stim.h`가 0바이트**: 5대 FSM 중 하나인데 헤더가 완전히 비어 있다. 진입점 함수명도 다른 4개(`tdc_isd_map_ecap_step` 등 `tdc_` 접두 규칙)와 달리 `testStimulation()`으로 규칙을 벗어난다. 1차 리팩토링의 "전 심볼 `tdc_` 정합"에서 이 5대 FSM 내부가 예외 처리되었다는 지시사항과 일치하는 현상으로 보이나, 파일 존재 자체는 사장이 아님(§3에서 실사용 확인).
- **헤더 중복 선언**: `tdc_isd_fpga_get_backtel_configuration_written_value`가 `tdc_isd_fpga.h`의 42행과 74행에 두 번 선언되어 있다(후보_8과 동일 항목, 컴파일에는 무해하나 정리 대상).
- **`isINGconnectionCheck_WithMapping`**(`tdc_isd.c:538`)와 `fill_pcmBuff_*` 3종(§3)은 파일 스코프 전역/함수인데 `static`이 빠져 있다. 사장 코드는 아니지만 링키지 위생 문제로 별도 기록.
