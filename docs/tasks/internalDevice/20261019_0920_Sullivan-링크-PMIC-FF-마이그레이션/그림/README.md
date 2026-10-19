# 이식 분석과 계획의 그림

[분석과 계획으로 돌아가기](../분석과%20계획.md)

Markdown 본문은 Mermaid 지원 여부와 관계없이 그림을 볼 수 있도록 PNG를 사용한다. SVG와 수정 가능한 `.mmd` 원본도 같은 폴더에 둔다. 그림은 코드상 관계/순서 또는 미구현 제안이며 실제 측정 화면이 아니다.

[HTML 문서](../마이그레이션%20분석.html)는 고정된 이미지 대신 [반응형 도해 정의](../HTML%20반응형%20도해.mjs)의 HTML 상자·라벨·분기 링크로 같은 관계를 표현한다. 창 너비에 따라1~3열로 재배치하며 기본 진행 연결선만 실제 상자 위치에서 다시 그린다. 확대 창을 사용하지 않는다. 도해9는 개정 계획11.1/11.5절의1차 단계0~5와 별도 FF 후속으로 갱신했으므로 아래 과거 전체 로드맵 이미지와 구분한다.

| 번호 | 내용 | 본문 그림 | 벡터 | 편집 원본 |
|---:|---|---|---|---|
| 1 | 칩/코어와 기능 역할 | [PNG](01-architecture.png) | [SVG](01-architecture.svg) | [Mermaid](01-architecture.mmd) |
| 2 | 백텔 PCM 시퀀스 | [PNG](02-backtel-layout.png) | [SVG](02-backtel-layout.svg) | [Mermaid](02-backtel-layout.mmd) |
| 3 | FF3 입력 상태 | [PNG](03-ff-input-states.png) | [SVG](03-ff-input-states.svg) | [Mermaid](03-ff-input-states.mmd) |
| 4 | FF3 장부와 적용 판단 | [PNG](04-ff-ledger-flow.png) | [SVG](04-ff-ledger-flow.svg) | [Mermaid](04-ff-ledger-flow.mmd) |
| 5 | 프로토콜 도메인 대응 | [PNG](05-protocol-routing.png) | [SVG](05-protocol-routing.svg) | [Mermaid](05-protocol-routing.mmd) |
| 6 | 설정 저장/CFX ACK 순서 | [PNG](06-config-sequence.png) | [SVG](06-config-sequence.svg) | [Mermaid](06-config-sequence.mmd) |
| 7 | 내부기 초기화/운용 상태 | [PNG](07-link-states.png) | [SVG](07-link-states.svg) | [Mermaid](07-link-states.mmd) |
| 8 | PMIC 중재 제안 | [PNG](08-power-arbitration.png) | [SVG](08-power-arbitration.svg) | [Mermaid](08-power-arbitration.mmd) |
| 9 | 이식 단계 | [PNG](09-migration-stages.png) | [SVG](09-migration-stages.svg) | [Mermaid](09-migration-stages.mmd) |

2026-10-19, Mermaid11.12.0 및 로컬 Edge로 생성/검토했다. 비공개 코드/문서를 외부 렌더 서비스로 전송하지 않았다. PNG는 글꼴이 렌더된 이미지이며 SVG 글꼴은 열람 환경에 따라 달라질 수 있다. 본문을 옮길 때는 이 `그림` 폴더와의 상대 위치를 함께 유지한다.
