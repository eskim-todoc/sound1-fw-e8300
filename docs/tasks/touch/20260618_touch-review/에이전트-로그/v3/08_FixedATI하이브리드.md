---
name: FixedATI하이브리드-핵심아키텍처
purpose: Fixed(안정성)와 Auto-ATI(적응성)를 조합해 부팅 터치 모호성·보드편차·드리프트를 다층 방어로 최대 근접 해결하는 핵심 아키텍처 도출
type: 에이전트-로그
maturity: in-progress
tags: [touch, iqs323, fixed, auto-ati, hybrid, cold-start, calibration, no-touch-gate, architecture, v3]
---

# 08 — Fixed+Auto-ATI 하이브리드 핵심 아키텍처

**TL;DR**: Fixed는 "부팅 안전"을, Auto-ATI는 "환경 적응"을 전담한다. 결합 원리는 하나다 — **공장 절대 기준으로 노터치를 판별하고, 노터치가 확인된 순간에만 Re-ATI를 허용**한다. 이 게이트 하나가 두 방식의 단점을 교차 보완하고, 크래들·사후 검증·자기 치유가 나머지 리스크를 다층으로 흡수한다.

---

## 1. 두 방식의 강점·약점 교차 분석

| | Fixed (현 펌웨어) | Auto-ATI Full |
|---|---|---|
| **강점** | 부팅 터치 시 게인 왜곡·ATI_Error 없음, 결정론적, I2C 블로킹 없음 | 보드편차 흡수(MULT/COMP 자동 정규화), 환경 드리프트 자동 보정 |
| **약점** | 보드·환경 편차 미보정 → counts 포화 침묵 실패(명제_J 조건부, v2 C-1) | 부팅 터치 시 게인 왜곡 + ATI_Error → RESEED 오염 연쇄, I2C 블로킹 |

**교차 보완 원리**: Fixed 결함(편차·드리프트) = Auto-ATI의 강점. Auto-ATI 결함(부팅 터치 취약) = Fixed의 강점. 두 방식이 서로의 약점을 정확히 채운다. 단, 이 교차 보완을 실현하려면 **"언제 Auto-ATI를 허용하는가"** 조건이 명확해야 한다. 그 조건이 노터치 게이트다.

---

## 2. 하이브리드 구조 — 세 층위

### 층위 1: Fixed 기저 (항상 유효)

- 매 부팅: MCLR → 공장 MULT/COMP 고정 write (`iqs323.c:651, :700~735`) → ATI Mode Disabled (`0x36` bits[2:0]=000)
- 공장 MULT/COMP는 보드별 공장 Auto-ATI 1회로 산출·EEPROM 저장 [추정: EEPROM 경로 신규 구현 필요, 확정 필요]
- 이 Fixed baseline이 항상 살아 있기 때문에 노터치 게이트 실패·race 시에도 최소 동작이 보장된다

**현 펌웨어 차이**: 현재는 공장 개별 캘리브레이션 없이 단일 고정값(모든 보드 동일) write. 공장 EEPROM 저장이 없으면 보드편차 보정 능력은 없고 Fixed 방식 그 자체다.

### 층위 2: 노터치 게이트 + Re-ATI (핵심 전환 메커니즘)

**"Fixed로 시작하고, 노터치가 확인된 순간 Re-ATI를 허용한다"**

```
부팅
  → 공장 MULT/COMP Fixed 적용
  → 현재 Counts 읽기
  ├─ Counts ∈ [공장 noTouch − 허용, 공장 noTouch + 허용]  → 노터치 확정
  │     → Re-ATI 실행 (게인·보상 그 시점 환경 기준 재보정)
  │     → Re-ATI 완료 대기 (ATI_Active bit5 폴링)
  │     → ATI_Error 감지 → 핸들러 → 수동 Re-ATI 재시도
  │     → 이후 LTA IIR 드리프트 추적 (런타임 자동)
  └─ Counts ≪ 공장 noTouch (터치 의심)
        → Re-ATI/RESEED 보류 (오염 방지)
        → Counts 회복 (손 뗌, counts 급상승) 감지 후 → Re-ATI ✅
        → timeout → Fixed 유지 + 저신뢰 경보 (자기 치유 대기)
```

**RESEED vs Re-ATI 선택**: 노터치 게이트 하에서 RESEED보다 Re-ATI가 우월하다(브레인스토밍 §6-B). RESEED는 LTA baseline만 갱신하고, Re-ATI는 MULT/COMP(게인) + baseline 전부를 그 시점 환경 기준으로 재보정한다 — 이것이 보드편차·온도·경년 드리프트를 포괄적으로 흡수한다. (`개념정리_터치동작모델.md §3-A` 참조: MULT가 층위①·②의 연결고리)

**공장값의 역할**: 절대값으로 LTA를 seed하는 게 아니다(RESEED 제약: 현재 counts로만, 데이터시트 §5.5.1). 공장값은 "지금 터치 중인가"를 판별하는 **절대 잣대**로만 사용한다 — 이 역할을 공장값이 없으면 어떤 SW도 대체 불가능하다.

### 층위 3: 다층 보완 (race·edge case 흡수)

| 보완 장치 | 담당 시나리오 | 근거 |
|---|---|---|
| **크래들 뚜껑 닫힘** | 노터치 절대 보장 구간 — race 자체 소멸. 현재 터치 폴링 완전 중단(미활용 상태). 뚜껑 닫힘 직후 Re-ATI 실행 기회 | `main.c:807` `func_cradle_lid_closed_loop`, 전제4 |
| **ATI_Error 핸들러** | Re-ATI 실패(부팅 터치 등) 시 수동 Re-ATI `0xC0 bit2` 재시도. 무시(현재 `(void)`) 제거 필수 | `touch.c:393`, v1 보완_1 |
| **사후 검증** | Re-ATI 실행 직후 Counts가 noTouch 윈도우 안인지 재확인 → 벗어나면 재시도 (race 사후 검출) | 브레인스토밍 §6-B(2) |
| **자기 치유** | race로 Re-ATI가 터치 상태를 캡처해도, 손 뗌 후 Counts가 윈도우 밖으로 회복 → 재게이트 또는 LTA IIR 수렴 → 일시 오류, 영구 먹통 아님 | 브레인스토밍 §6-B(2) |
| **stuck-touch 가드** | 부팅 터치 + LTA freeze 상태에서 CH timeout 재활성 → 무한 터치 차단 | `분석.md §2` ATI Disabled + CH timeout 비활성(이중 차단) 현황 |
| **counts 포화 감지 stub** | Fixed 유지 구간에서도 Max Counts 근접 시 경보 (`ci_printw`) — 침묵 실패(명제_J C-1) 조기 감지 | v2 분석 17_synthesis §5-2, 계획 Phase 0 |

---

## 3. "언제 Fixed, 언제 Re-ATI" — 전환 조건 정리

| 상태 | 동작 | 이유 |
|---|---|---|
| **부팅, Counts ∈ 노터치 윈도우** | Re-ATI 즉시 허용 | 노터치 확정 → 게인 재보정 안전 |
| **부팅, Counts ≪ 노터치 윈도우 (터치 의심)** | Fixed 유지, Re-ATI 보류 | 터치 기준 게인 왜곡·오염 방지 |
| **부팅, timeout (손 안 뗌)** | Fixed 유지 + 경보 | 최소 동작 보장, 자기 치유 대기 |
| **크래들 뚜껑 닫힘 직후** | Re-ATI 즉시 허용 | 노터치 절대 보장, race 없음 [확정 필요: 코드 구현 필요] |
| **런타임 정상 동작** | LTA IIR 자동 추적 (Re-ATI 조건부) | LTA drift → ATI Band 이탈 시 자동 Re-ATI |
| **충전기 연결 중** | 터치 계속 동작, 필요 시 Re-ATI 게이트 동일 적용 | `systemControl.c:162` df_Connected → 절전 진입 안 함 |
| **절전 진입** | RESEED (현재 counts로 LTA seed) + threshold 255 | 절전 복귀 = 재부팅 → 이후 부팅 시퀀스가 Re-ATI 게이트 적용 |
| **ATI_Error 발생** | ATI_Error 핸들러 → 노터치 확인 후 수동 Re-ATI 재시도 | 영구먹통 경로 차단 |

---

## 4. 100% 근접 달성도 평가

| 문제 | 해결 메커니즘 | 달성도 |
|---|---|---|
| 부팅 터치 모호성 (Cold-Start Ambiguity) | 공장 noTouch 절대 기준 → 부팅 즉시 노터치/터치 판별 | ✅ (공장 캘리브레이션 구현 후) |
| 보드편차 (MULT/COMP 고정 감도 불균일) | 공장 캘리브레이션으로 보드별 MULT/COMP 개별화 + 노터치 게이트 후 Re-ATI | ✅ (공장 캘리브레이션 구현 후) |
| 환경 드리프트 (온도·경년) | 노터치 게이트 후 Re-ATI + LTA IIR 런타임 추적 | ✅ |
| 부팅 터치 게이트 race | 크래들 노터치 보장(race 소멸) → 사후 검증 → 자기 치유 다층 | ✅ 충분히 낮은 확률 + 자기 치유 [추정] |
| ATI_Error 영구먹통 | ATI_Error 핸들러 + 수동 Re-ATI 재시도 | ✅ (보완_1 구현 후) |
| counts 포화 침묵 실패 (명제_J C-1) | Re-ATI로 Counts 절대 위치 재정규화 + 포화 감지 stub | ✅ (Re-ATI 활성 후) |
| 물리 ESD 전하 누적 | 독립 트랙 (HW 블리더 or 주기 방전) | 별도 — 이 아키텍처로 해결 불가 |
| 부분 터치 모호성 | 본질적 물리 한계 — 다층 완화 (절대 윈도우 + 손뗌 감지 + timeout) | 확률적 완화만 가능 |

**달성 가능한 현실적 목표**: "ESD·부분 터치를 제외한 모든 전기·SW 레벨 문제 = 다층 방어로 실사용 무시 가능 수준". 100% 불가 사유는 물리적 race·부분 터치 모호·ESD — SW로 원천 제거 불가.

---

## 5. 구현 순서 (계획 반영 권고)

```
Phase 0:  counts 포화 감지 stub (기반 전제, FIXED/ATI Full 공통)
Phase 1:  ATI_Error 핸들러 구현 (보완_1, 최우선 — 없으면 나머지 무의미)
Phase 2:  노터치 게이트 구현 (공장 noTouch 절대 기준 윈도우 비교)
          ├─ 현재: 공장값 EEPROM 없이 단일 고정값 사용 → 우선 고정값 윈도우로 1차 구현
          └─ 공장 캘리브레이션(EEPROM 저장·로드) 경로 추가 [확정 필요: ati-calib-mode 기존 작업 활용]
Phase 3:  Re-ATI 활성화 (노터치 게이트 위에 조건부 허용)
          + ATI_Active 폴링 대기 루프 (기존 iqs323.c:334~378 활용)
Phase 4:  크래들 뚜껑 닫힘 시 Re-ATI 실행 기회 연결 (main.c:807 활용)
Phase 5:  실측 게이트 — 실측5(Fixed 환경 drift 실재성), 실측_V2(Max Counts), t_ati 실측(K 산정)
```

**선행 필수 실측**:
- 실측5: Fixed 유지만으로 현장 drift가 실재하는지 — 없으면 Phase 3 불필요
- 실측_V2: Max Counts 확인 — counts 포화 윈도우 설정 기준
- [확정 필요] 공장 noTouch counts 절대값 안정 readout 가능성 — 데이터시트 + RTT 실측

---

## 6. 핵심 결론 (7줄)

1. Fixed는 부팅 안전 기저, Re-ATI는 노터치 확인 후에만 허용 — 이 단 하나의 조건이 두 방식을 결합한다.
2. 공장 noTouch 절대 기준이 없으면 "노터치 판별"이 불가능하다 — 공장 캘리브레이션이 전 아키텍처의 선행 조건이다.
3. 노터치 게이트 후 RESEED보다 Re-ATI가 우월하다 — MULT/COMP 게인까지 그 시점 환경 기준으로 재보정되기 때문이다.
4. 크래들 뚜껑 닫힘 구간이 유일한 race-free 기회다 — 현재 미활용(터치 폴링만 중단), Re-ATI 연결이 최대 이득 지점이다.
5. ATI_Error 핸들러(보완_1)가 단 하나의 필수 전제다 — 없으면 Re-ATI 활성화가 오히려 영구먹통 경로를 열 수 있다.
6. 게이트 race는 제거 불가이나 치명적이지 않다 — 크래들(소멸)·사후검증·자기치유 다층이 확률을 무시 가능 수준으로 누적 감쇄한다.
7. ESD·부분 터치는 이 아키텍처 범위 밖이다 — 각각 HW 블리더/주기 방전·절대 윈도우+timeout 완화로 별도 처리한다.
