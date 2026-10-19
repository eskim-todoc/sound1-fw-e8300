---
name: G6 ble/qcc 게이트 노트
purpose: G6(BleCommunication·QCC) 이동·rename + import_* 사장 실증 기록
type: tasks/게이트노트
applies_to: [Sound1]
tags: [cm3, refactoring, gate, g6, ble, qcc, dfu, mapping, remote]
---

# G6 — ble/qcc (BLE 통신 · QCC)

**TL;DR**: 17파일 약 6000줄. BLE 명령 디스패치(`mappingControl` 2721줄 · `remoteControl` 1549줄)를 `ble/` 로, QCC 를 `qcc/` 로, OTA/DFU(`ci_ble_control_ota`=`@file otaControl`)를 `dfu/` 로 이관. 함수 접두어가 극도로 다양(`import`·`change`·`fetch`·`get`·무접두)해 **도메인별 명시 매핑**. `mappingControl.c` 의 `import_*ForDebug` 15개는 ③ census 사장 후보 → 실증 후 판단.

## 1. 파일 매핑

| 현행 | 신규 | 도메인 |
|---|---|---|
| `BleCommunication/ble_communication.c/.h` | `ble/tdc_ble_communication.c/.h` | ble |
| `BleCommunication/ble_commonProtocol.h` | `ble/tdc_ble_protocol.h` | ble |
| `BleCommunication/mappingControl.c/.h` | `ble/tdc_ble_mapping.c/.h` | ble |
| `BleCommunication/remoteControl.c/.h` | `ble/tdc_ble_remote.c/.h` | ble |
| `BleCommunication/remoteControl_read_SP_para.c/.h` | `ble/tdc_ble_remote_sp_para.c/.h` | ble |
| `BleCommunication/tdc_remote_gain_control.c/.h` | `ble/tdc_ble_gain_control.c/.h` | ble (gain 병합분 정합) |
| `BleCommunication/tdc_remote_general_debug.c/.h` | `ble/tdc_ble_general_debug.c/.h` | ble |
| `BleCommunication/ci_ble_control_boot.c/.h` | `dfu/tdc_dfu_ble_boot.c/.h` | dfu |
| `BleCommunication/ci_ble_control_ota.c/.h` | `dfu/tdc_dfu_ble_ota.c/.h` | dfu (`@file otaControl`) |
| `QCC/snd_qcc.c/.h` | `qcc/tdc_qcc.c/.h` | qcc |

`BleCommunication/` · `QCC/` 소멸. `Gen1_5/dfu/`(ci_ota) 는 G8 이후 dfu 로 합류 판단.

## 2. 도메인 접두어

| 도메인 | 접두어 | 함수 예 |
|---|---|---|
| ble | `tdc_ble_` | `bleCommunication` → `tdc_ble_communication_step`, `mappingControl` → `tdc_ble_mapping_step`, `remoteControl` → `tdc_ble_remote_step` |
| dfu | `tdc_dfu_` | `ci_ble_fetch_packet_ota` → `tdc_dfu_ble_fetch_ota`, `tdc_get_ota_dfu_conn_state` → `tdc_dfu_get_conn_state` |
| qcc | `tdc_qcc_` | `snd_qcc_init` → `tdc_qcc_init`, `snd_qcc_set_mode` → `tdc_qcc_set_mode` |

gain 병합분(`tdc_remote_gain_control_*`, 프로토콜 상수 `TDC_GAIN_*`)도 이 게이트에서 `tdc_ble_gain_*` 로 정합 (G2 병합 시 예고).

## 3. import_* 사장 실증 (mappingControl)

③ `04_사장코드-후보.md` 가 `import_*ForDebug` 15개를 호출 0 후보로 등재. G6 에서 **직접 grep 실증 후 제거 판단**:
- `importMappingConnectForDebug` · `import_eCapDataForDebug` · `import_liveStart` 등
- 구 디버그 진입점으로 추정 (mapping 프로토콜 수동 주입)
- **제거 전 조건부 컴파일·함수포인터 테이블·매크로 경유 전수 확인** (G3 i2c_comm 교훈)

## 4. 불가침

| 항목 | 방침 |
|---|---|
| 공유 ABI | `EN__mapping_ReadWriteMap_command` · `EN__SND_BT_CMD_*` 등 공유 헤더 정의 프로토콜 enum 은 **보류 목록** |
| BLE 프로토콜 상수 | `ble_commonProtocol.h` 의 커맨드 코드(0x8F 등)는 QCC 펌웨어와 정합 - 값 불변, 심볼명만 |
| `tdc_ble_protocol.h` 피포함 | 다수 파일이 include - 파일명 변경 시 전수 갱신 |

## 5. 유닛/모듈 맵

| 모듈 | 유닛 | 검증 라벨 |
|---|---|---|
| ble | `tdc_ble_communication` · `tdc_ble_mapping`(FSM) · `tdc_ble_remote`(FSM) · `tdc_ble_gain_control` · `tdc_ble_general_debug` · `tdc_ble_remote_sp_para` | **[로직설명]** — rename/이동뿐 |
| dfu | `tdc_dfu_ble_boot` · `tdc_dfu_ble_ota` | **[로직설명]** |
| qcc | `tdc_qcc` | **[로직설명]** |

의존: `ble` → `pwr/battery`(0x34) · `isd`(G7) · `fs/gain` 통과 전제. `qcc` → `hal/i2c` 통과 전제.

> [!NOTE]
> `mappingControl`/`remoteControl` 은 FSM 성격(switch + static)이나 **이번 게이트는 rename/이동만**. FSM 표준화가 필요하면 G7(isd 매핑 FSM)과 함께 별도 판단 - 이번엔 동작 보존.

## 6. 검증

| 라벨 | 결과 |
|---|---|
| 검증_공통_1 (구 심볼 잔존 0) | ✅ 12패턴 0 (`bleCommunication`·`mappingControl`·`remoteControl`·`snd_qcc_`·`SND_QCC_`·`ci_ble_`·`tdc_remote_*`). `@file` 주석 파일명·qcc 내부 심볼(타입2·상수8·로그매크로5)까지 정합 |
| 검증_공통_2 (공유 ABI 무변경) | ✅ `cfx_cm3_sharedMemory.h` diff = include 파일명 1줄 |
| 검증_공통_3 (균형·훅) | ✅ ble 12 + dfu 4 + qcc 2 파일 균형 OK · 깨진 include 0 · 훅 통과 |
| 검증_공통_4 (사장 실증) | ✅ **import_* 15개 제거** (§3) |
| **검증_G6_특화 (BLE 커맨드 코드 불변)** | ✅ `tdc_ble_protocol.h` 커맨드 값 무변경(헤더 가드만 — 오타 `POROTOCOL` 정정). QCC 정합 커맨드 코드(0x8F 등) 불변 |
| 검증_공통_5 (빌드·실기) | ✅ PASS — 은수님 확인 (2026-07-22): "빌드 후 동작 확인했어" → **G6 폐쇄** |

## 7. 결과 정리

- **import_* 15개 제거** (§3 실증): 정의는 `mappingControl.c` `#if 0` 블록(396줄) 안에서 죽어 있었고 헤더 선언 16개도 호출 0(`import_livePause` 는 선언만 있고 정의조차 없는 완전 고아). 블록 통째 + 선언 전량 제거.
- 파일 이동 17개: `ble/`(12) · `dfu/`(4) · `qcc/`(2). `BleCommunication/` · `QCC/` 소멸.
- rename 356건: 함수 접두어가 극도로 다양(`import`/`change`/`fetch`/`get`/무접두)해 도메인별 명시 매핑. gain 병합분(`tdc_remote_gain_*` → `tdc_ble_gain_*`, `TDC_GAIN_*` → `TDC_BLE_GAIN_*`)도 정합.
- 오타 정정: `Disconneted`→`disconnected`, `Coltrol`→`clear_command`(remote), `POROTOCOL`→(가드).
- **mappingControl/remoteControl FSM 표준화는 보류** (§5 NOTE) — 이번엔 동작 보존, FSM 통일은 별도 판단.
