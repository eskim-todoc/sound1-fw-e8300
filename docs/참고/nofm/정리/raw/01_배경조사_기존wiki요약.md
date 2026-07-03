---
name: nofm-output-pattern-배경조사-기존wiki
description: 기존 projects/wiki/html/NofM 페이지 세트 조사 결과 — 겹치는 주제 및 재사용 가능 디자인 자산
metadata:
  type: raw-working-doc
---

## TL;DR

기존 wiki의 NofM 페이지 5종은 대부분 채널 매핑·알고리즘·튜닝 이력을 다루며, 이번에 만들 "duration→PCM/COLA 타이밍→IDLE 시각화" 주제와는 대부분 겹치지 않는다. 다만 `pcm-write.html`의 1ms 한도 분기 섹션은 밀접히 관련. 디자인 시스템(Catppuccin)은 참고용으로 재사용 가능.

## 각 파일 핵심 주제

1. **index.html** — NofM 알고리즘 개요, CIS vs NofM 비교표, Phase 0/1 사이클 다이어그램
2. **algorithm.html** — Peak Pick(32→16 채널 선택)·Interleaving·Packet 3단계 C 코드 설명
3. **mapping.html** — order→band→logical→physical 인덱스 변환, PCM 패킷 비트 배치
4. **pcm-write.html** — Phase 사이클 제어, NOP Pre-fill/Strided Overwrite, **Case A/B 1ms 한도 분기** (이번 작업과 가장 근접)
5. **tuning.html** — 실동작 검증 튜닝 이력 5건 (frameNumPerOneChannel=3 고정 등 포함)

## 이번 작업과의 겹침

- `pcm-write.html` §Case A/B: `df_MaxNumTransferableChannel`(24슬롯) 기반 1ms 한도 분기 — 타이밍 모델과 직접 연관
- `tuning.html`: "펄스 폭 설정에 따라 FPGA COLA 프로토콜 전송 시간이 1 프레임으로 끝날 가능성" 언급 — duration→idle 주제와 연결되나 시각화는 없음
- `algorithm.html`/`mapping.html`은 공간(채널 선택·매핑) 관점이라 이번 시간축 타이밍 시각화와 무관

## 재사용 가능 디자인 자산

- **색상**: Catppuccin Latte(라이트)/Mocha(다크) CSS 변수 팔레트 (`--base`,`--text`,`--blue`,`--mauve`,`--green`,`--yellow`,`--peach`,`--red`,`--teal`,`--sapphire`)
- **폰트**: 본문 Pretendard/Noto Sans KR, 코드 JetBrains Mono/D2Coding
- **레이아웃**: 좌측 사이드바 + 우측 카드형 콘텐츠, `.card-grid`/`.callout`/`.badge`/`.step-chain` 컴포넌트
- **JS**: 테마 토글(`localStorage` 영속) 헬퍼만 존재. **캔버스/SVG 드로잉·타이머 애니메이션 유틸 없음** — 신규 구현 필요
- 전체 톤: 차분한 파스텔(mauve 강조), 좌측 고정 네비 + 카드/테이블형(탭형 아님)

## 참고

이번 산출물은 Sound1 프로젝트 `docs/참고/nofm/정리/` 내 독립 단일 HTML 파일이므로 wiki 사이트에 종속될 필요는 없으나, UI 페르소나 분석 시 톤 참고용으로 활용.
