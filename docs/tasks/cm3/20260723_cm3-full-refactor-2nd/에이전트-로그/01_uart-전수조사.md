---
name: CM3 UART 관련 전 항목 전수 조사
purpose: CM3에서 UART를 완전히 제거하기 위한 사장 후보 전수 목록과 위험도·분류(제거/이관/존치) 판정
type: tasks/에이전트로그
applies_to: [Sound1]
tags: [cm3, dead-code, uart, 조사]
---

# 01_uart-전수조사

**TL;DR**: `hal/tdc_hal_uart.c/.h` 및 이를 `#include`하는 11개 파일을 전수 조사해 후보 20건을 도출했다. 안전 15건 · 주의 5건 · 위험 0건(단 FS_MEM_UART 관련 상수는 "이관" 대상으로 별도 취급, 02번 노드와 중복). 핵심 발견: UART 하드웨어 초기화 함수(`tdc_hal_uart_init`)가 애초에 존재하지 않고, 유일한 disable 경로(`tdc_hal_uart_uninit`)의 호출자 `tdc_sys_uninit()`조차 CM3 전체에서 호출처 0건이다. 물리적 UART TX/RX 핀(DIO20/21)은 이미 SPI CS·read-command 용으로 실사용 중이다.

## 1. 조사 범위와 방법

- 조사 파일: `hal/tdc_hal_uart.c`(88줄) · `hal/tdc_hal_uart.h`(140줄) 전문 통독
- `TDC_HAL_UART_*`, `FS_MEM_UART*`, `tdc_hal_uart_*`, `DIO_PIN_INDEX_forUART_*`, `TDC_PRINTF_INTERFACE*` 심볼을 `source/` 전역(153파일) grep
- `#include <tdc_hal_uart.h>`를 하는 파일 11개(자기 자신 포함) 전부를 열어 실제 참조 심볼 유무를 재확인 — grep 매치가 있어도 **살아있는 전처리 분기 안인지**를 개별 확인(제약 3)
- 호출 사슬 역추적: `tdc_hal_uart_uninit()` → `tdc_sys_init.c:193` → 그 함수 `tdc_sys_uninit()` 자체의 호출처를 전역 재검색
- 보드 정의 `Board_OTE_ver1_5.h`의 `#if 1 // Sullivan 1.5` 분기 활성 여부를 `board.h`·`processorDirective.h`와 대조해 확정
- 빌드 오버라이드 가능성 확인을 위해 `.cproject`/`Debug/makefile`에서 `TDC_PRINTF_INTERFACE` 재정의 여부 검색(없음 확인)

## 2. 사장 후보 목록

| # | 대상 | 위치 | 종류 | 근거 | 위험도 | 분류 |
|---|---|---|---|---|---|---|
| 후보_1 | `tdc_hal_uart_printf()` | `hal/tdc_hal_uart.c:34-88` | UART HW 함수 | 유일 호출 경로는 `util/tdc_printf.h:35`의 `#define TDC_PRINTF(...) tdc_hal_uart_printf(...)`인데, 이 라인은 `#if (TDC_PRINTF_INTERFACE == TDC_PRINTF_INTERFACE_UART)`(`tdc_printf.h:34`) 안에 있고 `TDC_PRINTF_INTERFACE`는 `tdc_printf.h:21`에서 `TDC_PRINTF_INTERFACE_SEGGER_RTT`로 고정. `.cproject`/`Debug/makefile`에 재정의 없음(빌드타임 오버라이드 불가 확인). 그 외 직접 호출 0건(전역 grep) | 안전 | 제거 |
| 후보_2 | `tdc_hal_uart_uninit()` | `hal/tdc_hal_uart.c:9-32` | UART HW 함수 | 유일 호출처 `sys/tdc_sys_init.c:193`(`tdc_sys_uninit()` 내부). `tdc_sys_uninit()` 자체는 `tdc_sys_init.h:20` 선언·`tdc_sys_init.c:150` 정의 외 **전역 어디에서도 호출되지 않음**(grep `tdc_sys_uninit(` 전체 결과 2건 = 선언+정의뿐). 즉 호출 사슬이 통째로 죽어있음(printf와 다른 유형: 전처리 소멸이 아니라 **호출자 자체의 무호출**) | 주의 (tdc_sys_uninit 자체의 존폐가 UART 범위 밖 판단이라 확정을 보류) | 제거 |
| 후보_3 | `TDC_HAL_UART_DIO_INIT_CFG` | `hal/tdc_hal_uart.h:21` | 매크로 | 정의만 있고 `tdc_hal_uart.c` 포함 전역 어디서도 미참조(대응하는 `_UNINIT_CFG`만 실제 쓰임) | 안전 | 제거 |
| 후보_4 | `TDC_HAL_UART_DIO_UNINIT_CFG`·`TDC_HAL_UART_DIO_TX`·`TDC_HAL_UART_DIO_RX` | `hal/tdc_hal_uart.h:22,24-25` | 매크로 | `tdc_hal_uart.c:26-27`(후보_2 내부, 죽음) 및 `hal/tdc_hal_dio.c:121-122`(후보_6, 역시 죽음)에서만 사용 | 주의 | 제거 |
| 후보_5 | `TDC_HAL_UART_TX_BUF_LEN` | `hal/tdc_hal_uart.h:27` | 매크로 | `tdc_hal_uart.c:7,52`(후보_1 내부)에서만 사용 → 후보_1과 동반 사장 | 안전 | 제거 |
| 후보_6 | `TDC_HAL_UART_BAUDRATE_115200`·`_921600`·`TDC_HAL_UART_BAUDRATE`·`TDC_HAL_UART_CONFIG` | `hal/tdc_hal_uart.h:29-35` | 매크로 | 정의만 있고 전역 어디서도 미참조. **UART 하드웨어를 실제로 초기화(`Sys_UART_Config` 호출)하는 함수 자체가 CM3 소스 전체에 존재하지 않음**(grep `tdc_hal_uart_init|Sys_UART` 0건) — 즉 baud/config 매크로는 애초에 쓰인 적이 없던 죽은 정의로 추정 | 안전 | 제거 |
| 후보_7 | `hal/tdc_hal_dio.c:121-122`의 `Sys_DIO_Config(TDC_HAL_UART_DIO_TX/RX, ...)` 2줄 | `hal/tdc_hal_dio.c:120-122` | UART DIO 처리 | 이 2줄이 속한 `tdc_hal_dio_configure_sleep()`의 유일 호출처는 `sys/tdc_sys_init.c:196`이며 그 호출부 역시 죽은 `tdc_sys_uninit()`(후보_2와 동일 함수) 내부. 함수 전체(`tdc_hal_dio_configure_sleep`)가 CM3에서 무호출 확인(grep 3건=선언·정의·죽은 호출부뿐) | 주의 (후보_2와 생사 연동) | 제거 |
| 후보_8 | `board/Board_OTE_ver1_5.h:68-69` `DIO_PIN_INDEX_forUART_TX`=DIO20, `_RX`=DIO21 | `board/Board_OTE_ver1_5.h:68-69` | 보드 DIO 정의 (활성 분기) | `#if 1 // Sullivan 1.5`(파일 25행, 리터럴 1이라 상시 참) 분기 안이라 **이 정의가 실제 컴파일됨**. 유일 소비처는 후보_4(TDC_HAL_UART_DIO_TX/RX)이며 그 매크로 자체가 사장(주의~안전). 또한 물리적으로 DIO20/21은 `board/board.h:23,27`에서 `NRF_SPI_CS_PIN`/`GPIO_PIN_ReadCommandForSPI_Master`로 **이미 재정의되어 SPI 통신용으로 실사용 중**(`hal/tdc_hal_dio.c:59-67` `tdc_hal_dio_configure_normal()`, 활성 - `tdc_sys_init.c:255`에서 호출) | 안전 (매크로 이름만 제거, 물리 핀은 SPI 이름으로 계속 존치) | 제거 |
| 후보_9 | `board/Board_OTE_ver1_5.h:118-119` `DIO_PIN_INDEX_forUART_TX`=DIO17(DMIC_CLK), `_RX`=DIO14(DMIC_OUT) | `board/Board_OTE_ver1_5.h:117-119` | 보드 DIO 정의 (죽은 분기) | `#else // Sound1 Test`(75행) 분기 — 68-69행이 `#if 1`로 항상 참이므로 **이 정의는 애초에 컴파일되지 않는다**. 은수님 지시서가 우려한 "DMIC 핀 공유" 리스크는 현재 빌드에서 발생하지 않음(추정 아님, `#if 1` 리터럴로 확정) | 안전(정의 자체는 죽어있음) — 단 `#else` 블록 전체(75-123행) 삭제는 UART 외 다른 보드 정의도 포함하는 더 큰 범위라 여기서는 UART 2줄만 후보로 한정 | 판단 보류(범위) |
| 후보_10 | `util/tdc_printf.h:15` `#include <tdc_hal_uart.h>`, `18` `TDC_PRINTF_INTERFACE_UART` 상수, `34-35` 죽은 분기 | `util/tdc_printf.h:15,18,34-35` | printf 인터페이스 선택 죽은 분기 | 후보_1 근거와 동일(TDC_PRINTF_INTERFACE 상시 SEGGER_RTT). `hw.h`는 `tdc_printf.h:13`에서 이미 직접 include하므로 tdc_hal_uart.h 제거해도 헤더 사슬 영향 없음 | 안전 | 제거 |
| 후보_11 | `main.c:41` `#include <tdc_hal_uart.h>` | `main.c:41` | 무의미 include | 파일 전체에서 `FS_MEM_UART`·`TDC_HAL_UART_*`·`tdc_hal_uart_*` 미참조(전역 grep 0건). `board.h`(8행)·`hw.h`(tdc_sys_init.h 경유)는 이미 별도 확보 | 안전 | 제거 |
| 후보_12 | `sys/tdc_sys_control.c:2` `#include <tdc_hal_uart.h>` | `sys/tdc_sys_control.c:2` | 무의미 include | 파일 내 UART 심볼 미참조. `DIO_NUM_NRF_ON_OFF_COMMAND`·`ENABLE_NRF_ADV_LowPower` 참조는 전부 주석 처리(40,45행) 또는 `#if 0`(113-121행) 안이라 board.h 필요성 자체가 없음. `hw.h`는 파일 자체가 직접 include(3행 상당) | 안전 | 제거 |
| 후보_13 | `hal/tdc_hal_spi.c:13` `#include <tdc_hal_uart.h>` | `hal/tdc_hal_spi.c:13` | 무의미 include | 파일 내 UART 심볼 미참조. `board.h`(11행)·`processorDirective.h`(8행) 이미 직접 include | 안전 | 제거 |
| 후보_14 | `fs/tdc_fs.h:25` `#include <tdc_hal_uart.h>` | `fs/tdc_fs.h:25` | 무의미 include | 파일 내 UART 심볼 미참조. `board.h`/`hw.h`/`tdc_stim_definitions.h`는 같은 파일 20행 `#include <tdc_shm.h>`(`cfx_link/tdc_shm.h:19-26`이 hw.h·board.h·tdc_stim_definitions.h 직접 포함)를 통해 이미 확보되어 있어 간접 include 상실 위험 없음 | 안전 | 제거 |
| 후보_15 | `util/tdc_util.h:18` + `util/tdc_util.c:6` `#include <tdc_hal_uart.h>` (중복 2건) | `util/tdc_util.h:18`, `util/tdc_util.c:6` | 무의미 include (중복) | 두 파일 모두 UART 심볼 미참조. `board.h`(util/tdc_util.h:15)·`hw.h`(13행) 이미 같은 헤더가 직접 include — 애초에 tdc_hal_uart.h가 필요했던 적이 없어 보임 | 안전 | 제거 |
| 후보_16 | `ble/tdc_ble_communication.h:4` `#include <tdc_hal_uart.h>` | `ble/tdc_ble_communication.h:4` | 무의미 include | `.h`·`.c`(`ble/tdc_ble_communication.c`) 모두 UART 심볼 미참조. 다만 이 `.h` 자체는 `board.h`/`hw.h`를 다른 경로로 직접 끌어오지 않음 — `.c`가 `tdc_shm.h`(18행)를 통해 간접 확보하므로 실사용상 안전하나, **이 헤더만 단독으로 다른 파일에 include될 경우**의 전이 의존성은 컴파일 검증을 권장 | 주의 | 제거 |
| 후보_17 | `hal/tdc_hal_dio.h:18` `#include <tdc_hal_uart.h>` | `hal/tdc_hal_dio.h:18` | include (후보_7과 연동) | `tdc_hal_dio.c`가 `TDC_HAL_UART_DIO_TX/RX`(후보_7, 죽은 `configure_sleep()` 안)를 쓰기 위한 유일한 이유. 후보_7 정리 후 이 include도 제거 가능 | 주의 (후보_7 생사 연동) | 제거 |
| 후보_18 | `DebugMode_t`·`debug_agc_t` typedef | `hal/tdc_hal_uart.h:48-79` | 타입 정의 | 유일 참조처가 `FS_MEM_UART_T`의 `#if 0` 블록(130-131행) 안. 즉 현재 컴파일되는 `FS_MEM_UART_T` 실제 레이아웃(`state`·`flag[32]`·`buffer[512]`)에는 전혀 관여하지 않는 완전 사장 타입 — **grep 함정 주의사항의 전형 사례**(1차 조사에서 놓칠 뻔한 유형) | 안전 (FS_MEM_UART_T 레이아웃 불변경 확인) | 제거 |
| 후보_19 | `FS_MEM_UART_BUF_LEN` | `hal/tdc_hal_uart.h:37` | 매크로 | 정의만 있고 전역 미참조. `FS_MEM_UART_T.buffer[512]`(85행)는 이 매크로(256) 대신 하드코딩된 512를 씀 — 애초에 서로 무관 | 안전 | 제거 |
| 후보_20 | `FS_MEM_UART_T`의 `#if 0` 블록(`enableCFX` 이하 전체) | `hal/tdc_hal_uart.h:86-132` | 죽은 전처리 블록 | 이미 `#if 0`로 비활성. 구조체 실레이아웃(`state`,`flag[32]`,`buffer[512]`)에 영향 없이 제거 가능 | 안전 | 제거 |
| 후보_21 | `board/DIO_PIN_Config.h:16,88` `UART_DIO_PIN_CFG` | `board/DIO_PIN_Config.h:16,88` | 매크로 (양쪽 보드분기 모두) | 두 분기 모두 정의하지만 전역 어디서도 미참조(grep 결과 정의 2건뿐) | 안전 | 제거 |
| 후보_22 | `board/processorDirective.h:117-137`의 `AudioInputSignal_Tx_usingUART`(`#if 0`로 이미 죽음)·`CFX_UART_USING_ISR`·`CFX_UART_BAUD_RATE`·`CFX_UART_RX_ENABLE`·`UART_bufferLength_forAudio` | `board/processorDirective.h:117-137` | 전처리 매크로군 | CM3 소스(`source/` 전역) 어디에서도 미참조. 이름상 CFX(1__cfx) 전용으로 추정되나 **본 조사는 2__cm3 범위 한정이라 CFX 측 실제 참조 여부·이 파일이 CFX와 별도 사본인지는 미확인** | 주의 (CFX 측 확인 필요, 범위 밖) | 판단 보류 |

## 3. 사장 아님으로 판정한 것 (오탐 방지 기록)

| 대상 | 위치 | 사장 아님 판정 근거 |
|---|---|---|
| `FS_MEM_UART` / `FS_MEM_UART_T`의 `state`·`flag[32]`·`buffer[512]` 실레이아웃 | `hal/tdc_hal_uart.h:81-85,138` | 과제 불가침 지정 항목. `cfx_link/tdc_shm.c:283` `FS_MEM_UART->flag[0] = 1;`이 `#if 1`(281행) 안에서 **현재도 실행됨** — CM3→CFX 디버그 신호로 활성 사용 중. 헤더 위치만 `tdc_hal_uart.h`에서 CFX_link 쪽으로 **이관**해야 하며(02번 노드 담당), 삭제 대상이 아님 |
| `FS_MEM_UART_STATE_RESET`·`_IDLE`·`_CM3` | `hal/tdc_hal_uart.h:39,41-42` | `tdc_hal_uart.c` 내부(후보_1·2)에서 세마포어 프로토콜로 사용됨. CM3 측 사용은 후보_1·2와 함께 사장이지만, **CFX 측이 동일 상수값(0x22/0x33/0x44 등)으로 이 필드를 참조할 가능성을 배제할 수 없어** 상수 자체(정수값)는 존치 판단 — 이관 설계(02번 노드)에서 최종 결정 필요 |
| `FS_MEM_UART_STATE_INIT`(0x11)·`FS_MEM_UART_STATE_CFX`(0x44)·`FS_MEM_DISABLE_CFX`·`FS_MEM_ENABLE_CFX` | `hal/tdc_hal_uart.h:40,43,45-46` | CM3에서는 전역 미참조이나, 명칭상 CFX 측이 `state`를 CFX 값으로 설정하는 상대편 프로토콜로 추정 — CM3 단독 판단으로 안전 단정 불가 (판단 보류, 02번 노드로 이관) |
| `cfx_cm3_sharedMemoryAll` 구조체 | (참조 없음, UART와 무관) | 과제 불가침 지정. 이번 조사 대상 아님(FS_MEM_UART와는 별개 구조체) |
| `*_IRQHandler` 8종 | 전역 | UART 전용 IRQ 핸들러(`UART_IRQHandler` 등)는 CM3에 아예 존재하지 않음(grep 0건) — 애초에 UART 인터럽트를 CM3에서 벡터로 받은 적이 없음. 기존 8종 핸들러는 UART와 무관하므로 판정 대상 자체가 아님 |
| SDK `UART->`, `DIO->SRC_UART[0]` 레지스터 접근 | `hal/tdc_hal_uart.c:12,17-18,21,44,60,65,70,76,78` | SDK(`hw.h`) 제공 레지스터 구조체 자체는 불가침. 단 이를 사용하는 **호출부**(후보_1·2)는 사장 — SDK 정의 자체는 제거 대상 아님, `tdc_hal_uart.c` 삭제 시 자연히 미참조로 남을 뿐 |
| `board/Board_OTE_ver1_5.h:68-69`가 정의하는 물리 핀 DIO20/DIO21 자체 | `board/board.h:23,27` | 핀 자체는 `NRF_SPI_CS_PIN`/`GPIO_PIN_ReadCommandForSPI_Master`로 **활성** 사용 중(SPI). "UART"라는 이름만 사장이며 핀 사용 자체는 존치 대상 |

## 4. 판단 보류 · 추가 확인 필요

1. **`tdc_sys_uninit()` 자체의 존폐** (`sys/tdc_sys_init.c:150`, `sys/tdc_sys_init.h:20`) — CM3 전체에서 호출처 0건 확인(grep). UART 범위를 넘는 더 큰 사장 후보이나, 후보_2·7(UART 관련 호출)이 바로 이 함수 안에 있어 UART 제거 계획에 직접 영향을 준다. **이 함수를 통째로 제거할지, UART 관련 2줄(193행 `tdc_hal_uart_uninit()`, 196행이 부르는 `tdc_hal_dio_configure_sleep()` 안의 UART 2줄)만 들어낼지 은수님 판단 필요**. 다른 노드(예: 08/09번, 횡단 사장조사)와 판정이 겹칠 가능성이 있어 교차 확인 권장
2. **`FS_MEM_UART_STATE_INIT`/`_CFX`/`FS_MEM_DISABLE_CFX`/`FS_MEM_ENABLE_CFX` 상수의 CFX 측 실사용 여부** — 2__cm3 범위 조사로는 확인 불가. 02번 노드(FS_MEM_UART 이관 설계) 또는 1__cfx 소스 직접 확인 필요
3. **`board/processorDirective.h`의 `CFX_UART_*`/`AudioInputSignal_Tx_usingUART`/`UART_bufferLength_forAudio`** — CM3에서는 완전 미참조이나 이 파일이 CFX와 공유/동기화되는 파일인지, 아니면 CM3 전용 독립 사본(단순 복사 잔재)인지 미확인. 독립 사본이면 이 5개 매크로는 CM3 사본에서 안전하게 제거 가능한 순수 잔재
4. **부트로더의 UART 활성 상태가 CM3 진입 후 실제로 언제 해제되는지** — `tdc_hal_dio_configure_normal()`(`hal/tdc_hal_dio.c:20-80`, `tdc_sys_init()` 최초 단계에서 호출)이 DIO20/21을 SPI CS/GPIO 출력 모드로 즉시 재설정하므로 전기적으로는 UART 기능이 곧바로 무력화되는 것으로 보이나(추정), **UART 페리페럴 자체의 클럭/인에이블 비트가 이 재설정 이전 극히 짧은 구간 동안 전류를 더 소모하는지는 하드웨어 실측이 필요**(정적 코드 분석 범위 밖)

## 5. 특이사항

- **핵심 발견**: CM3 소스 전체에 `tdc_hal_uart_init()`이나 `Sys_UART_Config()` 호출이 **단 한 건도 없다**. 즉 CM3 앱 코드는 UART 페리페럴을 능동적으로 켜는 코드를 가진 적이 없고, disable(`tdc_hal_uart_uninit`)과 write(`tdc_hal_uart_printf`)만 존재한다. 이는 은수님이 제시한 원 사례(`TDC_PRINTF_INTERFACE` 전처리 소멸)보다 한 겹 더 깊은 사장 패턴 — **호출자 자체가 무호출**이라 grep 1회로는 "사용 중"으로 보이는 전형적 함정이었다(제약 3에서 예고된 유형과 정확히 일치)
- **보드 분기 확정**: `board/Board_OTE_ver1_5.h`의 보드 선택은 매크로(`Board_is_OTE_VER_1_5`) 조건부가 아니라 파일 내부의 `#if 1`(25행) 리터럴로 고정되어 있어, "Sound1 Test"(`#else`, 75행) 분기는 소스 수정 없이는 영원히 죽어있다. 은수님이 우려하신 DMIC 핀(DIO17/DIO14) 공유 리스크는 **현재 빌드에서는 발생하지 않음**을 확정할 수 있었다(파일:라인 기반, 추정 아님)
- **`FS_MEM_UART` 세마포어 프로토콜의 실질적 우회 발견**: 유일하게 활성인 `FS_MEM_UART` 쓰기(`cfx_link/tdc_shm.c:283`)는 `state` 필드 기반 CM3/CFX 핸드셰이크(`FS_MEM_UART_STATE_IDLE` 대기 → `_CM3`로 점유 → `_IDLE`로 반환, `tdc_hal_uart.c:39-46,85`)를 전혀 거치지 않고 `flag[0]`을 직접 씀. 즉 CM3 쪽의 세마포어 참여자는 사실상 이미 없다 — 이 부분은 02번 노드(FS_MEM_UART 이관 설계)에 중요한 참고가 될 것으로 판단되어 기록해둔다
- **DIO20/DIO21 이름 충돌**: `Board_OTE_ver1_5.h:68-69`가 "UART"로 이름 붙인 DIO20/DIO21은 `board/board.h:23,27`에서 각각 `NRF_SPI_CS_PIN`(QCC_SPI_CS)·`GPIO_PIN_ReadCommandForSPI_Master`(QCC_SPI_FLAG)로 재정의되어 **실제로는 SPI 용도로 이미 쓰이고 있다**. `tdc_hal_dio_configure_normal()`(활성, `tdc_sys_init()` 최초 단계)이 부팅 즉시 이 두 핀을 GPIO 출력으로 설정하므로, UART 이름의 매크로/정의를 제거해도 물리 핀 동작에는 영향이 없다
