---
name: 1차 이월 45건 재판정 + board 잔여 3건
purpose: 1차 리팩토링 이월 사장코드 45건(G5 11·G7 15·G8 19) 재판정 및 board 잔여 3건(99_eeprom_address.h·electrodeMapping.c·tdc_ rename) 조사
type: tasks/에이전트로그
applies_to: [Sound1]
tags: [cm3, dead-code, 조사, fpga, isd, cfx-link, shared-memory, board]
---

# 10_이월분-재판정-board잔여

**TL;DR**: 이월 45건 재판정 결과 **G5 12건 중 9건 안전·3건 사장 아님으로 재판정**(`tdc_led_memory_error`·`tdc_touch_iqs323_clear_ulp`·`tdc_led_is_test_trigger_enabled`), **G7 FPGA accessor 13건 확정(안전, 단 자극 인접이라 등급은 주의)** - 원 문서 "15건" 표기와 불일치하며 isd 담당 노드(05번)가 놓친 1건(`tdc_isd_fpga_read_systemregister_2nd`)을 추가로 발견, **G8 19건은 목록화 위주로 확인**(3건 재확인, 2건은 사장 아닐 가능성 있어 재검토 요청). 이월_2 공유 ABI는 calibration 사본만 **3파 걸쳐 드리프트**(01-20/02-24/07-20) + enum 값 자체가 다른 항목 신규 발견. 이월_3 board 잔여는 `99_eeprom_address.h` 매크로 전량(약 40종) 미사용 확인, `electrodeMapping.c`는 명백히 사용 중(사장 아님)으로 판정.

## 1. 조사 범위와 방법

담당: 이월_1의 **목록화 전체 + G7 FPGA 15건 심층**, 이월_2(3프로젝트 ABI drift), 이월_3(board 잔여 3건). G5(LED/touch)는 노드4, G8(cfx_link)은 노드7이 심층 담당이라 본 조사에서는 가볍게 재확인만 했다.

읽은 선행 문서: `작업 목록.md §이월 항목`, `세션_인수인계/20260722_...md §5`, `게이트-노트/G5·G7·G8`, `20260722_cm3-board-cleanup/{분석,이력 및 결과}.md`, `분석-데이터/04_사장코드-후보.md`(G0 census 원본).

방법: (1) 헤더 선언 전수 추출 → 전체 소스(`source/` 153파일) `Grep` 호출 카운트 → 호출 발견 시 `Read`로 살아있는 분기인지 확인(`#if 0` 함정 방어). (2) FPGA는 `tdc_isd_fpga_\w+\(` 패턴으로 267개 occurrence를 전량 수거해 39개 선언 전부를 개별 대조. (3) ABI drift는 CM3 `cfx_link/tdc_shm.h`·CFX `1__cfx/environment/shared_memory.h`·calibration `5__calibration/include/cfx_cm3_sharedMemory.h` 세 파일의 struct 정의 구간을 `diff`로 직접 대조(추측 아닌 라인 단위 실측).

> [!IMPORTANT] 방법론 주의사항 - 현재 브랜치(`claude_hw_test_qcc_1.8v_current`)는 QCC 1.8V 전류 측정용 실험 브랜치로, `tdc_touch_init_begin()`(`sys/tdc_sys_init.c:441`)과 ISD FSM 6개 초기화 호출(`isd/tdc_isd.c`)이 `#if 0`/주석으로 임시 비활성화돼 있다(커밋 `ea27707`, `develop` 병합 안 함). **본 조사의 모든 판정은 `claude_develop` 브랜치 기준**이며, `git diff claude_develop..claude_hw_test_qcc_1.8v_current`로 두 브랜치가 5개 파일(`hal/tdc_hal_dio.c`·`isd/tdc_isd.c`·`isd/tdc_isd_stim_standalone.c`·`main.c`·`sys/tdc_sys_init.c`)만 다르고 나머지 148개는 완전히 동일함을 먼저 확인한 뒤 진행했다. LED/touch/FPGA 관련 파일은 이 5개에 포함되지 않아 재판정에 영향 없음.

## 2. 사장 후보 목록

### 2-1. G7 — FPGA accessor (본 노드 심층 담당, `isd/tdc_isd_fpga.c/.h`)

`tdc_isd_fpga_` 로 시작하는 함수 39개(중복 선언 1건 별도) 전수 대조. 전체 267개 호출/선언 occurrence를 파일별로 완전히 수거해 대조했다.

| # | 대상 | 위치 | 종류 | 근거 | 위험도 |
|---|---|---|---|---|---|
| 후보_1 | `tdc_isd_fpga_get_written_value` | `isd/tdc_isd_fpga.c:35`, `.h:35`(`#if 0` 안) | 함수 | 헤더 선언 자체가 `#if 0`(`.h:34-37`) 안에 있고, 유일한 두 호출부(`isd/tdc_isd_map_ecap.c:540,630`)도 전부 `#if 0` 안 - 이중으로 죽어 있음. G0 census(12건) 밖의 신규 발견 | 주의 |
| 후보_2 | `tdc_isd_fpga_update_written_value` | `isd/tdc_isd_fpga.c:75`, `.h:36`(`#if 0` 안) | 함수 | 헤더 선언이 `#if 0` 안. 호출처 전체 트리 0건 | 주의 |
| 후보_3 | `tdc_isd_fpga_get_systemregister_1st_written_value` | `isd/tdc_isd_fpga.c:109`, `.h:39` | 함수 | 정의 외 호출 0건(전체 267개 occurrence 중 선언·정의뿐) | 주의 |
| 후보_4 | `tdc_isd_fpga_get_systemregister_2nd_written_value` | `isd/tdc_isd_fpga.c:114`, `.h:40` | 함수 | 호출 0건 | 주의 |
| 후보_5 | `tdc_isd_fpga_get_pulse_phase_width_written_value` | `isd/tdc_isd_fpga.c:119`, `.h:41` | 함수 | 호출 0건 | 주의 |
| 후보_6 | `tdc_isd_fpga_get_backtel_configuration_written_value` | `isd/tdc_isd_fpga.c:124`, `.h:42`+`74`(헤더 중복 선언) | 함수 | 호출 0건. 헤더에 동일 시그니처가 두 번 선언(무해하나 정리 대상) | 주의 |
| 후보_7 | `tdc_isd_fpga_read_systemregister_2nd` | `isd/tdc_isd_fpga.c:174`, `.h:48` | 함수 | 호출 0건. **isd 도메인 담당 노드(05번, `05_사장조사-isd.md`)의 12건 목록에는 빠져 있던 항목** - 형제 함수 `read_systemregister_1st`는 `tdc_isd.c`/`tdc_isd_init_fpga.c`에서 다수 실사용되나 `_2nd`만 죽어 있어 grep으로 놓치기 쉬움 | 주의 |
| 후보_8 | `tdc_isd_fpga_read_io_mux` | `isd/tdc_isd_fpga.c:352`, `.h:56` | 함수 | 호출 0건 | 주의 |
| 후보_9 | `tdc_isd_fpga_read_optional_config` | `isd/tdc_isd_fpga.c:393`, `.h:58` | 함수 | 호출 0건 | 주의 |
| 후보_10 | `tdc_isd_fpga_write_systemregister_1st` | `isd/tdc_isd_fpga.c:517`, `.h:65` | 함수 | 호출 0건 | 주의 |
| 후보_11 | `tdc_isd_fpga_write_systemregister_2nd` | `isd/tdc_isd_fpga.c:538`, `.h:81` | 함수 | 호출 0건 | 주의 |
| 후보_12 | `tdc_isd_fpga_write_backtel_config` | `isd/tdc_isd_fpga.c:559`, `.h:82` | 함수 | 호출 0건 | 주의 |
| 후보_13 | `tdc_isd_fpga_change_backtel_cal` | `isd/tdc_isd_fpga.c:840`, `.h:75` | 함수 | 호출 0건. 형제 함수(`change_8_bit_backtel_mode`/`change_12_bit_backtel_mode`/`disable_backtel`)는 전부 실사용 중 | 주의 |

**원 문서 불일치**: G7 게이트 노트·`분석.md:125`는 "FPGA accessor 15개"라 적었으나, G0 census(`04_사장코드-후보.md`)에 실제 나열된 것은 12건뿐이었다. 본 조사는 39개 선언 전량을 267개 occurrence로 재검증해 **13건**을 확정했다(12건 재확인 + 후보_1 신규 발견). "15"의 근거는 찾지 못했다 - 원본 산정 당시의 어림값이거나 문서 오기로 추정한다(§4 기재). 위험도는 순수 도달가능성 기준으로는 "안전"이나, 자극 제어(ISD/FPGA) 인접 파일이라 프로젝트가 이미 채택한 신중 기조(G7 게이트 §3 "자극 계층 신중")를 따라 **주의**로 통일했다.

### 2-2. G5 — LED/touch (노드4 심층 담당, 본 노드는 교차검증만)

노드4(`04_사장조사-led-touch-ui-stim.md`)가 이미 심층 조사를 완료했다. 본 노드가 독립적으로 재현한 결과는 노드4와 100% 일치했다(교차검증 완료).

| # | 대상 | 위치 | 종류 | 근거 | 위험도 |
|---|---|---|---|---|---|
| 후보_14 | `tdc_led_black` | `led/tdc_led_output.c:772`, `.h:162` | 함수 | 호출 0건 | 안전 |
| 후보_15 | `tdc_led_white` | `led/tdc_led_output.c:777`, `.h:163` | 함수 | 호출 0건 | 안전 |
| 후보_16 | `tdc_led_clock_error` | `led/tdc_led_output.c:801`, `.h:166` | 함수 | 호출 0건 | 안전 |
| 후보_17 | `tdc_led_pattern_out` | `led/tdc_led_output.c:760`, `.h:154` | 함수 | 본문 자체가 "새 아키텍처에서는 arbiter_tick 이 담당"이라는 legacy 래퍼 주석. 호출 0건 | 안전 |
| 후보_18 | `tdc_led_disable_test_trigger` | `led/tdc_led_output.c:363`, `.h:157` | 함수 | 호출 0건 | 안전 |
| 후보_19 | `tdc_touch_get_state` | `touch/tdc_touch.c:405`, `touch/tdc_touch.h:65` | 함수 | 호출 0건 | 안전 |
| 후보_20 | `tdc_touch_iqs323_set_ulp` | `touch/tdc_touch_iqs323.c:41`, `.h:107` | 함수 | 호출 0건 (전역 `g_in_ulp_mode` 를 쓰는 setter인데 setter 자체가 안 불림) | 안전 |
| 후보_21 | `tdc_touch_iqs323_is_ulp` | `touch/tdc_touch_iqs323.c:49`, `.h:109` | 함수 | 호출 0건 | 안전 |
| 후보_22 | `tdc_touch_iqs323_public_settings` | `touch/tdc_touch_iqs323.c:292`, `.h:111` | 함수 | 호출 0건. 같은 파일의 `static touch_settings()`(:284-289)와 **바이트 단위로 동일한 구현**의 공개판 복제본 - 내부용만 실사용 | 안전 |

G5 원 목록은 "11건"이라 적혀 있으나 G5 게이트 노트 §6 본문에 실제 나열된 이름은 12개(LED 7 + touch 5)다 - 원 문서 자체의 집계 오류로 보인다(§5). 12개 중 위 9개만 사장으로 남고, 3개(`tdc_led_memory_error`·`tdc_led_is_test_trigger_enabled`·`tdc_touch_iqs323_clear_ulp`)는 §3 "사장 아님"으로 재판정했다.

### 2-3. G8 — cfx_link (노드7 심층 담당, 본 노드는 목록화 + 스팟체크 3건)

전체 81개 접근자 중 G8 게이트 노트가 예시로 든 대표 항목만 재확인했다. **정확한 9(EEPROM)+10(공유메모리) 분류·전수 재검증은 노드7 소관**이다.

| # | 대상 | 위치 | 종류 | 근거 | 위험도 |
|---|---|---|---|---|---|
| 후보_23 | `tdc_shm_read_battery_level_from_cfx` | `cfx_link/tdc_shm.c:155`, `.h:412` | 함수 | 재확인 완료 - 호출 0건, G8 판정 유지 | 주의 |
| 후보_24 | `tdc_shm_read_cfx_error_code` | `cfx_link/tdc_shm.c:658`, `.h:420` | 함수 | 재확인 완료 - 호출 0건, G8 판정 유지 | 주의 |
| 후보_25 | `tdc_shm_read_current_map_data` | `cfx_link/tdc_shm.h:357` | 함수 | 재확인 완료 - 호출 0건, G8 판정 유지 | 주의 |
| 후보_26 | `tdc_cfx_eeprom_erase_map_stamp_by_mapping` 외 EEPROM `_by_mapping`/`_to_repository`/`_mapping_app` 계열 (erase 5·recover 2·write 5·read 5, 실체는 중복 선언 제외 약 15개 선언) | `cfx_link/tdc_cfx_eeprom_{erase,recover,read,write}.h` 각 선언부 | 함수군 | G8 원 목록 계승 - 개별 호출 카운트는 미실시(노드7 소관). 파일·선언은 현재도 모두 존재함만 확인 | 주의(노드7 확인 대기) |

**목록화 결과**: G8 대상 파일(`cfx_link/*`) 11개는 1차 리팩토링 이후 구조 변화 없이 현존한다. 실제 함수 수는 `tdc_shm.h`만 약 60개 선언(전체 81개 중 다수)이라 본 조사에서 전부 나열하지 않았고, 대표 3건 재확인 + 아래 §4의 우려 2건만 별도 보고한다.

### 2-4. 이월_3 — board 잔여: `99_eeprom_address.h`

| # | 대상 | 위치 | 종류 | 근거 | 위험도 |
|---|---|---|---|---|---|
| 후보_27 | `99_eeprom_address.h` 전체 매크로(`AT_WREN`·`df_eepromLastData_Adrress`·`df_EERPROM_BaseAddr_*` 등 약 40종) | `board/99_eeprom_address.h:8-249` | 매크로 파일 전체 | 파일이 정의하는 매크로 전량을 이 파일 자신을 제외한 전체 소스에서 `Grep` 했으나 **참조 0건**. 이 파일을 `#include` 하는 3개 파일(`fs/tdc_fs.h:23`·`cfx_link/tdc_cfx_eeprom_read.h:15`·`cfx_link/tdc_shm_addr.h:7`) 모두 include만 하고 매크로를 실제로 쓰지 않는다. 부수로 `df_ByteLength_firstPulsePhase`(:140) 등 6개 매크로(:140,143,146,150,154,158)가 괄호 불균형(`(df_24bitWordLength_X)*3)` - 여는 괄호 1개·닫는 괄호 2개) 상태로, 실제로 참조되면 컴파일 에러가 나는 잠재 결함이 있다 - 아무도 안 써서 지금까지 발견되지 않은 것으로 추정 | 안전 |

## 3. 사장 아님으로 판정한 것 (오탐 방지 기록)

| 대상 | 위치 | 판정 이유 |
|---|---|---|
| `tdc_led_memory_error` | `led/tdc_led_output.c:782`, `.h:165` | G5 게이트 §6이 사장 7건 중 하나로 기재했으나 재조사 결과 오탐. `sys/tdc_sys_init.c:280` `while(1){ tdc_led_memory_error(); __WFI(); }` - 공유메모리 주소 검증 실패(`tdc_shm_shared_memory_address_error()`) 시 진입하는 런타임 에러 핸들러로, 전처리가 아닌 조건문이라 죽은 분기가 아님. 노드4도 동일하게 재판정함(교차검증) |
| `tdc_touch_iqs323_clear_ulp` | `touch/tdc_touch_iqs323.c:45`(정의), `:458`(호출) | `tdc_touch_iqs323_apply_settings()`(같은 파일:400) 안에서 호출되고, 이 함수는 `touch/tdc_touch.c:131`의 `try_finish_init()`(터치 부팅 ATI 완료 처리 - 매 tick 호출되는 상태머신)에서 실호출된다. 짝인 `tdc_touch_iqs323_set_ulp`(후보_20)가 죽어 있어 값을 항상 `false→false`로 다시 쓰는 사실상 no-op 이지만, 코드 경로 자체는 살아있어 "사장"이 아니다 |
| `tdc_led_is_test_trigger_enabled` | `led/tdc_led_output.c:368`(정의), `:975`(호출) | `tdc_led_out()`(매 LED tick 실행되는 핵심 출력 함수) 내부에서 직접 호출돼 구조적으로 살아있다. 다만 이 값을 켜는 유일한 setter(`tdc_led_enable_test_trigger`, `.c:358`)의 유일한 호출부가 `isd/tdc_isd_init.c:931`에서 **주석 처리**(`// tdc_led_enable_test_trigger();`)돼 있어 실질적으로는 항상 `false`를 반환하는 기능적 사장 상태다. 함수 자체(getter)는 제거 대상이 아니고, `tdc_led_enable_test_trigger`/`tdc_led_disable_test_trigger`(후보_18)를 포함한 "테스트 트리거" 서브시스템 전체를 하나로 묶어 판단해야 한다 |
| `electrodeMap[32]` (및 `electrodeMapping.c`의 `#if 1` 분기) | `board/electrodeMapping.c:2-9`, `board/electrodeMapping.h:5` | 실제 전극-자극칩 배선표로 `isd/tdc_isd_map_impedance.c`·`tdc_isd_map_ecap.c`·`tdc_isd_map_specific_stim.c`·`tdc_isd_map_test_stim.c`·`tdc_isd_stim_para_setting.c` 5개 자극 제어 파일에서 총 20회 이상 실사용(예: `tdc_isd_map_impedance.c:376`). **명백히 활성 코드이며 사장 후보가 아니다.** `#if 1`(실제 배선) vs `#else`(임피던스 측정 지그용 대체 배선) 는 `board.h`의 버전 선택자와 같은 성격의 "제조/시험 모드 빌드 스위치"이지 레거시 잔재가 아니다. `#else` 분기 자체는 지그 테스트용으로 의도적으로 보존된 것으로 판단되며, 전극 배선은 자극 전달에 직결되므로 **위험도: 위험**(자극 회로 접촉) - 변경 시 반드시 하드웨어 실측 필요 |

## 4. 판단 보류 · 추가 확인 필요

1. **G8 후보 2건이 사장 아닐 가능성**: `tdc_shm_read_isd_manufacture_id`(`.h:296`)·`tdc_shm_read_remocon_passkey_connected_isd`(`.h:304`) 두 심볼을 가볍게 grep 했더니 `isd/tdc_isd_init.c`(2건)·`ble/tdc_ble_remote.c`(1건)에서 외부 참조가 나왔다. G8 노트의 "10건" 목록이 정확히 이 두 개를 포함하는지 원문에 전체 나열이 없어 확인 불가능했다. **노드7이 이 두 심볼을 최우선으로 재확인해 달라** - 사장 후보에서 빠져야 할 수 있다.
2. **G5/G7/G8 원 문서 건수(11/15/19) vs 실제 재현 건수(12/13/19) 불일치**: G5는 나열된 이름이 12개인데 "11건"이라 표기, G7은 census 원본이 12개인데 "15개"라 표기(§2-1). 두 경우 모두 "15", "11"의 산출 근거를 찾지 못했다 - 리팩토링 초기 자동 추정치가 이후 사람이 옮겨 적으며 반올림·오기됐을 가능성. G8의 "19"는 본 조사에서 전수 재현하지 않아 판단 보류.
3. **board/ 11개 파일 `tdc_` rename 여부**: `0__bootloader`에 동일 이름 파일(`board.h`·`processorDirective.h`·`FPGA.h`·`electrodeMapping.*`)이 존재하지 않음을 확인했다(교차 프로젝트 ABI 제약 없음). 따라서 CFX/calibration 공유 ABI 같은 위험은 없고, 순수 **프로젝트 내부 네이밍 일관성** 문제다. rename 자체는 컴파일 위험이 낮으나(안전), board 선택자 3개(`board.h`/`FPGA.h`/`internalStimulationChip.h`)의 `#if/#elif` 조건부 매크로명까지 함께 바꿀지, 파일명만 바꿀지는 은수님 결정이 필요하다.
4. **`99_eeprom_address.h` 처리 방향**: 매크로 전량 미사용이 확인됐으나, "파일 자체를 삭제"할지 "매크로만 정리(괄호 결함 6건 포함)하고 파일은 EEPROM 레이아웃 문서로 보존"할지는 정책 판단이 필요하다. 이 파일이 실제 EEPROM 하드웨어 레이아웃을 기술하는 유일한 문서일 가능성이 있어(현재 코드가 이 레이아웃대로 EEPROM에 접근하는지는 `fs`/`cfx_link` 도메인 노드 확인 필요), 단순 삭제보다 "문서화 후 코드에서는 정리"를 권장한다.

## 5. 특이사항

- **이월_2 — 3프로젝트 공유 ABI drift 정밀 대조 결과**: `1__cfx/environment/shared_memory.h`는 CM3 `cfx_link/tdc_shm.h`와 **완전히 동기화**돼 있다(필드 순서·개수·enum 값 전부 일치, `diff` 결과 공백/매크로명(`MaxNumUser` vs `MAX_NUM_USER`) 차이만 존재). 반면 **calibration(`5__calibration/include/cfx_cm3_sharedMemory.h`)만 3파에 걸쳐 뒤처져 있다**:
  - 구조체 말미에 CM3/CFX 대비 **7개 필드 누락**: `is_enabled_mute_stimulation_under_t_level`·`mute_stimulation_t_level_offset`(2026-01-20, 자극 묵음) / `is_pcm_specific_command_reading`·`pcm_specific_command_read_index`(2026-02-24) / `gain_table_index_a`·`gain_table_index_b`·`is_i2s_source_cradle`(2026-07-20, 게인 변환 테이블 - **이번 리팩토링과 같은 날 추가**). 기존 문서는 "01-20부터 미동기"로만 기록했는데, 실제로는 그 뒤로 2번 더(02-24, 07-20) 벌어졌다.
  - **신규 발견**: `EN__SYSTEM_OP_MODE` enum의 값 1·2 멤버명이 CM3/CFX는 `en__mappingMode`/`en__systemReset`인데 calibration은 `en__charging_Mode`/`en__ulpMode`다(`tdc_shm.h:157-159` ↔ `cfx_cm3_sharedMemory.h:128-131`). 정수값(0/1/2)은 같아 레이아웃은 안 깨지지만, 같은 필드(`systemShare.system_opMode`)를 두고 3프로젝트가 서로 다른 의미를 부여하는 상태다. 다만 이 필드는 세 헤더 모두 "현재 사용되는 곳 없음. 지울 것"이라는 동일한 주석이 달려 있어 실질 영향은 낮을 것으로 추정된다.
  - `EN__mapping_ReadWriteMap_command`도 calibration만 `flash_Command_Recover`(값 4) 멤버가 없다(CM3/CFX는 5개 값, calibration은 4개).
  - 결론: 구조체 앞부분(prefix)이 동일해 ABI 레이아웃 자체는 안 깨진다는 기존 판단은 유효하나, **drift 파고(3회)와 enum 값 의미 불일치는 기존 문서보다 심각도가 높다.** 3프로젝트 동시 검증이 필요하다는 기존 결론은 유지하되, 이번에 찾은 구체적 라인 번호를 근거로 남긴다.
- **G7 census 재현 방법론**: `tdc_isd_fpga_\w+\(` 패턴으로 267개 occurrence를 전량 파일별 집계(main.c 2·sys/tdc_sys_init.c 4·isd/tdc_isd_fpga.c 47·isd/tdc_isd.c 30·isd/tdc_isd_map_specific_stim.c 9·isd/tdc_isd_map_ecap.c 14·isd/tdc_isd_map_test_stim.c 18·isd/tdc_isd_init.c 28·isd/tdc_isd_map_impedance.c 10·isd/tdc_isd_fpga.h 43·isd/tdc_isd_stim_para_setting.c 33·isd/tdc_isd_init_fpga.c 29)해 39개 선언 전부를 빠짐없이 대조했다. `isd/tdc_isd_map_live.c`·`isd/tdc_isd_stim_standalone.c`는 FPGA 접근자를 전혀 쓰지 않음(0건)도 확인.
- **grep 함정 재현**: `tdc_isd_fpga_get_written_value`(후보_1)는 유일한 호출부 2곳(`tdc_isd_map_ecap.c:540,630`)이 모두 `#if 0` 안에 있고, 그 `#else` 분기(:599,622,642)가 실제로는 다른 함수(`tdc_isd_fpga_write_clear_fifo`·`change_pulse_width_minimum`·`change_12_bit_backtel_mode`)를 쓰는 것으로 대체돼 있다 - "구현이 세대교체됐는데 구세대 호출부가 죽은 분기 안에 화석으로 남은" 전형적 사례.
- **`tdc_led_output.c`의 테스트 트리거 서브시스템**은 `enable`/`disable`/`is_enabled` 3함수 + `testLED_Trigger` 전역으로 구성되는데, `enable`만 유일 호출부가 주석 처리돼 있어 **셋을 따로따로 판단하면 안 되고 하나의 죽은 기능으로 묶어 봐야 한다**(§3). 노드4도 동일 결론(후보_8).
