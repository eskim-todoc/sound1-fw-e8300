---
name: rdy-window-timing-요약
purpose: IQS323 기본 세팅·스트리밍 RDY 윈도우 High/Low/주기 한 장 요약
type: 참고
maturity: stable
tags: [touch, iqs323, rdy, streaming, timing, summary]
---

# IQS323 스트리밍 모드 RDY 윈도우 타이밍 (요약)

> **TL;DR**: 기본 세팅(칩 default = Sound1 적용)은 **Streaming + Normal Power + Report Rate 0ms + Transaction Timeout 200ms**. RDY는 **측정 사이클마다 Low로 떨어져 통신 윈도우를 연다**. **Low ≈ 0.4~0.5ms**(정상 통신)/최대 200ms(무응답), **주기 = 측정 사이클 시간**(Report Rate=0 → 최대 속도, 실측 필요), **High = 주기 − Low**. 상세·근거: [`RDY 윈도우 타이밍 분석/`](RDY%20윈도우%20타이밍%20분석/).

## 답 (은수님 질문)

| 질문 | 답 |
|---|---|
| **얼마나 Low?** | 통신 윈도우 = 마스터가 정상 읽으면 **트랜잭션 길이 ≈ 0.4~0.5ms** (SCL≈128kHz·2~3byte). 마스터 무응답이면 **최대 200ms**(I²C Transaction Timeout) 후 강제 닫힘+데이터 손실 |
| **얼마나 High?** | **주기 − Low**. 통신을 빨리 끝낼수록 길다 |
| **주기는?** | Report Rate=0(기본)이라 IC가 **측정 끝나는 즉시** 다음 사이클 → 주기 = **1 측정 사이클 시간**. 데이터시트 단일값 없음 → **실측 필요** (참고 상한: §3.4 self-cap 3ch NP 예시 16ms) |

## 왜 주기가 단일 숫자가 아닌가
Report Rate 레지스터가 **0ms**면 보고 지연 없이 "as fast as possible". 측정 사이클 시간은 ATI Target·conversion freq·채널 수에 좌우되므로 칩에서 직접 재야 한다.

## 핵심 주의
- **t_Low = 200ms 아님** — 200ms는 마스터 무응답 시 *상한*일 뿐, 정상 통신 시 sub-ms.
- **"STOP 후 RDY High 200µs"는 원문에 없는 추정치** (RDY open-drain RC 의존). md 정리본 정정 권고.
- Sound1은 RDY를 **200ms 폴링**으로 받으므로 IC 고속 RDY 대비 대다수 윈도우가 닫힌다(정상). 실시간성 필요 시 **Event Mode + 인터럽트** 권장.

## 문서

| 문서 | 내용 |
|---|---|
| [00_입력](RDY%20윈도우%20타이밍%20분석/00_입력.md) | 오케스트레이터 입력·역할 분화 |
| [1_분석_데이터시트타이밍](RDY%20윈도우%20타이밍%20분석/1_분석_데이터시트타이밍.md) | RDY 전기특성·Timeout·STOP 지연 원문 추출 |
| [1_분석_스트리밍동작](RDY%20윈도우%20타이밍%20분석/1_분석_스트리밍동작.md) | Streaming vs Event·주기 관계·t_Low 시나리오 |
| [1_분석_레지스터기본값](RDY%20윈도우%20타이밍%20분석/1_분석_레지스터기본값.md) | NP/LP/ULP Report Rate·Timeout·모드 비트 default |
| [1_분석_펌웨어적용값](RDY%20윈도우%20타이밍%20분석/1_분석_펌웨어적용값.md) | Sound1 코드 실제 설정값 → RDY 타이밍 추정 |
| [1_분석_검증반론](RDY%20윈도우%20타이밍%20분석/1_분석_검증반론.md) | adversarial — 7개 오해 원문 반박 |
| [2_검증_핵심수치](RDY%20윈도우%20타이밍%20분석/2_검증_핵심수치.md) | fan-in 교차검증(합의·충돌·미해결) |
| [**3_종합결론**](RDY%20윈도우%20타이밍%20분석/3_종합결론.md) ★ | 최종 답·메커니즘·실측 권고 |
