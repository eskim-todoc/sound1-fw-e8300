---
name: Sullivan1.5 ↔ Sound1 signalProcessing 비교 분석
purpose: 베이스 프로젝트 Sullivan1_5__CM3 와 Sound1 2__cm3 의 signalProcessing 폴더 의미적 diff 정리
type: 분석
revision: Rev.0
status: 완료(signalProcessing 영역)
applies_to: [Sound1, sync, signalProcessing]
tags: [sullivan, baseline, diff, signal-processing, comparison]
---

# Sullivan1.5 ↔ Sound1 signalProcessing 비교 분석 (Rev.0)

**TL;DR**: `src/Sullivan1_5__CM3/Cortex-M3-src/signalProcessing/` ↔ `src/2__cm3/Cortex-M3-src/signalProcessing/` 4 파일 의미적 비교 결과 **전체 동등** (공백·빈줄·주석만 차이). 의미적 변경 없음. **Sullivan 후속 패치 없음, Sound1 후속 변경 없음.** 본 폴더는 sync 대상 외.

> [!IMPORTANT]
> 비교 기준은 [`분석 BleCommunication.md §7`](분석%20BleCommunication.md) 와 동일 — `git diff --no-index -w --ignore-blank-lines`. stat 명령 출력 = **빈 결과** (변경 없음).

## 1. 비교 범위

- Base (좌측 / `a`): `src/Sullivan1_5__CM3/Cortex-M3-src/signalProcessing/`
- Target (우측 / `b`): `src/2__cm3/Cortex-M3-src/signalProcessing/`
- 파일 4 개: `commonDataProcessing.c/.h`, `stimulationParaCal.c/.h`

## 2. 요약표 (파일 단위)

| # | 파일 | 동등? | 비고 |
|---|---|---|---|
| 1 | `commonDataProcessing.c` | **동등** | 공백/빈줄/주석만 차이 (있다면). 의미적 변경 없음. |
| 2 | `commonDataProcessing.h` | **동등** | 동일 |
| 3 | `stimulationParaCal.c` | **동등** | 동일 |
| 4 | `stimulationParaCal.h` | **동등** | 동일 |

→ **차이 있는 파일: 0 개** / **동등 파일: 4 개**

## 3. 결론

본 폴더는 Sound1 분기 이후 **양쪽 모두 의미적 변경이 없음**. 다음 중 하나로 해석:
- Sullivan 측 후속 변경 없음
- Sound1 측 후속 변경 없음
- (혹은 두 변경이 우연히 같은 결과)

**후속 권고 없음.** 본 폴더는 sync 작업 대상에서 제외.

## 4. 갱신 이력

| 날짜 | 변경 |
|---|---|
| 2026-05-14 | Rev.0 작성. 4 파일 모두 의미적 동등. 본 task (`sync/20260514_sullivan1.5-rel2`) 의 3 영역 분석 중 마지막 — 분석 종료, 종합 결론은 [`요구사항.md §결론`](요구사항.md) 또는 본 task 종합 결론 문서에서 정리. |
