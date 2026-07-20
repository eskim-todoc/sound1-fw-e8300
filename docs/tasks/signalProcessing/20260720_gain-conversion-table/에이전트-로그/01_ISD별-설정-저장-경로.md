---
name: ISD별 설정 저장·로드 경로 조사
purpose: cm3에서 ISD(내부기)별 사용자 설정이 파일시스템에 저장·로드되는 전체 경로를 슬롯 모델·저장 포맷·연결감지→로드·변경→저장·필드추가 영향 5가지 관점에서 근거와 함께 정리
type: tasks/에이전트-로그
tags: [isd, user-setting, filesystem, ci_map, ci_filesystem, persistence, gain-table]
---

# ISD별 설정 저장·로드 경로 조사

> [!NOTE]
> 본 문서는 Explore 서브에이전트(read-only)가 조사한 결과를 오케스트레이터(세션)가 그대로 옮겨 적은 것이다. 해당 노드에는 파일 쓰기 권한이 없어 직접 Write 하지 못했다. 핵심 사실의 재확인은 `분석-검증.md`에서 수행한다.

**TL;DR**: ISD 설정은 슬롯 1~4(`MaxNumUser=4`)마다 `/ISDn_USER_SETTING` 등 개별 FAT 파일로 저장되며 CRC로 무결성 검증한다(ISD 정보 파일만 AES128 암호화 추가). 연결 시 `changeConnected_isd_num_CFX()`가 파일→RAM미러→공유메모리 순으로 로드하고, 앱에서 볼륨 등을 바꾸면 `changeAudioVolume()` 등이 `system_opMode==en__normalMode`일 때 즉시 동기 기록한다. 버전/마이그레이션 로직이 없고 쓰기 함수엔 "data%16+4+padding==16" 정렬 불변식이 있어, 구조체 크기 변경 시 쓰기 자체가 실패하거나 CRC 불일치로 조용히 기본값 리셋된다.

---

## 0. 조사 범위와 한계

- 조사 대상: `src/2__cm3/` 전체(`Gen1_5/FS/`, `Cortex-M3-src/systemControl/`, `Cortex-M3-src/internalDevice/`, `Cortex-M3-src/BleCommunication/`).
- 항목 3(연결 감지→로드)의 흐름 일부는 `userSettingValueLoadedFlag`를 실제로 1로 세팅하는 코드가 cm3 쪽에는 없어, `src/1__cfx/systemControl/system_control.c`(DSP/CFX 프로젝트, **조사 범위 밖**)까지 확인했다. 해당 근거는 "[범위 외 참고]"로 별도 표시했다.
- 코드 조사 전용이며 어떤 파일도 수정하지 않았다.

## 1. ISD 슬롯 모델

- 슬롯 개수는 `#define MaxNumUser 4`로 고정 (`src/2__cm3/processorDirective.h:169`). 슬롯당 맵(프로그램) 개수는 `#define MaxNumMap 4` (`processorDirective.h:171`).
- 공유메모리 구조체 `ST__CFX_CM3_SharedMemory_ALL`에서 `int connected_ISD_num; // 0연결안됨, 1~4 연결된 ISD 번호` 필드가 "현재 연결된 슬롯 번호"다 (`Cortex-M3-src/systemControl/cfx_cm3_sharedMemory.h:198`). 같은 구조체에 슬롯별 ISD 정보 배열 `ST__CFX_CM3_SharedMemory_ISD_info cfx_ISD_info[MaxNumUser];` (`cfx_cm3_sharedMemory.h:202`)와, "현재 연결된 슬롯"의 활성 설정만 담는 단일 인스턴스 `ST__CFX_CM3_SharedMemory_userSettingValue userSettingValue;` (`cfx_cm3_sharedMemory.h:204`, 슬롯별 배열이 아님)가 있다.
- 슬롯 인덱스(1~4)와 실제 ISD 하드웨어(칩 시리얼)의 관계는 "등록된 ISD 식별자와의 매칭"으로 결정된다:
  - `isd_path_Open()` 상태머신이 내부기 칩으로부터 4바이트 ID(`isd_id` = `{isd_year, isd_month_model, isd_serial}` 조합)를 백텔로 읽는다 (`Cortex-M3-src/internalDevice/isd_interface_init_ISD.c:303-341`).
  - 이 `isd_id`를 부팅 시 이미 로드된 슬롯 0~3의 등록 정보 `read_ISD_manufacture_ID(i)`(파일에서 읽은 `isd_year/isd_month_model/isd_serial`을 24비트로 합성, `cfx_cm3_sharedMemory.c:299-312`)와 순차 비교해 일치 슬롯을 찾는다: `if (isd_id == isd_id_onFlash) { isd_id_match_num = i + 1; }` (`isd_interface_init_ISD.c:389-397`). 즉 슬롯 번호는 "몇 번째 등록 정보와 일치했는가"이며 시리얼 자체가 슬롯 번호는 아니다.
  - 패스키 검증까지 성공하면 `changeConnected_isd_num_CFX(isd_id_match_num)`을 호출해 매칭 결과를 `connected_ISD_num`으로 확정한다 (`isd_interface_init_ISD.c:677`). 매핑(청각사 앱) 모드 중에는 로드 없이 번호만 임시 대입한다 (`isd_interface_init_ISD.c:681`).
  - `ManufacturingDefault_ISD_No`(=1, `processorDirective.h:174`)는 등록된 사용자 이름이 제조사 기본값(`TODOC_OTE`)이거나 마스터키 ISD(`isd_id & 0xFFFF == 0xFFFF`)로 판정된 경우 강제 매핑되는 슬롯이다 (`isd_interface_init_ISD.c:381-412`).

## 2. 사용자 설정 저장 구조

### 2.1 구조체

`ST__CFX_CM3_SharedMemory_userSettingValue` (`Cortex-M3-src/systemControl/cfx_cm3_sharedMemory.h:89-98`): `mapNum`, `stimulVolume`, `audioVolume`, `indicatorLED_OnOff`, `indicatorStimul_OnOff`, `teleCoil_OnOff`, `Ble_Onff` — `int` 7개, `sizeof == 28바이트`.

### 2.2 3계층 저장 구조

1. **FAT 파일**(플래시, 논리드라이브 `"1:"` = `CI_FILESYSTEM_LOGICAL_DRIVE_NUM`, `Gen1_5/FS/ci_filesystem.h:33`) — 슬롯별 실제 영속 파일.
2. **RAM 미러** `g_ci_filesystem_ptr_entire_map`(DSP `PRAM3`, `CI_FILESYSTEM_BASE_ADDR_ENTIRE_MAP = DSP_PRAM3_REMAP_BASE`, `ci_filesystem.h:56`; 포인터 대입은 `snd_fatfs_init_mem_map()`/`ci_filesystem_mount()`, `ci_filesystem.c:43,190`) — 파일 원본 바이트(데이터+CRC+패딩)를 얹어두는 캐시. 타입 `CI_FILESYSTEM_ENTIRE_MAP_T { CI_FILESYSTEM_MAP_T map[MaxNumUser]; }` (`ci_filesystem.h:95-102`).
3. **CFX 공유메모리 "활성" 사본** `cfx_cm3_sharedMemoryAll.userSettingValue`(`cfx_cm3_sharedMemory.h:204`) — 현재 연결 슬롯 1개 분량만 담아 DSP 신호처리가 실제 참조하는 값.

### 2.3 파일명 규칙과 슬롯 분리

`ci_map.h` 매크로에 슬롯 번호를 실행 시 자릿수 치환한다:

| 데이터 | 템플릿 | 치환 위치 | 슬롯1 예시 |
|---|---|---|---|
| ISD 정보 | `/ISD*_INFO` (`ci_map.h:61-64`) | index 4(`CI_MAP_FILE_INDEX_ISD_NUM`=4, `ci_map.h:86`) | `/ISD1_INFO` |
| 사용자 설정 | `/ISD*_USER_SETTING` (`ci_map.h:66-69`) | index 4 | `/ISD1_USER_SETTING` |
| 맵 스탬프 | `/ISD*STAMP` (`ci_map.h:71-74`) | index 4 | `/ISD1STAMP` |
| 맵(프로그램) 데이터 | `/ISD*_MAP*` (`ci_map.h:76-79`) | index 4(ISD), index 9(`CI_MAP_FILE_INDEX_MAP_NUM`=9, 맵번호) | `/ISD1_MAP1` |

치환 코드 예: `name[CI_MAP_FILE_INDEX_ISD_NUM] = (char)('0' + isd_num);` (`ci_map.c:10, 24, 37, 49`). 슬롯 분리는 "파일 자체를 4개 따로 둔다"는 방식이며 한 파일 내부에서 오프셋으로 슬롯을 나누지 않는다.

### 2.4 파일 포맷 (고정 크기, CRC, 선택적 AES128)

읽기/쓰기는 모두 공용 함수 `ci_filesystem_read_with_crc_and_aes128()`/`ci_filesystem_write_with_crc_and_aes128()`(`Gen1_5/FS/ci_filesystem.c:195-489, 491-820`)를 거치며, `ci_map.c`의 4쌍 read/write 함수는 이 공용 함수의 얇은 래퍼다.

| 파일 | 함수 | data | CRC | AES128 | 패딩(파일 기록 여부) | 총 바이트 |
|---|---|---|---|---|---|---|
| ISD 정보 | `ci_map_read/write_isd_info`(`ci_map.c:7-18,66-78`) | 132B | 4B | **사용**(마지막 16B 블록 암호화) | 8B, 기록+암호화 | 144B |
| 사용자 설정 | `ci_map_read/write_user_setting_value`(`ci_map.c:20-31,80-93`) | 28B | 4B | 미사용 | 0B(NULL/0 전달, `ci_map.c:27,89`) | 32B |
| 맵 스탬프 | `ci_map_read/write_map_stamp`(`ci_map.c:33-43,95-108`) | 24B | 4B | 미사용 | 4B, 기록됨(평문 0, 암호화 안 됨) | 32B |
| 맵 데이터(프로그램) | `ci_map_read/write_map_data`(`ci_map.c:45-64,110-131`) | 948B | 4B | 미사용 | 8B, 기록됨(평문 0, 암호화 안 됨) | 960B |

AES 사용 여부는 각 호출부 마지막 인자(`enable_aes`)로 결정된다. `ci_map_write_isd_info()`는 `..., true, true)`(`ci_map.c:74`)로, `ci_map_write_user_setting_value()`는 `..., true, false)`(`ci_map.c:89`)로 호출한다. **사용자 설정값은 CRC로 무결성만 검증하고 암호화되지 않는다.**

패딩 기록의 비대칭: 쓰기 함수는 `aes128_padding_size > 0`이면 `enable_aes` 값과 무관하게 패딩 바이트를 파일에 기록한다(`ci_filesystem.c:723-733`, 사전에 0으로 memset — `ci_filesystem.c:541-544`). 반면 읽기 함수는 `enable_aes`가 true일 때만 패딩 바이트를 읽는다(`ci_filesystem.c:260-273`). 즉 `enable_aes=false`인 맵 스탬프·맵 데이터 파일은 쓰기 시 패딩 바이트가 파일에 남지만 읽기 시 소비되지 않는다(동작 오류로 이어지진 않음. 701행 주석 "read 로직과 대칭되게"와는 실제로 어긋나는 부분이나, 읽기가 필요한 만큼만 읽고 닫으므로 문제는 없음).

기록은 4KB 고정 크기가 아니라 위 표의 실바이트만 기록된다. 쓰기 함수의 `f_lseek(&g_ci_filesystem_ohdl, 4096); f_lseek(&g_ci_filesystem_ohdl, 0);`(`ci_filesystem.c:622-623`)는 클러스터 프리얼로케이션 트릭일 뿐이다(주석: "FATFS에게 4KB로 고정된 파일을 생성할 수 있게 의도적으로 파일 포지션을 4096으로 설정").

### 2.5 슬롯별 유효성 검사·초기화

`ci_map_init_map_data(isd_num, force_init, ...)`(`ci_map.c:133-781`)가 슬롯별 4종 파일을 모두 읽어 유효성(반환값 <0이면 무효, `ci_map.c:156-162`)을 검사하고 무효 항목만 하드코딩 기본값으로 재기록한다 — 사용자 설정 기본값: `mapNum=1, stimulVolume=4, audioVolume=1, indicatorLED_OnOff=1, indicatorStimul_OnOff=1, teleCoil_OnOff=2, Ble_Onff=1`(`ci_map.c:500-506`, 쓰기는 `ci_map.c:508`). `ci_map_init_map_data_all()`이 슬롯 1~`MaxNumUser`를 순회하며 이를 호출한다(`ci_map.c:783-793`, 785행).

## 3. 연결 감지 → 로드 시점 (호출 체인)

1. **[cm3] 내부기 인식**: `isd_path_Open()` 상태머신이 칩 ID를 읽어(`isd_interface_init_ISD.c:246-478`) 등록 슬롯과 매칭(`isd_interface_init_ISD.c:389-397`)하고, 패스키 핸드셰이크까지 통과하면(case 27/31/35, `isd_interface_init_ISD.c:481-726`) 매핑 모드가 아닐 때 `changeConnected_isd_num_CFX(isd_id_match_num)`을 호출한다(`isd_interface_init_ISD.c:675-677`).
2. **[cm3] 파일→RAM미러 로드 + RAM미러→공유메모리 복사**: `changeConnected_isd_num_CFX(isd_num)`(`cfx_cm3_sharedMemory.c:319-351`)가 순서대로:
   - `ci_map_read_isd_info(isd_num)`, `ci_map_read_user_setting_value(isd_num)`, `ci_map_read_map_stamp(isd_num)`, `ci_map_read_map_data(isd_num, 1..4)` — 파일 → RAM 미러(`g_ci_filesystem_ptr_entire_map->map[isd_num-1]`) 로드(`cfx_cm3_sharedMemory.c:337-343`).
   - `fn_copy_MapInfo_toCM3(isd_num)` — 맵 스탬프·맵별 매핑일자·사용가능 맵 인덱스를 공유메모리 `connected_ISD_Map_info`로 복사(`cfx_cm3_sharedMemory.c:345`, 구현: `Gen1_5/fn_fromCFX/fn_from_cfx_eeprom_read.c:20-66`).
   - `fn_copy_userSettingParameters_toCM3(isd_num)` — **RAM 미러 → 공유메모리 `userSettingValue` 복사**(`cfx_cm3_sharedMemory.c:346`, 구현: `cfx_cm3_sharedMemoryAll.userSettingValue = g_ci_filesystem_ptr_entire_map->map[isd_num-1].user_setting_value;` — `fn_from_cfx_eeprom_read.c:68-71`).
   - 마지막으로 `cfx_cm3_sharedMemoryAll.connected_ISD_num = isd_num;`(`cfx_cm3_sharedMemory.c:350`)로 연결 번호를 확정.
3. **[범위 외 참고, src/1__cfx] `userSettingValueLoadedFlag` 세팅**: DSP측 `LB_Normal_PowerMode()`가 매 주기 `m_current_isd = Addr_SharedMem->connected_ISD_num;`(`src/1__cfx/systemControl/system_control.c:44`)로 읽고, 0이 아니면 `Normal_PowerMode_isdConnected()`(`system_control.c:204-250`) 호출. `if (m_previous_isd != m_current_isd) { Addr_SharedMem->userSettingValueLoadedFlag = 1; m_previous_isd = m_current_isd; }`(`system_control.c:206-213`)로 새 슬롯 확인을 cm3에 알린다. 연결 해제 시 `userSettingValueLoadedFlag = 0`(`system_control.c:274`).
4. **[cm3] 로드 완료 확인 → 맵 번호 확정**: `main.c`의 `update_mapNum()`(매 메인루프 호출, `Cortex-M3-src/main.c:637`)이 `isUserSettingValueLoaded_CFX()`(`cfx_cm3_sharedMemory.c:200-210`)를 폴링하고 `false→true` 상승 엣지(`main.c:193`)에서 2단계에서 이미 로드된 `readProgramMapNum()`(=`userSettingValue.mapNum`)을 사용가능 맵 인덱스와 대조해(`main.c:195-218`) `changeProgramMapNum(mapNum)`을 호출, CFX에 맵 로드를 명령한다(`main.c:221`, 구현 `cfx_cm3_sharedMemory.c:420-468`).

**실제 "파일→공유메모리" 사용자 설정 로드는 2단계에서 이미 끝나며**, `userSettingValueLoadedFlag`/`update_mapNum()`은 CFX(DSP)가 이를 인지해 맵 프로그램 활성화로 이어지도록 하는 후속 동기화 단계다.

## 4. 설정 변경 시 저장 시점 (즉시 vs 지연)

BLE 리모컨 앱 명령이 `remoteControl.c`에서 직접 `change*()` 함수를 호출한다:

| 앱 명령 | 코드 위치 | 호출 함수 |
|---|---|---|
| 마이크 볼륨 0x46 | `remoteControl.c:1114-1150` | `changeAudioVolume(volume)`(`:1126,1134`) |
| 자극 볼륨 0x45 | `remoteControl.c:1076-1112` | `changeStimulVolume(volume)`(`:1088,1096`) |
| 텔레코일 On/Off 0x47 | `remoteControl.c:1152-1171` | `changeTeleCoil_OnOff(...)`(`:1156`) |
| 자극 인디케이터 On/Off 0x48 | `remoteControl.c:1173-1192` | `changeStimulIndicator_OnOff(...)`(`:1177`) |
| LED On/Off 0x49 | `remoteControl.c:1194-` | `changeLED_indicatorOnOff(...)`(`:1198`) |

각 `change*()`는 공유메모리 값을 즉시 바꾼 뒤 `system_opMode == en__normalMode`일 때만 그 자리에서 동기적으로 `fn_write_userSettingParameters(connected_ISD_num)`를 호출한다. 예 `changeAudioVolume()`(`cfx_cm3_sharedMemory.c:490-498`):
```c
cfx_cm3_sharedMemoryAll.userSettingValue.audioVolume = volume;
if (cfx_cm3_sharedMemoryAll.systemShare.system_opMode == en__normalMode)
{
    fn_write_userSettingParameters(cfx_cm3_sharedMemoryAll.connected_ISD_num);
}
```
동일 패턴이 `changeStimulVolume`(`:475-483`), `changeLED_indicatorOnOff`(`:505-514`), `changeTeleCoil_OnOff`(`:521-529`), `changeStimulIndicator_OnOff`(`:536-544`), `changeProgramMapNum`(`:420-468`, 449-451행)에도 반복된다.

`fn_write_userSettingParameters()`(`Gen1_5/fn_fromCFX/fn_from_cfx_eeprom_write.c:8-16`)는 `mapNum > 0`일 때만(초기화 미완료 가드) 공유메모리 값을 RAM 미러로 복사하고 `ci_map_write_user_setting_value(connected_ISD_num)`를 호출해 **즉시 FAT 파일에 동기 기록**한다. 디바운스·배치·지연 큐 로직은 발견되지 않았다(관련 키워드 검색 0건, 콜스택 전체가 인터럽트 없는 동기 함수 호출). **즉 매 볼륨 조절 이벤트마다 즉시 파일 쓰기가 발생한다.**

매핑(청각사) 앱 경로는 별도: `setReadWriteMapDataFlashCommand()`(`cfx_cm3_sharedMemory.c:570-610`) → `fn_write_Mapdata_mappingApp()`(`fn_from_cfx_eeprom_write.c:79-98`)도 동일하게 동기 즉시 기록이다.

쓰기 함수의 `int` 반환값은 상위 호출부에서 검사되지 않는다(`fn_from_cfx_eeprom_write.c:14`; `cfx_cm3_sharedMemory.c:494-497` 등) — 쓰기 실패가 조용히 무시될 수 있다.

## 5. 새 필드 추가 시 영향 (호환성)

### 5.1 버전/마이그레이션 로직

`Gen1_5/FS/` 전체에서 `version`/`magic` 키워드 검색 결과 0건 — **파일 포맷 버전 필드나 마이그레이션 로직은 존재하지 않는다.** 무결성 검증은 CRC-CCITT뿐이다.

### 5.2 고정 크기 레코드 + AES 블록 정렬 불변식

각 파일은 구조체 `sizeof` 그대로의 고정 크기 레코드다(가변 길이/TLV 아님). 쓰기 함수는 **AES 사용 여부와 무관하게 무조건** 다음 불변식을 강제한다(`ci_filesystem.c:519-524`):
```c
int remain = data_size % 16;
if ((remain + 4 + aes128_padding_size) != 16)
{
    ci_printe("[FS] INVALID TAIL: REMAIN(%d) + 4 + PAD(%d) != 16\r\n", remain, aes128_padding_size);
    return ret;  // -1, 파일에 아무것도 쓰지 않고 즉시 리턴
}
```
각 호출부의 `aes128_padding_size`는 하드코딩 상수다(사용자 설정=0 `ci_map.c:89`, ISD정보=8 `ci_map.c:74`, 맵스탬프=4 `ci_map.c:104`, 맵데이터=8 `ci_map.c:120`). 현재 `sizeof(ST__CFX_CM3_SharedMemory_userSettingValue)=28`이므로 `28%16=12`, `12+4+0=16`으로 정확히 맞는다. **여기에 `int` 필드를 1개(4바이트) 추가하면 32바이트가 되어 `32%16=0`, `0+4+0=4≠16`이 되므로 쓰기 함수가 즉시 실패(-1)를 반환하고 파일에 아무것도 쓰지 못한다.** (4바이트 필드를 4개 단위로 추가하면 정렬이 우연히 다시 맞을 수 있음 — 이는 오히려 위험하다: 정렬 조건이 개발자에게 드러나지 않은 채 통과/실패가 오락가락한다.)

### 5.3 정렬을 맞춰도 남는 문제 — 기존 파일과의 하위호환

정렬을 맞춰 쓰기가 통과해도, **이미 저장된(구 버전 크기) 파일을 읽을 때**:
- `f_read()`를 새 `data_size`만큼 시도하고 `br != data_size`면 즉시 실패(-1)(`ci_filesystem.c:233-239`).
- 바이트 수가 우연히 맞아도 CRC가 새 레이아웃 기준으로 재계산되므로 구 파일 CRC와 불일치해 실패(-1)(`ci_filesystem.c:404-489`, 481행).
- 이 실패는 부팅 시 `ci_map_init_map_data_all(false)`(`initialize.c:321`) 경로에서 감지되어 **해당 슬롯의 사용자 설정 레코드만 하드코딩 기본값으로 되돌려 즉시 재기록**된다(`ci_map.c:498-509`) — 필드 단위 마이그레이션이 아니라 **전체 리셋**이다. 사용자가 저장해 둔 볼륨·LED 등 설정이 조용히 초기값으로 되돌아간다.
- 연결 시점 로드 경로(`changeConnected_isd_num_CFX` → `ci_map_read_user_setting_value`)는 반환값을 검사하지 않으므로(`cfx_cm3_sharedMemory.c:338`), 읽기 실패 시에도 `fn_copy_userSettingParameters_toCM3()`가 RAM 미러의 (부팅 시 이미 기본값으로 재기록된) 내용을 그대로 공유메모리에 복사한다.

### 5.4 참고: 이번 태스크(gain-conversion-table)와의 직접 관계

2026-07-20 커밋 `1f6ecc0`로 최상위 공유메모리 구조체 `ST__CFX_CM3_SharedMemory_ALL`에 `gain_table_index_a`, `gain_table_index_b`, `is_i2s_source_cradle` 3개 필드가 추가되었다(`cfx_cm3_sharedMemory.h:246-248`). **이 필드들은 본 문서 2~5절의 슬롯별 FS 영속 구조(`CI_FILESYSTEM_MAP_T`/`ST__CFX_CM3_SharedMemory_userSettingValue`)에 속하지 않으며, 매 부팅 시 `tdc_remote_gain_control_init()`가 하드코딩 기본값으로 재설정하는 RAM 전용 값이다**(`Cortex-M3-src/BleCommunication/tdc_remote_gain_control.c:25-30`, 호출 `initialize.c:350`). 즉 현재 게인 인덱스는 ISD별로 저장·복원되지 않으며, 이를 영속화하려면 본 문서의 슬롯별 파일 저장 패턴(파일명 추가, `CI_FILESYSTEM_MAP_T`에 필드 추가, 5.2의 정렬 불변식 고려, 3·4절의 로드/저장 호출 체인 편입)을 새로 설계·적용해야 한다(설계 자체는 본 조사 범위 밖).
