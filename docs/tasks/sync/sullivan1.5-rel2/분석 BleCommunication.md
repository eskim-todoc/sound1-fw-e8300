---
name: Sullivan1.5 ↔ Sound1 BleCommunication 비교 분석
purpose: 베이스 프로젝트 Sullivan1_5__CM3 와 Sound1 2__cm3 의 BleCommunication 폴더 의미적 diff 정리
type: 분석
revision: Rev.0
status: 완료(BleCommunication 영역)
applies_to: [Sound1, sync, ble]
tags: [sullivan, baseline, diff, ble-communication, comparison]
---

# Sullivan1.5 ↔ Sound1 BleCommunication 비교 분석 (Rev.0)

**TL;DR**: `src/Sullivan1_5__CM3/Cortex-M3-src/BleCommunication/` ↔ `src/2__cm3/Cortex-M3-src/BleCommunication/` 13 파일 의미적 비교(공백·빈줄 무시, 주석은 명시 시 별도 표시). **9 파일 동등, 4 파일 차이**. 차이의 대부분은 Sound1 측 추가(QCC 0x33~0x35 시스템 정보 패킷·`snd_boot_*` 타입·QCC SHUTDOWN 시퀀스·RTT 로그 전환)이며, **Sullivan에만 있고 Sound1에 미반영**된 항목은 ① `ci_ble_control_ota.c` 의 `snd_fatfs_remount_twice()` 마운트 2회 패치 7~8 위치, ② 동 파일의 DFU 디버그 로그 7 줄. 본 두 항목은 사용자 검토 후 Sound1 반영 여부 결정 필요.

> [!IMPORTANT]
> **비교 기준**: `git diff --no-index -w --ignore-blank-lines` (모든 공백·빈줄 차이 무시). 주석/매크로 변경은 출력에 포함되므로 본 문서에서 별도 표시.
>
> **사용자 가설**: "Sullivan → Sound1 병합 이후 Sullivan 쪽에 추가 업데이트가 있었던 것으로 기억". 본 분석 결과는 이 가설과 부분 일치 — `ci_ble_control_ota.c` 일부 변경(remount 2회 패치)이 그에 해당. 나머지는 대부분 Sound1 측 후속 작업.

## 1. 비교 범위

- Base (좌측 / `a`): `src/Sullivan1_5__CM3/Cortex-M3-src/BleCommunication/`
- Target (우측 / `b`): `src/2__cm3/Cortex-M3-src/BleCommunication/`
- 파일 13 개:
  - `ble_commonProtocol.h`
  - `ble_communication.c` / `.h`
  - `ci_ble_control_boot.c` / `.h`
  - `ci_ble_control_ota.c` / `.h`
  - `mappingControl.c` / `.h`
  - `remoteControl.c` / `.h`
  - `remoteControl_read_SP_para.c` / `.h`

## 2. 요약표 (파일 단위)

| # | 파일 | 동등? | 변경 방향 | 비고 |
|---|---|---|---|---|
| 1 | `ble_commonProtocol.h` | 차이 | Sound1 → 추가 | 신규 enum `EN__SND_BT_CMD_SYSTEM_INFO` (0x33/0x34/0x35) |
| 2 | `ble_communication.c` | 차이 | Sound1 → 대규모 추가 | QCC 0x33~0x35 패킷 처리, ISD 미연결 더미 응답, SPI ERROR 로그 |
| 3 | `ble_communication.h` | **동등** | - | 공백/빈줄만 차이 |
| 4 | `ci_ble_control_boot.c` | 차이 | Sound1 → 추가 | `snd_qcc.h` include, 타입 마이그레이션(`snd_boot_*`), QCC SHUTDOWN 시퀀스, `ci_printw → ci_printf` |
| 5 | `ci_ble_control_boot.h` | **동등** | - | 공백/빈줄만 차이 |
| 6 | `ci_ble_control_ota.c` | 차이 | **양방향 분기** | Sound1: 타입 마이그레이션·로그 RTT 전환·디버그 로그 제거·**mount 1회 환원**. Sullivan: **mount 2회 패치**·DFU 디버그 로그 유지 |
| 7 | `ci_ble_control_ota.h` | **동등** | - | 공백/빈줄만 차이 |
| 8 | `mappingControl.c` | **동등** | - | 공백/빈줄만 차이 |
| 9 | `mappingControl.h` | **동등** | - | 공백/빈줄만 차이 |
| 10 | `remoteControl.c` | **동등** | - | 공백/빈줄만 차이 |
| 11 | `remoteControl.h` | **동등** | - | 공백/빈줄만 차이 |
| 12 | `remoteControl_read_SP_para.c` | **동등** | - | 공백/빈줄만 차이 |
| 13 | `remoteControl_read_SP_para.h` | **동등** | - | 공백/빈줄만 차이 |

→ **차이 있는 파일: 4 개** (1, 2, 4, 6) / **동등 파일: 9 개** (3, 5, 7~13)

## 3. Sullivan에만 있는 변경 (Sound1 미반영 — 검토 필요)

본 절은 **사용자 주요 관심사**. Sullivan에 존재하지만 Sound1에는 없는 항목으로, 후속 반영 여부 판단이 필요한 후보들이다.

### 3.1 `ci_ble_control_ota.c` — `snd_fatfs_remount_twice()` 마운트 2회 패치

Sullivan 측 IMPORTANT 코멘트:

```c
// IMPORTANT: unmount, mount를 2번 수행해야 정상 동작함 (원인 불명)
```

**적용 위치 8 곳** (Sullivan 기준):

| # | 함수 | 상황 | Sullivan | Sound1 |
|---|---|---|---|---|
| 1 | `_handle_command_option_write()` | 부트 드라이브로 전환 | `snd_fatfs_remount_twice(0)` | `snd_fatfs_remount(SND_FATFS_LDRV_NUM_BOOT)` |
| 2 | `_handle_command_option_write()` | mkdir 실패 시 사용자 드라이브 복귀 | `snd_fatfs_remount_twice(1)` | `snd_fatfs_remount(SND_FATFS_LDRV_NUM_USER_DATA)` |
| 3 | `_handle_command_option_write()` | 파일 오픈 실패 시 복귀 | `snd_fatfs_remount_twice(1)` | `snd_fatfs_remount(SND_FATFS_LDRV_NUM_USER_DATA)` |
| 4 | `_handle_command_option_size()` | 부트 드라이브로 전환 | `snd_fatfs_remount_twice(0)` | `snd_fatfs_remount(SND_FATFS_LDRV_NUM_BOOT)` |
| 5 | `_handle_command_option_size()` | 사용자 드라이브 복귀 | `snd_fatfs_remount_twice(1)` | `snd_fatfs_remount(SND_FATFS_LDRV_NUM_USER_DATA)` |
| 6 | `_fetch_packet_data()` | 데이터 인덱스 에러 시 복귀 | `snd_fatfs_remount_twice(1)` | `snd_fatfs_remount(SND_FATFS_LDRV_NUM_USER_DATA)` |
| 7 | `_fetch_packet_data()` | 파일 쓰기 실패 시 복귀 | `snd_fatfs_remount_twice(1)` | `snd_fatfs_remount(SND_FATFS_LDRV_NUM_USER_DATA)` |
| 8 | `_fetch_packet_data()` | 마지막 데이터 인덱스 완료 시 복귀 | `snd_fatfs_remount_twice(1)` | `snd_fatfs_remount(SND_FATFS_LDRV_NUM_USER_DATA)` |

**해석 (가설 3 개)**:
1. **Sullivan 후속 패치**: Sound1 분기 이후 Sullivan 측에서 마운트 1회로는 간헐 실패가 발견되어 2회로 보완 → Sound1에 미흡수. (사용자 기억과 부합)
2. **Sound1 의도적 환원**: Sullivan 측 2회 패치를 인지하고도 Sound1 측에서 1회로 되돌림 (이유 미상 — 성능? 또는 fatfs 드라이버 자체 개선?).
3. **공통 분기 후 양쪽이 각자 변화**: Sullivan 은 2회 보완, Sound1 은 다른 fix(예: `snd_fatfs_remount()` 자체 내부 개선)로 1회 유지.

**검토 항목**:
- Sound1 측 `snd_fatfs_remount()` 구현이 내부적으로 재시도/2회 처리를 이미 포함하는지 확인 → 포함되었다면 Sullivan 패치는 불필요.
- Sound1 측 OTA DFU 실기 테스트에서 마운트 실패가 관찰되는가? → 관찰된다면 본 패치 흡수 고려.

### 3.2 `ci_ble_control_ota.c` — DFU 디버그 로그 7 줄

Sullivan 에는 존재, Sound1 에서는 모두 제거됨:

| # | 위치 (함수) | 로그 내용 |
|---|---|---|
| 1 | `_handle_command_option_write()` | `ci_printi("[DFU] TRY TO REMOUNT BOOT DRIVE \r\n")` |
| 2 | `_handle_command_option_write()` | `ci_printe("[DFU] ANOTHER FILE IS BEING WRITTEN, SO CLOSE THE FILE. \r\n")` |
| 3 | `_handle_command_option_write()` | `ci_printi("[DFU] THERE IS NO '%s' PATH, SO MKDIR \r\n", path)` |
| 4 | `_handle_command_option_write()` | `ci_printe("[DFU] FAILED TO MKDIR '%s' \r\n", path)` |
| 5 | `_handle_command_option_write()` | `ci_printe("[DFU] FAILED TO OPEN '%s' \r\n", path)` |
| 6 | `_fetch_packet_data()` | `ci_printe("[DFU] DATA INDEX INVALID, EXPECT : %d, RECEIVED : %d \r\n", ...)` |
| 7 | `_fetch_packet_data()` | `ci_printe("[DFU] FAILED TO WRITE, WHEN DATA INDEX : %d \r\n", data_index)` |

**해석**: Sound1 의 service RTT 콘솔 작업([`bootloader/service-rtt-debug-console`](../../bootloader/service-rtt-debug-console/)) 또는 그 이전 로그 정리 단계에서 본 DFU 진단 로그까지 함께 제거된 것으로 추정. OTA DFU 실패 시 원인 추적용으로 유용한 로그이므로, 적절한 채널(예: `RTT_printf` 또는 같은 작업에서 도입한 BLOCK 모드 RTT)로 부활시킬지 검토 필요.

## 4. Sound1에만 있는 변경 (Sullivan 미반영 — 단순 기록)

본 절은 Sound1 측 후속 작업 성과로, 본 task 범위 밖이지만 비교를 위해 기록.

### 4.1 `ble_commonProtocol.h` — QCC 0x33~0x35 시스템 정보 enum

```c
typedef enum
{
    EN__SND_BT_CMD_SYSTEM_INFO_BATTERY = 0x33,
    EN__SND_BT_CMD_SYSTEM_INFO_POWER   = 0x34,
    EN__SND_BT_CMD_SYSTEM_INFO_LED_IND = 0x35,
} EN__SND_BT_CMD_SYSTEM_INFO;
```

### 4.2 `ble_communication.c` — QCC 0x33~0x35 패킷 처리 + ISD 미연결 더미 응답

| 변경 | 상세 |
|---|---|
| 헤더 추가 | `batteryNPowerControl.h`, `ci_timer.h`, `ci_printf.h` |
| `fetch_readDataForBleSetting()` 확장 | 0x34/0x35 명령은 헤더 외 데이터 페이로드 복사 |
| `setting_nrf_ble_adv_info()` 신규 분기 | • 0x30 (`en__bleSetting_ReadConnected_ISD_info`) 처리에 **ISD 미연결 시 더미 응답** 추가 (이전엔 무조건 응답) — `ISD NOT CONNECTED` 로그 + tx_index=0<br>• 0x33 BATTERY: `snd_batt_get_percent()` + `snd_charger_get_state()` 응답<br>• 0x34 POWER: 수신 페이로드로 `snd_batt_set_percent()` / `snd_batt_set_state()` / `snd_charger_set_state()` 업데이트 + 응답<br>• 0x35 LED_IND: 수신 페이로드로 `tdc_led_set_ind_state()` 호출 + 응답 |
| `bleCommunication()` 본체 | 0x33~0x35 라우팅 분기 추가 (`fetch_readDataForBleSetting()` 경유) |
| `bleCommunication()` SPI ERROR | 디버그 헤더 로그 (`### [SPI ERROR HANDLED] t3 = %d ms`) 추가 + "SPI 플래그 신호 유지" 코멘트 추가 |
| SPI RX 로그 한 줄 | 3 줄 multi-line → 2 줄 multi-line (clang-format off 영역, 표시 형식만 변경) |

### 4.3 `ci_ble_control_boot.c` — 타입 마이그레이션 + QCC SHUTDOWN 시퀀스

| 변경 | Sullivan | Sound1 |
|---|---|---|
| include | (없음) | `#include <snd_qcc.h>` 추가 |
| 타입 | `ST__CI_LIB_BOOT_STATUS boot_status` | `snd_boot_status_t boot_status` |
| 상태 enum | `SDK_CI_BOOT_STATE_ALT_BOOT` | `SND_BOOT_STATE_ALT_BOOT` |
| 서브상태 enum | `SDK_CI_BOOT_SUB_STATE_BOOT_TRY` | `SND_BOOT_SUB_STATE_BOOT_TRY` |
| 결과 enum | `SDK_CI_BOOT_ALT_BOOT_RESULT_NONE` | `SND_BOOT_ALT_BOOT_RESULT_NONE` |
| 재부팅 시퀀스 | 응답 송신 직후 → `for(i<100) _DELAY_MS(10)` (1000 ms 워치독 리프레시) → `SYS_WATCHDOG_RESET()` | 응답 송신 직후 → 20 ms 딜레이 → **`snd_qcc_set_mode(SND_QCC_MODE_SHUTDOWN)`** → `for(i<2200) _DELAY_MS(1)` (2200 ms 워치독 리프레시) → `SYS_WATCHDOG_RESET()` |
| 로그 함수 | `ci_printw("[BOOT] INFO/SELECT")` | `ci_printf("[BOOT] INFO/SELECT")` |
| 코멘트 오타 | "선택한 슬록으로" | "선택한 슬롯으로" |

→ Sound1 측에서 QCC SHUTDOWN 시퀀스 통합(2 초 이상 QCC_CTRL=0 유지로 QCC 셧다운 보장) 작업이 본 함수에 흡수됨.

### 4.4 `ci_ble_control_ota.c` — 타입 마이그레이션 + RTT 로그 전환

`ble_commonProtocol`/`boot.c` 와 동일 패턴:
- `ST__CI_LIB_BOOT_STATUS` → `snd_boot_status_t`
- `ci_printd("Slot=...")` / `ci_printd("Total byte=...")` → `RTT_printf(...)` (2 곳)
- `ci_printw("[OTA] WRITING DONE.")` → `RTT_printf(...)` (1 곳)
- `ci_printw("[OTA] DATA INDEX %d")` → `ci_printf(...)` (1 곳)

## 5. 양방향 분기 (양쪽 다 다른 부분)

`ci_ble_control_ota.c` 의 로그 채널 변경은 의미상 "Sound1 → RTT 전환" 으로 일원화 해석 가능 — 4.4 절에 정리. 별도 양방향 분기 항목 없음.

## 6. 결론 및 후속 권고

| 우선순위 | 항목 | 권고 |
|---|---|---|
| 🟡 검토 | §3.1 `snd_fatfs_remount_twice()` 8 곳 | Sound1 측 `snd_fatfs_remount()` 내부 구현 확인 + OTA DFU 실기 테스트에서 마운트 실패 관찰 여부 확인 후 흡수 여부 결정 |
| 🟡 검토 | §3.2 DFU 디버그 로그 7 줄 | OTA DFU 디버깅 가치 vs 채널 정책(RTT/BLOCK) 일관성. RTT 채널로 부활 권고 |
| ⚪ 정보 | §4 Sound1 측 추가 항목 | 본 task 범위 밖. 비교 참고용 기록만. |
| 🟢 진행 | `internalDevice/` 비교 | 본 task 다음 단계, 동일 방법(`git diff -w --ignore-blank-lines`)으로 진행 |
| 🟢 진행 | `signalProcessing/` 비교 | 본 task 다음 단계 |

## 7. 방법론 메모 (재현 명령)

```bash
# 1) 변경 파일 식별 (stat)
git diff --no-index --stat -w --ignore-blank-lines \
  src/Sullivan1_5__CM3/Cortex-M3-src/BleCommunication \
  src/2__cm3/Cortex-M3-src/BleCommunication

# 2) 개별 파일 의미적 diff
git diff --no-index -w --ignore-blank-lines \
  src/Sullivan1_5__CM3/Cortex-M3-src/BleCommunication/<파일> \
  src/2__cm3/Cortex-M3-src/BleCommunication/<파일>
```

옵션 의미:
- `--no-index`: 두 임의 경로(repo 무관) 비교
- `-w` (`--ignore-all-space`): 모든 공백 무시
- `--ignore-blank-lines`: 빈 줄 추가/삭제 무시

**주의**: 주석 텍스트 변경, 중괄호 위치 변경(다른 줄로 이동) 은 이 옵션으로 잡힘. 본 분석에서는 hunk 를 보면서 의미적 변경만 추렸음 — 향후 internalDevice / signalProcessing 분석 시 동일 기준 유지.

## 8. 갱신 이력

| 날짜 | 변경 |
|---|---|
| 2026-05-14 | Rev.0 작성. 사용자(다) 복귀 당일 명시 트리거 — "베이스 프로젝트 비교, BleCommunication 우선, 실제 코드 변경만". 결과: 13 파일 중 4 파일 차이, 9 파일 동등. Sullivan에만 있는 후속 패치 후보 = `snd_fatfs_remount_twice()` 8 곳 + DFU 디버그 로그 7 줄. Sound1 측 후속 작업은 §4 에 단순 기록. 다음 단계 = `internalDevice/` / `signalProcessing/`. |
