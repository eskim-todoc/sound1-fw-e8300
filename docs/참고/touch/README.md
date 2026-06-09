---
name: touch-reference-index
purpose: IQS323 터치 참고 문서 전체 색인 — 레퍼런스(데이터시트 지식)와 이슈해결(트러블슈팅 로그) 2축
type: index
maturity: stable
tags: [touch, iqs323, index, reference, troubleshooting]
---

> **TL;DR**: Sound1 터치(IQS323) 참고 문서 색인. **레퍼런스/** 는 데이터시트 기반 변하지 않는 지식, **이슈해결/** 은 시간순으로 누적하는 이슈→해결 로그. 새 이슈는 `이슈해결/YYYY-MM_<슬러그>.md`로 추가하고 본 색인에 한 줄 등록한다.

---

## 구조

```
docs/참고/touch/
├── README.md                  ← 본 색인
├── 레퍼런스/                   데이터시트 지식 (영속)
├── 이슈해결/                   트러블슈팅 로그 (시간순 누적)
├── iqs323_datasheet.pdf       Azoteq 원본 데이터시트 v1.11
└── Touch Sensor Schematic.JPG 회로도 원본
```

---

## 레퍼런스 (데이터시트 지식)

| 문서 | 내용 |
|---|---|
| [동작원리](레퍼런스/IQS323-동작원리.md) | 정전용량·ATI·LTA·Threshold 동작 원리 (초심자용 1차 입문) |
| [개념-종합가이드](레퍼런스/IQS323-개념-종합가이드.md) | 용어 사전·채널·카운트·LTA·ATI·autoATI 폭넓은 설명 |
| [레지스터-맵](레퍼런스/IQS323-레지스터-맵.md) | 레지스터 reset value·비트맵·ATI/CalCap/ATI Error 동작 종합 |
| [회로-구성](레퍼런스/IQS323-회로-구성.md) | 핀 연결·부품값·CRX0/CRX1·C52·J4 채널 배치 |
| [RESEED](레퍼런스/IQS323-RESEED.md) | counts·LTA·delta·drift·RESEED·절전 설정 순서 |

## 이슈해결 (트러블슈팅 로그)

| 일자 | 문서 | 핵심 |
|---|---|---|
| 2026-06 | [CRX1-ESD-더미채널](이슈해결/2026-06_CRX1-ESD-더미채널.md) | ESD 방전용 CH0 disable → 단일채널 정지 → Internal Reference 오용(0x43 먹통) → CalCap 더미 채널 → CH1 ATI Disabled |

---

## 새 이슈 추가 방법

1. `이슈해결/YYYY-MM_<슬러그>.md` 작성 (frontmatter `type: 이슈해결`, name `issue-YYYY-MM-<슬러그>`).
2. 구조: 배경 → 이슈/시도/해결 타임라인 → 현황 → 교훈.
3. 데이터시트 근거는 [레지스터-맵](레퍼런스/IQS323-레지스터-맵.md)에 누적하고 `[[iqs323-register-map]]`으로 링크.
4. 본 README 이슈해결 표에 한 줄 등록.
