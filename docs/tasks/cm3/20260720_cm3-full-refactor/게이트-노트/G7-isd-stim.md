---
name: G7 isd/stim 게이트 노트
purpose: G7(internalDevice·stimulationParaCal·indicator) 이동·rename + FSM 표준화 판단 기록
type: tasks/게이트노트
applies_to: [Sound1]
tags: [cm3, refactoring, gate, g7, isd, stimulation, fsm, mapping]
---

# G7 — isd/stim (내부기 인터페이스 · 자극)

**TL;DR**: 이번 리팩토링 **최대 규모(약 11,000줄) + 최대 위험(자극 제어)** 게이트. **2단계 분리**: G7-1(이동·rename, 동작 100% 보존) → G7-2(매핑 5대 FSM 표준화, 1함수 1커밋). 자극 제어라 rename 과 FSM 재구조화를 절대 섞지 않는다(회귀 원인 분리 불가).

## 1. 파일 매핑 (G7-1)

`Cortex-M3-src/internalDevice/` 26파일 + `signalProcessing/` + `isdExecution/driver_PCM.h`(G3 이월) → `isd/` · `stim/`.

| 현행 | 신규 | 도메인 |
|---|---|---|
| `internalDevice/isd_interface.c/.h` | `isd/tdc_isd.c/.h` | isd (핵심 진입점) |
| `internalDevice/isd_interface_FPGA.c/.h` | `isd/tdc_isd_fpga.c/.h` | isd |
| `internalDevice/isd_interface_init_FPGA.c/.h` | `isd/tdc_isd_init_fpga.c/.h` | isd |
| `internalDevice/isd_interface_init_ISD.c/.h` | `isd/tdc_isd_init.c/.h` | isd |
| `internalDevice/isd_interface_stimulationParaSetting.c/.h` | `isd/tdc_isd_stim_para_setting.c/.h` | isd |
| `internalDevice/isd_interface_stimulationStandAlone.c/.h` | `isd/tdc_isd_stim_standalone.c/.h` | isd |
| `internalDevice/isd_interface_mapping_Live.c/.h` | `isd/tdc_isd_map_live.c/.h` | isd (FSM) |
| `internalDevice/isd_interface_mapping_eCAP_Measurement.c/.h` | `isd/tdc_isd_map_ecap.c/.h` | isd (FSM) |
| `internalDevice/isd_interface_mapping_impedanceMeasurement.c/.h` | `isd/tdc_isd_map_impedance.c/.h` | isd (FSM) |
| `internalDevice/isd_interface_mapping_testStimulation.c/.h` | `isd/tdc_isd_map_test_stim.c/.h` | isd (FSM) |
| `internalDevice/isd_interface_mapping_SepcificStimulation.c/.h` | `isd/tdc_isd_map_specific_stim.c/.h` | isd (FSM, 오타 Sepcific 정정) |
| `internalDevice/isd_interface_mapping_readWrtieMapData.c/.h` | `isd/tdc_isd_map_data.c/.h` | isd (오타 readWrtie 정정) |
| `internalDevice/indicatorByStimul.c/.h` | `stim/tdc_stim_indicator.c/.h` | stim |
| `signalProcessing/stimulationParaCal.c/.h` | `stim/tdc_stim_para_cal.c/.h` | stim |
| `signalProcessing/commonDataProcessing.c/.h` | `stim/tdc_stim_common.c/.h` | stim |
| `signalProcessing/definitionsForAlgorithm.h` | `stim/tdc_stim_definitions.h` | stim (§4 복제 주의) |
| `isdExecution/driver_PCM.h` | `isd/tdc_isd_pcm.h` | isd (G3 이월 - PCM 비트스트림 모드) |

`internalDevice/` · `signalProcessing/`(2곳) · `isdExecution/` 소멸.

## 2. 도메인 접두어 (G7-1)

| 도메인 | 접두어 | 예 |
|---|---|---|
| isd | `tdc_isd_` | `isd_interface` → `tdc_isd_step`, `change_isd_state` → `tdc_isd_change_state`, `snd_isd_interface_get_state` → `tdc_isd_get_state` |
| stim | `tdc_stim_` | `stimulationParaCal` 계열, `indicatorByStimul` → `tdc_stim_indicator_*` |

접두어 극다양(`read`17·`write`11·`change`6·`get`6·`fill`3·무접두)에 **오타 다수**(`chang`·`upadte`·`Sepcific`·`readWrtie`·`Chaged`·`Resgister`) → 도메인별 명시 매핑 + 오타 정정.

## 3. FSM 표준화 (G7-2) — 생략 확정 (2026-07-22)

매핑 5대(`Live`/`eCAP`/`impedance`/`testStim`/`specificStim`)는 각 14~16 static 의 대형 FSM이며 **동일 패턴의 복제 5벌**(② census). 자극 제어 = 의료 기능.

> [!IMPORTANT]
> **은수님 판단: G7-2 생략, 현행 유지.** 5대 FSM 은 이미 동작 검증된 자극 제어 코드이므로 구조 변경 위험을 감수하지 않는다. G7-1 이동·rename 으로 파일명·심볼만 정합하고 제어 흐름은 그대로 둔다.
>
> 밸리데이션 계층화 지침(§6 "테스트를 위한 분해 금지", §7.2 "동작 보존 전제")과 정합. 향후 실제 필요(신규 기능·버그)가 생기면 그때 개별 FSM 을 표준 패턴으로 정리한다.

## 4. 불가침

| 항목 | 방침 |
|---|---|
| 공유 ABI | `ST__ISD_STATUS` · `EN__ISD_CONTROL_STATE` · `ST__CFX_CM3_SharedMemory_*` 등 공유/피참조 타입은 **보류 목록 대조** |
| `definitionsForAlgorithm.h` | CFX·calibration 복제본 존재(③ §7) — **rename 시 복제 심볼 동결 목록 대조**, 2__cm3 전용만 |
| PCM 모드 상수 | `PcmBitStream_Mode_*` (driver_PCM.h) - 값 불변, 심볼명만 |
| 동작 보존 | G7-1 은 rename/이동만. FSM 은 G7-2 분리 |

## 5. 유닛/모듈 맵

| 모듈 | 유닛 | 검증 라벨 |
|---|---|---|
| isd | `tdc_isd`(진입 FSM) · `tdc_isd_fpga`/`init_*`(초기화) · `tdc_isd_map_*`(매핑 5 FSM) · `tdc_isd_stim_*` | G7-1 **[로직설명]**, G7-2 매핑 FSM **[통합테스트]**(실기) |
| stim | `tdc_stim_para_cal` · `tdc_stim_indicator` · `tdc_stim_common` | **[로직설명]** |

의존: `isd` → `hal/i2c`(G3) · `drv/isl*`(PMIC) · `led`(G5) · `fs/map`(G2) 통과 전제. `stim` → `isd` 통과 전제.

## 6. 검증

### G7-1 (이동·rename)

| 라벨 | 결과 |
|---|---|
| 검증_공통_1 (구 심볼 잔존 0) | ✅ 구 파일명·함수 잔존 0. 잔존한 `Resgister`(FPGA 헤더 상수)·`Sepcific`(`fillSepcificCommndBuffer`=공유메모리 G8)·주석 2건은 **G7 범위 밖** |
| 검증_공통_2 (공유 ABI 무변경) | ✅ `cfx_cm3_sharedMemory.h` diff = include 파일명 2줄(`driver_PCM`→`tdc_isd_pcm`, `definitionsForAlgorithm`→`tdc_stim_definitions`) |
| 검증_공통_3 (균형·훅) | ✅ isd 25 + stim 7 파일 **활성 분기 균형 OK** · 깨진 include 0 · 훅 통과 |
| 검증_공통_4 (사장 실증) | ⏳ 보류 — FPGA accessor 15개(③ census)는 G7-2 또는 별도 일괄 정리 (자극 계층 신중) |
| **검증_G7_특화 (복제 심볼 동결)** | ✅ `tdc_stim_definitions.h`(구 definitionsForAlgorithm) 동결 34종 전부 무변경 — `df_MaxNumOfElectrode` 등 살아있음 |
| 검증_공통_5 (빌드·실기) | ⏳ 은수님 게이트 — **ISD 연결·자극·매핑(임피던스/eCAP/라이브) 확인 필수** |

### G7-2 (매핑 FSM 표준화) — **생략 확정** (은수님 판단, §3)

## 7. G7-1 결과 · 발견

- **깨진 include 정정**: `driver_PCM.h` 가 `<isdExecution/driver_PCM.h>` 형태(경로 접두어)로 include 돼 있었는데, 파일 이동으로 경로가 어긋나 10파일이 깨졌다. `<tdc_isd_pcm.h>`(경로 접두어 제거, isd/ 는 include 경로 등록됨)로 정정. **빌드 실패 직결 위험을 검증이 차단**.
- **균형 검사기 개선**: `#if 1 / { / #else / { / #endif / }` 조건부 분기 관용구(impedance 등)를 단순 중괄호 카운트가 `b=1` 로 오탐. **활성 분기 기준 균형 검사**로 재확인 — 원본부터 정상이었고 rename 무영향.
- 오타 대량 정정: `Resgister`→`register`, `upadte`→`update`, `chang_`→`change_`, `Sepcific`→`specific`(파일명), `readWrtie`→`data`, `Chaged`→`changed`, `Loade`→`loaded`, `Rigth`→`rshift`, `Channle`/`Channe`→`channel`, `Simulation`→`stimulation`, `sitmulation`→`stim`, `Indicato`/`Stimlul`→`indicator/level`.
- 파일 이동 27개: `isd/`(25) · `stim/`(7). `internalDevice/` · `signalProcessing/`(2곳) · `isdExecution/` 소멸.
