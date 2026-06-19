---
name: touch-적용가이드-index
purpose: IQS323 외부 앱노트 기반 제어 운영 적용가이드 폴더 색인
type: index
maturity: stable
tags: [touch, iqs323, 적용가이드, index, azd004, azd125]
---

> **TL;DR**: Azoteq 외부 자료(AZD004·AZD125·IQS323 User Guide·Arduino 예제코드)를 정독·adversarial 검증해 Sound1 제어 방향(Fixed MULT/COMP + 노터치 게이트 조건부 Re-ATI)에 매핑한 개발 참조 문서 모음. `데이터시트/`(원문)·`레퍼런스/`(개념 해설)와 달리, 외부 앱노트의 실무 정량·설계 권고를 우리 회로 제약과 교차한 **적용 판정**이 핵심.

---

## 문서

| 문서 | 핵심 |
|---|---|
| [IQS323-제어운영-적용가이드](IQS323-제어운영-적용가이드.md) | 신호처리(ATI 2단계·민감도)·LTA 드리프트 보정·환경 정량(온도 −18cnt/°C, 감소 방향)·부팅/절전 시퀀스·통신 윈도우·레이아웃 감사·개발 체크리스트·데이터시트 잔여 4과제 |

## 출처 자료 (docs/참고/touch/)

- `azd004_azoteq_sensing_v1.1.pdf` — Azoteq 센싱 기술 일반론(ATI·필터·LTA·민감도)
- `azd125-capacitive_sensing_design_guide_v1.2.pdf` — 자가용량 설계 가이드(접지·온도·습도·노이즈)
- `IQS323_User_Guide_v1.1.pdf` — IQS323 운영(스트리밍·명령·Device Setup)
- `iqs323-example-code/` — Arduino 예제(드라이버 init 시퀀스)

## 분석 과정

[`tasks/touch/20260619_external-reference-integration`](../../../tasks/touch/20260619_external-reference-integration/분석.md) — 10노드 Workflow(정독 5 + 매핑 2 + adversarial 검증 2 + 종합 1).
