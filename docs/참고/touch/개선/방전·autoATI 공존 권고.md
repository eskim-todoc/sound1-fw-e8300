---
name: touch-개선-방전autoATI-공존권고
purpose: auto-ATI 중 CRX0 수동방전 공존 가능성 검토의 정식 결과·대안 권고
type: 개선
maturity: stable
tags: [touch, iqs323, auto-ati, discharge, calcap, esd, coexist, recommendation]
---

# auto-ATI 중 CRX0 방전 공존 — 평결·권고 (정식)

> **TL;DR**: ESD 물리누적이 미지수라 방전(CRX0 VSS + CRX1 CalCap)을 유지하며 auto-ATI를 쓰려는 질문에 대해 — **"auto-ATI Full 상시"로는 공존 불가**(Re-ATI 진동·I²C hang·0x30 비트 충돌), **단 "auto Re-ATI만 끄면(부팅 1회 ATI + Disabled 고정 + LTA 추적) 공존 가능"**. **🔴 그러나 먼저 "현재 방전(0x30 0x02)이 실칩에서 진짜 핀 VSS 방전인가"부터 의심·실측해야 한다** — 데이터시트상 0x02는 VSS가 아니라 Linearise.

> [!NOTE]
> 검증 과정: [방전·autoATI 공존 검토/](방전·autoATI%20공존%20검토/) — [00 입력](방전·autoATI%20공존%20검토/00_입력.md)·[1_분석 C1~C5]·[2_검증 W1~W4](방전·autoATI%20공존%20검토/2_검증_핵심주장.md)·[3_종합평결](방전·autoATI%20공존%20검토/3_종합평결.md). 상위: [은수님 안 평결·하이브리드 권고](은수님%20안%20평결·하이브리드%20권고.md).

---

## 1. 은수님 질문에 대한 답

> "ESD 물리누적이 미지수이면, CRX1을 CalCap으로 살리고, CRX0 수동방전을 auto-ATI 사용 중에도 할 수 있는지, 어렵다면 어떻게 하는 게 좋을지."

| 질문 | 답 |
|---|---|
| **CRX1 CalCap 유지?** | 방전(채널 토글)을 유지하는 한 measurement cycle 유지용으로 **그대로 살림**. 방전 폐기 시 CalCap도 불요 |
| **auto-ATI 중 방전 가능?** | **"Full 상시"는 불가**(아래 충돌). **"부팅 1회 ATI + auto Re-ATI 끔"이면 가능** |
| **어려우면?** | ① 통신관리 선행 ② **방전 진위 실측**(0x30=VSS?) ③ ESD 정체 실측 ④ 물리누적이면 방전 안전화 또는 HW 방전 |

## 2. 왜 "Full 상시"는 어려운가 (충돌 4축)

| 축 | 내용 |
|---|---|
| 가 | 방전이 `0x30 LSB`를 blind write → Linearise·Invert·Release UI·Enable 비트 매 200ms 클리어 |
| 나 | 방전 복원 카운트 튐 → LTA가 ATI Band 이탈 → **auto Re-ATI 재트리거 → 진동** |
| 다 | ATI 수렴 중 채널 disable 토글이 수렴 교란 |
| 라 | **Full auto-ATI = I²C 무응답**(현 펌웨어가 ATI Disabled 택한 실증 이유) |
| 🔴 치명 | I²C hang 중 방전 disable(0x02)만 들어가고 복원(0x01)이 막히면 **CH0 영구 disable + 터치 먹통**(verify 없어 SW 감지 불가) |

→ **충돌 근원은 "런타임 자율 Re-ATI" 하나.** auto-ATI(부팅 캘리브레이션)와 auto-Re-ATI(런타임 자동 재보정)를 분리해 **후자만 끄면 진동(나·다)이 원천 제거**된다.

## 3. 🔴 우선 짚을 것 — "현재 방전이 진짜 방전인가"

검증이 라이브로 발견: `discharge_crx0`의 `0x30 LSB 0x02`는 데이터시트 A.5상 **"Linearise=1 + Enable=0"**이지 **CRX0 VSS가 아니다**. 핀 VSS 단락은 `0x34`(Inactive Rxs), CRx 핀 enable은 `0x33`. → **현재 코드는 "채널 disable→enable + Linearise flip"일 뿐, 핀-VSS 물리 단락이 일어난다는 보장이 없다.** 

**즉 방전 존폐를 논하기 전에, 현재 방전이 ESD 전하를 실제로 빼고 있는지부터 실측해야 한다.** 만약 안 빼고 있다면 방전 코드를 `0x34`/`0x33` 경로로 고쳐야 진짜 방전이 된다(이 경우 0x30 비트 충돌도 자연 해소).

## 4. 레이어 분리 (방전의 고유성)

방전(핀 전기상태=①입력단)은 auto-ATI/Re-ATI(IC 내부 MULT·COMP·LTA=②③)로 **대체 불가한 고유 기능이 맞다**. 단 **조건부**: ESD가 LTA 드리프트(온·습도)면 auto-ATI/LTA가 흡수 → 방전 redundant. ESD가 핀 물리 전하 누적이면 방전 고유 기능 → 유지 필요. **이 갈래 판별이 ESD 실측의 핵심.**

## 5. 권장 단계 (실측 게이트 중심)

| 순서 | 작업 | 목적 |
|---|---|---|
| 0 | 통신관리 선행(RDY 핸드셰이크·타임아웃·방전 verify+강제복원) | I²C hang·CH0 영구 disable 방지 |
| 1 (실측) | **방전 물리효력**(0x30 0x02 실칩 VSS?) + **ESD 정체**(드리프트 vs 물리누적) | 방전 진위·존폐 대전제 |
| 2 | **auto Re-ATI 억제**(부팅 노터치 게이트 후 ATI 1회 → Disabled 고정 + 수동 Re-ATI + LTA 추적) | 진동 원천 제거, 공존 성립 |
| 3a | ESD=드리프트 → **방전 폐기**(auto-ATI/LTA 해소) | 공존 문제 소멸 |
| 3b | ESD=물리누적 → **방전 유지+안전화**(0x30 RMW 또는 0x34 경로·verify·빈도 2~5s) | 충돌·영구 disable 완화 |
| 4 | (심함) **HW 방전**(직렬저항·블리더) | 0x30 토글 제거 = 충돌 근본 소멸 |

## 6. 하이브리드 권고와의 정합

본 결과는 [하이브리드 권고](은수님%20안%20평결·하이브리드%20권고.md) **§4 Phase 3(방전·CalCap 폐기 검토, ESD 실측 후)** 를 정교화한다:
- 하이브리드의 "ATI Full"은 **"부팅 1회 ATI + auto Re-ATI 억제"로 구체화**해야 방전과 공존(또는 방전 폐기 결정)이 안전.
- ESD 실측 게이트는 하이브리드 실측 게이트와 동일 1순위.
- **추가**: "현재 방전 물리효력(0x30=VSS) 실측"이 신규 최우선 항목.

## 7. 한 줄 결론

> **auto-ATI를 "Full 상시"가 아니라 "부팅 1회 + auto Re-ATI 끔"으로 쓰면 방전과 공존 가능하다. 단 그 전에 "현재 방전이 실제 핀 VSS 방전인지(0x30=VSS)"와 "ESD가 물리누적인지 LTA 드리프트인지"를 실측해야 하며, 드리프트면 방전 자체가 불필요하고 물리누적이면 방전을 안전화(RMW·verify·빈도·HW)해 유지한다.**
