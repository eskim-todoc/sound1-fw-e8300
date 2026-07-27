---
name: 사장 조사 - led touch ui stim (19파일)
purpose: led(2)·touch(8)·ui(2)·stim(7) 도메인 19파일 전수 조사로 사장 코드 후보와 위험도를 확정
type: tasks/에이전트로그
applies_to: [Sound1]
tags: [cm3, dead-code, 조사, led, touch, ui, stim]
---

# 04_사장조사-led-touch-ui-stim

**TL;DR**: led/touch/ui/stim 19파일 전수 조사 결과 **사장 후보 19건**(안전 12 · 주의 5 · 위험 2). 1차 이월 11건 중 10건 재확인, **`tdc_led_memory_error` 는 재판정 결과 사장 아님**(실사용 확인)으로 이월 목록에서 제외. UI 도메인 12건은 전부 함수포인터 디스패치 테이블 오탐으로 확인. `stim/tdc_stim_para_cal.c` 에서 전하량 제한 안전 검사가 매크로명 불일치로 영구 비활성 상태인 것을 발견 - 별도 확인 필요.

## 1. 조사 범위와 방법

담당 폴더: `led`(2파일) `touch`(8파일) `ui`(2파일) `stim`(7파일), 총 19파일.

방법: 각 파일의 헤더에 선언된 public 함수·전역·매크로·타입을 추출 → `Grep` 으로 CM3 전체 소스(`source/` 전 도메인)에서 참조처 조사 → 호출이 발견되면 해당 라인이 `#if 0`/상시-거짓 전처리 분기 안인지 `Read` 로 직접 확인 → 참조 0건 또는 죽은 분기 안 참조만 있으면 사장 후보로 분류.

선행 문서 확인:
- `docs/tasks/cm3/20260720_cm3-full-refactor/게이트-노트/G5-led-touch-ui.md` - 1차 이월 11건(LED 레거시 7 + touch 4) 근거
- `docs/tasks/cm3/20260720_cm3-full-refactor/분석-데이터/08_보류-동결-목록.md` - `definitionsForAlgorithm.h`(현 `tdc_stim_definitions.h`) 동결 27종 (다른 헤더 포함 전체는 70종)
- `docs/tasks/cm3/20260720_cm3-full-refactor/게이트-노트/G7-isd-stim.md` - stim 도메인 배경, G7-2(FSM 표준화) 생략 확정 사실 확인

## 2. 사장 후보 목록

| # | 대상 | 위치 | 종류 | 근거 | 위험도 |
|---|---|---|---|---|---|
| 후보_1 | `tdc_led_black` | `led/tdc_led_output.c:772`, `.h:162` | 함수 | 정의·선언 외 호출 0건 (전체 소스 grep). 1차 이월 11건 중 하나, 재확인 결과 여전히 사장 | 안전 |
| 후보_2 | `tdc_led_white` | `led/tdc_led_output.c:777`, `.h:163` | 함수 | 호출 0건. 1차 이월분 재확인 | 안전 |
| 후보_3 | `tdc_led_clock_error` | `led/tdc_led_output.c:801`, `.h:166` | 함수 | 호출 0건. 1차 이월분 재확인 | 안전 |
| 후보_4 | `tdc_led_pattern_out` + `tdc_led_pattern_t`(`TDC_LED_PATTERN_*` 8종, `.h:29-39`) | `led/tdc_led_output.c:760-766`, `.h:29-39,154` | 함수+열거형 | 본문이 빈 래퍼(`(void) ledOutputPattern;`뿐). 호출 0건 - `main.c:526` 은 주석 속 언급뿐(`* QCC 미수신 시... TDC_LED_PATTERN_NA)`). 열거값 8종은 이 함수의 인자 타입으로만 존재 - 1차 이월분 재확인 | 안전 |
| 후보_5 | `tdc_led_disable_test_trigger` | `led/tdc_led_output.c:363`, `.h:157` | 함수 | 호출 0건. 1차 이월분 재확인 | 안전 |
| 후보_6 | `tdc_led_enable_test_trigger` | `led/tdc_led_output.c:358`, `.h:156` | 함수 | 유일 호출부가 `isd/tdc_isd_init.c:931`에서 **주석 처리**(`// tdc_led_enable_test_trigger();`) - 죽은 참조. 1차 이월분 재확인, grep 함정(주석) 케이스 | 안전 |
| 후보_7 | `tdc_led_get_ind_state` | `led/tdc_led_output.c:353`, `.h:152` | 함수 | 전체 소스 중 유일한 다른 참조가 `main.c:759` **코드 주석 문장**(`* ... tdc_led_get_ind_state() 는 BLE 가...`) 뿐, 실제 호출 0건. **신규 발견**(1차 이월 목록에 없었음) | 안전 |
| 후보_8 | `tdc_led_is_test_trigger_enabled` + `testLED_Trigger`(static) 서브시스템 | `led/tdc_led_output.c:323,368-371,975` | 함수+전역 | 함수 자체는 `tdc_led_out()`(같은 파일:975)이 호출해 구조적으로는 "사용 중"이나, 값을 켜는 유일한 setter(`tdc_led_enable_test_trigger`, 후보_6)가 죽어 있어 `testLED_Trigger` 는 항상 `false` - 기능적으로 죽은 디버그 트리거. 단독 제거 시 `tdc_led_out()` 분기 로직 변경 필요 | 주의 |
| 후보_9 | `tdc_touch_get_state` | `touch/tdc_touch.c:405`, `touch/tdc_touch.h:65` | 함수 | 호출 0건. **문서 드리프트 확인**: `tdc_touch.c:12`/`tdc_touch.h:13` 주석이 "절전 ULP 는 main.c func_sleep 이 get_state 를 직접 호출"이라 명시하나, 실제 `main.c:1140-1141`은 `tdc_touch_iqs323_read_status()`를 **직접** 호출해 동일 로직을 중복 구현 - 주석이 가리키는 호출은 실재하지 않음. 1차 이월분 재확인, 문서-코드 괴리 특이사항 겸함 | 안전 |
| 후보_10 | `tdc_touch_iqs323_set_ulp` | `touch/tdc_touch_iqs323.c:41`, `.h:107` | 함수 | 호출 0건 | 안전 |
| 후보_11 | `tdc_touch_iqs323_is_ulp` | `touch/tdc_touch_iqs323.c:49`, `.h:109` | 함수 | 호출 0건. `main.c:1130` 부근 코드 주석이 "is_ulp 플래그 미사용" 이라고 명시적으로 자인 | 안전 |
| 후보_12 | `tdc_touch_iqs323_public_settings` | `touch/tdc_touch_iqs323.c:292`, `.h:111` | 함수 | 호출 0건. `static touch_settings()`(같은 파일:284-290)와 **바이트 단위로 동일한 구현**의 공개판 - 내부용 static 만 실사용, 공개판은 죽은 복제본 | 안전 |
| 후보_13 | `led_debug_blink_blue` / `led_debug_blink_12bits` / `tdc_touch_led_debug` | `touch/tdc_touch.c:161,189,225` | 함수 3개 | 헤더(`tdc_touch.h`)에 선언 없음(외부 링크만 있고 미공개) + 유일 호출 체인이 `tdc_touch.c:315-317` `#if 0 /* --- LED를 사용한 터치 디버깅 --- */ tdc_touch_led_debug(...); #endif` 안에 있어 컴파일 시 제외됨. grep 함정 전형 사례(호출부가 죽은 전처리 분기) | 안전 |
| 후보_14 | `df_MaxDeliveryCharge_nC` | `stim/tdc_stim_definitions.h:90` | 매크로 | 정의 외 참조 0건(짝인 `df_MaxDeliveryCharge_pC` 는 `isd/tdc_isd_map_impedance.c:169` 등 4곳에서 실사용). **08_보류-동결-목록.md 동결 27종 census(2026-07-20) 이후 추가된 심볼로 목록에 없음** - 타 프로젝트(CFX/calibration) 복제 여부 미확인 | 주의 |
| 후보_15 | `#if 0` 사장 분기 - 구 `tdc_stim_indicator_set_level_255` 구현 | `stim/tdc_stim_indicator.c:73-125` | 코드 블록 | `#if 0 ... #else(128) ... #endif(179)` 구조로 동일 함수의 신·구 두 구현이 존재, 구버전(73-125)은 영구 비활성. 구버전 내부가 참조하는 `getCalculatedStimulationIndcatorLevel()`(85행) 은 **CM3 전체에 이 한 줄 외 존재하지 않는 함수** - 활성화 시 링크 실패 확정, 오래전에 이미 죽은 코드임을 뒷받침 | 주의 |
| 후보_16 | `#if 0` 사장 분기 - 구 `tdc_stim_calc_transferable_channel_num` 알고리즘 | `stim/tdc_stim_para_cal.c:68-96` | 코드 블록 | `#if 0(68) ... #else(97) ... #endif(101)` - 구현 A(옛 반복 계산)는 영구 비활성, `#else` 의 1줄 나눗셈(99행)만 실행 | 주의 |
| 후보_17 | `#ifdef Df_MaxDeliveryChargeLimitation` 분기 - 전하량 초과 검사 | `stim/tdc_stim_para_cal.c:216-225` | 코드 블록 (조건부 안전 로직) | **매크로명 불일치로 영구 비활성**: 이 파일이 검사하는 심볼은 `Df_MaxDeliveryChargeLimitation` 인데, `board/processorDirective.h:112` 가 실제로 정의한 심볼은 `Df_MaxDeliveryChargeLimitationKKK`(끝에 KKK 접미) - **철자가 다른 별개 매크로**라 `#ifdef` 가 항상 거짓으로 평가되어 `tdc_stim_set_range()` 안의 `MaxDeliveryChargeOver` 전하량 초과 검사가 상시 `false`(=검사 안 함)로 컴파일됨. `Df_MaxDeliveryChargeLimitationKKK` 는 08_보류-동결-목록.md 의 동결 43종 중 하나(교차 프로젝트 복제 존재)라 **이름 자체는 임의 변경 금지 대상** - 즉 "사장 코드 정리"의 대상이 아니라 **의도한 매핑이 다른 것 아닌지 은수님 확인이 필요한 별도 사안**으로 승격 보고 | 위험 |
| 후보_18 | `df_muteStimulationUnder_TLevel`·`T_levelOffset`·`ISD_registerAddr_forwardPath_check_Data` | `stim/tdc_stim_definitions.h:6,11-13` | 매크로 3종 | CM3 소스 전체에서 정의 라인 외 참조 0건(특히 `df_muteStimulationUnder_TLevel` 은 값 없는 빈 매크로라 `#ifdef` 로만 쓰일 수 있는데 그 `#ifdef` 조차 없음) - **그러나 08_보류-동결-목록.md 동결 27종에 전부 포함**되어 CFX/calibration 복제본과 정합해야 하는 공유 ABI 취급 대상이라 제거 후보에서 **제외**(§4 불가침 규칙 적용). 판정 근거로 기록만 남김 | 위험(제거 시) |
| 후보_19 | `TDC_LED_DBG_LONG_TOUCH_IGNORE` 디버그 피드백 서브시스템 | `led/tdc_led_output.h:5`, `.c:306-308,629-632`, `sys/tdc_sys_control.c:212,224`(참고) | 매크로+분기 | `#define TDC_LED_DBG_LONG_TOUCH_IGNORE 0` (헤더 자체 주석: "0 설정 시 기존 동작 복원") - 매크로가 0이라 관련 `#if TDC_LED_DBG_LONG_TOUCH_IGNORE` 분기(우선순위 76 부여, DBG burst 자가해제)가 전부 컴파일 제외. 은수님이 예시로 든 `TDC_PRINTF_INTERFACE` 패턴과 구조적으로 동일(매크로 하나가 상위 분기를 전부 소멸시킴). 단, 헤더 주석에 "디버깅용" 토글로 명시돼 있어 **의도된 기능 스위치**(오프 상태) - 사장이라기보다 "현재 비활성 기능"에 가까움 | 주의 |

## 3. 사장 아님으로 판정한 것 (오탐 방지 기록)

| 대상 | 위치 | 판정 이유 |
|---|---|---|
| `tdc_led_memory_error` | `led/tdc_led_output.c:782`, `.h:165` | **1차 게이트 이월 목록(G5, §6)에 사장 7건 중 하나로 기재돼 있었으나 재조사 결과 오탐으로 확인.** `sys/tdc_sys_init.c:280`의 `while(1) { tdc_led_memory_error(); __WFI(); }` 에서 실사용 - 공유메모리 주소 검증 실패 시 진입하는 런타임 조건(전처리 아님)이라 죽은 분기가 아니다. **이월 목록에서 제외 권고** |
| `tdc_led_turn_on_red/green/blue` | `led/tdc_led_output.c:843,862,881` | `touch/tdc_touch.c:170,203,208`(led_debug_blink_*, 이건 dead), `sys/tdc_sys_init.c:381,406`, `main.c:979` 등 다수 라이브 호출 확인 |
| `tdc_led_set_isd_conn_state` | `led/tdc_led_output.c:89` | `main.c:453` 라이브 호출 |
| `tdc_led_set_ind_state` | `led/tdc_led_output.c:327` | `ble/tdc_ble_communication.c:179` 라이브 호출(QCC 패킷 0x35 파싱 경로) |
| `tdc_touch_debug_get_recent_*` 7종 | `touch/tdc_touch.c:56-89` | `ble/tdc_ble_general_debug.c:75-81` 전부 라이브 호출(BLE 0x8F option1) |
| `tdc_touch_iqs323_clear_ulp` | `touch/tdc_touch_iqs323.c:45,458` | `apply_settings()` 내부에서 자체 호출 - 구조적으로 살아있음. 다만 짝인 `set_ulp`(후보_10)가 죽어 있어 항상 이미-false 를 false 로 다시 쓰는 no-op - 별도 제거 없이 후보_10/11 과 함께 판단 권고 |
| ui `handle_help/led/battery/isd/map/error/volume/program/init_all_map/dump_log/write_integrity_error/gating` 12종 | `ui/tdc_ui_command.c:264,304,452,492,518,544,600,630,678,722,738,752` | **1차 조사(04_사장코드-후보.md, static 후보 12개)가 함수포인터 디스패치 테이블을 놓친 오탐.** 전부 `s_tdc_commands[]`(794-807행) 에 `command_fn_t` 함수포인터로 등록되고 `dispatch()`(864-874행) 가 `s_tdc_table[i].fn(argc, argv)` 로 간접 호출 - grep 이름 검색으로는 "직접 호출 0건"처럼 보이나 실제로는 매 RTT 커맨드 입력마다 호출됨 |
| ui 모듈 전체(`tdc_ui_command_init/poll`, override 6종, `tdc_ui_command_set_mapping_connected`, `tdc_ui_command_is_led_override`) | `ui/tdc_ui_command.c` 전역 | `board/processorDirective.h:85` 의 `#define ENABLE_UI_CMD` 가 **주석 없이 활성**이라 `main.c`·`sys/tdc_sys_control.c` 의 모든 `#ifdef ENABLE_UI_CMD` 분기가 실제로 컴파일됨(라이브). ui 도메인은 사장 후보 0건 |
| `tdc_stim_data_extract_and_rshift`/`tdc_stim_data_clear_bit` | `stim/tdc_stim_common.c:3,18` | `drv/tdc_drv_isl9122.c:96`, `sys/tdc_sys_earpiece.c:69`, `isd/tdc_isd_init.c:925`, `isd/tdc_isd_fpga.c:507` 등 다수 라이브 호출 |
| `tdc_stim_indicator_is_triggered`/`set_trigger`/`out`/`set_level_255` | `stim/tdc_stim_indicator.c` 전역 | `main.c:633`(indicator_out), `isd/tdc_isd_map_live.c:147,317` 등에서 라이브 호출 확인. `set_trigger` 의 외부(`isd_map_live.c:311`) 호출부는 주석 처리돼 죽어있으나 파일 내부(`indicator.c:35,41`)에서 살아있어 함수 자체는 사장 아님 |
| `tdc_stim_calc_para_and_cfx_share`/`calc_frame_per_channel`/`calc_transferable_channel_num`/`read_dac_register_value`/`read_dac_offset_value`/`set_range` | `stim/tdc_stim_para_cal.c` 전역 | `isd/tdc_isd_stim_standalone.c`, `isd/tdc_isd_map_live.c`, `isd/tdc_isd_map_test_stim.c`, `isd/tdc_isd_map_specific_stim.c`, `isd/tdc_isd_stim_para_setting.c`, `ble/tdc_ble_remote_sp_para.c` 등 6개 이상 소비 파일에서 라이브 호출 |
| `tdc_stim_definitions.h` 동결 27종 중 실사용 24종(`df_MaxNumOfElectrode`, `df_stimulation_Max/Min`, `FFT_Size` 등) | `stim/tdc_stim_definitions.h` | 각각 `stim/tdc_stim_para_cal.c`·`isd/*` 등에서 실사용 확인 - 동결 대상이자 실사용도 확인되는 이중 안전 |
| `touch/tdc_touch_logic.c` 전체(`tdc_touch_logic_init/set_boot_ignore/step`, `stuck_eval`) | `touch/tdc_touch_logic.c` | 전부 `tdc_touch.c` 가 호출(라이브). `TDC_TOUCH_STUCK_TIMEOUT_MS`(30000, 0 아님)라 `#if` 분기도 활성 |

## 4. 판단 보류 · 추가 확인 필요

1. **후보_17 (`Df_MaxDeliveryChargeLimitation` vs `Df_MaxDeliveryChargeLimitationKKK` 매크로명 불일치)** - 자극 전하량(charge) 초과 안전 검사가 조용히 비활성 상태다. `KKK` 접미는 08_보류-동결-목록.md 동결 목록(타 프로젝트 복제 확인됨)에 이미 등재돼 있어 이 리팩토링에서 발생한 문제는 아니고 원래부터 이런 상태였을 가능성이 높다(사장 조사 범위를 넘는 잠재 결함). **삭제/정리 대상이 아니라 은수님 확인 필요 사안으로 별도 보고 권고**: (a) 원래 의도대로 `Df_MaxDeliveryChargeLimitation` 검사를 살릴 것인지, (b) `KKK` 상태가 의도된 영구비활성(예: 이미 다른 계층에서 전하량을 제한해 중복 검사라 일부러 꺼둔 것)인지 확인 필요.
2. **후보_14 (`df_MaxDeliveryCharge_nC`)** - 08_보류-동결-목록.md census(2026-07-20) 이후 추가된 매크로라 CFX/calibration 복제 여부를 이 저장소만으로는 확인 불가. 제거 전 타 프로젝트 대조 필요.
3. **후보_8 (test-trigger 서브시스템)** - `tdc_led_enable/disable_test_trigger`+`is_test_trigger_enabled`+`testLED_Trigger` 4개를 묶어서 한 번에 제거할지, 디버깅용으로 존치할지는 실기 디버깅 관행에 대한 은수님 판단 필요(1차 게이트 노트도 "실기 동작 검증이 특히 중요한 계층이라 이번 게이트에서 제거하지 않는다"고 보류한 바 있음 - 동일 논리 적용 가능).

## 5. 특이사항

- **1차 이월 11건 재판정 결과**: LED 레거시 7건 중 6건(`black`/`white`/`clock_error`/`pattern_out`/`disable_test_trigger`/`enable_test_trigger` - `is_test_trigger_enabled` 는 구조적으로 살아있어 재분류) 사장 재확인, **`tdc_led_memory_error` 1건은 재조사 결과 오탐으로 정정**(§3). touch 4건(`get_state`/`iqs323_set_ulp`/`is_ulp`/`public_settings`) 전부 사장 재확인. 즉 이월 11건 중 10건 사장 유지 + 1건(`tdc_led_memory_error`) 제외.
- **문서-코드 드리프트 2건 발견**: (1) `tdc_touch.c:12`/`tdc_touch.h:13` 헤더 주석이 실재하지 않는 호출 경로(`func_sleep` → `tdc_touch_get_state`)를 설명 - 실제로는 `main.c` 가 `tdc_touch_iqs323_read_status()` 를 직접 호출해 동일 로직을 중복 구현 중(§2 후보_9). (2) `stim/tdc_stim_para_cal.c:216` 근처는 주석 없이 `#ifdef Df_MaxDeliveryChargeLimitation` 만 있어 코드만 봐서는 정상 동작처럼 보이나 실제로는 매크로명 불일치로 죽어있음(§2 후보_17) - 은수님이 예시로 든 UART/`TDC_PRINTF_INTERFACE` 패턴과 같은 유형.
- **UI 도메인은 사장 후보 0건.** 1차 조사가 static 함수 12개를 "호출 0건"으로 냈던 것은 함수포인터 디스패치 테이블(`command_fn_t`)을 grep 이 추적하지 못한 전형적 오탐 - `ENABLE_UI_CMD` 가 현재 빌드에서 활성이라 도메인 전체가 라이브.
- **grep 함정 3건 직접 확인**: (1) `isd/tdc_isd_init.c:931` 의 `// tdc_led_enable_test_trigger();` 주석 호출, (2) `touch/tdc_touch.c:315-317` 의 `#if 0` 안 `tdc_touch_led_debug()` 호출, (3) `stim/tdc_stim_indicator.c` 와 `stim/tdc_stim_para_cal.c` 의 `#if 0/#else` 쌍(같은 함수의 신·구 구현이 공존) - 셋 다 "grep 은 사용 중이라 하나 실제로는 죽은 분기" 케이스로, 과제 지시에서 경고한 함정 그대로 재현됨.
- **`sections.ld`/`cfx_cm3_sharedMemoryAll`/`FS_MEM_UART`/`*_IRQHandler`/SDK `Sys_*`/`isd/tdc_isd_map_*`/`lib/` 원본 API** 등 불가침 목록에 해당하는 항목은 담당 폴더(led/touch/ui/stim) 안에 존재하지 않아 배제 판단이 발생하지 않음. `tdc_stim_definitions.h` 의 동결 27종만 §4 규칙 적용 대상이었음.
