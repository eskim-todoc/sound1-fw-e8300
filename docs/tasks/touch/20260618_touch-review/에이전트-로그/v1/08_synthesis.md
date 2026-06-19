---
name: touch-review-synthesis
purpose: 7개 노드(분석5·반증2) 종합 — 은수님 retry-and-count 전략 최종 판정표·핵심 갈림길·채택 권고·실측 게이트 갱신·계획 재설계 권고
type: 에이전트-로그
maturity: stable
tags: [touch, iqs323, retry-and-count, ati-full, synthesis, i2c, failure-mode, 판정]
---

# 08 종합 서기 — retry-and-count 전략 최종 판정 및 권고

> **TL;DR**: 은수님 retry-and-count 전략은 "조건부 채택"이다. 가정·전략·판별 모두 전제 조건이 충족될 때에만 성립하며, ATI_Error 영구먹통·POR burst 오판·ATI Full 전제 미검증이라는 3대 구조적 결함을 보완해야 한다. 핵심 갈림길은 "ATI Full 도입 여부"이며, 이 결정이 retry-count 논의 전체의 상위 분기다. 채택 시 5개 필수 보완이 요구된다.

---

## 1. 가정·전략·판별 최종 판정표

| 항목 | 판정 | 근거 (노드·라인) |
|---|---|---|
| **가정_1** (ATI Full 중 I2C 실패 발생) | **타당** | §8.4: ATI 중 RDY window 미열림 → `force_window_open()` 타임아웃으로 false 반환 확인 (02노드, `iqs323.c:143~172`) |
| **가정_2** (터치 중 auto-ATI 미동작) | **조건부 성립** | 정상 터치 인식 후(CHx Touch bit=1)에는 LTA freeze → ATI Band 안 유지 → Re-ATI 간접 억제(03노드 §3.2). **단**: 부팅 시점 터치에서는 delta≈0 → 터치 미인식 → freeze 없음 → Re-ATI 발동 가능(03노드 §5.2 시나리오_나). 데이터시트에 터치 중 Re-ATI 억제 명시 문구 없음(DS 미규정). |
| **전략** (실패=auto-ATI 간주 → 재시도) | **구조적 결함** | `is_auto_ati_done_single_read()`가 통신 실패와 ATI 진행 중을 모두 false로 반환하여 구분 불가(02노드 §3.4, `iqs323.c:312~331`). ATI_Error 발생 시 ATI_Active=0 → RDY 재개방 → read_status 성공 → 카운터 증가 없음 → 자동 복구 주체 없음(06노드 §3-B). |
| **판별** (10회=고장) | **부분 기각** | POR auto-ATI 1.5초 burst가 POLL_INTERVAL=100ms 기준 N_fail=15를 만들어 정상을 고장으로 오판(04노드 §2.3·§2.4). 런타임 Re-ATI(~100~수백ms)에 한정하고 t_ati 실측 후 K 재산정 시 조건부 성립(04노드 §4 결론). |
| **근거_확률** (10회 연속 확률 낮다) | **조건부 성립** | t_ati < T_read이면 독립 베르누이 p^10≈0으로 타당(04노드 §2.2). **단**: t_ati ≥ 10×T_read이면 burst 1회가 임계를 채워 독립 가정 붕괴(04노드 §2.3). |

---

## 2. 핵심 갈림길 — ATI Full 도입 vs FIXED 유지

### 갈림길 구조

```
         ┌─ ATI Full 유지(FIXED) ─────────────────────────────┐
         │   retry-count 논의 전체 불필요                      │
         │   I2C 블로킹 원천 제거                              │
지금 ──► │                                                     │
         │   단: 환경 drift 보정 없음 → 장기 감도 열화 리스크  │
         └─────────────────────────────────────────────────────┘

         ┌─ ATI Full 도입 ─────────────────────────────────────┐
         │   retry-count 전략이 의미를 가짐                    │
         │   5개 필수 보완 요구                                │
         │                                                     │
         │   단: I2C 무응답 재현 경로(이미 실증: drv.c:644)    │
         └─────────────────────────────────────────────────────┘
```

**이 결정이 retry-count 논의 전체의 상위 분기다.** FIXED가 필드에서 충분하다면 Phase 1a~retry-count 논의 자체가 불필요해진다(06노드 §2-B).

### ATI Full 결정 전에 반드시 알아야 할 실측 항목

| 실측 항목 | 측정 방법 | 결정 내용 |
|---|---|---|
| **실측_A**: FIXED 환경 변화 실재성 | 현장 조건(온도·습도 변화)에서 장기 LTA drift 측정 → ATI Band 이탈 빈도 확인 | FIXED 유지 충분 여부 판단 (이 결과가 나와야 ATI Full 도입이 필요한지 결정 가능) |
| **실측_B**: ATI Full 재활성 I2C 무응답 재현 | ATI Full 전환 후 RTT로 RDY 점유·통신 멈춤 확인 | Phase 1a/2 설계 기준 (기존 실측2) |
| **실측_C**: t_ati 정량 (ATI_Active bit 구간 RTT 측정) | RTT로 ATI_Active=1 → 0 구간을 ms 단위 측정(런타임 Re-ATI·POR 각각) | 임계 K 산정 기준 — 이 값 없이 K=10은 추정치에 불과 |
| **실측_D**: ATI_Error 발생 빈도 | ATI Full 재활성 후 일정 시간 동안 0x10 bit6 발생 횟수 확인 | ATI_Error 핸들러 우선순위 결정 |

> [!IMPORTANT]
> **실측_A가 가장 먼저**여야 한다. "FIXED가 충분하면 아무것도 필요 없다"는 06노드 §2-B의 지적(허점_2, 심각도 높음)이 나머지 논의 전체를 조건화하기 때문이다. 현재 분석 7개 노드 전부가 이 전제를 물어보지 않고 ATI Full 도입을 기정사실로 받아들였다.

---

## 3. 은수님 retry-and-count 채택 권고

### 판정: **조건부 채택 — 5개 필수 보완 이행 후**

단독 채택은 위험하다(FM-01 Critical, FM-09 High 등 11개 실패 모드 중 1개 Critical·4개 High). 단, ATI_Active 폴링과 결합하지 않아도 되는 경우가 있으며(아래 06노드 허점_1 최종 판단 참조), 보완 5개를 이행하면 실용적 채택이 가능하다.

### 필수 보완 5개

| # | 보완 항목 | 근거 노드 | 구현 규모 |
|---|---|---|---|
| **보완_1**: ATI_Error 전용 핸들러 — 0xC0 bit2 수동 Re-ATI 발행으로 영구먹통 차단 | `tdc_touch.c:389~393`의 `(void) ati_error` 무시를 제거하고, ATI_Error(0x10 bit6) 감지 → 노터치 확인 → `re_ati_trigger()`(drv.c:569~579 이미 존재) 호출 경로 연결 | 06노드 §3, 07노드 FM-09 | ~10줄 |
| **보완_2**: POR auto-ATI와 런타임 Re-ATI 카운터 분리 — READY 전이 후에만 카운터 시작 | MCLR_DONE 상태에서는 카운터 증가 금지. READY 전이 시 `s_i2c_fail_count = 0` 명시 리셋. INIT 경로는 기존 INIT_TIMEOUT_MS(2500ms)로 흡수 | 04노드 §2.3·§5, 07노드 FM-01(Critical)·FM-07 | ~3줄 |
| **보완_3**: 임계 K를 t_ati 실측 후 산정 — 현재 K=10은 t_ati 미측정 추정치 | `K > ceil(t_ati_max / T_read) + 안전마진` 공식 적용. T_read=200ms(실측) 기준으로 런타임 Re-ATI t_ati 실측값 확보 후 재산정. 실측 전 임시값: K=7(=ceil(500ms/200ms)+4, 보수적) | 04노드 §4·§5, 07노드 FM-03 | 상수 1개 변경 |
| **보완_4**: read 실패 시 hold-last 정책 명시 — 현재 묵시적 hold-last가 stuck-touch 롱터치 오발 위험 | `got_state=false` 분기에서 명시적 정책 결정: ① NOT_TOUCH 강제(안전, 탭 잔류 없음) vs ② N회 후 NOT_TOUCH 강제(응답성 보호). 묵시적 방치 금지 | 07노드 FM-04·FM-05 (각 High) | ~5줄 + 설계 결정 |
| **보완_5**: ATI_Active bit 명시 구분으로 3-상태 해소 — `is_auto_ati_done_single_read()`의 통신 실패·ATI 진행 중 미구분 | 반환값을 3-상태(성공/ATI_중/통신_에러)로 확장하거나, 최소한 force_window_open() 타임아웃을 "ATI 중 window 미열림"으로 명시 처리. 현재는 둘 다 false로 묶여 카운터가 ATI_Active 중에도 오름 | 02노드 §3.4·결론, 07노드 FM-06 | ~10줄 |

### 06노드 "상보성 허위" 비판에 대한 최종 판단

06노드 허점_1의 논지: "ATI_Active 폴링이 살아있으면 retry-count 불필요, 둘이 동시에 죽으면 결합도 무의미."

**최종 판단: 비판은 조건부 유효하나 결합 포기 근거는 아니다.**

- 비판이 유효한 조건: ATI_Active 폴링이 완벽하게 동작하는 환경에서 retry-count는 중복이다.
- 비판이 무효한 조건: ATI_Active 폴링 자체가 IQS323의 ATI_Error 상태를 놓치는 경우(ATI_Error 발생 시 ATI_Active=0 → 폴링 통과 → retry-count도 카운터 안 오름 → 둘 다 무력화). 이 경로는 ATI_Error 핸들러(보완_1)로만 차단된다.
- **결론**: 결합보다 선행하는 단 하나의 우선 조건은 보완_1(ATI_Error 핸들러)이다. 보완_1~5를 이행한 retry-count는 ATI_Active 폴링이 없어도 대부분의 위험을 커버하며, ATI_Active 폴링과 결합 시 추가 안전층을 얻는다. 결합 비용(~19줄)이 작으므로 결합이 유리하다.

---

## 4. 실측 게이트 갱신 — 기존 4종 + 신규 3종

### 기존 4종 (요구사항.md §4 기준)

| ID | 기존 항목 | 변경 여부 |
|---|---|---|
| 실측1 | 방전 물리 효력 | 유지 |
| 실측2 | ATI Full I2C 무응답 재현 | 유지 + 선행 조건 강화: ATI_Error 발생 여부도 동시 측정 |
| 실측3 | BETA 부호 세만틱 | 유지 |
| 실측4 | 오염 실채널 확정 | 유지 |

### 신규 3종 (이번 검증에서 도출)

| ID | 신규 항목 | 측정 방법 | 결정 내용 |
|---|---|---|---|
| **실측5**: FIXED 환경 변화 실재성 | 실운용 환경에서 LTA 장기 drift → ATI Band 이탈 빈도 | ATI Full 도입 필요성 판단 — 이탈 없으면 FIXED 유지가 정답 |
| **실측6**: t_ati 정량 (RTT ATI_Active 구간 측정) | ATI_Active bit(0x10 bit5) set→clear 구간을 RTT ms로 측정. POR(부팅 직후)·런타임 Re-ATI 각각 5회 이상 측정 | 임계 K 산정의 분기점. K 설계 없이 구현 불가 |
| **실측7**: ATI_Error 발생 빈도 | ATI Full 재활성 후 1시간·정상 사용 중 0x10 bit6 발생 횟수 | 보완_1 핸들러 우선순위 및 설계 복잡도 결정 |

> [!NOTE]
> **실측5·6은 Phase 1a 착수 전에 필요**. 실측5는 ATI Full 자체의 필요성을 결정하고, 실측6은 임계 K를 결정한다. 둘 다 없이 구현하면 임계가 추정치에 그친다.

---

## 5. 계획.md Phase 1a / Phase 2 재설계 권고

### 5-1. Phase 1a 재설계

**현행 계획.md §3**: "ATI_Active 폴링 + ATI_Error 핸들러 + I2C 타임아웃 복구" 3-요소 구조.

**권고 변경**:

1. **ATI_Error 핸들러를 Phase 1a의 1순위로 격상** — 기존 계획에서는 나열 순서가 ATI_Active 폴링 → ATI_Error 순이었으나, 06·07 노드에서 ATI_Error 영구먹통이 가장 심각한 구조적 결함임이 판명됨. 핸들러 없이 ATI_Active 폴링만 있으면 ATI_Error 경로가 열린 채다(FM-09·FM-02 연쇄).

2. **retry-and-count를 Phase 1a의 구성 요소로 편입** — 별도 "은수님 안 도입 검토" 항목이 아니라, ATI_Error 핸들러·ATI_Active 폴링과 동일 레이어의 최후 안전망으로 계획서에 명기. 보완_1~5 이행 조건 첨부.

3. **실측5·6(FIXED 실재성·t_ati)을 Phase 1a 설계 선행 게이트로 추가** — 현행 계획은 Phase 1a를 실측 없이 진행 가능으로 표기했으나, t_ati 없이 K를 설계할 수 없고 ATI Full 필요성 미확인이면 Phase 1a 자체가 불필요할 수 있음.

4. **POLL_INTERVAL 100ms 단축은 게이트 통과 후 조건부** — 방전 주기 교란(FM-11)·전력 증가 미검증(06노드 §4-B) 상태에서 즉시 적용 금지. 실측1(방전 효력)·실기 전력 측정 후 결정.

**Phase 1a 개정 구성 (우선순위순)**:

```
Phase 1a-① ATI_Error 전용 핸들러 (보완_1, 최우선)
              → 0x10 bit6 감지 → 노터치 확인 → 0xC0 bit2 Re-ATI
Phase 1a-② retry-and-count 기본 구현 (보완_2·3·4·5 이행 조건)
              → READY 전이 후 카운터 시작 (보완_2)
              → K=실측6 기반 산정, 임시 K=7 (보완_3)
              → read 실패 시 NOT_TOUCH 명시 강제 (보완_4, 기본값)
              → 3-상태 반환 또는 ATI_Active 명시 분기 (보완_5)
Phase 1a-③ ATI_Active 폴링 대기 루프 (기존 계획 유지)
Phase 1a-④ I2C 타임아웃 복구 경로 (기존 계획 유지)
```

### 5-2. Phase 2 재설계

**현행 계획.md §5**: ATI Full 복귀 + 비대칭 BETA + Max Counts — 전제: Phase 1a 완료 + 실측2·3.

**권고 변경**:

1. **실측5(FIXED 실재성) 통과를 Phase 2 선행 게이트에 추가** — ATI Full 필요성이 미확인된 상태에서 Phase 2 진입 금지. 실측5에서 FIXED로도 drift 없음이 확인되면 Phase 2 전체를 보류 또는 폐기.

2. **ATI Full 재활성 시 ATI_Error 핸들러 동시 ON 강제** — 계획서에 "Phase 2 빌드 가드 ON = Phase 1a-① 핸들러 동시 활성" 조건 명기. 핸들러 없는 Phase 2 활성 금지.

3. **부팅 터치 시나리오 Re-ATI 위험 명기** — 03노드 §5.2 시나리오_나: 부팅 중 터치 → LTA drift → Re-ATI 발동 → I2C 무응답. Phase 2 검증 시 "터치한 채 부팅" 시나리오를 필수 테스트케이스로 추가(기존 AC-1과 별도, AC-3 보강).

4. **POLL_INTERVAL 변경 결정을 Phase 2 이전에 확정** — 100ms vs 200ms 결정이 임계 K 산정과 직결. Phase 2 이전 실측1·전력 측정 결과로 결정, 계획서에 분기 표기.

---

## 6. 종합 판정 요약 (8줄)

1. **가정_1 타당, 가정_2 조건부, 전략 구조적 결함, 판별 부분 기각** — 위 판정표 기준.
2. **핵심 갈림길**: ATI Full 필요성(실측5)이 최우선. FIXED가 충분하면 retry-count 논의 전부 불필요.
3. **ATI_Error 핸들러(보완_1)가 단 하나의 필수 전제** — 이것 없이 retry-count 단독도, ATI_Active 폴링 결합도 ATI_Error 영구먹통 경로를 막지 못함.
4. **POR/런타임 카운터 분리(보완_2)가 Critical 실패(FM-01) 차단** — READY 전이 전 카운터 시작 시 정상 부팅이 고장 판정.
5. **임계 K는 t_ati 실측 후에만 확정** — K=10은 추정치. 실측6(ATI_Active 구간 RTT 측정) 선행 없이 K 고정 금지.
6. **hold-last 정책을 명시 결정** — 묵시적 hold-last(현재)는 FM-05(stuck-touch 롱터치 오발) High 위험. NOT_TOUCH 명시 강제를 기본값으로 권고(보완_4).
7. **결합은 유리하지만 보완_1 없는 결합은 무의미** — 06노드 허점_1 비판은 "ATI_Error 핸들러가 없으면 둘 다 무력화"로 수렴. 보완_1 이행 후 결합이 최적.
8. **계획 Phase 1a는 우선순위 역전 필요** — ATI_Error 핸들러(①) → retry-count(②) → ATI_Active 폴링(③) 순서로 재배치. 실측5·6을 Phase 1a 선행 게이트로 추가.
