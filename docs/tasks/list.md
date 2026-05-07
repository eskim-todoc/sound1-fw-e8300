# 작업 목록 (Sound1 tasks index)

Sound1 프로젝트(`E:\Claude\projects\Sound1`) 작업 레지스트리.

- 신규 작업 시 먼저 이 목록을 조회해 관련 작업 여부 확인 → 기존 폴더에서 이어갈지 신규 분리할지 사용자 확인
- 컨벤션 상세: 루트 [`지침/일반/문서 작성 규칙.md`](../../../../docs/지침/일반/문서%20작성%20규칙.md) (섹션 3·4·5)

---

## 활성

| 작업명 | 모듈 | 태그 | 폴더 | 상태 | 시작일 | 연계 | 요약 |
|---|---|---|---|---|---|---|---|
| _(없음)_ | | | | | | | |

## 완료

| 작업명 | 모듈 | 태그 | 폴더 | 상태 | 시작·완료 | 연계 | 요약 |
|---|---|---|---|---|---|---|---|
| LED 운용 방식 | LED | operation, scheme | [LED/operation-scheme](LED/operation-scheme/) | 완료 | ~2026-04-21 | - | 분석·구현·이력 완결 (이력 문서 존재) |
| LED Fade-Off 비차단 변환 | LED | non-blocking, isr, fade, refactor | [LED/non-blocking-fade-off](LED/non-blocking-fade-off/) | 완료 | 2026-04-27 ~ 2026-04-27 | ← [LED/bootloader-power-on-indicator](LED/bootloader-power-on-indicator/) §4 (발견 경위) | 폴링 fade-off (`turnOffLED` 30ms · `led_force_fade_off` 40ms) → ISR 상태머신 변환. 사용자 검증 통과 (30 ms 절약 확인). |
| 부트로더 단계 LED 조기 점등 | LED | bootloader, startup, indicator | [LED/bootloader-power-on-indicator](LED/bootloader-power-on-indicator/) | 완료 | 2026-04-27 ~ 2026-04-30 | ← [LED/non-blocking-fade-off](LED/non-blocking-fade-off/) (A·B 단계), ↔ [LED/power-on-early-lighting](LED/power-on-early-lighting/) (별개 진행) | 부트로더 단계 SKYBLUE fade 점등 — 첫 LED 인지 1670 ms → ~540 ms (~68% 단축). Timer2 SW PWM (40 kHz tick, PWM 100 bin / 400 Hz, fade 30+300+30+180=540 ms/cycle), G:B=30:40 (k_led_mix[SKYBLUE]). 빌드 가드 `TDC_BOOT_LED_ENABLE` (디폴트 1) + 런타임 UART 검증 핀 2 단 분기. Step 3 SLOWCLK_DIV32=40kHz 보정 후 정상 동작 확인. |
| POWER_ON LED 조기 점등 및 초기화 병렬화 | LED | init, startup, parallelization, fsm, isr | [LED/power-on-early-lighting](LED/power-on-early-lighting/) | 완료 | 2026-04-23 ~ 2026-05-06 | ← [LED/bootloader-power-on-indicator](LED/bootloader-power-on-indicator/) (별개 진행, 보완 관계) | CM3 단계 LED_ST_POWER_ON 조기 점등 (P3-Early = `ci_dio_configure_normal()` 직후, Initialize 단계 5 후) + Initialize 나머지 LED 버스트 병렬 진행. TIMER3 ISR 책임 분리 (LED 전담) + `g_tdc_timer_t3_tick` 신규 카운터 (LED·터치 공유) + tdc_touch FSM 명시화. POWER_OFF burst pending set/clear 책임 분리 (Fix B-LED-3 — `s_tdc_burst_pending`, `tdc_led_is_burst_pending()`). V1~V9 통과. V10 (ULP wakeup SPI race) = 본 작업 외부 (QCC 시퀀스 영역) 로 분리, 이후 필요 시 재개. 16 commit + 마무리 1, 머지 사용자 승인 대기. |
| 저전력 모드 전환 | power | sleep, low-power | [power/low-power-mode](power/low-power-mode/) | 완료 | ~2026-04-22 | - | ULP 모드 SYSCLK 30.72 MHz → 2.56 MHz, `ci_power_sleep()` 활성화. 코드 적용 확인 ([main.c:707](../../src/2__cm3/Cortex-M3-src/main.c)). |
| 터치 레이어 분리 | touch | layer, refactor | [touch/layer-separation](touch/layer-separation/) | 완료 | ~2026-04-21 | → [touch/init-split](touch/init-split/) (Step A) | 기능 레이어(`tdc_touch.c`) ↔ 드라이버 레이어(`tdc_drv_iqs323.c`) 분리. IC 교체 시 수정 범위 한정. 코드 적용 확인. |
| 터치 센서 초기화 분할 | touch | init, sequencing | [touch/init-split](touch/init-split/) | 완료 | ~2026-04-21 | → [touch/layer-separation](touch/layer-separation/) (Step B) | Auto-ATI 대기 1.5 s를 POWER_ON LED 버스트와 병렬 진행 (`tdc_touch_init_begin()` / `tdc_touch_process()` 분리). 코드 적용 확인. |
| Sound1 docs 폴더 구조 세분화 | meta | docs, convention, migration | [meta/docs-restructure](meta/docs-restructure/) | 완료 | 2026-04-24 ~ 2026-04-24 | → 루트 [meta/docs-restructure](../../../../docs/tasks/meta/docs-restructure/), → [meta/internal-links-fix](meta/internal-links-fix/) | flat `[프리픽스]` 22건 → 폴더 기반 + tasks 레지스트리. 22 파일 이동 + `tasks/list.md` 신설 + CLAUDE.md 갱신. 후속 tech debt = 깨진 markdown 링크 25+ 건 (internal-links-fix). |
| 내부 링크 경로 정정 | meta | docs, links, tech-debt | [meta/internal-links-fix](meta/internal-links-fix/) | 완료 | 2026-05-07 ~ 2026-05-07 | ← [meta/docs-restructure](meta/docs-restructure/) | docs-restructure 후 깨진 markdown 링크 25 건 정정. 검증 grep 0 건. src 코드 (`../src/...`) 상대경로 깊이 오류는 별도 후속 작업 (본 작업 범위 외). |
| docs 폴더별 README 추가 | meta | docs, readme, lazy-loading | [meta/folder-readmes](meta/folder-readmes/) | 완료 | 2026-05-07 ~ 2026-05-07 | ← 루트 [meta/docs-consistency-audit](../../../../docs/tasks/_archive/meta/docs-consistency-audit/) (후속 분기 1번) | `docs/지침/`·`사용방법/`·`참고/`·`tasks/` 4개에 README.md 인덱스 신규 추가 (루트 `문서 작성 규칙 §9.3` 충족). 인덱스 자료는 Explore 서브에이전트로 일괄 추출. `참고/LED/` 서브폴더 README는 후속 검토. |
| 영속 문서 frontmatter+TL;DR 일괄 소급 | meta | docs, frontmatter, retrofit, lazy-loading | [meta/frontmatter-retrofit](meta/frontmatter-retrofit/) | 완료 | 2026-05-07 ~ 2026-05-07 | ← [meta/folder-readmes](meta/folder-readmes/) (점검 빈틈 후속) | 사용자 명시 요청으로 일괄 소급. `docs/{지침·사용방법·참고}/` 영속 9 파일에 frontmatter 5필드 + `**TL;DR**:` 한 줄 in-place 추가. general-purpose 서브에이전트 1회 위임 + spot check 2 파일. 본문 무변경. |

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
| 2026-04-30 | `LED/bootloader-power-on-indicator` 구현 단계 진입 — **Step 1 commit `4099af7`**: `TDC_BOOT_LED_ENABLE` 가드 도입 + `tdc_boot_led` 빈 골격 (가드 OFF 회귀 검증용). **Step 2 commit `75fa380`**: `tdc_boot_led.c` 본체 구현 — `TIMER_2_IRQHandler` (10 us, 100 kHz), PWM 100 bin/1 kHz, phase 머신 5 단계 (FADE_IN 30 → PEAK 300 → FADE_OUT 30 → OFF 180 → DONE = 540 ms), G/B duty 30:40, Timer2 reload 307 (측정 후 보정), SDK API 는 `ci_timer.c` 패턴 (`Sys_Timer_Config` + `TIMER_PRESCALE_1` + `TIMER_FREE_RUN`). 사용자 Eclipse 빌드 + 오실로 측정 대기 — 회귀 (가드 OFF) 와 동작 (가드 ON × UART 검증 핀 두 케이스). 진행상황.md §6 측정 절차 안내. |
| 2026-04-30 | `LED/bootloader-power-on-indicator` 사용자 1 차 측정 → 오동작. 사용자 진단: "타이머 클럭에 문제. CM3 시스템 클럭 설정 후 클럭 div 설정을 부트로더에서도 동일하게". 발견 — RSL10 General Timer = **SLOWCLK / 32 = 40 kHz** ([`ci_timer.h:56`](LED/bootloader-power-on-indicator/) `T = 2^prescale × (tick+1) / 40 kHz`). Step 2 의 reload=307 (30.72 MHz 가정) 잘못. 또한 부트로더는 [`ci_power.c:154-156`](LED/bootloader-power-on-indicator/) 의 D_CLK 분주 설정 자체가 없어 SLOWCLK 가 디폴트 — 이중 문제. **Step 3 commit `27f2c28`**: ① `tdc_boot_led_init()` 안에 D_CLK->CFG_1/CFG_2 설정 추가 (CM3 와 동일, `SLOWCLK_PRESCALE_24 → SLOWCLK = 1.28 MHz → Timer = 40 kHz`) + `tdc_delay_ms(5)` 안정화 대기. ② PWM 매개변수 재계산: ISR 25 us (Timer 분해능 한계, reload=0), PWM 분해능 100 → 주기 2.5 ms (400 Hz), 1 ms 단위 phase tick 은 ISR 40 회 마다 별도 카운터 (s_tdc_boot_led_isr_in_ms). duty G:B=30:40 그대로. 사용자 재측정 대기. |
| 2026-04-30 | `LED/bootloader-power-on-indicator` 활성 → **완료**. 사용자 재측정 정상 동작 확인. **Step 4 commit `9bc5163`** = 정식 채택 정리 (헤더 위치 `include/led/`, `.cproject` include path, 가드 디폴트 0→1). 이력.md 작성. 머지 (`claude_feature_bootloader-power-on-indicator` → `claude_develop`) 사용자 승인 대기. |
| 2026-05-06 | `LED/power-on-early-lighting` 활성 → **완료**. LED·터치 V1~V9 통과 + POWER_OFF (Fix B-LED-3) 검증 OK. V10 (ULP wakeup SPI race) 은 회로 단서 (DIO24 풀-다운 10K) + QCC 담당자 피드백 cross-check 결과 QCC 펌웨어 시퀀스 영역으로 본 작업 외부 분리, 별도 task 신설 없이 종결 (이후 필요 시 진행상황·이력 §V10 시발점으로 재개). 이력.md 작성. 머지 (`claude_feature_early-power-on-led` → `claude_develop`) 사용자 승인 대기. |
| 2026-05-07 | 일괄 정리 — ① 완료 _(추정)_ 3건 (`power/low-power-mode`, `touch/layer-separation`, `touch/init-split`) 코드베이스 검증 후 정식 완료 확정 + 각 폴더 `이력.md` 작성. ② `meta/docs-restructure` 활성 → 완료 (실작업은 2026-04-24 commit `129212e` 머지로 종결, 진행상황.md 만 stale 상태였음). ③ `meta/internal-links-fix` 작업 완료 — 깨진 markdown 링크 25건 정정 (작업 대상 8 파일 + 추가 발견 2 파일). src 코드 상대경로 깊이 오류는 별도 후속. ④ 미사용 feature 브랜치 3개 삭제 (`led-spec-and-sleep-guard`, `non-blocking-fade-off` 머지 완료, `touch-log-level` 폐기). |
| 2026-05-07 | `meta/folder-readmes` 등재·완료 (루트 `meta/docs-consistency-audit`의 후속 분기 1번). `docs/지침/·사용방법/·참고/·tasks/` 4개 README.md 신규 추가 (루트 §9.3 충족). Sound1 archive 컨벤션상 폴더 이동 없이 위치(`meta/folder-readmes/`) 유지·list.md 완료 등재. |
| 2026-05-07 | `meta/frontmatter-retrofit` 등재·완료 (folder-readmes 점검 빈틈 후속). 사용자 (다) 명시 요청으로 일괄 소급. `docs/{지침·사용방법·참고}/` 영속 9 파일에 frontmatter+TL;DR 일괄 추가, general-purpose 서브에이전트 1회 위임 + spot check 통과. 본문 무변경. CLAUDE.md·tasks/_archive 산출물은 본 task 외(권고로 이력에 명시). |
| 2026-05-07 | task 산출물 frontmatter 일괄 소급 — 사용자 명시 트리거 "전부다 빼지말고 다 적용"로 frontmatter-retrofit 후속 영향에서 권고했던 항목 처리. `tasks/{LED·touch·power·meta}/<>/` 활성 위치 산출물 34 파일 점검 후 신규 23 / TL;DR만 11 / skip 12. general-purpose 서브에이전트 1회 위임. 본문 무변경, 신 §9.2 정책(80~250자) 준수. |
