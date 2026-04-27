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
| 부트로더 단계 LED 조기 점등 | LED | bootloader, startup, indicator | [LED/bootloader-power-on-indicator](LED/bootloader-power-on-indicator/) | 진행 | 2026-04-27 | ← [LED/non-blocking-fade-off](LED/non-blocking-fade-off/) (A·B 단계), → [LED/power-on-early-lighting](LED/power-on-early-lighting/) (보완) | 본 작업 본격 진입 준비 완료 — A 완료 / B(LED G) 자연 해소. 다음 세션(2026-04-30 목)부터 `요구사항.md` 작성 |

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
