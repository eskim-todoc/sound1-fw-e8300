---
name: G3 drv 게이트 노트
purpose: G3(driver_* -> hal/ · drv/ 분류 이관) 파일·심볼 매핑, 유닛맵, 검증 기록
type: tasks/게이트노트
applies_to: [Sound1]
tags: [cm3, refactoring, gate, g3, driver, hal, chip]
---

# G3 — drv (드라이버 계층)

**TL;DR**: `driver_*` 22파일을 **MCU 내부 페리페럴(`hal/tdc_hal_*`)** 과 **외부 칩(`drv/tdc_drv_<chip>_*`)** 으로 분류 이관한다. 승인된 역할 표식(`tdc_hal_` / `tdc_drv_`)의 첫 본격 적용 게이트. PMIC 3종은 보드 조건부라 **비활성 2종도 원형 이관**(제거 금지 — 보드 변형 지원).

## 1. 분류 (승인 약어표 기준)

### hal/ — MCU 내부 페리페럴

| 현행 | 신규 | 근거 |
|---|---|---|
| `systemControl/driver_SPI.c/.h` | `hal/tdc_hal_spi.c/.h` | MCU SPI + DMA IRQ |
| `systemControl/driver_i2c.c/.h` | `hal/tdc_hal_i2c.c/.h` | MCU I2C0 마스터 |
| `systemControl/driver_DMA.h` | `hal/tdc_hal_dma.h` | MCU DMA 상수 |
| `systemControl/driver_timmer.c` | `hal/tdc_hal_cfx_tick.c` | **오타 정정 + 실체 반영** — CM3 에 타이머가 없어 CFX 인터럽트(`CFX_0`/`FIFO_5_IRQHandler`)를 tick 으로 쓰는 코드. "timer" 가 아니라 CFX tick 수신부 |
| `systemControl/driver_cfx_i2c.c/.h` | `hal/tdc_hal_i2c_cfx.c/.h` | CFX 경유 I2C 채널 |
| `systemControl/driver_i2c_for_ISD.c/.h` | `hal/tdc_hal_i2c_isd.c/.h` | ISD 전용 I2C 래퍼 (`driver_i2c` 위 얇은 계층) |
| `systemControl/driver_i2c_state_cfx.h`(루트) | `hal/tdc_hal_i2c_state.h` | I2C 상태 코드 상수 — 루트 잔여 디렉토리 해소 |

> [!NOTE]
> `driver_PCM.h`(`isdExecution/`)는 **PCM 비트스트림 모드 상수**로 ISD 자극 프로토콜 정의다. 페리페럴 드라이버가 아니므로 **G7(isd) 로 이월**한다.

### drv/ — 외부 칩

| 현행 | 신규 | 칩 |
|---|---|---|
| `internalDevice/driver_REN_ISL9122.c/.h` | `drv/tdc_drv_isl9122.c/.h` | PMIC (**활성** — OTE_1_5) |
| `internalDevice/driver_REN_ISL91128.c/.h` | `drv/tdc_drv_isl91128.c/.h` | PMIC (OTE_1_2 전용, 비활성) |
| `internalDevice/driver_REN_ISL98608.c/.h` | `drv/tdc_drv_isl98608.c/.h` | PMIC (OTE_1_3 전용, 비활성) |
| `systemControl/driver_MIS2DH.c/.h` | `drv/tdc_drv_mis2dh.c/.h` | 가속도 센서 |
| `systemControl/driver_MAX17262.c/.h` | `drv/tdc_drv_max17262.c/.h` | 배터리 게이지 |

## 2. 불가침 · 주의

| 항목 | 방침 |
|---|---|
| **IRQ 핸들러명** | `SPI1_COM_IRQHandler` · `DMA0/1_IRQHandler` · `I2C_0_IRQHandler` · `CFX_1_IRQHandler` · `CFX_0_IRQHandler` · `FIFO_5_IRQHandler` — SDK 벡터 링크, **rename 금지** |
| **비활성 PMIC 2종** | `ISL91128` · `ISL98608` 은 현 보드에서 컴파일되지 않으나 **보드 변형 지원 자산** — 원형 이관, 제거 금지. 사장 판정 대상에서 제외 |
| **보드 조건부 include** | `isd_interface.c` 등이 `#if defined(Board_is_*)` 로 3종을 분기 include — 파일명 변경 시 **전 분기 동시 갱신** 필요 |
| `driver_MAX17262` | ③ census 사장 후보(배터리 게이지, QCC 이관 추정). **외부 칩 드라이버라 보류** — 제거 판단은 G4(pwr) 에서 배터리 도메인과 함께 |

## 3. 유닛/모듈 맵

| 모듈 | 유닛 | 검증 라벨 |
|---|---|---|
| hal(확장) | spi · i2c · i2c_cfx · i2c_isd · dma · cfx_tick | **[로직설명]** — rename/이동뿐, 로직 무변경 |
| drv | isl9122 · isl91128 · isl98608 · mis2dh · max17262 (칩별 독립) | **[로직설명]** (동일) |

의존: `hal/i2c_isd` · `hal/i2c_cfx` → **`hal/i2c` 통과 전제**. `drv/*` → **`hal/i2c` 통과 전제**(전 칩이 I2C 경유). `hal/cfx_tick` → `hal/timer`(G1) 통과 전제.

## 4. 검증

| 라벨 | 결과 |
|---|---|
| 검증_공통_1 (구 심볼 잔존 0) | ✅ 15패턴 전부 0 (`driver_*` 계열 · 구 함수명 · 구 상수명) |
| 검증_공통_2 (공유 ABI 무변경) | ✅ `cfx_cm3_sharedMemory.h` diff = include 파일명뿐 |
| 검증_공통_3 (균형·훅) | ✅ hal 12 + drv 10 파일 브레이스·전처리기 균형 OK, 깨진 include 0 |
| 검증_공통_4 (사장 실증) | ✅ 공개 함수 73개 전수 → **15건 제거**(고아 선언 12 + dead 정의 1 + 그 선언 2). **3건 보존** — §5 참조 |
| **검증_G3_특화 (SDK 심볼 보존)** | ✅ 13종 확인: `I2C_0_IRQHandler` · `I2C_ENABLE` · `I2C_ACK` · `I2C_NACK` · `I2C_LAST_DATA` · `I2C_STOP_INT_ENABLE` · `I2C_0_IRQn` · `SPI_1_COM_IRQHandler` · `CFX_0/1_IRQHandler` · `FIFO_5_IRQHandler` · `DMA0/1_IRQHandler` |
| **검증_G3_특화 (보드 분기 정합)** | ✅ `isd_interface.c` · `isd_interface_FPGA.c` · `isd_interface_init_FPGA.c` 의 3분기 include 가 신규 파일명으로 동시 갱신 |
| 검증_공통_5 (빌드·실기) | ✅ PASS — 은수님 확인 (2026-07-21): "빌드 후 동작 확인했어" → **G3 폐쇄** |

## 5. 사장 판정 — 보존 3건 (조건부 컴파일)

| 심볼 | 보존 사유 |
|---|---|
| `tdc_hal_i2c_comm` | **독립 함수가 아님.** `#ifdef TDC_HAL_I2C_USING_ISR` 로 `I2C_0_IRQHandler` 와 **본문을 공유**하는 `#else` 분기다. 제거 시 I2C 인터럽트 핸들러까지 소실 |
| `tdc_drv_mis2dh_transfer_acceleration_value` | `#ifdef UART_isDedicated_CM3_DATA` 블록 내부 — 비활성 빌드 변형용 |
| `tdc_drv_mis2dh_update_xyz_acceleration` | 호출 0 이나 본문에 전처리기 분기 포함 — 정의·선언 모두 유지, 제거는 후속 판단 |

## 6. 도구 결함 3건 (기록 — G1·G2 계열 재발)

| # | 결함 | 발견 경위 | 조치 |
|---|---|---|---|
| 1 | **접두어 기반 rename 이 SDK 심볼 파괴** — `I2C_` 로 시작하는 모든 식별자를 rename 해 `I2C_ENABLE` · `I2C_ACK` · `I2C_0_IRQHandler` 등 ON Semi SDK 제공 심볼 20여 종이 오염 | 검증에서 `I2C_0_IRQHandler` 개수 0 발견 | 작업트리 복원 후, **해당 헤더가 실제로 정의하는 심볼 집합**으로 한정하는 규칙으로 교체 |
| 2 | **Allman 중괄호 미인식** — 정의/선언 분류기가 같은 줄의 `{` 만 봐서, 다음 줄에 `{` 가 오는 프로젝트 스타일에서 정의를 선언으로 오분류 | 직접 grep 결과와 불일치 | 직접 grep(.c/.h 파일 수) 기준으로 재분류 |
| 3 | **조건부 함수 정의 미인식** — `#ifdef A / void f1() / #else / void f2() / #endif / { 본문 }` 구조를 몰라 f2 제거 시 f1 까지 삭제 | 전처리기 균형 검사에서 `p=1` 불균형 발견 | 제거 구간에 전처리기 지시자가 있거나 직전 줄이 전처리기면 **자동 SKIP** 하는 가드 추가 |

> [!IMPORTANT]
> 세 결함 모두 **"자동 검출 결과를 직접 grep·균형검사로 교차 확인"** 했기에 잡혔다. G1(테이블 참조) · G2(호출문 오분류) 와 같은 계열이며, 이 교차 검증 습관이 없었다면 I2C 인터럽트 핸들러 소실이 빌드까지 갔을 것이다.
