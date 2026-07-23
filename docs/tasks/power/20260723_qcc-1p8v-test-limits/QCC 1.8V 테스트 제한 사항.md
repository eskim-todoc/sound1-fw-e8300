---
name: QCC 1.8V 테스트 제한 사항
purpose: claude_hw_test_qcc_1.8v_current 브랜치에서 전류 측정을 위해 제한·제거한 코드 전수 목록과 복원 방법
type: tasks/산출물
applies_to: [Sound1]
tags: [hw-test, qcc, power, fpga, touch, isd, temporary]
---

# QCC 1.8V 테스트 제한 사항

**TL;DR**: QCC 1.8V 레일 전류 측정을 위해 FPGA 활성화·터치센서·ISD 연결 시퀀스·자극 파라미터 설정·CFX ADC DEM 을 제한하거나 제거한 내역 전수. 총 11건(코드 10건 + 기 커밋 1건). **본 브랜치는 `claude_develop` 에 병합하지 않는 실험 전용**이며, 각 항목은 주석 해제로 원복 가능하다.

> [!CAUTION]
> 이 브랜치(`claude_hw_test_qcc_1.8v_current`)의 펌웨어는 **정상 동작하지 않는다.** ISD 연결 FSM 이 하드웨어 응답과 무관하게 무조건 성공으로 전이하고, 자극 출력용 10V 가 인가되지 않으며, 전원 버튼이 동작하지 않는다. 전류 측정 목적 외 용도로 사용 금지.

## 1. 목적

QCC(BLE SoC) 1.8V 레일의 소비 전류를 측정하기 위해, 측정에 노이즈를 주거나 전류를 소모하는 주변 블록(FPGA·터치센서·ISD 링크·자극 출력)을 부팅 경로에서 배제한다. ISD 연결 FSM 은 각 단계의 실제 하드웨어 초기화를 건너뛰고 다음 상태로 즉시 전이시켜, 링크가 성립된 것처럼 상위 로직을 진행시킨다.

## 2. 제한·제거 전수 목록

### 2.1 FPGA

| 항목 | 위치 | 변경 | 원래 동작 |
|---|---|---|---|
| FPGA 활성화 핀 | [`tdc_hal_dio.c:46`](../../../../src/2__cm3/source/hal/tdc_hal_dio.c) | `Sys_GPIO_Set_High()` → `Sys_GPIO_Set_Low()` | `DIO_PIN_INDEX_for_FPGA_SLEEP` 을 High 로 올려 FPGA 를 활성 상태로 둔다(소스 주석 "FPGA 활성화 핀"). Low 로 내려 비활성 상태로 유지 |
| FPGA 초기화 시퀀스 | [`tdc_isd.c:182`](../../../../src/2__cm3/source/isd/tdc_isd.c) | `tdc_isd_init_fpga()` 주석 처리 → `tdc_isd_change_state(en__isdStatus_FPGA_Ok)` | FPGA 소프트웨어 리셋 · PCM Abort · Preamble · Nop 패킷 전송 후 싱크 로스트 등 에러 검증 |

### 2.2 터치센서 (IQS323)

| 항목 | 위치 | 변경 | 원래 동작 |
|---|---|---|---|
| 터치 초기화 | [`tdc_sys_init.c:443~452`](../../../../src/2__cm3/source/sys/tdc_sys_init.c) | `tdc_touch_init_begin()` 호출부를 `#if 0 ... #endif` 로 감쌈 (MILESTONE 로그 포함) | POWER_ON 진입 시 1회 IQS323 MCLR 트리거 + Auto-ATI 시작(~1.5s, LED 버스트와 병렬) |
| 터치 폴링·전원 버튼 입력 | [`main.c:597`](../../../../src/2__cm3/source/main.c) | `ev->power_button = tdc_touch_process()` → `ev->power_button = false` | 매 tick 터치 폴링(초기화 진행 + 액션 디스패치) 및 전원 버튼 눌림 상태 공급 |

> [!WARNING]
> **잔존 의존 2건** — 터치 초기화만 껐고 IQS323 직접 호출 경로는 남아 있다.
>
> - 절전 진입 루프([`main.c:1088~1156`](../../../../src/2__cm3/source/main.c))는 여전히 `tdc_touch_iqs323_read_status()` · `_re_ati()` · `_reseed()` 를 직접 호출한다 → **MCLR·Auto-ATI 를 거치지 않은 IQS323 을 읽게 되므로 절전 경로 동작은 보장되지 않는다.**
> - `power_button` 이 항상 `false` 이므로 `tdc_sys_control_step()` 의 전원 버튼 트리거(수동 전원 off 등)가 발생하지 않는다.
>
> [`main.c:590`](../../../../src/2__cm3/source/main.c) 의 주석("`tdc_touch_process()` 와 ... 은 내부에")은 원복 기준으로 쓰인 것이라 현재 코드와 불일치한다. 원복 시 함께 확인할 것.

### 2.3 ISD 연결 과정 (`tdc_isd_step()` FSM)

모든 초기화 함수 호출을 주석 처리하고, 그 자리에서 다음 상태로 즉시 전이시킨다. 결과적으로 **하드웨어 응답을 한 번도 확인하지 않고 링크 성립 상태까지 도달**한다.

| 단계(상태) | 위치 | 스킵된 함수 | 대체 동작 | 원래 동작 |
|---|---|---|---|---|
| `PowerIC_Reset` | [`tdc_isd.c:174~176`](../../../../src/2__cm3/source/isd/tdc_isd.c) | `tdc_isd_init_tx_power_ic()` | → `PowerIC_OK` 전이 | 링크 5V PMIC 를 최소 전압에서 최대 전압으로 단계적 증가 |
| `PowerIC_OK` | [`tdc_isd.c:182~184`](../../../../src/2__cm3/source/isd/tdc_isd.c) | `tdc_isd_init_fpga()` | → `FPGA_Ok` 전이 | §2.1 참조 |
| `FPGA_Ok` | [`tdc_isd.c:190~192`](../../../../src/2__cm3/source/isd/tdc_isd.c) | `tdc_isd_init_device()` | → `ISD_Power_Ok` 전이 | RF Tx(10MHz 캐리어 클럭)를 일정 기간 죽인 후 내부기 전송 시작, 내부기 칩 전원 레벨 판독 |
| `ISD_Power_Ok` | [`tdc_isd.c:207~219`](../../../../src/2__cm3/source/isd/tdc_isd.c) | `tdc_isd_path_open()` | → `ISD_pathOpen_Ok` 전이 + `tdc_shm_change_connected_isd_num_cfx(ManufacturingDefault_ISD_No)` (=1, [`processorDirective.h:169`](../../../../src/2__cm3/source/board/processorDirective.h)) + 사용자 설정 값 RTT 출력 유지 | 맵 파일 로드 → 맵 데이터 영역에서 공유 메모리로 해당 ISD 정보 복사 → CFX 가 처리하도록 연결 ISD 번호 갱신 |
| `ISD_pathOpen_Ok` | [`tdc_isd.c:226~228`](../../../../src/2__cm3/source/isd/tdc_isd.c) | `tdc_isd_enable_stimul_10v()` | → `stimul_10V_Ok` 전이 | 자극 출력용 내부기 칩 외부 10V ON + `VTG_LOCK_ENABLE` 설정 및 자극발생부 활성화 검증 |
| 라이브 구간 | [`tdc_isd.c:263~267`](../../../../src/2__cm3/source/isd/tdc_isd.c) | `tdc_isd_update_link_by_backtel_live()` | PCM fired 플래그만 클리어 (`BackelCircuitDisabled_readPcmFired_duringLiveStimulation`, [`tdc_isd_pcm.h:36`](../../../../src/2__cm3/source/isd/tdc_isd_pcm.h)) | 백텔 회로로 라이브 자극 중 링크 상태 갱신 · ISD 전원 레벨 판정 |

> [!NOTE]
> `ISD_Power_Ok` 단계의 RTT 출력(`[ISD] ISD ID MATCH NUM` · 사용자 설정 7종)은 `tdc_isd_path_open()` 내부에 있던 것을 밖으로 꺼내 유지했다. 측정 중에도 공유 메모리의 사용자 설정 값을 눈으로 확인하기 위함이다.

### 2.4 자극 파라미터 설정

| 항목 | 위치 | 변경 | 원래 동작 |
|---|---|---|---|
| PCM 자극 설정 | [`tdc_isd_stim_standalone.c:108~110`](../../../../src/2__cm3/source/isd/tdc_isd_stim_standalone.c) | `tdc_isd_stim_setting_step()` 주석 처리 → `done_setting_StimulPara_variable()` ([`tdc_isd_stim_para_setting.c:26`](../../../../src/2__cm3/source/isd/tdc_isd_stim_para_setting.c)) 직접 호출 | PCM 프로토콜로 내부기 칩 설정(모노폴라 · 바이폴라 등) 수행 후 완료 표시. 현재는 설정 없이 완료 표시만 |

### 2.5 CFX ADC (기 커밋)

| 항목 | 위치 | 변경 | 커밋 |
|---|---|---|---|
| ADC FBDAC DEM | [`lib_audio_in.h:105`](../../../../src/1__cfx/lib_cfx/lib_audio_in.h) | `LIB_ADC_CFG_VAL` 의 `ADC_FBDAC_DEM_ENABLE` → `ADC_FBDAC_DEM_DISABLE` | `8a353a0` (2026-07-22) |

## 3. 함께 들어간 비기능 변경

clang-format 재정렬이 다음 파일에 섞여 있다. 동작에는 영향이 없다.

- [`main.c`](../../../../src/2__cm3/source/main.c) — include 주석 정렬, 구조체 멤버 정렬, 함수 인자 들여쓰기
- [`tdc_isd.c`](../../../../src/2__cm3/source/isd/tdc_isd.c) — `[LINK] STATE` 삼항 연쇄 정렬 2곳, 주석 간격
- [`tdc_isd_stim_standalone.c`](../../../../src/2__cm3/source/isd/tdc_isd_stim_standalone.c) — 지역 변수 선언 정렬
- [`tdc_hal_dio.c`](../../../../src/2__cm3/source/hal/tdc_hal_dio.c) — 주석 처리된 `Sys_SPI_DIOConfig()` 라인의 주석 간격

## 4. 복원 방법

> [!IMPORTANT]
> 본 브랜치는 `claude_develop` 에 **병합하지 않는다.** 측정이 끝나면 브랜치를 그대로 두거나 폐기하고, `claude_develop` 에서 작업을 이어간다. 아래 절차는 이 브랜치 위에서 정상 동작 펌웨어가 필요할 때만 사용한다.

| 항목 | 복원 절차 |
|---|---|
| §2.1 FPGA 활성화 핀 | `Sys_GPIO_Set_Low` → `Sys_GPIO_Set_High` |
| §2.2 터치 초기화 | `tdc_sys_init.c:443` `#if 0` / `452` `#endif` 삭제 |
| §2.2 터치 폴링 | `ev->power_button = false;` → `ev->power_button = tdc_touch_process();` |
| §2.3 ISD 6개 지점 | 주석 처리된 원 함수 호출 복구 + 그 아래 추가된 `tdc_isd_change_state()` · `tdc_shm_change_connected_isd_num_cfx()` · `TDC_PRINTF_V` 블록 · PCM 플래그 클리어 블록 삭제 |
| §2.4 자극 파라미터 | `done_setting_StimulPara_variable();` 삭제 + `tdc_isd_stim_setting_step(startSettingTrigger);` 주석 해제 |
| §2.5 ADC DEM | `git revert 8a353a0` 또는 `ADC_FBDAC_DEM_DISABLE` → `ADC_FBDAC_DEM_ENABLE` |

전체를 한 번에 되돌리려면 `claude_main` 기준으로 새 브랜치를 따는 편이 확실하다. 본 브랜치의 제한 커밋만 골라 `git revert` 하는 것도 가능하다.

## 5. 검증 상태

| 항목 | 상태 |
|---|---|
| 사실 근거 | 전 항목 `파일:라인` 확인 완료 (`git diff` + 변경 후 파일 grep) |
| 빌드 | **미확인** — 본 문서는 은수님이 직접 적용한 변경의 사후 정리이며, 빌드·실기 확인은 측정 세션에서 수행 |
| 잔존 의존 | §2.2 경고 블록 2건 (절전 루프 IQS323 직접 호출 · 전원 버튼 무효화) |
