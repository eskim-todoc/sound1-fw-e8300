---
name: FS_MEM_UART 의 cfx_link 이관 설계
purpose: FS_MEM_UART 전 참조처 조사, CFX/calibration 레이아웃 정합 실증, cfx_link 이관 설계안 제시
type: tasks/에이전트로그
applies_to: [Sound1]
tags: [cm3, dead-code, 조사, cfx_link, ABI]
---

# 02_fs-mem-uart-이관설계

**TL;DR**: `FS_MEM_UART`(3필드: state/flag[32]/buffer[512])는 CM3-CFX 간 유일 활성 경로(`cfx_link/tdc_shm.c:283` -> `1__cfx/signalProcessing/nonlinearMapping.c:465,602`)를 갖는 살아있는 공유메모리 ABI로, 레이아웃이 CFX 사본과 정확히 일치함을 실증했다. calibration 사본은 없음. 이 ABI 자체와 밀접한 주변부에서 사장 후보 8건(안전 7 · 주의 1 · 위험 0)을 찾았고, 새 파일 `cfx_link/tdc_shm_debug.h` 로의 이관 설계안을 제시한다.

## 1. 조사 범위와 방법

- 대상: `2__cm3/source` 전체에서 `FS_MEM_UART` · `FS_MEM_UART_T` · `FS_MEM_UART_STATE_*` · `FS_MEM_UART_BUF_LEN` · `FS_MEM_DISABLE_CFX` · `FS_MEM_ENABLE_CFX` · `FS_MEM_DEBUG_AGC_*` · `DebugMode_t` · `debug_agc_t` 전 참조처를 `Grep` 전수 검색.
- CFX 측 대조군: `1__cfx/OTE_1_5_gen/OTE_1_5_gen_UART.h`, `1__cfx/signalProcessing/{nonlinearMapping.c,stimulationStrategy.c}` 를 직접 `Read` 하여 실제 활성 전처리 분기 여부를 눈으로 확인 (grep 함정 회피).
- calibration 측: `5__calibration` 전체를 동일 패턴으로 검색 (사본 없음 확인).
- 주소 정합: `DSP_PRAM1_REMAP_BASE`(CM3) / `D_DSP_PRAM1_BASE`(CFX) 정의처를 저장소 전체에서 검색. 빌드 산출물(`2__cm3/Debug/2__cm3.map`, `1__cfx/Debug/1__cfx.elf.map`)에서 리터럴 심볼 검색도 시도했으나, 두 매크로 모두 소스에 값이 없고(외부 SDK 헤더 제공, 저장소 미포함) `objdump`/`readelf` 류 도구가 이 환경에 없어 16진수 실측은 **불가** — 아래 2절에서 근거 기반 정황 대조로 대체했다.
- 함정 회피: 발견된 모든 `FS_MEM_UART` 호출부는 감싸는 `#if` 를 직접 읽어 활성/비활성 분기를 판정했다 (예: `nonlinearMapping.c:483-597` 는 `#if 0`, `:462`/`:599` 는 `#if 1`).

## 2. 사장 후보 목록

| # | 대상 | 위치 | 종류 | 근거 | 위험도 |
|---|---|---|---|---|---|
| 후보_1 | `FS_MEM_UART_STATE_INIT`(0x11), `FS_MEM_UART_STATE_CFX`(0x44) | `hal/tdc_hal_uart.h:40,43` | 매크로(열거값) | 정의 외 CM3·CFX 전체 0건 참조. 실사용은 `STATE_RESET/IDLE/CM3` 3종뿐(`hal/tdc_hal_uart.c:29,39,42,46,85`) | 안전 |
| 후보_2 | `FS_MEM_UART_BUF_LEN` (256) | `hal/tdc_hal_uart.h:37` | 매크로 | 정의 외 0건 참조. 실제 `buffer[512]`(hal/tdc_hal_uart.h:85) 와 크기 자체가 불일치 — 동기화된 적 없는 방치 흔적 | 안전 |
| 후보_3 | `FS_MEM_DISABLE_CFX`(0x00), `FS_MEM_ENABLE_CFX`(0xAA) | `hal/tdc_hal_uart.h:45-46` | 매크로 | CM3·CFX 전체 0건 참조. 연관 필드 `enableCFX` 도 CM3 `#if 0` 블록(:87) 안에만 존재, CFX 사본엔 애초에 없음 | 안전 |
| 후보_4 | `DebugMode_t` 구조체(:48-62), `debug_agc_t` 구조체(:70-79), `FS_MEM_DEBUG_AGC_STATE_*` 5종(:64-68) | `hal/tdc_hal_uart.h:48-79` | 타입/매크로 | 유일 사용처가 `hal/tdc_hal_uart.h:130-131` 인데 그 자체가 `#if 0`(86-132) 안. CFX `FS_MEM_UART_T`(`OTE_1_5_gen_UART.h:17-22`)는 애초 3필드뿐 — 대응 필드 자체가 CFX 에 존재한 적 없음 | 안전 |
| 후보_5 | `FS_MEM_UART_T` 내부 `#if 0` 확장 필드 19종 (`enableCFX`, `FIFO_A0_0`, `inputAudio_Mix`, `freqBandOrder`, `electrodIndex`, `stimulusLevel`, `agc*` 관련 10필드, `vMag`, `channelRepresentiveValue`, `test_cfx_*` 등) | `hal/tdc_hal_uart.h:86-132` | 죽은 구조체 필드(전처리 비활성) | CM3 쪽은 `#if 0`. CFX 쪽 대응 참조(`1__cfx/signalProcessing/stimulationStrategy.c:604-621`, `freqBandOrder`/`electrodIndex`/`stimulusLevel` 사용)도 동일하게 `#if 0`. 즉 양쪽 모두 죽어있어 레이아웃 불일치 리스크 없이 안전하게 버릴 수 있음 | 안전 |
| 후보_6 | `tdc_hal_uart_printf()` 함수 전체 | `hal/tdc_hal_uart.c:34-88` (선언 `hal/tdc_hal_uart.h:136`) | 함수 | 유일 호출 경로는 `TDC_PRINTF` 매크로(`util/tdc_printf.h:35`, `#if TDC_PRINTF_INTERFACE==TDC_PRINTF_INTERFACE_UART`)인데 `tdc_printf.h:21` 이 `TDC_PRINTF_INTERFACE_SEGGER_RTT` 로 고정되어 이 분기는 항상 거짓. 직접 호출도 전무(grep 확인). 은수님이 예로 든 사례와 정확히 동일 패턴 | 주의 (제거 시 `FS_MEM_UART_STATE_IDLE/CM3` 도 연쇄로 고아가 됨 — 4절 참고. UART 담당 노드(01)와 중복 가능성 있어 교차 확인 권고) |
| 후보_7 | `TDC_HAL_UART_BAUDRATE_115200/921600/BAUDRATE`, `TDC_HAL_UART_CONFIG`, `TDC_HAL_UART_DIO_INIT_CFG` | `hal/tdc_hal_uart.h:21,29-35` | 매크로 | 정의 외 0건 참조. 이 코드베이스엔 UART 페리퍼럴 `init` 함수 자체가 없음(`uninit`/`printf` 만 존재) — `UART->CTRL = UART_ENABLE` 류 활성화 코드가 전무함을 확인(전수 grep) | 안전 (FS_MEM_UART 범위 밖 부수 발견, UART 전담 노드(01)와 중복 가능) |
| 후보_8 | `tdc_hal_uart.h` 를 include 하지만 그 안의 어떤 심볼도 쓰지 않는 6개 파일 | `util/tdc_util.h:18`, `util/tdc_util.c:6`, `sys/tdc_sys_control.c:2`, `hal/tdc_hal_spi.c:13`, `main.c:41`, `ble/tdc_ble_communication.h:4`, `fs/tdc_fs.h:25` | include 구문 | 각 파일 전체를 `FS_MEM_UART*`/`TDC_HAL_UART_*`/`tdc_hal_uart_*` 패턴으로 재검색 — include 줄 자체 외 0건 | 안전 (지금 지워도 무해하나, 이번 이관 범위 밖이므로 후속 과제로 남김. `tdc_hal_uart.h` 완전 삭제 시 컴파일 에러 방지를 위해 사전 정리 권장) |

## 3. 사장 아님으로 판정한 것 (오탐 방지 기록)

| 대상 | 위치 | 사장 아닌 이유 |
|---|---|---|
| `FS_MEM_UART_T` (3필드: state/flag/buffer), `FS_MEM_UART` 매크로 | `hal/tdc_hal_uart.h:81-85,138` | **불가침 지정 항목.** 유일 활성 경로 실증: `cfx_link/tdc_shm.c:283`(`#if 1`, `flag[0]=1`) -> `1__cfx/signalProcessing/nonlinearMapping.c:465`(`flag[0]==1` 읽고 `buffer[0..223]` 채움, `#if 1`) -> 같은 파일 `:602`(`flag[0]=2`, `#if 1`). 3구간 모두 살아있는 분기 안임을 직접 확인 |
| `FS_MEM_UART_STATE_RESET/IDLE/CM3` | `hal/tdc_hal_uart.h:39,41,42` | `hal/tdc_hal_uart.c:29,39,42,46,85` 에서 실사용. `tdc_hal_uart_uninit()` 는 `sys/tdc_sys_init.c:193` 에서 살아있는 슬립 진입 시퀀스로 호출됨 |
| `tdc_hal_uart_uninit()` 함수 | `hal/tdc_hal_uart.c:9-32` | `sys/tdc_sys_init.c:193` 에서 호출(슬립 진입 직전 SPI/DMA/I2C 비활성화와 같은 대열). 내부에서 `Sys_DIO_Config(TX/RX, UNINIT_CFG)` 로 실제 물리 핀을 안전 상태로 되돌리는 하드웨어 부수효과가 있음 — UART 페리퍼럴 자체는 한 번도 활성화된 적이 없지만(4절 참고), DIO 핀 재구성은 실질적 전력/누설 관리 동작이라 사장 코드로 볼 수 없음 |
| `TDC_HAL_UART_DIO_TX`, `TDC_HAL_UART_DIO_RX` | `hal/tdc_hal_uart.h:24-25` | `hal/tdc_hal_dio.c:121-122` (슬립 진입 시 DIO 리셋 루틴)에서 실사용 |
| CFX `FS_MEM_UART_T` (`OTE_1_5_gen_UART.h:17-22`) | 1__cfx 사본 | 조사 대상 밖(2__cm3 전용 임무)이나, 레이아웃 대조를 위해 확인 — CM3 활성 3필드와 이름·개수·순서 정확히 일치 |
| `cfx_cm3_sharedMemoryAll`, `*_IRQHandler`, SDK `Sys_*`/`hw.h`, `isd/tdc_isd_map_*`, `lib/` 원본 API, `sections.ld` | 전역 | 프롬프트 지정 불가침 항목 — 이번 조사에서 직접 만난 사례 없음(단, `cfx_cm3_sharedMemoryAll` 은 2절 주소 대조에서 간접 언급) |

## 4. 판단 보류 · 추가 확인 필요

1. **`DSP_PRAM1_REMAP_BASE` / `D_DSP_PRAM1_BASE` 의 실제 16진수 값** — 둘 다 저장소에 값 정의가 없다(각각 CM3 SDK `hw.h`, CFX/chess 컴파일러 SDK 헤더 제공으로 추정, 둘 다 불가침 SDK 계열). 정황 근거는 확보했으나(아래) 리터럴 주소 자체의 실측은 이 환경에 `objdump`/`readelf` 가 없어 **불가**. 배포 전 실제 빌드의 `.map`/디버거로 두 값이 동일 물리 뱅크를 가리키는지 최종 확인 권고.
   - `cfx_link/tdc_shm_addr.h:22` 주석: `DSP_PRAM5_REMAP_BASE` 는 `cfx_cm3_sharedMemoryAll`(주 ABI)이 쓰는 뱅크이며 "Cortex-M3 Remapping 주소는 0x70000" 이라고 명시 — `FS_MEM_UART` 가 쓰는 `DSP_PRAM1_REMAP_BASE` 는 이와 **다른 뱅크**(PRAM1 vs PRAM5)임이 명명 자체로 확인됨.
   - `0__bootloader/source/bootloader.c:164` 가 `DSP_PRAM5_POWER_ENABLE | DSP_PRAM4_POWER_ENABLE | DSP_PRAM3_POWER_ENABLE | DSP_PRAM2_POWER_ENABLE | DSP_PRAM1_POWER_ENABLE | DSP_PRAM0_POWER_ENABLE` 를 한 묶음으로 전원 활성화 — PRAM1 뱅크가 시스템 설계상 의도적으로 전원 공급되는 뱅크임을 뒷받침(우연한 잔재 주소가 아님).
   - 같은 물리 뱅크를 다른 용도로 또 쓰는 충돌은 없음: CFX 쪽 다른 FS_MEM 계열은 `D_DSP_PRAM3_BASE`(`1__cfx/OTE_1_5_gen/OTE_1_5_gen_FS_MEM.h:10`), `D_DSP_PRAM4_BASE`(:14,18) 를 쓰고 PRAM1 은 오직 `FS_MEM_UART` 전용.
2. **LPDSP32(CFX) `int` 워드 폭이 CM3 `int`(32비트)와 완전히 같은지** — 필드 이름·개수·선언 순서 일치는 소스 대조로 실증했으나, chess 컴파일러의 `chess_storage(IOMEM)` 공간에서 `int` 가 정확히 32비트로 정렬되는지는 SDK/툴체인 문서 영역이라 소스만으로 최종 확정 불가. 다만 이 채널이 이미 실기에서 동작 중인 이력(주석·코드 흔적)이 있어 **위험도 자체는 낮게 평가**함.
3. **`tdc_hal_uart_printf()` 삭제 여부는 UART 전담 노드(01번)와 판정이 겹칠 수 있음** — 이번 노드는 FS_MEM_UART ABI 관점에서만 확인했고, 최종 삭제 결정은 01번 로그와 교차 검토 후 오케스트레이터가 조율 권고.

## 5. 특이사항 — 이관 설계안

### 5.1 새 파일 위치·이름

- **`cfx_link/tdc_shm_debug.h`** (헤더만, `.c` 불필요 — 함수 없이 구조체/매크로 선언뿐)
- 근거: 기존 `cfx_link/tdc_shm_addr.h`(주소 매크로 전용 헤더) 와 동일한 "`tdc_shm_` + 역할명" 명명 패턴을 따름. `tdc_` 규약(파일명)과 헤더명 전역 유일성을 만족(저장소 전체에 `tdc_shm_debug` 문자열 0건, 충돌 없음 확인). `uart` 라는 이름을 배제해 "이름만 UART" 오해를 원천 차단.

### 5.2 이관 범위 — 가져갈 것 / 버릴 것

| 항목 | 처리 | 사유 |
|---|---|---|
| `FS_MEM_UART_T`(3필드), `FS_MEM_UART` 매크로 | **이관** (이름 그대로 유지) | 활성 ABI 본체 |
| `FS_MEM_UART_STATE_RESET/IDLE/CM3` | **이관** | `hal/tdc_hal_uart.c` 가 실사용 중이므로 새 헤더가 정의를 제공해야 함 |
| `FS_MEM_UART_STATE_INIT/CFX` (후보_1) | **버림 권고** (0건 참조) | 굳이 완결성 위해 남기고 싶다면 무해하니 은수님 재량 |
| `FS_MEM_UART_BUF_LEN`(후보_2), `FS_MEM_DISABLE_CFX/ENABLE_CFX`(후보_3) | **버림** | 0건 참조, 특히 BUF_LEN 은 실제 크기와 불일치해 오히려 혼란 유발 |
| `DebugMode_t`/`debug_agc_t`/`FS_MEM_DEBUG_AGC_STATE_*`(후보_4), `#if 0` 확장 필드 19종(후보_5) | **버림** | CM3·CFX 양쪽 모두 죽어있어 옮길 실익 없음. 옮기면 "이 헤더도 확장 가능해 보인다"는 잘못된 신호만 남김 |

결과적으로 `tdc_shm_debug.h` 는 구조체 1개(3필드) + 매크로 4개(`FS_MEM_UART`, `STATE_RESET/IDLE/CM3`) 만 남는 슬림한 헤더가 된다.

### 5.3 이름 변경 가능 여부 (ABI 관점)

- **필드 오프셋·개수·순서는 절대 불변** — `state`/`flag[32]`/`buffer[512]` 그대로.
- **필드/타입/매크로 "이름"은 컴파일러 관점에선 자유**(오프셋에 영향 없음)이나, `cfx_link/tdc_shm.h:1-14` 가 이미 채택한 선례(파일은 `tdc_` 접두이지만 내부 ABI 타입명은 CFX 대조 편의를 위해 원명 유지)를 그대로 따를 것을 권고. 즉 파일명(`tdc_shm_debug.h`)만 `tdc_` 규약을 따르고, **`FS_MEM_UART_T`/`FS_MEM_UART` 라는 타입·매크로 이름 자체는 바꾸지 않는다.** 굳이 바꾸려면 CFX(`1__cfx/OTE_1_5_gen/OTE_1_5_gen_UART.h`) 헤더도 동시에 갱신하는 별도 작업으로 진행.

### 5.4 헤더 상단 경고 주석 초안

`cfx_link/tdc_shm.h:1-14` 와 같은 톤으로 작성:

```c
/* ============================================================================
 * [공유 ABI - 단독 rename/제거 금지] (2026-07-23, FS_MEM_UART cfx_link 이관, cm3 2차 리팩토링)
 *
 * 이름은 UART 이지만 실제 UART 하드웨어와 무관하다.
 * CM3 DSP_PRAM1_REMAP_BASE 와 CFX D_DSP_PRAM1_BASE 가 같은 물리 뱅크(LPDSP32 PRAM1)를
 * 가리키는 CM3<->CFX 공유메모리이며, 로그매핑 계수 디버그 덤프 채널로 쓰인다.
 * (같은 성격의 주 ABI 는 tdc_shm.h 의 cfx_cm3_sharedMemoryAll 을 볼 것 - 이건 별도 뱅크의 별도 채널)
 *
 * 유일하게 살아있는 경로:
 *   CM3  cfx_link/tdc_shm.c            : 계수 계산 완료 -> flag[0] = 1
 *   CFX  signalProcessing/nonlinearMapping.c : flag[0]==1 이면 buffer[]에 계수를 채우고 flag[0] = 2
 *
 *  - 필드 제거·추가·순서 변경 = 레이아웃(ABI) 파괴 -> CFX 오동작
 *  - CM3 단독 rename = CFX(1__cfx/OTE_1_5_gen/OTE_1_5_gen_UART.h) 와 소스 불일치
 *  - calibration 쪽엔 이 채널의 사본이 없음(2026-07-23 확인) - CFX 헤더 하나만 대조 대상
 *
 * 변경이 필요하면 CM3·CFX 두 헤더를 동시에 바꾸는 별도 작업으로 진행할 것.
 * 근거: docs/tasks/cm3/20260723_cm3-full-refactor-2nd/에이전트-로그/02_fs-mem-uart-이관설계.md
 * ========================================================================== */
```

### 5.5 이관 후 include 수정이 필요한 파일

| 파일 | 현재 상태 | 필요한 변경 |
|---|---|---|
| `cfx_link/tdc_shm.c` | `FS_MEM_UART` 를 **직접 include 없이** 사용 중 — `tdc_shm.h:29`(`#include <tdc_printf.h>`) -> `tdc_printf.h:15`(`#include <tdc_hal_uart.h>`) 전이(transitive) 경로로만 노출되던 상태 | `#include <tdc_shm_debug.h>` 를 **명시적으로 추가**. 지금처럼 우연한 전이 include 에 의존하면 `tdc_hal_uart.h` 슬림화 순간 조용히 깨질 위험(5.6 참고) |
| `hal/tdc_hal_uart.c` | `tdc_hal_uart.h` 를 통해 `FS_MEM_UART`/`STATE_*` 획득 | `#include <tdc_shm_debug.h>` 추가 (직접 사용하므로) |
| `hal/tdc_hal_uart.h` | `FS_MEM_UART_T` 등 정의 보유(:37-133) | 해당 정의 전부 삭제. DIO 매크로(`TDC_HAL_UART_DIO_*`)와 함수 선언(`tdc_hal_uart_uninit`/`_printf`)만 남김 |
| 그 외 8개 파일(후보_8: `tdc_util.*`, `tdc_sys_control.c`, `tdc_hal_spi.c`, `main.c`, `tdc_ble_communication.h`, `tdc_fs.h`) | `tdc_hal_uart.h` 를 include 하지만 `FS_MEM_UART` 미사용 | **변경 불필요** — 이 파일들은 FS_MEM_UART 이관과 무관. `tdc_hal_uart.h` 자체가 존재하는 한 그대로 컴파일됨(후속 과제로 정리 권고, 4절/후보_8 참고) |

### 5.6 위험 요소와 검증 방법

1. **전이 include 손실**: `cfx_link/tdc_shm.c` 가 현재 우연한 3단 전이(`tdc_shm.h`→`tdc_printf.h`→`tdc_hal_uart.h`)로 `FS_MEM_UART` 를 얻고 있다. 이관 시 `tdc_hal_uart.h` 에서 해당 정의를 빼면 이 경로가 끊긴다. **검증**: 이관 직후 clean build — 정의가 통째로 빠지므로 링크가 아닌 컴파일 단계에서 `FS_MEM_UART` 미선언 에러로 즉시 드러나는 "안전한 실패 모드"다(조용히 깨지지 않음). 5.5 표의 명시적 include 추가로 사전 예방.
2. **버림 대상(후보_1~5)의 은닉 사용처 누락 가능성**: 이번 조사가 grep 전수 + 활성 분기 직접 확인으로 CM3·CFX·calibration 3처를 모두 훑었으나, `.c`/`.h` 텍스트 검색으로 커버되지 않는 경로(예: 링커 스크립트에서 심볼명 직접 참조, 디버거 스크립트)는 원천적으로 검증 밖이다. **검증**: 이관 후 CM3·CFX 두 프로젝트 모두 clean build 하여 미정의 심볼 에러가 없는지 확인.
3. **주소 매크로 원천 확인**: `DSP_PRAM1_REMAP_BASE`(`<hw.h>`, CM3), `D_DSP_PRAM1_BASE`(CFX SDK) 모두 새 헤더가 `#include <hw.h>` 등 원 헤더를 빠짐없이 가져와야 한다(현재 `hal/tdc_hal_uart.h:14` 가 `<hw.h>` 를 이미 include). 빠뜨리면 미정의 매크로로 컴파일 에러 — 역시 안전한 실패 모드.
4. **실제 물리 주소 일치의 최종 확인 불가**: 4절 판단 보류 1번 참고. 이 저장소 환경에는 `objdump`/`readelf` 가 없어 16진수 리터럴 대조를 못 했다. **권고**: 이관 작업 완료 후 실제 빌드의 `.map` 파일 또는 디버거로 두 값이 물리적으로 동일한지 최종 1회 확인.
5. **`tdc_hal_uart.h` 전체 폐기(후속 과제)와의 경계**: 은수님 지시("UART 관련 항목을 다 제거")대로 `hal/tdc_hal_uart.h`/`.c` 자체를 나중에 통째로 지운다면, `tdc_hal_uart_uninit()` 의 실제 HW 동작(UART 페리퍼럴 비활성화, DIO 핀 원복 — 3절 참고)과 `TDC_HAL_UART_DIO_TX/RX` 매크로(`hal/tdc_hal_dio.c:121-122` 가 사용)의 새 거처를 별도로 정해야 한다. **이번 FS_MEM_UART 이관 설계의 범위 밖**이지만, 두 작업이 같은 파일을 건드리므로 순서를 "① FS_MEM_UART 이관 -> ② tdc_hal_uart.h 슬림화 -> ③ 남은 DIO/uninit 로직 재배치 -> ④ tdc_hal_uart.h 완전 삭제" 로 잡을 것을 권고.
6. **경고 주석 상호 참조 누락**: `tdc_shm.h` 와 `tdc_shm_debug.h` 는 서로 다른 PRAM 뱅크를 쓰는 별개의 ABI 채널이라 헷갈리기 쉽다. 5.4 초안처럼 두 헤더가 서로를 명시적으로 언급하게 해, 한쪽만 보고 다른 채널의 존재를 놓치는 일을 방지할 것.
