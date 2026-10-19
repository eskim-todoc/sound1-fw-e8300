---
name: bipolar 자극 파라미터 설정 경로 - 구버전 대 현재 회귀 대조
purpose: 매핑 앱 bipolar 모드 동작 시 실기 에러의 원인을 찾기 위해 구버전(정상)과 현재(에러) isd bipolar 자극 파라미터 설정 코드를 함수 단위·한 줄 단위로 대조하고, 회귀 여부를 판정
type: tasks/에이전트-로그
applies_to: [Sound1]
tags: [isd, bipolar, regression-analysis, stim-para-setting, cm3]
---

# 05. bipolar 자극 파라미터 설정 경로 - 구버전 대 현재 회귀 대조

**TL;DR**: `tdc_isd_set_stim_para_bipolar()`(현재)와 `settingStimulPara_biPolarMode()`(구버전) 및 관련 디스패치·레지스터 조합부·상수 정의를 전수 대조한 결과, 순수 심볼 개명 외에 **동작에 영향을 주는 차이를 찾지 못했다**. 두 파일의 bipolar 경로는 기능적으로 동일하다.

> [!NOTE]
> 본 문서는 `E:\workspace\projects\sullivan-1-5-fw-hw-test-link-pwr-bt-period\Sullivan1_5__CM3\Cortex-M3-src\internalDevice\isd_interface_stimulationParaSetting.c`(구버전, 정상 동작)와 `E:\workspace\projects\sound1-fw-e8300\src\2__cm3\source\isd\tdc_isd_stim_para_setting.c`(현재, 에러 발생)의 **bipolar 자극 파라미터 설정 경로**만을 대상으로 한다. 읽기 전용으로 조사했으며 어떤 소스 파일도 수정하지 않았다.

## 0. 조사 범위와 방법

- 주 대상 함수: 현재 `tdc_isd_set_stim_para_bipolar()`(`tdc_isd_stim_para_setting.c:392-1094`) ↔ 구버전 `settingStimulPara_biPolarMode()`(`isd_interface_stimulationParaSetting.c:398-1117`). **두 함수 전체(각각 약 700줄)를 처음부터 끝까지 한 줄씩 대조**했다(오프셋 구간별로 4회 분할 Read).
- 부가 대상: 레지스터 값 조합부(모노폴라 함수 내 `case en__bipolar` 분기, 현재`:219-228`/구버전`:225-234`), 디스패치 함수(현재 `tdc_isd_stim_setting_step`:1096-1141 / 구버전 `stimulationSetting`:1119-1165).
- 상수·매크로는 정의 파일까지 따라가 확인: `unusedReferenceElectrode_DummyNum`, `ISD_registerAddr_en__bipolar_referenceElectroldIndex`, `ISD_writeRegister`/`ISD_readRegister`, `ISD_registerAddr_cipherDataStatus`/`DAC_offsetValue`/`StimulationConfig`, `pcm_Mold_configuration`/`NopBacktel`/`NopStandby`, `FPGA_pulsePhaseWidth_minimum`, `df_MaxNumOfElectrode`(32)/`df_MaxNumTransferableChannel`(24), `electrodeMap[32]` 배열 실제 값, `EN___STIMULATION_MODE` enum 순서, `DisalbedBackTel` 매크로 정의 여부.
- **범위 밖으로 남긴 것**: 이 함수들이 호출하는 헬퍼 함수의 **내부 구현**(`tdc_isd_fpga_read_backtel_fifo` 등, `tdc_isd_fpga.c`) 및 `p_mapdata`(공유메모리 맵 데이터)가 채워지는 BLE/매핑 앱 파싱 경로. 아래 §4에서 명시.

## 1. 이미 확인된 사항 (재확인 안 함)

- 백텔 검증부 `electrodeMap[...]` 사용 줄이 주석 처리되고 `bipolarReferenceElectrodeNum[i]`를 직접 비교(구 `:816-817`, 현재 `:793-794`) — 양쪽 동일, 회귀 아님.
- 바이폴라 기준전극 버퍼 구성부(구 `:474-502`, 현재 `:457-479`) — 구버전 `#if 0`(구`:484-490`)가 현재는 죽은 코드 제거로만 바뀌었고 `#else` 실행 경로는 동일.

## 2. 차이 목록표

전 구간을 한 줄씩 대조했으며, 아래는 발견된 표면적 차이 전부다. **동작 영향이 "있음"인 항목은 없다.**

| 차이_번호 | 구버전 파일:라인 | 현재 파일:라인 | 내용 | 동작 영향 |
|---|---|---|---|---|
| 차이_1 | `isd_interface_stimulationParaSetting.c:21,31,36` | `tdc_isd_stim_para_setting.c:21,31,36` | 함수명 개명: `clear_sitmulationParaSettingDone`→`tdc_isd_clear_stim_para_setting_done`, `isStimulParaIsSettingDone`→`tdc_isd_is_stim_para_setting_done`, `settingStimulPara_monoPolarMode`→`tdc_isd_set_stim_para_monopolar` | 없음 |
| 차이_2 | `isd_interface_stimulationParaSetting.c:398` | `tdc_isd_stim_para_setting.c:392` | 함수명 `settingStimulPara_biPolarMode`→`tdc_isd_set_stim_para_bipolar` (본문 로직은 동일) | 없음 |
| 차이_3 | `isd_interface_stimulationParaSetting.c:400-408,422-424,431` | `tdc_isd_stim_para_setting.c:394-409` | 구버전에만 있는 미사용 지역변수 선언: `w_FPGA_registerValue`, `bitReverse`, `r_isd_registerValue`, `fifoCounter`, `compare`, `en__bipolarRefereceElectorodeIndex`, `referencElectrod_index`, `en__bipolarRefer_EelectrodeNum`, `stimulElectrodeNum`, `tempA/B/C/D` — 전부 함수 본문에서 실제로 읽거나 쓰는 곳이 없는 죽은 선언(구버전 `#if 0` 분기의 잔재로 추정). 현재는 정리되어 없음 | 없음 (미사용 변수라 실행 경로에 관여하지 않음) |
| 차이_4 | `isd_interface_stimulationParaSetting.c:484-490` | `tdc_isd_stim_para_setting.c:467-469` | `#if 0`로 죽어있던 `usableStimulationElectrodIndex != 99` 분기 삭제, 현재는 설명 주석만 남김. `#else` 실행 경로(electrodeMap 경유 계산)는 동일 | 없음 (§1에서 기확인) |
| 차이_5 | 전 구간 | 전 구간 | `ci_printi/ci_printe/ci_printw/ci_printv` → `TDC_PRINTF_I/E/W/V`, `fillSepcificCommndBuffer`→`tdc_shm_fill_specific_command_buffer`, `changeNextPcmOutputMode`/`changePcmOutputMode`→`tdc_shm_change_next_pcm_output_mode`/`tdc_shm_change_pcm_output_mode`, `chang_PulseWidth_minimum`→`tdc_isd_fpga_change_pulse_width_minimum`, `upadte_fpga_pulsePhaseWidth_written_Value`→`tdc_isd_fpga_update_fpga_pulse_phase_width_written_value`, `change_8BitBacktel_mode`→`tdc_isd_fpga_change_8_bit_backtel_mode`, `fill_pcmBuff_lastSimulationOut`→`tdc_isd_fill_pcm_last_stimulation_out`, `readStimulDAC_RegisterValue`→`tdc_stim_read_dac_register_value`, `getPointerCurrentMapData`→`tdc_shm_get_pointer_current_map_data`, `write_FPGA_clear_FIFO`→`tdc_isd_fpga_write_clear_fifo`, `check_FPGA_FIFO_empty`→`tdc_isd_fpga_check_fpga_fifo_empty`, `change_isd_state`→`tdc_isd_change_state`, `clearIsdControlStateChagedFlag`→`tdc_isd_clear_control_state_changed_flag`, `read_FPGA_FIFO_counter`→`tdc_isd_fpga_read_fifo_counter`, `read_FPGA_backtel_FIFO`→`tdc_isd_fpga_read_backtel_fifo`, `read_FPGA_PulseWidth`→`tdc_isd_fpga_read_pulse_width`, `read_FPGA_systemError_Flag`→`tdc_isd_fpga_read_system_error_flag`, `chang_PulseWidth`→`tdc_isd_fpga_change_pulse_width`, `disable_Backtel`→`tdc_isd_fpga_disable_backtel`, `stimulationSetting`→`tdc_isd_stim_setting_step` | 없음 (호출 인자·순서·반환값 처리 전부 동일, 순수 개명) |
| 차이_6 | `isd_interface_stimulationParaSetting.c:1152-1157` | `tdc_isd_stim_setting_step.c:1129-1134`(신) | 구버전 디스패치는 `en__commonground`를 `settingStimulPara_monoPolarMode` 호출로 처리, 현재는 동일하게 `tdc_isd_set_stim_para_monopolar` 호출 | 없음 (개명 외 동일) |

레지스터 조합부(모노폴라 함수 내 bipolar 분기, 요청 지정 구간)도 대조:

| 차이_번호 | 구버전 | 현재 | 내용 | 영향 |
|---|---|---|---|---|
| 차이_7 | `:225-234` | `:219-228` | `switch(p_mapdata->stimulationMode)` 4-way 분기(`en__monopolr_*`→0, `en__bipolar`→1, `en__commonground`→2, `en__semi_simultaneously`→3), 시프트·마스크 순서 | 완전 동일 (없음) |
| 차이_8 | `:900-901`(모노폴라 함수와 별개로 bipolar 함수 case 30 내부) | `:900-901` | `case en__bipolar: w_isd_registerValue \| 1;` | 완전 동일 (없음) |
| 차이_9 | `:1145-1150`(구) `stimulationSetting`) | `:1122-1127`(신 `tdc_isd_stim_setting_step`) | `case en__bipolar: error = settingStimulPara_biPolarMode(startTrigger);` ↔ `tdc_isd_set_stim_para_bipolar(startTrigger)` | 개명만, 없음 |

`ISD_registerAddr_en__bipolar_referenceElectroldIndex` 8개 사용 지점(요청 지정: 현재 `:543,571,598,627,656,685,714,743`, 대응 구버전 `:566,594,621,650,679,708,737,766`) 전부 다음 3단계 조합이 좌우 완전히 동일함을 확인:
1. `w_isd_registerValue = ISD_registerAddr_en__bipolar_referenceElectroldIndex;` (0x04)
2. `<<1 | ISD_writeRegister(1)` 또는 `<<1 | ISD_readRegister(0)` (쓰기 4곳/읽기 6곳, 좌우 배치·순서 동일)
3. `<<8 | (0x1F & bipolarReferenceElectrodeNum[i])` 또는 `| pcm_Mold_configuration`

## 3. 상수·매크로 정의 대조 (정의 파일까지 확인)

| 상수 | 구버전 값 | 구버전 정의 파일 | 현재 값 | 현재 정의 파일 | 일치 |
|---|---|---|---|---|---|
| `unusedReferenceElectrode_DummyNum` | 31 | `Sullivan1_5__CM3\99_includeBoard\isd_ver1_1_2.h:42` (외 1_1_0/1_1_1 동일값) | 31 | `src\2__cm3\source\board\isd_ver1_1_2.h:42` | 일치 |
| `ISD_registerAddr_en__bipolar_referenceElectroldIndex` | 0x04 | 상동 `:17` (1_1_0/1_1_1도 0x04) | 0x04 | 상동 `:17` | 일치 |
| `ISD_writeRegister` / `ISD_readRegister` | 1 / 0 | `isd_ver1_1_2.h:7-8` | 1 / 0 | `isd_ver1_1_2.h:7-8` | 일치 |
| `ISD_registerAddr_cipherDataStatus` / `DAC_offsetValue` / `StimulationConfig` | 0x02 / 0x05 / 0x06 | `isd_ver1_1_2.h:15,18,19` | 0x02 / 0x05 / 0x06 | 상동 | 일치 |
| `pcm_Mold_configuration`/`NopBacktel`/`NopStandby` | 0x50000 / 0x35555 / 0x25555 | `dirver_PCM.h:18-23` | 0x50000 / 0x35555 / 0x25555 | `tdc_isd_pcm.h:18-23` | 일치 |
| `FPGA_pulsePhaseWidth_minimum` | 13 (전 버전 헤더 공통) | `FPGA_ver2_7_0.h` 등 | 13 | `FPGA_ver2_7_0.h:133` | 일치 |
| `df_MaxNumOfElectrode` / `df_MaxNumTransferableChannel` | 32 / 24 | `definitionsForAlgorithm.h` | 32 / 24 | `tdc_stim_definitions.h:61,68` | 일치 |
| `electrodeMap[32]` 배열 실제 값 | `{31,12,11,9,7,5,3,1,0,2,4,6,8,10,13,15,17,18,20,22,24,26,28,30,14,29,27,25,23,21,19,16}` | `99_includeBoard\electrodeMapping.c:3` | 동일 배열 | `source\board\electrodeMapping.c:3` | 일치 (바이트 단위 동일, 주석의 한글만 구버전 파일에서 CP949 손상 표시로 보임 - 배열 데이터 자체는 무관) |
| `EN___STIMULATION_MODE` enum 순서 | `en__referenceNA=0, monopolr_body, monopolr_rod, monopolr_BothRodBody, bipolar, commonground, semi_simultaneously` | `cfx_cm3_sharedMemory.h:55-61` | 동일 순서 | `tdc_shm.h:69-75` | 일치 |
| `usableStimulationElectrodIndex[]`/`usableReferenceElectrodIndex[]`/`numFrequencyBand`/`stimulationPulsePhaseWidth`/`stimulationMode` 필드 타입 | 전부 `int` (배열 크기 `df_MaxNumOfElectrode`=32) | `cfx_cm3_sharedMemory.h:105-111` | 전부 `int`, 동일 크기 | `tdc_shm.h:119-125` | 일치 (타입 변경 없음) |
| `DisalbedBackTel` 매크로 정의 여부 | 헤더·`.cproject` 어디에도 `#define` 없음 (`#ifdef`/`#ifndef`만 존재) → 비활성(백텔 검증 활성) | `definitionsForAlgorithm.h:83` 등 | 동일 - 정의 없음 → 비활성 | `tdc_stim_definitions.h:83` 등 | 일치 |

## 4. 확인하지 못한 구간 (미확인)

요청받은 두 파일 범위를 벗어나 **직접 코드 대조를 수행하지 않은** 영역이다. 이 로그의 "회귀 없음" 결론은 어디까지나 두 지정 파일에 한정되며, 아래는 근거 라인 없이 이름만 나열한다 (추측 방지를 위해 "다음에 열어봐야 할 후보"로만 취급할 것):

- `tdc_isd_fpga_read_backtel_fifo()` / `tdc_isd_fpga_read_fifo_counter()` 실제 구현 — `src\2__cm3\source\isd\tdc_isd_fpga.c:264`, `:220`. 구버전 대응 구현(`isd_interface_FPGA.c` 등)과 아직 대조하지 않았다. 호출 시그니처와 반환값 처리(현재 파일 쪽)는 구버전과 동일함을 확인했으나, **내부에서 I2C 재시도 횟수·타임아웃·버퍼 클리어 방식이 바뀌었는지는 미확인**.
- `tdc_shm_fill_specific_command_buffer()` (`src\2__cm3\source\cfx_link\tdc_shm.c:107`) 내부 구현 — PCM 버퍼 경계 체크 로직이 리팩토링 중 바뀌었는지 미확인.
- `p_mapdata`(`ST__CFX_CM3_SharedMemory_mapData`)의 `usableStimulationElectrodIndex[]`/`usableReferenceElectrodIndex[]`/`numFrequencyBand`가 **매핑 앱의 BLE 패킷으로부터 실제로 채워지는 경로** (`tdc_ble_mapping.c` 등) — 구조체 필드 타입·순서는 양쪽 `int`로 동일함을 확인했으나, 그 안에 들어가는 **값 자체**(예: 미사용 밴드의 sentinel 값이 99인지, 다른 값으로 바뀌었는지)는 이번 조사 범위 밖이다. 이 프로젝트 폴더가 "BLE 8비트 패킷 재설계" 작업 하위에 있다는 점을 고려하면, 실기 에러의 실제 원인은 **이 두 파일이 아니라 BLE 패킷을 이 구조체로 파싱해 넣는 경로**에 있을 가능성이 있으나, 이번 조사에서는 증거를 확인하지 못했다.
- `isd_ver1_1_0.h`/`isd_ver1_1_1.h` (구버전에만 존재, 현재는 `isd_ver1_1_2.h` 하나만 존재) 중 **구버전 빌드가 실제로 어느 헤더를 include하는지**는 확인하지 못했다. 다만 3개 버전 전부 본 조사에서 대조한 상수값(`ISD_writeRegister` 등)이 동일했으므로, 어느 버전이 선택되든 결과에 영향은 없다.

## 5. 우선순위표 (동작 영향 "있음" 항목)

**없음.** 지정된 두 파일의 bipolar 경로 전체(주 함수 약 700줄 + 레지스터 조합부 + 디스패치)를 한 줄씩 대조했으나, 순수 개명·죽은 코드 정리 외에 동작에 영향을 주는 차이를 찾지 못했다.

## 6. 가장 유력한 원인 후보 (파일 내 회귀는 아님 - 다음 확인 지점)

아래 3가지는 "이 두 파일을 대조해서 찾은 회귀"가 **아니다**. 두 파일이 기능적으로 동일하다고 확인된 이상, 다음에 열어봐야 할 곳을 근거와 함께 제시한다(각각 미확인으로 표기).

1. **미확인 - 매핑 앱→공유메모리 값 자체의 변화**: `bipolarReferenceElectrodeNum[i] != (0x1F & backtelBuff[i])`(현재`:794`/구`:817`) 에러가 실제로 뜬다면, 코드 로직이 같으므로 **입력값(`p_mapdata->usableStimulationElectrodIndex[]`/`usableReferenceElectrodIndex[]`)이 실기 앱 데이터에서 이미 다르게 채워지고 있을 가능성**이 가장 크다. 증상: 특정 채널에서만 반복적으로 REF INDEX 불일치가 나고, 재시도해도 같은 채널에서 계속 발생한다.
2. **미확인 - `tdc_isd_fpga_read_backtel_fifo`/`read_fifo_counter` 내부 I2C 타이밍**: 호출부는 동일하지만 내부 구현을 대조하지 않았다. 증상: "READ BACKTEL COUNT IS 0"이나 "I2C FAILED TO READ FIFO COUNT" 에러가 간헐적/타이밍 의존적으로 발생.
3. **미확인 - PCM 버퍼 채우기(`tdc_shm_fill_specific_command_buffer`) 경계 로직**: 호출 인자·횟수는 동일하나 내부에서 버퍼 오버플로/언더플로 가드가 달라졌을 가능성. 증상: 특정 case(예: case 8/10 쓰기 직후) 이후 FIFO에 쓰레기 값이 들어가 REF INDEX 자체가 이상값(0~31 범위를 벗어나지 않지만 의도와 다른 값)으로 검출.

**결론**: 지정된 두 파일(`tdc_isd_stim_para_setting.c` ↔ `isd_interface_stimulationParaSetting.c`)의 bipolar 경로는 회귀가 없다. 에러 재현 시 위 §4 미확인 항목(특히 매핑 앱 데이터 파싱 경로)을 다음 조사 대상으로 좁히는 것을 제안한다.
