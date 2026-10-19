---
name: 사장 조사 - ble qcc dfu (22파일)
purpose: ble(13)·qcc(2)·dfu(7) 도메인 22파일 전수 조사로 사장 코드 후보와 위험도를 기록
type: tasks/에이전트로그
applies_to: [Sound1]
tags: [cm3, dead-code, 조사, ble, qcc, dfu]
---

# 06_사장조사-ble-qcc-dfu

**TL;DR**: ble·qcc·dfu 22파일(6,911줄) 전수 조사 결과 사장 후보 13건(안전 11 · 주의 1 · 위험 1, 안전 항목 중 하나는 dfu OTA 헤더의 미사용 상수 31개 묶음). `tdc_ble_communication_reset_globals`/`tdc_ble_mapping_is_program_connected`는 선언만 있고 구현조차 없는 유령 함수, `dfu/tdc_dfu_ota.c` 전체(417줄)는 실제 BLE OTA(`tdc_dfu_ble_ota.c`)에 완전히 대체된 무호출 구모듈이다. 가장 중요한 발견은 `ble/tdc_ble_mapping.c`의 `en__mapping_testStimulation` 분기가 `#ifndef RELEASE`인데 `RELEASE`가 무조건 정의돼 있어 영구 사장이고, 그 유일한 호출 대상인 `isd/tdc_isd_map_test_stim.c`(891줄)를 **05번 isd 보고서가 "살아있음"으로 오판**했다는 점 — 교차 확인 필요.

## 1. 조사 범위와 방법

- 대상: `E:/workspace/projects/sound1-fw-e8300/src/2__cm3/source` 아래 `ble/`(13파일), `qcc/`(2파일), `dfu/`(7파일) = 22파일, 6,911줄.
- 방법: 각 헤더의 public 함수·전역변수·매크로·타입을 추출 → `Grep`으로 `source/` 전체(153파일)에서 참조처 검색 → 호출이 발견되면 그 호출을 감싸는 `#if`/`#ifdef`/`#ifndef`을 실제 매크로 정의 상태(`board/processorDirective.h`)와 대조해 **살아있는 분기인지 재확인**.
- `board/processorDirective.h`를 먼저 확인해 이 도메인에 영향을 주는 조건부 매크로를 고정했다.

| 매크로 | 실제 상태 (processorDirective.h) | ble 도메인 영향 |
|---|---|---|
| `RELEASE` | 22행, 무조건(래핑 없이) `#define` — "메모리 부족으로 테스트용 자극 출력 비활성화" 주석 | `ble/tdc_ble_mapping.c`의 `#ifndef RELEASE` 분기(en__mapping_testStimulation 처리 2곳) 영구 사장. Eclipse 빌드 설정은 "Debug" 단일 구성뿐이라 다른 빌드 경로로도 undefine 되지 않음 |
| `DebuggerEnable` | 20행, 무조건 정의 | ble 도메인 직접 참조 없음 (영향 없음) |

- **BLE 스택 콜백 우려에 대한 확인 결과(음성)**: 지시사항은 "BLE 스택 콜백은 SDK가 이름/함수포인터 테이블로 결속하니 호출처 0건이어도 사장이 아닐 수 있다"고 경고했다. 그러나 `ble/*.c` 전체에서 `GAP`·`GATT`·`ble_evt`·`sd_ble`·`softdevice` 등 BLE SDK 심볼을 검색한 결과 **0건**이었다. 이 코드베이스의 "BLE"는 CM3 내부에 SDK가 통합된 것이 아니라, 별도 nRF(BLE) 칩을 SPI 프로토콜(`tdc_hal_spi_*`)로 통신하는 상대측으로 취급한다. 따라서 함수 포인터 콜백 테이블 문제는 이 도메인에는 적용되지 않는다(호출부는 전부 명시적 함수 호출).
- **`ble/tdc_ble_communication.h`가 `tdc_hal_uart.h`를 include하는 이유**: 조사 결과, 이 include가 실제로 필요해서가 아니라 **불필요한 잔재**로 판단된다. `tdc_ble_communication.c`(502줄) 전체에서 "uart"/"UART" 문자열이 단 한 번도 등장하지 않고, `FS_MEM_UART`·`tdc_hal_uart_printf`·`tdc_hal_uart_uninit` 어느 것도 쓰이지 않는다. 이 헤더를 참조하는 `main.c`/`main.h`도 마찬가지다. `tdc_hal_uart.h`는 `util/tdc_util.h`·`util/tdc_printf.h`·`fs/tdc_fs.h`·`hal/tdc_hal_dio.h`·`main.c`·`sys/tdc_sys_init.c`·`sys/tdc_sys_control.c`·`hal/tdc_hal_spi.c` 등 다른 8개 파일에서 이미 폭넓게 include되므로, 이 한 곳에서 제거해도 `FS_MEM_UART`(불가침 지정 항목) 가용성에는 전혀 영향이 없다. 과거 이 파일이 UART 기반 통신이었다가 SPI로 전환되면서 남은 흔적으로 추정(추정).

## 2. 사장 후보 목록

| # | 대상 | 위치 | 종류 | 근거 | 위험도 |
|---|---|---|---|---|---|
| 후보_1 | `tdc_ble_communication_reset_globals` | `ble/tdc_ble_communication.h:20` | 함수 선언 | 헤더에 선언만 있고 `tdc_ble_communication.c`(502줄 전체 확인) 어디에도 정의가 없다. 호출처도 0건. `tdc_hal_uart_printf()`와 동일한 "선언은 있는데 실체가 없는" 패턴 | 안전 |
| 후보_2 | `tdc_ble_mapping_is_program_connected` | `ble/tdc_ble_mapping.h:135` | 함수 선언 | 헤더 선언만 있고 `tdc_ble_mapping.c`(2,328줄 전체 확인) 어디에도 정의가 없다. 호출처도 0건. 대신 동일 역할을 하는 함수-스코프 `static bool mappingProgramConnected`(`tdc_ble_mapping.c:1880`)가 `ST__MAPPING_STATE.mappingConnection`으로 반환되어 실사용 중 — 이 함수는 그 대체 이전에 설계된 미완성 API로 추정(추정) | 안전 |
| 후보_3 | `tdc_ble_mapping_get_command` | `ble/tdc_ble_mapping.c:45`, 선언 `.h:138` | 함수 | 정의는 있으나 `source/` 전체에서 호출처 0건(grep 전수 확인) | 안전 |
| 후보_4 | `tdc_ble_mapping_update_program_connection` | `ble/tdc_ble_mapping.c:2294`, 선언 `.h:149` | 함수 | 정의는 있으나 호출처 0건. `mappingPacket.fetched_command`를 connect/disconnect로 강제 설정하는 함수인데, 정작 `mappingPacket.fetched_command`는 `tdc_ble_mapping_fetch_packet()` 내부에서만 갱신됨 | 안전 |
| 후보_5 | `tdc_ble_remote_get_command` | `ble/tdc_ble_remote.c:54`, 선언 `.h:50` | 함수 | 정의는 있으나 호출처 0건. 대응하는 전역 `remoteDataPacket`(`.c:25`, non-static)도 `tdc_ble_remote.c` 자기 자신만 참조(grep으로 다른 파일 참조 0건 확인) | 안전 |
| 후보_6 | `en__BLE_COMM_COMMAND_connecteLogDate` (0x31) | `ble/tdc_ble_protocol.h:35` | 열거값 | 정의만 있고 어디서도 비교/case로 쓰이지 않음. `tdc_ble_communication_step()`의 커맨드 라우팅 5단 분기 어디에도 0x31 범위가 없어, 이 값이 수신돼도 매칭되는 곳이 없다 | 안전 |
| 후보_7 | `en__BLE_COMM_COMMAND_REPlY_ERROR_BLE` (0xF0) | `ble/tdc_ble_protocol.h:37` | 열거값 | 정의만 있고 미사용. 동일 값 0xF0을 갖는 `en__ERROR_Response`(`tdc_ble_protocol.h:23`)가 `sys/tdc_sys_error.c:164`에서 실사용되고 있어 이름만 다른 죽은 중복 | 안전 |
| 후보_8 | `PayloadSize_ExternalDeviceInfo_Size_byte` | `ble/tdc_ble_protocol.h:122` | 매크로 | 정의만 있고 `source/` 전체에서 사용처 0건. 같은 줄 인근의 `Max_ImpedanceReturnDataSize`는 `isd/tdc_isd_map_impedance.c:646`에서 실사용 확인(대조군) | 안전 |
| 후보_9 | `ble/tdc_ble_communication.h`의 `#include <tdc_hal_uart.h>` | `ble/tdc_ble_communication.h:4` | 불필요 include | §1 서술 참고. 파일·소비처 어디서도 UART 심볼 미사용, 다른 8개 파일이 이미 동일 헤더를 include해 제거해도 영향 없음 | 안전 |
| 후보_10 | `RECV_PKT_SIZE_BOOT_INFO` / `RECV_PKT_SIZE_BOOT_SELECT` | `dfu/tdc_dfu_ble_boot.h:28-29` | 열거값(패킷 크기 상수) 2개 | 정의만 있고 `tdc_dfu_ble_boot.c` 어디서도 참조 없음. 실제 수신 파싱은 이 상수 대신 `p_packet[...]` 원시 인덱스 접근으로 처리됨. 대응하는 `RESP_PKT_SIZE_BOOT_INFO`/`RESP_PKT_SIZE_BOOT_SELECT`는 `tdc_dfu_ble_boot.c:54,79,110`에서 실사용(대조군) | 안전 |
| 후보_11 | `dfu/tdc_dfu_ble_ota.h`의 미사용 상수 클러스터(31개 심볼) | `dfu/tdc_dfu_ble_ota.h:67-141` | 열거값·매크로·구조체 타입 다수 | `RECV_PKT_SIZE_OTA_WRITE/READ/SIZE`(3) · `RESP_PKT_SIZE_OTA_WRITE/READ/SIZE`(3) · `RECV_PKT_IDX_OTA_*`(8) · `RESP_PKT_IDX_OTA_*`(8) · `ST__OTA_CONTROL_PACKET`(구조체 타입, 인스턴스화 0건) · `OTA_RETRY_MAX` · `CI_OTA_STATE_E`(`CI_OTA_STATE_FALL_BACK`/`_OTA_UPDATE`, 2) · `CI_OTA_UPDATE_STATE_E`(4) · `CI_OTA_RESULT_E`(2) 전부 `tdc_dfu_ble_ota.c` 어디서도 참조 안 됨. 실제 구현은 응답 패킷 크기·인덱스를 매직넘버(9, 13, `p_packet[4]` 등)로 직접 처리한다. 심지어 `RESP_PKT_SIZE_OTA_READ=8`로 정의돼 있지만 실제 read 응답은 9바이트를 보내(`tdc_dfu_ble_ota.c` 관련 코드, `_send_resp_packet_boot(resp_packet, 9)`) 값 자체가 실제 동작과 어긋난다 — 애초에 초안으로 만들다 만 채 방치된 것으로 추정(추정) | 안전 |
| 후보_12 | `dfu/tdc_dfu_ota.c` + `dfu/tdc_dfu_ota.h` 전체 모듈 | `dfu/tdc_dfu_ota.c`(417줄) 전체, `.h`(53줄) 전체 | 파일 전체(함수 4개: `tdc_dfu_ota_command_parsing`·`ota_update_file`·`tdc_dfu_ota_prepare_file`·`tdc_dfu_ota_write_file`) | 유일한 공개 API `tdc_dfu_ota_command_parsing()`(`.c:154`, 헤더선언 `.h:51`)이 `source/` 전체에서 호출처 0건. RTT 콘솔 입력(`RTT_getch`)으로 `--write=`/`--boot=`/`--valid` 커맨드를 파싱하는 디버그 콘솔 유틸로 보이나 어디서도 기동되지 않는다. 내부 `ota_update_file()`은 같은 파일 안에서만 호출(고아). `tdc_dfu_ota_prepare_file()`/`tdc_dfu_ota_write_file()`은 헤더에 선언조차 없는 전역 함수인데 호출처 0건 — 실제 BLE OTA 파일 쓰기는 `dfu/tdc_dfu_ble_ota.c`의 `_handle_command_option_write()`/`_fetch_packet_data()`가 자체 `f_open`/`f_write` 호출로 완전히 대체 구현하고 있어 중복·구식 모듈로 판단. `.cproject`에 이 파일을 빌드에서 제외하는 `excluding` 설정도 없어(확인 완료) 실제로 컴파일은 되지만 도달 불가능한 코드다 | 주의 (파일 전체 삭제 영향 범위가 크고 FatFs 의존 코드라 삭제 전 빌드 확인 필요) |
| 후보_13 | `en__mapping_testStimulation`(0x90) 처리 분기 2곳 | `ble/tdc_ble_mapping.c:1768-1853`(패킷 파싱), `2213-2230`(상태머신 디스패치) | 죽은 전처리 분기(`#ifndef RELEASE`) | `board/processorDirective.h:22`에서 `RELEASE`가 래핑 없이 무조건 `#define`되어 있어(§1) 두 `#ifndef RELEASE` 블록은 현재 빌드에서 **영구 컴파일 제외**. 이 중 `2221`행의 `testStimulation(mappingCommandStartFlag)` 호출이 `isd/tdc_isd_map_test_stim.c:30`의 `testStimulation()`(891줄)의 **유일한 호출자**인데, 그 호출 자체가 죽은 분기 안에 있으므로 해당 isd 파일 전체가 RELEASE 빌드에서 도달 불가능하다. §4에서 상세 서술 | 위험 (의료기기 자극 출력(ISD) 인접 코드이고, 삭제 여부가 isd 도메인 파일에도 영향을 미쳐 은수님 확인 및 05번 보고서와 교차 검토가 반드시 필요) |

**요약 카운트**: 안전 11 · 주의 1 · 위험 1 (합계 13건, 후보_11은 31개 심볼을 1건으로 묶어 계상)

## 3. 사장 아님으로 판정한 것 (오탐 방지 기록)

| 대상 | 위치 | 제외 사유 |
|---|---|---|
| `cfx_cm3_sharedMemoryAll` | `ble/tdc_ble_communication.c:18,221`, `ble/tdc_ble_gain_control.c:17,29-31` 등 | 지시사항 불가침 목록 명시 (CFX·calibration ABI) |
| `FS_MEM_UART` / `FS_MEM_UART_T` | `hal/tdc_hal_uart.h:37-138` | 지시사항 불가침 목록 명시. 후보_9(불필요 include 제거)는 이 타입 자체가 아니라 `ble/tdc_ble_communication.h`에서의 **불필요한 재-include**만을 대상으로 함 — 다른 8개 파일의 include는 그대로 유지되어 `FS_MEM_UART` 가용성에는 영향 없음 |
| `tdc_dfu_sdk_boot.h`의 `tdc_boot_status_t` | `dfu/tdc_dfu_sdk_boot.h:47-65` | 부트로더와 공유하는 파일 ABI. `src/0__bootloader/include/sdk_ci_boot.h`의 부트 상태 구조체와 필드명·오프셋·크기(64바이트)가 완전히 일치함을 직접 대조 확인(`major_ver`~`crc32`, `reserved[47]`). CM3 쪽에서 이 타입을 사용하는 함수가 적어도 `dfu/tdc_dfu_ble_boot.c`(`tdc_boot_get_status`/`tdc_boot_update_status` 경유)에서 실호출되므로 활성 ABI. 삭제 후보 아님 |
| BLE 스택 콜백 테이블 | `ble/*.c` 전체 | §1 서술 참고. `GAP`/`GATT`/`ble_evt`/`sd_ble`/`softdevice` 검색 0건 — 이 코드베이스에 SDK 콜백 결속 패턴 자체가 존재하지 않음(음성 확인이지만 지시사항이 명시적으로 요구해 기록) |
| `en__remoteControl_read_OwnerNameOfExternalDevice`(0x55, write_SerialNumOfExternalDevice) / `write_ParingKey`(0x56) 등 "nRF 자체 처리" 주석이 붙은 리모콘 커맨드 | `ble/tdc_ble_protocol.h:68-74` | CM3 측에 case 핸들러가 없는 것은 사실이나, 헤더 주석에 "확인 완료: nRF 자체 처리"라고 이미 명시돼 있다. 즉 이 커맨드들은 애초에 CM3까지 SPI로 전달되지 않고 컴패니언 nRF(BLE) 칩 선에서 응답까지 완결되도록 설계된 것 — 미구현이 아니라 설계상 무핸들러. 사장 아님 |
| `tdc_ble_remote_read_sp_para` | `ble/tdc_ble_remote_sp_para.c:12`, 호출 `ble/tdc_ble_remote.c:1217` | 실사용 확인. 8단계 상태머신(flowCounter 0~7)으로 자극 파라미터를 SPI로 순차 전송하는 구현, 전량 정상 참조 |
| `tdc_ble_gain_control_init/handle`, `tdc_ble_general_debug_handle`, 각 파일의 `static` 내부 헬퍼 전부(`gc_*`, `gd_*`) | `ble/tdc_ble_gain_control.c`, `ble/tdc_ble_general_debug.c` | 전부 실호출 확인(`sys/tdc_sys_init.c:345`, `ble/tdc_ble_remote.c:1348,1362` 등). 내부 static 헬퍼도 각 진입점 함수 안에서 전부 참조됨 |
| `tdc_qcc_init/set_isd/set_mode` | `qcc/tdc_qcc.c` 전체 | 전부 실호출 확인(`hal/tdc_hal_dio.c:77`, `isd/tdc_isd.c:263-281`, `main.c`/`sys/tdc_sys_init.c`/`dfu/tdc_dfu_ble_boot.c:117`). qcc 도메인은 2파일 모두 사장 후보 없음 |
| `tdc_dfu_ble_fetch_boot/ota/ota_start_end`, `tdc_dfu_get_conn_state/set_conn_state`, `tdc_dfu_ble_ota.c`의 `static` 헬퍼(`_send_resp_packet_boot` 등) | `dfu/tdc_dfu_ble_boot.c`, `dfu/tdc_dfu_ble_ota.c` | 전부 `ble/tdc_ble_communication.c:183-360`, `ble/tdc_ble_general_debug.c:110-114`, `isd/tdc_isd.c:228`에서 실호출 확인. 실질적인 BLE OTA·부트 슬롯 전환 로직은 이 두 파일이 전담하며 사장 후보 없음 |
| `en__mapping_testStimulation`을 05번(isd) 보고서가 "살아있음"으로 판정한 근거(`tdc_ble_mapping.c:2221`) | `docs/tasks/cm3/20260723_cm3-full-refactor-2nd/에이전트-로그/05_사장조사-isd.md:56` | **정정 필요**: 05번 보고서는 이 호출을 발견했지만 그 호출을 감싸는 `#ifndef RELEASE`(§2 후보_13)를 확인하지 않은 것으로 보인다. `RELEASE`가 무조건 정의돼 있어 이 호출은 실제로 죽은 분기 안에 있다 — §4에서 교차 확인 요청 |

## 4. 판단 보류 · 추가 확인 필요

- **후보_13과 05번(isd) 보고서의 상충**: `ble/tdc_ble_mapping.c:2221`의 `testStimulation()` 호출은 `#ifndef RELEASE`(`.c:2213`) 안에 있고, `RELEASE`는 `board/processorDirective.h:22`에서 어떤 조건도 없이 `#define`돼 있다(주석: "메모리 부족으로 테스트용 자극 출력(동물실험 및 내부기 시험용 코드 비활성화)"). 즉 **현재 소스 트리로 빌드하는 한 이 분기는 항상 컴파일에서 빠진다.** 05번 보고서(§3 "사장 아님" 표, `tdc_isd_map_test_stim.c/h`)는 이 호출을 근거로 "살아있음"이라 판정했는데, 전처리 분기를 확인하지 않은 채 grep 결과만 본 것으로 보인다(1차 조사의 "드라이버 5종 중 1종만 생존" 함정과 동일 패턴). `isd/tdc_isd_map_test_stim.c`(891줄) 전체가 RELEASE 빌드에서 도달 불가능한 코드인지 여부는 **isd 담당 노드·오케스트레이터가 재확인**해야 한다. 단, 이 매크로 자체가 "메모리 부족으로 인한 임시 비활성화"라는 명시적 의도를 담고 있어 곧바로 삭제 대상으로 단정하기보다는 은수님의 판단(디버그 빌드 부활 계획이 있는지)을 거쳐야 한다.
- **`en__remoteControl_readSystemErrorCode`(0x58)**: 헤더 주석에 은수님 본인이 "NOTE: 실제 사용되고 있나? 이거 보내면 묵묵부답으로 시간초과 타임아웃 발생함"이라고 남겼다. 실제로 `ble/tdc_ble_remote.c`의 커맨드 스위치(`.c:806-1531`)에는 이 값에 대한 `case`도 `default:`도 없어, 이 커맨드가 수신되면 스위치 전체가 아무 것도 하지 않고 응답이 나가지 않는다(타임아웃 재현 가능). "사장 코드"라기보다 "핸들러가 없는 활성 프로토콜 값"이라 이번 조사의 본래 범위(미사용 코드)와는 결이 다르지만, 실제 버그로 이어질 수 있어 참고용으로 기록한다. 삭제 대상이 아니라 구현 필요 여부를 은수님이 판단해야 함.
- **패스키 인증 우회(`tdc_ble_remote.c:730-739`)**: "패스키 인증을 더 이상 사용하지 않는다"는 주석과 함께 실제 매치 비교 루프가 `#if 0`로 비활성화되어 있고, `remocon_passkey_Match`가 항상 `true`로 고정된다(`.c:728,741`). 그 결과 `else { bufferForSPI_tx[tx_index++] = 2; }`(불일치 응답) 분기가 조건상 도달 불가능해졌다. 다만 이는 개별 지역 변수 분기(1줄)라 파일/함수 단위 사장 후보로 별도 등재하지 않고 특이사항으로만 기록한다.

## 5. 특이사항

- **`RELEASE` 매크로가 사장 코드 판정의 핵심 축**: `board/processorDirective.h:22`의 `#define RELEASE`는 조건 없이 항상 켜져 있고, Eclipse `.cproject`에는 빌드 구성이 "Debug" 하나뿐이라(별도 컴파일러 `-D` 플래그로 껐다 켰다 하는 경로도 없음) 이 프로젝트를 지금 형태로 빌드하는 한 `#ifndef RELEASE` 코드는 절대 살아나지 않는다. 후보_13 외에 다른 도메인에도 `#ifndef RELEASE`가 있는지는 이번 조사 범위(ble/qcc/dfu) 밖이라 다루지 않았지만, 09번(죽은 전처리) 계열 보고서가 있다면 이 매크로를 §1 표에 반영해 전사 재확인할 가치가 있다.
- **1차 리팩토링에서 이미 정리된 사장 코드**: `ble/tdc_ble_mapping.c:2305-2308`의 주석에 따르면 `import_*ForDebug`/`import_live*` 계열 15개 함수(맵핑 프로토콜 수동 주입용 구 디버그 진입점)가 2026-07-22 G6 작업에서 이미 제거되었다(그 중 `import_livePause`는 선언만 있고 정의조차 없는 고아였다고 기록돼 있음 — 이번 조사에서 발견한 후보_1/2 유령 함수와 동일한 패턴이 1차 조사에도 있었다는 방증). 이번 2차 조사에서 새로 발견한 후보들과는 별개다.
- **텔레코일 기능 강제 비활성화**: `en__remoteControl_OnOffTelecoil`(0x47) 커맨드 자체는 `ble/tdc_ble_remote.c:1152`에서 여전히 처리되어 응답을 정상적으로 돌려주지만, 실제 저장되는 값은 `ble/tdc_ble_mapping.c:1350-1354`의 `#if 0`/`#else`로 인해 사용자가 무슨 값을 보내든 항상 `2`(비활성)로 고정된다("Telecoil은 현재 버전에서 비활성화 시킨다" 주석). 함수 자체는 사장이 아니라 기능이 죽어있는 케이스라 후보 목록에는 넣지 않았다.
- **RTT 콘솔 디버그 유틸(`dfu/tdc_dfu_ota.c`)의 존재**: 이 파일은 이름과 달리 BLE OTA 프로토콜 파서가 아니라, SEGGER RTT 콘솔 입력을 통해 `--write=`/`--boot=`/`--valid` 텍스트 커맨드를 받는 개발자 전용 디버그 콘솔이었던 것으로 보인다(추정). 실제 제품의 OTA는 전량 `dfu/tdc_dfu_ble_ota.c`(BLE 패킷 기반)로 이관된 뒤 이 파일이 정리되지 않고 남은 것으로 판단된다.
