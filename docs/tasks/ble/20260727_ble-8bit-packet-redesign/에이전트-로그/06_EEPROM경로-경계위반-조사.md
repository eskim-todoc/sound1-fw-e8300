---
name: 매핑 데이터 수신-EEPROM/파일시스템 저장 경로 배열 경계 위반 조사
purpose: 실기에서 확인된 electrodeMap[99-1] 범위 밖 읽기 결함과 같은 유형(특수값 인덱스·경계검사 누락·오프셋 감산·인덱스 누적·크기 불일치)이 매핑 데이터 수신부터 EEPROM/파일시스템 저장까지 전 경로에 더 있는지 전수 조사
type: tasks/에이전트-로그
applies_to: [Sound1]
tags: [ble, eeprom, fs, isd, array-bounds, security-review, cm3]
---

# 06. 매핑 데이터 수신-EEPROM/파일시스템 저장 경로 배열 경계 위반 조사

**TL;DR**: `tdc_cfx_eeprom_write.c`의 4개 쓰기 함수 중 3개가 내부기 슬롯 인덱스 경계검사 없이 `map[isd_num-1]`을 쓴다. BLE 0x6C/0x6D는 슬롯값 0을 허용해 `map[-1]` 오프-바이-원 쓰기가 확정 발생한다. 읽기(`tdc_cfx_eeprom_read.c`)도 동일 결함으로 정보 노출 가능. `tdc_isd_map_specific_stim.c`에서는 원 결함(전극 99 미검사)과 동일한 패턴을 별도 경로에서 확정 재현했다.

> [!NOTE]
> 읽기 전용 조사. `src/` 어떤 파일도 수정하지 않았다. 조사 루트는 `E:\workspace\projects\sound1-fw-e8300`이며, 과제가 지정한 5개 경로(`tdc_ble_mapping.c`, `tdc_shm.c`, `tdc_cfx_eeprom_*.c`, `tdc_fs_map.c`, `tdc_isd_map_data.c`)를 전부 따라갔고, 그 과정에서 드러난 인접 경로(`tdc_ble_remote.c`, `tdc_isd_map_specific_stim.c`, `1__cfx/signalProcessing/stimulationStrategy.c`)도 같은 패턴이 확인되어 §7에 별도 표기했다.

## 0. 참고 상수 (직접 확인)

| 상수 | 값 | 근거 |
|---|---|---|
| `df_MaxNumOfElectrode` | 32 | `stim/tdc_stim_definitions.h:68` |
| `df_MaxNumTransferableChannel` | 24 | `stim/tdc_stim_definitions.h:61` |
| `electrodeMap[]` | 32원소 | `board/electrodeMapping.h:5` |
| `MaxNumUser` | 4 | `board/processorDirective.h:91` |
| 미사용 전극 표식 | 99 | 앱이 보내는 관례값 (원 결함 참조) |
| `unusedReferenceElectrode_DummyNum` | 31 | `board/isd_ver1_1_2.h:42` |
| `numPacket_writeMapData_ISDnSetting` | 3 | `isd/tdc_isd_map_data.h:11` |
| `numPacket_writeMapData_stimulPara` | 15 | `isd/tdc_isd_map_data.h:13` |

## 1. 조사 방법

1. `tdc_ble_mapping.c`의 0x6C(`write_SlotData_ISD_N_USER`)·0x6D(`write_Mapdata_STIMUL_PARA`) 파싱부를 처음부터 끝까지 라인 단위로 읽고, 슬롯/맵/전극 인덱스 필드마다 검증 범위와 실제 배열 크기를 대조.
2. `tdc_shm_get_pointer_repository_for_read_write_map_data_isd_info/_stimul_para`가 가리키는 구조체(`ST__CFX_CM3_SharedMemory_ISD_info`, `ST__CFX_CM3_SharedMemory_mapData`)의 필드별 word 오프셋을 계산해, BLE 파싱부가 채우는 인덱스 누적(`stimulPara_index++`, `i-1`)이 실제 오프셋과 정확히 일치하는지 워드 단위로 검산.
3. `tdc_cfx_eeprom_write/read/erase/recover.c`의 모든 함수에서 `isd_num`/`map_num`이 `g_tdc_fs_ptr_entire_map->map[]` 인덱스로 쓰이기 전에 경계검사가 있는지 함수별로 대조.
4. `tdc_fs_map.c`의 실제 EEPROM read/write 헬퍼(`tdc_fs_map_write_*`/`tdc_fs_map_read_*`)에 자체 경계검사가 있는지 확인.
5. `tdc_isd_map_data.c`의 슬롯/맵 인덱스 디스패치 함수(`tdc_isd_map_write_info_setting`, `tdc_isd_map_write_stim_para` 등)가 BLE에서 받은 값을 재검증하는지 확인.
6. 위 과정에서 전극번호(`usableStimulationElectrodIndex`/`usableReferenceElectrodIndex`/`CIS_FreqBandOrder`)의 실제 소비처를 전수 grep으로 추적해, 원 결함과 동일한 `electrodeMap[value-1]` 패턴이 다른 파일에도 있는지 확인.

## 2. 발견 목록표

| 발견_번호 | 파일:라인 | 패턴 유형 | 판정 | 도달 조건 | 예상 증상 |
|---|---|---|---|---|---|
| 발견_1 | `ble/tdc_ble_mapping.c:1538-1539,1552-1553,1566-1567,1580-1581,1594-1595,1608-1609` | 패턴_1 (특수값 인덱스 오염원) | 조건부 | 0x6D 명령에서 전극·밴드순서 필드에 `1~100` 범위(실제 유효값은 `1~32`)로 앱이 99 등을 보내면 그대로 EEPROM에 저장됨 | 저장 자체는 안전하나, 하류 소비처가 방어하지 않으면 확정 결함으로 발현 (원 결함이 이 경로의 산출물이었음) |
| 발견_2 | `isd/tdc_isd_map_specific_stim.c:224-225` (BLE 검증: `ble/tdc_ble_mapping.c:267-268,274-281`) | 패턴_1+패턴_2 | **확정** | 0x65(`specific_stimulation`) 명령에서 `stimulatonMode=en__bipolar(4)`이면서 `bipolarReferenceElectrodeNum=99`를 보내면 BLE 검증(라인277 `!= 99` 예외)을 통과하고, `electrodeMap[99-1]`=`electrodeMap[98]` OOB 읽기 발생 | 원 결함과 동일한 인접 전역 오염. `bipolarReferenceElectrodeNum[]` 배열에도 그 값이 잘못 기록되어 자극 기준전극 설정이 깨짐 |
| 발견_3 | `cfx_link/tdc_cfx_eeprom_write.c:18-34`(user_setting), `:36-47`(map_stamp), `:63-77`(map_data) | 패턴_2+패턴_5 | **확정** | 0x6C/0x6D의 슬롯 인덱스 검증이 `(tempValue>=0)&&(tempValue<=MaxNumUser)`로 **0을 허용**(`ble/tdc_ble_mapping.c:1192`,`:1448`). `isd_index=0`이 `tdc_isd_map_data.c:346,592`에서 재검증 없이 `FlashCommand.isd_index`에 대입되고, `tdc_shm.c:491-497`이 무조건 `tdc_cfx_eeprom_write_mapdata_mapping_app()`을 호출 | `map[-1]` 오프-바이-원 쓰기. `TDC_FS_ENTIRE_MAP_T`가 `map[MaxNumUser]` 단일 멤버(`fs/tdc_fs.h:92-99`)이고 PRAM3 영역을 98.83% 채우는 구조라, `map[-1]`은 PRAM3 시작 주소 이전 메모리를 침범 |
| 발견_4 | `cfx_link/tdc_cfx_eeprom_write.c:49-61`(`tdc_cfx_eeprom_write_isd_info_by_mapping`) | 방어 사례 | 방어됨 | 같은 파일 내 유일하게 `(0 < isd_num) && (isd_num <= MaxNumUser)` 가드 보유(라인55) | 발견_3과 대조되는 "정상 패턴" 참고용. 같은 파일 내 함수 간 가드 유무가 불일치한다는 사실 자체가 누락의 방증 |
| 발견_5 | `cfx_link/tdc_cfx_eeprom_read.c:78-89,91-103,105-113,115-123` (전체 4개 repository 복사 함수) | 패턴_2+패턴_5 | **확정** | 0x6B(`read_Mapdata_STIMUL_PARA`) 슬롯 검증도 `(tempValue>=0)&&(tempValue<=MaxNumUser)`로 0 허용(`ble/tdc_ble_mapping.c:1396`). 4개 함수 모두 `isd_num` 경계검사 없음 | `map[-1]` OOB 읽기 결과가 `repositoryForReadWriteMapData`를 거쳐 **BLE 응답으로 앱에 전송** — 정보 노출 가능성 |
| 발견_6 | `fs/tdc_fs_map.c:8-126` (`tdc_fs_map_read/write_isd_info`·`_user_setting_value`·`_map_stamp`·`_map_data` 전 함수) | 패턴_2 (구조적) | 방어없음(구조적) | 이 계층 자체에는 `isd_num` 경계검사가 전혀 없음 — 발견_3·5가 실제로 도달 가능한 근본 원인. 유일한 방어선은 호출부(`tdc_cfx_eeprom_*.c`)뿐인데 그마저 3/4가 누락 | 파일명 생성(`'0'+isd_num`)과 실제 메모리 접근(`map[isd_num-1]`)이 분리되어 있어, 파일명은 그럴듯해도 접근 주소는 어긋남 |
| 발견_7 | `ble/tdc_ble_remote.c:344,402` | 패턴_2 (중복 진입점) | **확정** | 원격제어(리모컨) BLE 프로토콜의 `en__remoteControl_read/write_Mapdata_STIMUL_PARA`도 동일하게 `(tempValue>=0)&&(tempValue<=MaxNumUser)`로 슬롯 0 허용. `tdc_isd_map_write_stim_para` 등 동일 함수로 합류 | 발견_3·5와 동일한 결과. 매핑 앱 경로를 막아도 리모컨 경로로 동일 결함 재현 가능 |
| 발견_8 | `cfx_link/tdc_cfx_eeprom_erase.c` 전체, `tdc_cfx_eeprom_recover.c` 전체 | 패턴_2 (구조적이나 방어됨) | 방어됨 | 함수 자체엔 `isd_num` 경계검사가 없으나, 호출측 BLE 명령 0x6E(`ble/tdc_ble_mapping.c:1713`)·0x6F(`:1727`)가 슬롯을 `1<=x<=4`로(0 배제) 강제하고, 0x70/0x71(recover)은 슬롯을 패킷에서 받지 않고 내부 고정 루프(1~4, 2~4)만 사용 | 현재는 안전하나 단일 계층 방어라 향후 회귀 위험 있음(§8 참고) |
| 발견_9 | `ble/tdc_ble_mapping.c:1538-1608` 및 `isd/tdc_isd_map_live.c:92-101,491-498,682-689` | 패턴_4 (인덱스 누적) | 방어됨 | `stimulPara_index`는 case 1~15를 통해 정확히 237개 int(구조체 전체 크기)로 귀결되도록 설계됨(§4에서 워드 단위 검산 완료). `numPacket_writeMapData_stimulPara=15`로 case 이후 값은 전부 `default:{}`(no-op)라 초과 누적 불가 | 실질적 발현 없음. `tdc_isd_map_live.c`가 같은 전극 배열을 `currentMapData`에 그대로 복사하지만, 소비처(`tdc_isd_stim_para_setting.c`)의 기존 수정으로 커버됨 |

## 3. 범위 확장 발견 (과제 지정 5개 파일 밖, 참고용)

과제는 `2__cm3`의 5개 경로를 지정했으나, 발견_1의 오염 데이터(`CIS_FreqBandOrder`)를 끝까지 추적하는 과정에서 CFX(DSP) 프로세서 쪽 소비 코드에 도달했다. 조사 범위는 아니지만 심각도가 높아 별도 기록한다.

- **발견_9(범위 밖)**: `1__cfx/signalProcessing/stimulationStrategy.c:476,481,483` (및 CHESS 어셈블리 버전 `:610-616`)
  ```c
  freqBandOrder = addr_MapProgramData_CIS_FreqBandOrder[i] - 1;                          // 1~100 허용값 그대로 -1
  electrodIndex = (addr_MapProgramData_StimulusChannelAssignedElectrodIndex[freqBandOrder] - 1);  // 1차 미검증 인덱스
  electrodeMap  = addr_electrodeMap[electrodIndex] << electrodIndexPositionAtPCM_Mold;    // 2차 미검증 인덱스
  ```
  `addr_MapProgramData_StimulusChannelAssignedElectrodIndex[32]`(`1__cfx/systemControl/system_control.c:20`)와 `addr_electrodeMap[32]`(`1__cfx/signalProcessing/stimulationStrategy.c:14`) 모두 32원소 고정 배열인데, `CIS_FreqBandOrder`는 BLE 계층에서 `1~100`까지 검증 없이 통과(발견_1)한다. 값이 99면 `freqBandOrder=98`로 1차 OOB 읽기가 나고, 그 결과(제어 불가능한 값)로 2차 인덱스를 계산해 `addr_electrodeMap[]`을 재차 인덱싱한다. **이중 미검증 체이닝**이라 원 결함보다 파급 범위가 크다(하드 폴트 가능성 포함). `2__cm3` 밖(1__cfx)이라 이번 조사의 정식 범위는 아니며, 재현 조건(값이 실제로 CFX 공유메모리까지 전달되는 시점의 검증 여부)은 미확인이다.

## 4. 워드 오프셋 검산 (발견_9 방어 근거)

`ST__CFX_CM3_SharedMemory_mapData`(`cfx_link/tdc_shm.h:114-131`)는 237 word(mappingDate 6 + 단일필드 7 + 32word 배열 7개=224, 합 237). 0x6D의 case별 누적을 검산한 결과:

| case | 필드 | 누적 시작 | 누적 끝 |
|---|---|---|---|
| 1 | mappingDate+단일필드 7개+알람크기 | 0 | 12 |
| 2-3 | usableStimulationElectrodIndex | 13 | 44 |
| 4-5 | usableReferenceElectrodIndex | 45 | 76 |
| 6-7 | CIS_FreqBandOrder | 77 | 108 |
| 8-11 | T_level_uA | 109 | 140 |
| 12-15 | C_level_uA | 141 | 172 |
| (완료 후 자동 추가) | audio_input_x_mim/x_max | 173 | 236 |

최종 `stimulPara_index=237`로 구조체 크기와 정확히 일치. 0x6C(`ST__CFX_CM3_SharedMemory_ISD_info` 33word + userSettingValue 7word + mapStamp 6word=46word)도 case 1~3 누적이 인덱스 0~45로 정확히 일치함을 확인(`ble/tdc_ble_mapping.c:1196-1353`). 두 명령 모두 패턴_4(인덱스 누적 초과)는 **해당 없음**.

## 5. 확정·조건부 발견 우선순위표

| 순위 | 발견_번호 | 판정 | 핵심 근거 |
|---|---|---|---|
| 1 | 발견_3 | 확정 | `tdc_cfx_eeprom_write.c`의 user_setting/map_stamp/map_data 쓰기 3개 함수가 `isd_num` 경계검사 없이 `map[isd_num-1]`을 씀. 0x6C/0x6D가 슬롯값 0을 허용 |
| 2 | 발견_5 | 확정 | `tdc_cfx_eeprom_read.c` 4개 함수 전부 동일 결함, OOB 읽기 결과가 BLE 응답으로 앱에 전송 |
| 3 | 발견_2 | 확정 | `tdc_isd_map_specific_stim.c:224-225`, 원 결함(`electrodeMap[99-1]`)과 완전히 동일한 패턴이 0x65 경로에 별도로 존재 |
| 4 | 발견_7 | 확정 | `tdc_ble_remote.c`가 발견_3·5와 동일한 취약 함수로 합류하는 두 번째 진입점 |
| 5 | 발견_1 | 조건부 | 0x6D 전극/밴드 필드 검증범위(1~100)가 실제 유효범위(1~32)보다 넓어 오염 데이터가 EEPROM까지 그대로 저장됨(하류 소비처 방어 여부에 결과가 갈림) |

## 6. 방어된 지점 목록

| 지점 | 방어 위치 | 방어 내용 |
|---|---|---|
| 0x6A(`read_SlotData_ISD_N_USER`) 슬롯 | `ble/tdc_ble_mapping.c:1146-1147` | `1<=tempValue<=MaxNumUser`, 0 배제 |
| 0x6E(`erase_SlotData_manufacture`) 슬롯 | `ble/tdc_ble_mapping.c:1713` | `1<=slot_index<=4`, 0 배제 |
| 0x6F(`erase_mapData_STIMUL_PARA`) 슬롯·맵 | `ble/tdc_ble_mapping.c:1727,1733` | 슬롯·맵 모두 `1~4`, 0 배제 |
| 0x70/0x71(recover) 슬롯 | `ble/tdc_ble_mapping.c:1741-1755` | 패킷에서 슬롯을 받지 않고 내부 고정 루프(1~4 또는 2~4)만 사용 |
| `tdc_cfx_eeprom_write_isd_info_by_mapping` | `cfx_link/tdc_cfx_eeprom_write.c:55` | `(0 < isd_num) && (isd_num <= MaxNumUser)` |
| 바이폴라 전극번호(원 결함) | `isd/tdc_isd_stim_para_setting.c:482-492` | 2026-07-27 수정된 `1~df_MaxNumOfElectrode` 범위 검사 |
| 0x6D 전극 인덱스 누적(패턴_4) | `isd/tdc_isd_map_data.h:13` (`numPacket_writeMapData_stimulPara=15`) + `ble/tdc_ble_mapping.c` switch 구조 | case 15 초과 값은 `default:{}` no-op이라 237word 한도를 구조적으로 넘을 수 없음(§4 검산) |

## 7. 조사했으나 해당 없는 파일·패턴

| 대상 | 사유 |
|---|---|
| `fs/tdc_fs_map.c:611-628`의 `numFrequencyBand=24` 분기 | 루프 상한이 아니라 공장초기값 데이터일 뿐이며, 배열 초기화 루프는 항상 `df_MaxNumOfElectrode`(32)로 전 구간 채움. 패턴_3 해당 없음 |
| `tdc_shm.c`/`tdc_cfx_eeprom_*.c`/`tdc_fs_map.c`/`tdc_isd_map_data.c`/`tdc_ble_mapping.c` 내 `memcpy` 사용 | 5개 파일 전체에 `memcpy` 호출 자체가 없음(구조체 대입 `*p_dst=*p_src`만 사용, 동일 타입 간 대입이라 크기 불일치 없음). 패턴_6 해당 없음 |
| `tdc_isd_map_data.c`의 `tdc_isd_map_write/read_original_info_setting` | 슬롯 인덱스를 파라미터로 받지 않는 고정 슬롯 전용 함수 — 이번 조사 대상(가변 슬롯 인덱스 위험)과 무관 |
| `tdc_isd_map_reset_nvm_selected`/`_map_data`(erase/recover 디스패치) | 발견_8과 동일 사유로 BLE 레이어 가드에 의해 현재 방어됨 |

## 8. 확인하지 못한 구간

- `tdc_shm.c:485-515`의 `flash_Command_Read/Erase/Recover` 분기가 CFX 프로세서 측 별도 검증을 한 번 더 거치는지(2프로세서 간 이중 방어 여부)는 CFX 측 대응 코드(`1__cfx/systemControl/system_control.c`)까지 깊이 들어가지 않아 미확인.
- 발견_9(범위 밖, `stimulationStrategy.c`)의 `addr_MapProgramData_CIS_FreqBandOrder`가 실제로 CM3 EEPROM에 저장된 값과 몇 프레임 지연으로 동기화되는지, 그리고 CFX 부팅 시 별도 sanity 값으로 덮어써지는 경로가 있는지는 `1__cfx` 전체를 조사 범위에 포함하지 않아 미확인.
- 발견_3·5의 `map[-1]` 실제 물리 주소(`DSP_PRAM3_REMAP_BASE` 이전 영역)에 어떤 데이터가 위치하는지(다른 PRAM 뱅크인지, 예약 영역인지)는 링커 스크립트(`sections.ld`)를 확인하지 않아 미확인 — 파급 범위(단순 인접 변수 오염 대 하드폴트)는 추가 확인 필요.
- 슬롯 인덱스 0이 실제 매핑 앱·리모컨 펌웨어에서 정상 운용 중 전송되는 값인지(원 결함의 99처럼 "관례적으로 보내는 값"인지, 순수 이론적 가능성인지)는 앱 측 소스가 조사 범위 밖이라 확인 불가.
