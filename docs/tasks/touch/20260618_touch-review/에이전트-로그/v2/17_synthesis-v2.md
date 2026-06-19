---
name: touch-review-synthesis-v2
purpose: v1(01~08) + v2(10~16) 통합 종합 — 명제 G~J 최종 판정·핵심 질문 답·갈림길 갱신·retry-count 전략 영향·분석·계획 반영안
type: 에이전트-로그
maturity: stable
tags: [touch, iqs323, synthesis, ati-full, fixed, lta, rdy-window, streaming, retry-count, 명제_G, 명제_H, 명제_I, 명제_J]
---

# 17 종합 서기 v2 — v1·v2 통합 최종 판정

> **TL;DR**: 명제 G(통신 윈도우 비용 큼) 참, 명제 H(200ms 개방·force 불필요) 전부 거짓, 명제 I(auto-ATI=Re-ATI 동일어) 조건부 참, 명제 J(FIXED LTA delta 유효) 조건부 참. 핵심 갈림길은 여전히 "ATI Full 필요성(실측5)"이며, 명제_J의 counts 포화 반증이 FIXED 유지 전제를 "조건부"로 격하한다. v1 결론(조건부 채택 5개 보완)은 유효하나 Event Mode 전환 비교·counts 포화 감지 조건 2항이 계획에 추가된다.

---

## 1. 명제 G~J 최종 판정표

| 명제 | 내용 | 판정 | 핵심 근거 |
|---|---|---|---|
| **명제_G** | 통신 윈도우 폐쇄·재개방(force_window_open) 비용이 실제로 크다 | **참** | t_wait = 0.1~45 ms (DS §8.13); `read_register()` 1회 = force_window_open **2회** (`.c:L233·L246`, 13_code-verify §2); 최악 130 ms (45+20+45+20), 정상도 수 ms 이상; 100/200 ms 폴링 예산 내 비중 큼 |
| **명제_H-A** | 스트리밍 모드에서 통신 윈도우가 약 200ms 열려 있다 | **거짓** | 200 ms는 마스터 무응답 시 watchdog 상한(DS §8.8). 정상 서비스 시 t_Low ≈ 0.4~0.5 ms (11_streaming §3.2) |
| **명제_H-B** | 스트리밍 모드이므로 force_window_open이 불필요하다 | **거짓** | IQS323 RDY 주기 수 ms, 폴링 주기 100/200 ms → 폴링 시점에 윈도우 열림 확률 5% 이하 [추정]; force 없이 I2C 시도 시 구조적 통신 실패 (11_streaming §4.2) |
| **명제_H-C** | 스트리밍 read만으로 터치 상태 취득 충분하다 | **거짓** | RDY에 인터럽트 미연결, Report Rate=0(기본값) vs 200 ms 폴링 → 우연 포착 확률 극히 낮음 (11_streaming §5) |
| **명제_I** | auto-ATI와 Re-ATI는 큰 맥락에서 같은 말이다 | **조건부 참** | 동일 ATI 알고리즘 사용. Auto-ATI = POR 시 IC 내부 강제 1회; Re-ATI = LTA drift 조건부 런타임 재실행. 트리거 경로·시점 다름 (12_ati-lta §1.2, DS §5.9·§5.10) |
| **명제_J** | ATI Disabled + LTA IIR만으로 터치 판정(threshold)이 계속 유효하다 | **조건부 참** | 터치 판정식 `(LTA - Counts) > Touch Threshold`는 ATI 모드 무관 (DS §5.7); Re-ATI 없어도 LTA IIR이 noTouch 기준선 추적하면 delta 성립. **단**: counts 포화(Max Counts 근접) 시 delta가 threshold 미달로 실질 무효화 가능(15_adversary §3.2~§3.4); 급격 환경 변화 시 LTA 추적 지연으로 가짜 delta [추정] |

---

## 2. 핵심 질문 최종 답변

### (2-1) 통신 윈도우 비용 실제로 큰가

**결론: 크다 — 매 read마다 구조적으로 발생.**

`read_register()` 1회 호출 시:
1. `force_window_open()` 1차 (`.c:L233`) — 0.1~45 ms 대기 (DS §8.13)
2. `i2c_write(reg_addr, 1)` + STOP → 윈도우 닫힘 (DS §8.9)
3. `wait_rdy_window_closed()` — 최대 20 ms 타임아웃 (`.h:L32`)
4. `force_window_open()` 2차 (`.c:L246`) — 또 0.1~45 ms 대기
5. `i2c_read(2B)` + 닫힘 대기 — 최대 20 ms

**최악 상한 130 ms = 45+20+45+20** (13_code-verify §4).  
정상 시에도 수 ms ~ 20 ms이며, 100/200 ms 폴링 예산 대비 비중이 크다.  
비용의 원인은 "스트리밍 모드 자체"가 아니라 **스트리밍 + 폴링 주기 불일치**다.  
Event Mode + 인터럽트로 전환하면 force_window_open 첫 줄 즉시 반환(0xFF 전송 없음) → 비용 거의 0 (15_adversary §4.2).

---

### (2-2) 스트리밍 모드이면 force_window_open이 불필요한가

**결론: 현 폴링 구조에서 불필요 불가. Event Mode + 인터럽트 전환 시에만 비용 0.**

- 현 펌웨어: 스트리밍 모드 확인(System Control bit[7]=0, `.c:L382·L945·L1024·L1147·L1155`; 13_code-verify §3에서는 "스트리밍 모드 설정 코드 0건"으로 보고했으나 11_streaming §2.1의 System Control write 분석이 우선 — Report Rate write 없으므로 기본값 0ms = 측정 완료 즉시 RDY 토글).
- RDY 주기 수 ms vs 폴링 주기 100~200 ms → 폴링 도착 시 윈도우 열림 확률 5% 미만 [추정].
- 200 ms는 Transaction Timeout(무응답 watchdog) 상한이지 t_Low(개방 유지 시간)가 아님(DS §8.8).
- **스트리밍 모드 + 폴링 구조에서 force_window_open은 구조적 필수 요소.**
- 단, Event Mode + falling-edge 인터럽트 전환 시 이 비용이 사라짐 (DS §8.11.2 제품 권장).

---

### (3) auto-ATI = Re-ATI 정리

| 구분 | auto-ATI (POR ATI) | Re-ATI |
|---|---|---|
| 발동 | 전원 ON(MCLR 리셋) 후 IC 내부 자동 1회 | LTA가 `ATI Target ± ATI Band` 이탈 시 자동; `0xC0 bit2`로 수동 트리거 가능 |
| 알고리즘 | **동일** — MULT/COMP 자동 산출, Counts → ATI Target 수렴 | 동일 |
| 코드 감지 | `ATI_Active(bit5)` high 구간 폴링 (`.c:L334~378`) | `ATI_Event(bit4)` set |
| 전원 ON + 터치 시 | Counts = 터치 counts → ATI Target 수렴 불가 → timeout "정상" (`.c:L376` 주석) | RESEED로 LTA를 현재 counts에 맞추고 진행 |

**결론**: 같은 ATI 알고리즘이나 트리거 경로·시점이 달라 완전 동의어가 아님.  
은수님 질문("켜질 때 터치 상태면 auto-ATI 수행")은 **맞음** — POR 시 auto-ATI 즉시 실행, 터치 중이면 수렴 실패(timeout)가 "정상"으로 처리됨.

---

### (3-1) FIXED LTA delta로 threshold 계속 유효한가

**결론: 정상 counts 범위 내에서는 유효. counts 포화 근접 시 실질 무효화 위험.**

유효한 이유:
- 판정식 `(LTA - Counts) > Touch Threshold`는 ATI 모드와 무관 (DS §5.7).
- LTA IIR은 ATI 상태와 무관하게 매 측정마다 noTouch 기준선 추적.
- Re-ATI 없어도 MULT/COMP 고정 → noTouch counts 절대값 유지 → LTA 추적 → delta 보존.
- 현 펌웨어 ATI Disabled + RESEED + IIR 구조가 실제 동작 중이며 의도된 설계.

유효성 붕괴 조건 (15_adversary §3.2~§3.4):
- **counts 포화**: 환경 drift로 noTouch Counts가 Max Counts 근접 → 터치 시 감소 폭 제한 → delta < threshold → **터치 미인식(침묵 실패)** [추정, DS 미규정]
- **LTA 추적 지연**: 급격 환경 변화 → Beta 스무딩으로 LTA가 뒤처짐 → 가짜 delta → 오인식 또는 먹통 [추정]
- **기기 간 편차**: ATI Full 없으므로 MULT/COMP 고정 → 기기별 noTouch counts 편차 → threshold 마진 감소 [추정]

따라서 명제_J는 "합리적 counts 범위 안에 머무는 한"이라는 묵시적 전제 하에서만 참.  
실측_V1(noTouch Counts 장기 범위)·실측_V2(Max Counts 값) 없이는 포화 위험 현실성 미확정.

---

## 3. 상위 갈림길(ATI Full vs FIXED) — v2 갱신 판정

### v1 결론 (분석.md §8 핵심 갈림길)
> "FIXED 유지 시 I2C 블로킹 문제 원천 제거 — 실측5(환경 drift 실재성)가 최우선 결정 게이트."

### v2 갱신

| 항목 | v1 판정 | v2 갱신 | 근거 |
|---|---|---|---|
| FIXED 유지의 위험 | 장기 감도 열화 리스크(언급) | **counts 포화 침묵 실패 경로 추가(High)** | 15_adversary §3.4; 16_failmode C-1·C-2 |
| 명제_J 참 신뢰도 | 참 | **조건부 참으로 격하** | counts 포화 반증 경로가 "threshold 항상 유효" 전제를 깸 |
| 실측 우선순위 | 실측5(drift 실재성) 최우선 | 실측5 유지 + **실측_V2(Max Counts)를 FIXED 유지 결정 선행 게이트 추가** | 포화 margin 없이 FIXED 유지 결정 불완전 |
| FIXED 유지 선택 시 | Phase 1a·retry-count 불필요 | **Counts 포화 감지 조건 추가 필요(C-1 대응)** | 16_failmode §5 우선순위 3 |
| Event Mode 전환 | Phase 3 선택 사항 | **통신 비용 절감(명제_G) 관점에서 우선순위 격상 필요** | 15_adversary §2.4·§4.3; 16_failmode A-1 |

**갱신 결론**: 갈림길 구조는 유지되나, FIXED를 "안전한 기본"으로 선택해도 counts 포화 감지 로직이 없으면 침묵 실패 위험이 남는다. ATI Full이든 FIXED든 **counts 범위 모니터링이 기반 전제**가 됨.

---

## 4. retry-count 전략 — v2 영향 평가

### v1 결론 (분석.md §8): 조건부 채택, 5개 보완 이행 전제

v2(10~16)의 주요 추가 발견이 retry-count 전략에 미치는 영향:

| v2 발견 | retry-count에 대한 영향 |
|---|---|
| **read_register = force_window_open 2회** (13_code-verify §2) | read 1회 실패 = 최대 130 ms 블로킹. 폴링 주기 100 ms 안에서 타임아웃이 터지면 연속 실패로 카운터가 과다 증가할 수 있음. 보완_3(K 산정)에 "force_window_open 이중 타임아웃 가능성" 반영 필요 |
| **ATI 실행 중 RDY 무응답** (16_failmode E-2) | ATI_Error 무시 상태에서 ATI가 비정상 루프 시 → 매 폴링마다 force_window_open 2회×45 ms = 90 ms 대기 → retry 카운터 폭발. 보완_1(ATI_Error 핸들러)이 이를 막는 유일한 경로 — v1 결론 강화 |
| **절전 클럭 강하 시 타임아웃 단축** (16_failmode D-1, `.h:L31`) | `ci_timer_get_tick()`이 사이클 카운트 기반이면 절전 중 실효 타임아웃 단축 → 정상 응답 IC를 타임아웃 실패로 오판 → 카운터 오증가. 카운터 절전 상태 분기 처리 검토 필요 |
| **RESEED 직후 LTA 수렴 지연** (16_failmode B-1) | 전원 ON 직후 터치→손 뗌 구간에서 터치 미인식. I2C는 정상이나 delta가 threshold 미달 → read_status 성공이지만 터치 상태 오판. retry-count와 무관하며 Phase 1b(노터치 게이트)가 담당 |

**v2 영향 요약**: v1의 5개 보완 결론은 유효. 추가 고려: ① K 산정 시 force_window_open 이중 타임아웃(130 ms 상한) 반영, ② 절전 클럭 분기 처리, ③ ATI_Error 핸들러(보완_1)의 필수성이 v2에서 더 강화됨.

---

## 5. 분석.md / 계획.md 반영안

### 5-1. 분석.md §8 추가 섹션 (v2 보완)

아래 내용을 분석.md §8 끝에 추가:

```
### v2 추가 판정 (2026-06-18 에이전트-로그 10~16, 17)

명제_G~J 최종 판정 (에이전트-로그 17 §1 참조):
- 명제_G(통신 윈도우 비용): 참 — read 1회 = force×2, 최악 130ms
- 명제_H(스트리밍 200ms·force 불필요): 거짓 — 200ms는 watchdog 상한, force는 구조 필수
- 명제_I(auto-ATI=Re-ATI): 조건부 참 — 알고리즘 동일, 트리거 경로 다름
- 명제_J(FIXED LTA delta 유효): 조건부 참 — counts 포화 근접 시 실질 무효화 위험

갈림길 갱신: FIXED 유지 선택 시에도 counts 포화 감지 조건 추가 필요 (침묵 실패 C-1).
실측_V2(Max Counts 값) 확보 전 FIXED "안전" 선언 불완전.
Event Mode 전환은 Phase 3 선택이나 통신 비용 절감 관점에서 우선순위 재검토 권고.
```

### 5-2. 계획.md 반영 권고

| 항목 | 위치 | 변경 내용 |
|---|---|---|
| 실측 게이트 추가 | §2 실측 게이트 표 | **실측_V2(Max Counts 값)**: 데이터시트 정밀 확인 또는 채널 차폐 후 포화 측정 → FIXED 유지 안전마진 확인 |
| 실측 게이트 추가 | §2 실측 게이트 표 | **실측_V3(스트리밍 전류 소모)**: NP 모드 + Report Rate=0 조건 전류 프로브 → Event Mode 전환 전력 절감 정량 |
| Phase 0 추가 항목 | §1 Phase 0 | **Counts 포화 감지 stub 추가**: `tdc_drv_iqs323_read_status()` 내 Counts 상한 경보 조건 (`ci_printw`) — FIXED/ATI Full 공통. 침묵 실패(C-1) 조기 감지 |
| Phase 1a 보완_3 갱신 | §3 Phase 1a ② | K 산정 시 "force_window_open 이중 타임아웃(130 ms) 고려" 명기. 실측6(t_ati) + 실제 read 1회 소요 시간 RTT 측정 병행 권고 |
| Phase 3 우선순위 재검토 메모 | §6 Phase 3 | "Event Mode + 인터럽트 전환 시 force_window_open 0xFF 비용 0 → 통신 지연 원천 제거. 절전 전류 실측(실측_V3) 후 Phase 3 우선순위 재검토 권고" 주석 추가 |
| 갈림길 §0-A 보완 | §0-A | "FIXED 유지 선택 시에도 counts 포화 감지 로직(Phase 0 추가) 필수. 실측_V2 확보 전 FIXED 안전 선언 불완전" 경고 추가 |

---

## 6. 통합 판정 요약 (12줄)

1. **명제_G 참**: read 1회 = force×2, 최악 130 ms — 비용 크다. 원인은 "스트리밍+폴링 불일치" (10·13노드).
2. **명제_H 전부 거짓**: 200 ms는 watchdog 상한, t_Low 0.5 ms; force는 현 폴링 구조의 구조 필수 요소 (11·13노드, DS §8.8·§8.9).
3. **명제_I 조건부 참**: auto-ATI(POR 강제 1회) vs Re-ATI(LTA drift 조건부). 알고리즘 동일, 트리거 다름 (12노드).
4. **명제_J 조건부 참**: LTA IIR + threshold 판정식은 ATI 모드 독립. 단 counts 포화 시 침묵 실패 위험(15·16노드).
5. **갈림길 갱신**: FIXED 유지도 counts 포화 감지 없이는 불완전. 실측_V2(Max Counts) 추가 게이트 필요.
6. **Event Mode 격상**: 통신 비용 절감의 근본 해법. DS §8.11.2 제품 권장. Phase 3 우선순위 재검토 권고.
7. **retry-count v1 결론 유지**: 5개 보완 유효. K 산정에 force 이중 타임아웃(130 ms) 반영 추가.
8. **보완_1(ATI_Error 핸들러) 필수성 강화**: E-2 연쇄(ATI_Error × 스트리밍 miss)가 사실상 서비스 불능 수준 — v2가 v1 결론을 더 강하게 지지.
9. **RESEED 직후 LTA 수렴 지연(B-1)**: 전원 ON 직후 터치→손 뗌 구간 수 초 터치 불능 — Phase 1b 노터치 게이트가 담당, retry-count와 무관.
10. **절전 클럭 타임아웃 위험(D-1)**: `ci_timer_get_tick()` 절전 추종 여부 확인 후 분기 처리 검토 (Phase 1a 추가 확인 항목).
11. **분석.md §8**: v2 판정 섹션 추가 필요 (본 문서 §5-1 내용).
12. **계획.md**: 실측_V2·V3 게이트 추가, Phase 0 포화 감지 stub, Phase 1a 보완_3 갱신, Phase 3 우선순위 메모 추가 필요.
