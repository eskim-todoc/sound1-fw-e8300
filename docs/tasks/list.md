# 작업 목록 (Sound1 tasks index)

Sound1 프로젝트(`E:\Claude\projects\Sound1`) 작업 레지스트리.

- 신규 작업 시 먼저 이 목록을 조회해 관련 작업 여부 확인 → 기존 폴더에서 이어갈지 신규 분리할지 사용자 확인
- 컨벤션 상세: 루트 [`지침/일반/문서 작성 규칙.md`](../../../../docs/지침/일반/문서%20작성%20규칙.md) (섹션 3·4·5)

---

## 활성

| 작업명 | 모듈 | 태그 | 폴더 | 상태 | 시작일 | 연계 | 요약 |
|---|---|---|---|---|---|---|---|
| Sound1 docs 폴더 구조 세분화 | meta | docs, convention, migration | [meta/docs-restructure](meta/docs-restructure/) | 진행 | 2026-04-24 | → 루트 [meta/docs-restructure](../../../../docs/tasks/meta/docs-restructure/), → [meta/internal-links-fix](meta/internal-links-fix/) (예정) | 루트 컨벤션 Sound1 적용 |
| 내부 링크 경로 정정 | meta | docs, links, tech-debt | [meta/internal-links-fix](meta/internal-links-fix/) _(폴더 미생성)_ | 대기 | - | → [meta/docs-restructure](meta/docs-restructure/) | migration 후 남은 구 프리픽스 링크 25+ 건 정정 |
| POWER_ON LED 조기 점등 및 초기화 병렬화 | LED | init, startup, parallelization | [LED/power-on-early-lighting](LED/power-on-early-lighting/) | 진행 | 2026-04-23 | ← [LED/bootloader-power-on-indicator](LED/bootloader-power-on-indicator/) (상위 단계, 보완 관계) | Rev.0 요구사항·분석 작성 (사용자 진행 중) |
| 부트로더 단계 LED 조기 점등 | LED | bootloader, startup, indicator | [LED/bootloader-power-on-indicator](LED/bootloader-power-on-indicator/) | 진행 | 2026-04-27 | ← [LED/non-blocking-fade-off](LED/non-blocking-fade-off/) (A·B 단계), ↔ [LED/power-on-early-lighting](LED/power-on-early-lighting/) (별개 진행) | `구현계획.md` Rev.1 — 네이밍 지침 (`tdc_boot_led_*`) 적용 + ISR IRQ Clear 제거. 신규 모듈 `tdc_boot_led` (Timer2 ISR 10us, PWM 100bin/1kHz, phase 머신 5단계). 통합 4지점: main.h `TDC_BOOT_LED_ENABLE`, main.c 가드, bootloader.c (T-B init+start, 점프 직전 540ms wait+stop). 단계별 commit 7-step. 사용자 승인 → 구현 진입 대기 |

## 완료

| 작업명 | 모듈 | 태그 | 폴더 | 상태 | 시작·완료 | 연계 | 요약 |
|---|---|---|---|---|---|---|---|
| LED 운용 방식 | LED | operation, scheme | [LED/operation-scheme](LED/operation-scheme/) | 완료 | ~2026-04-21 | - | 분석·구현·이력 완결 (이력 문서 존재) |
| LED Fade-Off 비차단 변환 | LED | non-blocking, isr, fade, refactor | [LED/non-blocking-fade-off](LED/non-blocking-fade-off/) | 완료 | 2026-04-27 ~ 2026-04-27 | ← [LED/bootloader-power-on-indicator](LED/bootloader-power-on-indicator/) §4 (발견 경위) | 폴링 fade-off (`turnOffLED` 30ms · `led_force_fade_off` 40ms) → ISR 상태머신 변환. 사용자 검증 통과 (30 ms 절약 확인). |
| 저전력 모드 전환 | power | sleep, low-power | [power/low-power-mode](power/low-power-mode/) | 완료 _(추정)_ | ~2026-04-22 | - | 분석·구현계획 Rev.0 — 이력 미작성, 상태 사용자 확인 요망 |
| 터치 레이어 분리 | touch | layer, refactor | [touch/layer-separation](touch/layer-separation/) | 완료 _(추정)_ | ~2026-04-21 | - | 구현계획 Rev.1 — 이력 미작성, 상태 사용자 확인 요망 |
| 터치 센서 초기화 분할 | touch | init, sequencing | [touch/init-split](touch/init-split/) | 완료 _(추정)_ | ~2026-04-21 | - | 구현계획 Rev.2 + 로그 (성공·실패 포함) — 이력 미작성, 상태 사용자 확인 요망 |

> "완료 _(추정)_" 표시 항목은 파일 누적 상태에서 추정한 것. 실제 상태와 다르면 사용자가 **활성**으로 이동하거나 `이력.md`를 추가해 정식 완료로 확정.

---

## 상태 정의

| 상태 | 의미 |
|---|---|
| `진행` | 작업 진행 중 |
| `대기` | 선행 조건 충족 대기 |
| `차단` | 외부 요인으로 진행 불가 |
| `완료` | 병합·릴리즈 완료 |
| `취소` | 작업 취소 |

## 모듈 정의 (Sound1 도메인)

| 모듈 | 범위 |
|---|---|
| `LED` | LED 운용·밝기·dimming·패턴·표시 상태 |
| `touch` | 터치 센서·레이어·초기화·이벤트 처리 |
| `power` | 저전력 모드·전원 관리 |
| `meta` | 문서·워크플로우 등 프로젝트 메타 작업 |
| (추가 예정) | `comms`, `audio`, `calibration` 등 필요 시 |

## 갱신 이력

| 날짜 | 이벤트 |
|---|---|
| 2026-04-24 | 레지스트리 생성. 기존 Sound1 작업 5건 등재 (활성 2, 완료(추정) 3 + 완료 1). `meta/docs-restructure` (migration 작업) 활성으로 등재. |
| 2026-04-24 | 구조 정정: `기준/` → `지침/` 통합 (1건 이동). `참고/` 모듈 분할 — `칩/`·`LED/`·`터치/` 서브폴더 생성 (9건 이동). `참고/LED/` 내 3 파일 프리픽스 "LED 시스템 —" 제거 후 상호참조 링크 갱신. `참고/칩/Ezairo 클럭·타이머·I2C 스펙 정리.md` PDF 링크 연결. `참고/터치/터치센서 운용 방식.md`의 IQS323 링크 정정 (malformed 이스케이프 시퀀스 교체). Sound1 CLAUDE.md layout 섹션 갱신. |
| 2026-04-27 | 활성 작업 2건 등재: `LED/bootloader-power-on-indicator` (조사 단계, 진행상황 인계 commit `e1630e9`) · `LED/non-blocking-fade-off` (요구사항.md 작성, 사용자 승인 대기). |
| 2026-04-27 | `LED/non-blocking-fade-off` 활성 → 완료. 사용자 검증 통과 (30 ms 절약 확인, 회귀 없음). 잔존 이슈는 LED G 미스터리 (별도 작업). |
| 2026-04-27 | `LED/non-blocking-fade-off` 사후 fix (commit `85c250f`) — fade-off cancel 시 cross-fade Phase A 잔존 색 가동 anomaly 해소. 부수 발견: LED G 미스터리(B 작업) 도 동일 원인이라 함께 해소. 이력.md §8 추가. |
| 2026-04-27 | `LED/bootloader-power-on-indicator` 진행상황.md 갱신 (B 자연 해소·C 진입 준비). 회고 신규 카테고리 [`코딩.md`](../../../../docs/회고/코딩.md) 생성 — "비차단/ISR 시스템에서 동기 폴링 회피". CLAUDE.md / 회고 README 카테고리 목록 갱신. 다음 세션 = 2026-04-30 (목) C 작업 진입. |
| 2026-04-30 | `LED/bootloader-power-on-indicator` C 작업 진입 — 작업 브랜치 `claude_feature_bootloader-power-on-indicator` 생성, `요구사항.md` Rev.0 작성 (사용자 승인 대기). |
| 2026-04-30 | `LED/bootloader-power-on-indicator` `요구사항.md` Rev.1 갱신 — 사용자 피드백 반영: §6 매핑 C4(SW PWM) 채택, AC-2 강화 (CM3 와 동일 mix/PWM/dimming), 인접 작업과 별개 진행 가능, 빌드 가드 매크로 `BOOTLOADER_LED_INDICATOR_ENABLE` 정식 채택. |
| 2026-04-30 | `LED/bootloader-power-on-indicator` `요구사항.md` Rev.2 갱신 — 사용자 피드백 2차 반영: ① **R-11/Q-PWM-0 (CM3 이미지 로드 중 부트로더 ISR 가용성)** 을 분석 1순위 / 본 작업 선결 조건으로 격상, ② SW PWM 종료 시점 = `bootloader_boot_cm3()` 호출 직전 ([`bootloader.c:1374`](LED/bootloader-power-on-indicator/) 부근, AC-7 신설), ③ 점등 시작 시점을 T-A(`main()` 직후, 클럭 7.68 MHz) / T-B(`Sys_Trims_SetOperatingFrequency()` 30.72 MHz 전환 후) 두 후보로 분기 — 클럭 변경까지 시간이 짧으면 T-B 채택 (Q-PWM-3, R-12). |
| 2026-04-30 | `LED/bootloader-power-on-indicator` `요구사항.md` Rev.3 갱신 — 사용자 피드백 3차 반영: ① **R-11 영향 시 본 작업 폐기** 분기 명시 (우회 방안 미검토) + 가치 이전 대상 = `LED/power-on-early-lighting` (CM3 부팅 직후 즉시 LED 점등 형태로 흡수). 분석 단계를 Phase 1(R-11 단독 선결) / Phase 2(나머지) 로 분리. ② AC-3 / AC-7 무점등 시간 제약 완화 — CM3 측이 OFF 에서 fade-in 으로 자연스럽게 시작하므로 부트로더 OFF → CM3 OFF → 정식 LED 사이의 무점등 구간은 끊김으로 보지 않음 (§1.4 보강). |
| 2026-04-30 | `LED/bootloader-power-on-indicator` `분석.md` Rev.0 — Phase 1 (R-11) **가능 ✅** 판정. 근거: ① 부트로더 PRAM5(0x60000) 와 CM3 적재 영역(PRAM0~6) 의 운용 분리 검증 (실 운용 사실), ② [main.c:39-42](LED/bootloader-power-on-indicator/) 에서 NVIC 전체 disable + pending clear 로 단일 상태, ③ 부트로더는 polling-only — 활성 ISR 핸들러 없음, ④ 부트로더에 타이머 사용 흔적 전무. 본 작업 진행 결정. Phase 2 진입 사용자 승인 대기. |
| 2026-04-30 | `LED/bootloader-power-on-indicator` `분석.md` Rev.1 — Phase 2 정밀 분석 추가. 결정 D-1~D-9: ① 타이머 = Timer3 외 (CM3 점유 회피, ci_timer.c:18), ② LED 엔진 = 자체 구현 (사용자 결정 — fade-in 30 + peak 300 + fade-out 30 + off 180 = 540 ms/cycle, peak 만 정식 LED_ST_POWER_ON 의 2 배), ③ 점등 시작 = **T-B** (`Sys_Trims_SetOperatingFrequency()` 직후, bootloader.c:643) — T-A 채택 시 클럭 변경으로 fade 시간 1/4 단축 회피, ④ 초기 디버그 LED 빌드 가드 분기, ⑤ `bootloader_boot_cm3()` 직전 540 ms wait, ⑥ UART RX 와 G LED (DIO15) 충돌 → 빌드 가드 분기, ⑦ active-low 가정 + 구현계획 단계 검증. 구현계획.md 진입 사용자 승인 대기. |
| 2026-04-30 | `LED/bootloader-power-on-indicator` `분석.md` Rev.2 — 사용자 2 차 피드백 반영. ① **D-1 = Timer2** (CM3 미점유 확인 — F-10), ② D-3 = T-B 확정, ③ **D-4 변경** = main.c 디버그 GPIO 블록을 빌드 가드로 무력화 → GPIO `DIO_MODE_DISABLE` (hi-Z) 유지 → 풀다운 저항으로 자연 OFF (사용자 명시 "물리적으로 LED 가 OFF 상태로 전원이 켜짐") → T-B 시 SW PWM fade-in 자연 점등, ④ **D-7 변경** = 빌드 가드 + 런타임 UART 검증 핀 (`Sys_GPIO_Read(DIO_NUM_UART_ENABLE) == DIO_ACTIVE_LEVEL_UART_ENABLE`, [main.c:59](LED/bootloader-power-on-indicator/)) 2 단 분기 — 가드 ON + UART 활성=LED 비활성, 가드 ON + UART 비활성=LED 활성, ⑤ **D-8 = active HIGH** 확정 (non-blocking-fade-off 이력 §1 의 "active LOW" 정보 갱신, `LED_IS_ACTIVELOW` 미정의 상태와 일관). 구현계획.md 진입 사용자 승인 대기. |
| 2026-04-30 | `LED/bootloader-power-on-indicator` `구현계획.md` Rev.0 작성. 신규 모듈 `boot_led` (헤더 + .c) — Timer2 10 us ISR, PWM 100 bin/1 kHz, phase 머신 IDLE/FADE_IN/PEAK/FADE_OUT/OFF/DONE, k_led_mix[en__LED_SKYBLUE] G:B=30:40 비율 차용. 통합 4 지점: ① main.h 매크로 디폴트 정의, ② main.c:16-24 `#if !BOOTLOADER_LED_INDICATOR_ENABLE` 가드, ③ bootloader.c:1496 직후 (T-B) → 검증 핀 read + init/start, ④ bootloader.c:1374 직전 → 540 ms wait + stop. 단계별 commit 7-step (가드 OFF 회귀 → SDK 함수 확정 → ISR PWM → phase tick → wait/stop → UART 분기 → 종합). 위험·롤백 명시 (Timer SDK fallback, reload 보정, 풀다운 보드별 차이). 사용자 승인 → 구현 진입 대기. |
| 2026-04-30 | `LED/bootloader-power-on-indicator` `구현계획.md` Rev.1 — 사용자 피드백 2 건 반영. ① **네이밍 지침** ([`docs/지침/코딩/네이밍 컨벤션.md`](../../지침/코딩/네이밍%20컨벤션.md)) 의 `tdc_` prefix 일관 적용 — 모듈 = `tdc_boot_led`, 파일 = `tdc_boot_led.h/.c`, 함수 = `tdc_boot_led_init/start/stop/is_done/elapsed_ms`, 매크로 = `TDC_BOOT_LED_*`, 빌드 가드 = `TDC_BOOT_LED_ENABLE` (Rev.0 의 `bli_*` / `BOOTLOADER_LED_INDICATOR_ENABLE` 폐기), 타입 = `tdc_boot_led_phase_t`, static 전역 = `s_tdc_boot_led_*`. ② **Cortex-M3 ISR IRQ Clear 제거** — NVIC 가 ISR entry/exit 시 active bit 자동 set/clear, peripheral pending bit 도 RSL10 Timer 는 hardware auto-clear 또는 read-to-clear 가정 (Step 2 측정에서 재확인, 필요 시 ack 추가). init 시점의 `NVIC_ClearPendingIRQ` 는 잔여 pending 보호 차원으로 유지. |
