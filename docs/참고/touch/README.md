---
name: touch-reference-index
purpose: IQS323 터치 참고 문서 전체 색인 — 데이터시트(원문 1차)·레퍼런스(2차 해설)·이슈해결(트러블슈팅) 3축
type: index
maturity: stable
tags: [touch, iqs323, index, datasheet, reference, troubleshooting]
---

> **TL;DR**: Sound1 터치(IQS323) 참고 문서 색인. **데이터시트/** 는 Azoteq 원문 1차 정리(SSOT, README+8파일), **레퍼런스/** 는 그 위의 2차 해설·Sound1 적용, **이슈해결/** 은 시간순 이슈→해결 로그. 새 이슈는 `이슈해결/YYYY-MM_<슬러그>.md`로 추가하고 본 색인에 한 줄 등록한다.

---

## 구조

```
docs/참고/touch/
├── README.md                  ← 본 색인
├── 데이터시트/                 데이터시트 원문 1차 정리 (SSOT, README+8파일)
├── AZD004 센싱 일반 가이드/     앱노트 AZD004 원문 정리 (센싱 일반·ATI·필터, README+6파일)
├── AZD125 정전용량 설계 가이드/ 앱노트 AZD125 원문 정리 (레이아웃·버튼·온도, README+6파일)
├── IQS323 예제코드/            Arduino 예제 코드·가이드 정리 (드라이버·init, README+4파일)
├── 레퍼런스/                   2차 해설·Sound1 적용 (영속)
├── 적용가이드/                 외부 앱노트(AZD004·AZD125 등) → 제어 운영 적용
├── 개선/                       제어 재설계·검토 산출물
├── 이슈해결/                   트러블슈팅 로그 (시간순 누적)
├── iqs323_datasheet.pdf       Azoteq 원본 데이터시트 v1.11
├── (외부 앱노트 PDF)          AZD004·AZD125·User Guide·예제코드 등
└── Touch Sensor Schematic.JPG 회로도 원본
```

---

## 데이터시트 (원문 1차 정리, SSOT)

Azoteq IQS323 데이터시트(v1.11, 68p) 원문을 빠짐없이 정리. 레지스터·임계값·동작을 PDF 재독 없이 참조.

| 문서 | 구간 | 내용 |
|---|---|---|
| [README (색인·빠른참조)](데이터시트/README.md) | — | 8파일 색인 + 빠른 참조표(주소·공식·전류·기본값) |
| [01_개요·전기·타이밍](데이터시트/01_개요·전기·타이밍.md) | §1~4 | 개요·핀맵·전기특성·타이밍 |
| [02_proxfusion동작](데이터시트/02_proxfusion동작.md) | §5 | Count·LTA·ATI·Threshold·Sensor Setup |
| [03_하드웨어설정](데이터시트/03_하드웨어설정.md) | §6 | Prox Control·Dead Time·Conv Freq·Reset |
| [04_부가기능·UI](데이터시트/04_부가기능·UI.md) | §7 | OutA·Slider·Reference/Release/Movement UI |
| [05_i2c인터페이스](데이터시트/05_i2c인터페이스.md) | §8 | 통신·이벤트모드·Force Comm·Program Flow |
| [06_레지스터레퍼런스](데이터시트/06_레지스터레퍼런스.md) ★ | §9+부록A | 전체 레지스터 맵(주소+비트 전수) |
| [07_오더링·패키지](데이터시트/07_오더링·패키지.md) | §10~11 | Order Code·패키지·Tape&Reel |
| [08_개정이력·이슈](데이터시트/08_개정이력·이슈.md) | 부록B·C | Revision History·Known Issues |

## 앱노트·예제코드 (외부 원문 1차 정리)

Azoteq 앱노트 2종 + IQS323 Arduino 예제 코드를 데이터시트 폴더처럼 원문 충실 정리(SSOT). 각 폴더 README가 파일 색인. 모든 수치·표는 할루시네이션 검증(원문 전수 대조) 완료.

| 폴더 | 원본 | 내용 |
|---|---|---|
| [AZD004 센싱 일반 가이드](AZD004%20센싱%20일반%20가이드/README.md) | `azd004_..._v1.1.pdf` (33p) | 전하 전달·센싱 4종·ATI·필터·선형화·전력·EMC·I²C (6파일) |
| [AZD125 정전용량 설계 가이드](AZD125%20정전용량%20설계%20가이드/README.md) | `azd125-..._v1.2.pdf` (41p) | 레이아웃 베스트프랙티스·근접/접지·터치 버튼·슬라이더/휠·온도/방수/노이즈 (6파일) |
| [IQS323 예제코드](IQS323%20예제코드/README.md) | `iqs323-example-code/` | Arduino 드라이버 API·init 시퀀스·EV-Kit 설정·가이드 (4파일) |

## 레퍼런스 (2차 해설·Sound1 적용)

데이터시트 원문 위에 쌓은 개념 해설·비유·Sound1 실측·코드 적용.

| 문서 | 내용 |
|---|---|
| [동작원리](레퍼런스/IQS323-동작원리.md) | 정전용량·ATI·LTA·Threshold 동작 원리 (초심자용 1차 입문) |
| [개념-종합가이드](레퍼런스/IQS323-개념-종합가이드.md) | 용어 사전·채널·카운트·LTA·ATI·autoATI 폭넓은 설명 |
| [레지스터-맵](레퍼런스/IQS323-레지스터-맵.md) | 레지스터 reset value·비트맵·ATI/CalCap/ATI Error 동작 종합 |
| [회로-구성](레퍼런스/IQS323-회로-구성.md) | 핀 연결·부품값·CRX0/CRX1·C52·J4 채널 배치 |
| [RESEED](레퍼런스/IQS323-RESEED.md) | counts·LTA·delta·drift·RESEED·절전 설정 순서 |

## 적용가이드 (외부 앱노트 → 제어 운영 적용)

Azoteq 외부 자료(앱노트·User Guide·예제코드)를 정독·검증해 Sound1 제어 방향에 매핑한 개발 참조 문서.

| 문서 | 내용 |
|---|---|
| [IQS323-제어운영-적용가이드](적용가이드/IQS323-제어운영-적용가이드.md) | AZD004·AZD125·User Guide·예제코드 종합 — ATI 2단계·온도 드리프트 정량(−18cnt/°C)·Follow UI 적용불가·SNR≥5·부팅/절전 시퀀스·개발 체크리스트·데이터시트 잔여 과제 |

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
