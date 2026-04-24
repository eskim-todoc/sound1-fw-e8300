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
| POWER_ON LED 조기 점등 및 초기화 병렬화 | LED | init, startup, parallelization | [LED/power-on-early-lighting](LED/power-on-early-lighting/) | 진행 | 2026-04-23 | - | Rev.0 요구사항·분석 작성 (사용자 진행 중) |

## 완료

| 작업명 | 모듈 | 태그 | 폴더 | 상태 | 시작·완료 | 연계 | 요약 |
|---|---|---|---|---|---|---|---|
| LED 운용 방식 | LED | operation, scheme | [LED/operation-scheme](LED/operation-scheme/) | 완료 | ~2026-04-21 | - | 분석·구현·이력 완결 (이력 문서 존재) |
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
