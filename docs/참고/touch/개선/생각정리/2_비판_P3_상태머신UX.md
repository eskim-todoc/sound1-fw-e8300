---
name: 비판-P3-상태머신UX
purpose: 설계안 P3(상태머신·UX파)에 대한 3렌즈(가능성·요구이슈·복잡도리스크) 교차 비판 종합
type: 개선
maturity: experimental
tags: [touch, iqs323, redesign, critique, fsm, boot-fsm, release-ui, persona-ux]
---

# 비판 · 설계안 P3 (상태머신·UX파)

> **TL;DR**: A·B 조건부, **C는 조건부이나 치명 결함 3건으로 사실상 탈락에 근접**. Touch FSM의 롱터치/COOLDOWN UX 구조는 견고하나, **콜드부트 CHECK_FINGER 가드가 원리적으로 작동 불가**하다 — 실측·문서화된 'MCLR 직후 auto-ATI가 sensor_setup보다 먼저 자율 실행' 때문에 호스트가 ATI 타이밍에 개입할 틈이 없다. 또 **Boot FSM의 COLD/WARM 구분 근거가 데이터시트에 없다**(reset_event는 단일 비트, '최초 전원 정황'은 IC가 제공 불가→E8300 비휘발성 플래그 필요, 미명시). Release UI 해제 공식의 self-cap 부호 정합·롱터치 중 Activation LTA 수렴도 미검증. 살아남으려면 Release UI를 강화책으로 강등하고 표준 LTA+finger-guard FSM을 본안으로, 콜드부트 가드를 전면 재설계해야 한다.

> [!NOTE]
> 본 문서는 워크플로우가 생성한 18개 비판 중 P3 관련 3개를 종합한 것이다. 비판 원문을 왜곡하지 않되 구조화했다. 설계안 본문은 [1_설계안_P3_상태머신UX](1_설계안_P3_상태머신UX.md) 참조.

---

## 렌즈 A — 가능성 (HW·데이터시트 실현가능성) · verdict: 조건부

### 핵심 findings
- **[위반 없음] order 001 제약** — 001=I²C 0x44 + Release UI. Movement UI 레지스터 미사용, 0x30 bit6을 Release UI로 set. 펌웨어 I²C 주소 0x44 일치.
- **[용도 불일치] 데이터시트 보장 범위** — §7.4는 Release UI를 'long term touch·proximity 해제'용으로 명시. '전원 스위치 press/long-hold/release edge 검출'용이 아님. 설계 스스로 '전원스위치 적합성은 실측'으로 표기→**off-label 적용**, datasheet-guaranteed 아님.
- **[치명 의존] Release UI 공식 방향 — Linearise/Invert 의존** — §7.4 공식 `(Counts−ActivationLTA)>임계`는 self-cap에선 부호상 음수(터치 시 counts 감소)→공식이 fire 안 됨. Linearise+Invert로 극성을 뒤집어야 release edge에서 fire. 설계가 이를 포함한 건 정확하나 **실칩에서 §7.4 공식과 부호가 맞물리는지 데이터시트 미보장**.
- **[경고] Linearise+Invert 비트 해석** — 펌웨어 분석이 직접 경고: 현 코드는 0x30 LSB를 'CRX inactive 상태'로 해석(0x02/0x0A), 데이터시트 A.5는 동일 비트를 bit1 Linearise·bit3 Invert·bit0 Enable로 정의. **같은 비트 위치 정면 충돌, 실칩 동작 미확정.** 핵심 전제가 이 미검증 비트필드 위에 섬.
- **[사실확인] STUCK timeout ↔ ULP 상호배타** — §5.8 WARNING. Self-Cap 1ch ULP 4µA vs LP 37µA(≈9배). 설계가 'STUCK을 호스트 FSM 타이머로'라는 대안 제시→가능성 확보.
- **[정합] Events 모드 ↔ Watchdog** — comm window 밖 IC 자동 watchdog kick→255ms reset 루프 위험 없음.
- **[HW 확인] RDY/MCLR 배선** — active pull-up(내부 200kΩ)+C58 1nF, INTERRUPT_n 배선 존재→HW 변경 없이 인터럽트 전환 가능. 단 I²C/RDY 풀업이 칩 측 부재(마스터 측 추정)→실측 확인 필요.

### blocking issues
| # | blocking |
|---|---|
| A-1 | **Release UI 변화율 판정의 self-cap 부호 정합 미검증.** §7.4 공식은 Linearise+Invert로 self-cap 극성을 뒤집어야만 release edge에서 fire. 이 조합은 (a)§7.4 전원스위치 시나리오 미보장, (b)펌웨어 분석이 0x30 LSB 비트 해석 충돌로 '실칩 동작 미확정' 경고한 바로 그 비트필드. 부호 정합 깨지면 release 영영 미검출(먹통) 또는 즉시 오이탈→설계 근간 붕괴. |
| A-2 | **롱터치(2~3초) 중 Activation LTA 수렴→long-hold 전 release 오판정.** §7.4상 Activation LTA는 터치 중 계속 update. 2~3초 채우기 전 손가락 counts로 수렴하면 (Counts−ActivationLTA)→0→release 판정→ACTION_FIRED 안 됨. beta를 LONG_HOLD_MS보다 느리게 + STUCK 회복도 보장하는 beta 윈도우가 실제 존재하는지 데이터시트 미보장. 양립 불가하면 Release UI 단독으로 전원스위치 UX 불가. |

### suggestions 요약
- 실칩 검증 우선순위: ①Linearise+Invert ON에서 Counts·Activation LTA 부호 방향 측정 ②Delta Snapshot 기록 확인 — 이 2개가 P3 전체 전제
- Activation LTA beta 스윕: 3초 롱터치 미발생 + 30초 부착 STUCK 회복이 동시 성립하는 윈도우 존재 실측, 부재 시 STUCK을 호스트 FSM 타이머로 분리
- fallback(표준 LTA freeze + finger-guard FSM)을 1급 설계로 승격, Release UI는 강화 옵션
- 칩 고정(001)을 설계 제약으로 명문화 (A01 변경 시 0xD4가 Movement Timeout으로 의미 변경)
- No-ULP Self-Cap 1ch 소비 LP(37µA) 실측

---

## 렌즈 B — 요구·이슈 충족 · verdict: 조건부

### 핵심 findings
- **[견고] 롱터치 2~3초** — Touch FSM LONG_HOLD_MS로 직접 충족(현 코드 노말 2.4s/절전 2.2s 일반화). 단 절전 ULP 폴링 200ms 해상도에 묶임(허용 가능).
- **[정합] 상시전원·웜부트** — Boot FSM WARM 분기(reset_event=0→ATI·reseed 미수행)가 비대칭 전원 구조와 정확히 맞물림. **이 분기가 이슈 1~4 해소의 실질 주력**이며 Release UI는 보조층.
- **[과대평가 지적] 이슈 1~4 해소 실주체** — Release UI(§7.4)는 칩 동작 중 long-term touch '해제'만 다루지 부팅 시점 LTA seed 문제는 안 건드림. 핵심은 **타이밍 분리(부팅에서 LTA 분리)**이지 Release UI가 아님. 설계 §1.3 'Release UI가 핵심' 표현은 요구이슈 관점에서 과대평가. 설계 자체가 fallback에서 'Release UI 없이도 finger-guard FSM만으로 이슈 1~4 일부 해결' 인정.
- **[타당] 이슈 5~8 해소** — Full ATI 재도입으로 RESEED 카운트 340~660 변동을 IC 자동보정. 200ms 방전 폐기는 논리적 귀결.
- **[타당] 이슈 9 해소** — delta 판정+비대칭 히스테리시스+Debounce. 단 이슈 9 근인은 'FIXED라 raw count 시점 의존'이고 Full ATI 복원이 진짜 해결책.
- **[충실] 데이터시트 정합** — 인용 레지스터·공식 모두 실재 확인.

### blocking issues
| # | blocking |
|---|---|
| B-1 | **Release UI 해제 공식 부호 정합이 self-cap에서 미검증, 틀리면 release 판정 전체 무력화.** §7.4 공식은 touch 시 counts가 '커지는' 규약(mutual). self-cap은 감소→부호 반대. Linearise+Invert로 맞춘다 가정하나 설계 §6 '확인 필요'. Activation LTA가 Invert/Linearise 후 도메인에서 동작하는지 §7.4 미명시. 깨지면 STUCK 자동해제·reseed 모두 오작동→이슈 1~4 재발. **단일 최대 결함.** |
| B-2 | **충전 채터링(~1µs) 시 'WARM이 기준 보존' 전제가 바로 그 상황에서 붕괴.** 채터링이 IC reset시키면 reset_event=1·LTA 초기화→IC는 더 이상 웜 아님. SUSPECT 분기가 콜드 복구로 가는데 손가락 닿아 있으면 CHECK_FINGER에 의존. CHECK_FINGER는 'counts 비정상 높음'으로 판별하나 콜드부트 직후 신뢰 LTA 기준 없어 판별 기준 모호(FIXED 부팅 카운트 340~660 변동). 요구가 명시 권고한 가장 취약 케이스에서 finger-guard 신뢰도 미입증. |
| B-3 | **reset_event로 COLD/WARM 구분 정확도 미검증인데 Boot FSM 전체가 이 1비트 분기.** 노이즈 reset 거동 불확실하거나 절전 복귀 경계에서 IC reset_event가 0이 아닐 케이스 있으면 WARM→COLD 오분류→불필요한 Full ATI가 터치 중 실행→이슈 1~3 정확히 재발. fallback(LTA/counts cross-check) 미설계. |

### suggestions 요약
- Release UI를 '강화책'으로 강등, 1차 판정은 표준 delta+히스테리시스+finger-guard FSM(단계적 채택). 호스트 FSM 타이머 STUCK으로 No-ULP 강제도 회피
- 채터링 SUSPECT 분기에 WAIT_RELEASE + 타임아웃 강제진행 후 무조건 재seed 2중화, CHECK_FINGER 판별을 절대 카운트 아닌 보존 LTA/Max Counts 근접 기준으로 재정의
- reset_event 단일 분기에 cross-check 추가(counts vs 보존 LTA·Power Mode 비트·Power Event)
- 롱터치 중 Activation LTA 수렴 정량 가드(beta 상한 계산식 도출)
- RDY 인터럽트 전환 단일점 실패 대비 폴링 fallback + watchdog 안전망 병행

---

## 렌즈 C — 복잡도·리스크 · verdict: 조건부 (치명 결함 다수)

### 핵심 findings
- **[치명·근거] Boot FSM 콜드/웜 구분 근거가 데이터시트상 부재** — System Status(0x10) bit7은 'Reset Event' 단일 비트로 '리셋 발생 여부'만, POR/MCLR/watchdog/brown-out 구분 못 함. IC는 자기 리셋 안 나면 reset_event=0. COLD 분기 트리거 '최초 전원 정황'은 **IC가 알려줄 수 없는 정보→E8300 비휘발성 플래그 필요**. 설계는 이 외부 의존 미명시.
- **[치명·실측 반례] 콜드부트 CHECK_FINGER 가드 원리적 작동 불가** — 이미 실측·문서화된 실패: 'MCLR 직후 IC가 auto-ATI를 자동 수행하며 sensor_setup보다 먼저 돈다'. 호스트가 ATI 타이밍을 통제 가능하다는 전제가 틀림. 콜드부트에 손가락 있으면 정전용량이 ATI 기준으로 굳는 이슈 1~3 핵심이 그대로 재현→설계 '근본 해결'이 콜드 경로에서 미해결.
- **[치명] auto-ATI 선행은 Release UI로도 못 막음** — Release UI는 ATI가 잡은 기준 위 상위 레이어. ATI가 손가락 정전용량으로 수렴하면 Activation LTA·Delta Snapshot 전부 오염 baseline 위에서 출발. Delta Snapshot은 settling 안정 구간 필요한데 손가락 계속 붙어 있으면 그 구간이 손가락 상태에서 잡혀 release 기준 오염. **부트스트랩 시점 손가락 문제는 Release UI도 해결 못 함.**
- **[복잡도] 상태기 2개·13상태+ 폭증** — Touch FSM 7상태 + Boot FSM 6상태. 현 구현(proc_long_touch 1함수+플래그+3상태) 대비 폭증. 신규 튜닝 상수(DEBOUNCE/LONG_HOLD/COOLDOWN/STUCK_TIMEOUT/INIT_TIMEOUT + Beta·Release%·Sample Delay·Settling Threshold·threshold·hysteresis·debounce)가 상호 결합→독립 튜닝 불가.
- **[튜닝 충돌] Beta ↔ 롱터치** — 동일 파라미터(Beta)가 '롱터치 중 미수렴'(느려야)과 'stuck 빠른 회복'(빨라야)을 동시 요구. **단일 자유도로 상충 요구 만족 — 구조적 긴장.**
- **[전력 회귀] STUCK 자동해제 ↔ ULP 상호배타** — No-ULP 강제 시 ULP 9µA 포기. 호스트 타이머 STUCK 대안은 ULP에서 주기 wake-read 필요→이벤트모드 전력 이점 부분 상쇄.
- **[정합 미해결] 0x30 LSB 비트 해석 충돌** — INACTIVE_RXS vs Linearise/Invert/Enable이 미해결인데 그 위에 Linearise(bit1)+Invert(bit3)+Release UI(bit6) 동시 set→오기록 위험.
- **[가시성 후퇴] write_and_verify 미검증** — 값 일치 검증 안 되는 기존 결함 유지하며 레지스터 항목 9+개로 대폭 증가→오기록 미감지 리스크 비례 증가. ati_error 전역 무시 유지 시 콜드부트 ATI 실패 은폐.

### blocking issues
| # | blocking |
|---|---|
| C-1 | **콜드부트 손가락 가드(CHECK_FINGER) 원리 불가** — MCLR 직후 auto-ATI가 sensor_setup·host read보다 먼저 자율 실행됨이 실측·문서화. 호스트는 콜드부트 ATI 타이밍 개입 불가→손가락 닿아 있으면 이슈 1~3 콜드 경로 재현. 설계 '근본 해결'이 콜드에서 미성립. **선결 검증·재설계 없이 핵심 전제 붕괴.** |
| C-2 | **Boot FSM COLD/WARM/SUSPECT 3분류 판별 근거 부재** — reset_event 단일 비트로 콜드/웜/채터링 구분 못 함. '최초 전원 정황'은 IC 제공 불가→E8300 비휘발성 상태 추적이라는 미명시 외부 의존 필요. 설계화 없이 분기 자체 구현 불가. |

### suggestions 요약
- **선결 PoC를 분기 게이트로**: ①MCLR 후 auto-ATI 선행에서 '손가락 부착 콜드부트'를 host가 가드 가능한지 ②Release UI Delta Snapshot이 손가락 상태 부트스트랩 시 release 정확도 — 통과 시에만 진행
- Boot FSM을 reset_event 의존에서 분리, E8300 비휘발성 플래그로 콜드/웜 권위 판정
- FSM 2개 합치거나 Boot FSM 최소화(COLD/WARM 2분기+finger-guard만, SUSPECT를 COLD 흡수)
- STUCK 구현 전략 조기 확정(ULP 유지면 host 타이머, 포기면 No-ULP+Channel Timeout)
- write_and_verify 실값 비교 승격, Full ATI 시 ati_error 콜드부트 한정 노출, 0x30 LSB 충돌 선해소

---

## 종합 판정

| 항목 | 내용 |
|---|---|
| **3렌즈 verdict** | A 조건부 · B 조건부 · C 조건부(치명 결함 다수) |
| **생존성** | **낮음~조건부** (탈락 후보에 근접) — 콜드부트 경로 두 치명 결함이 '근본 해결' 주장을 콜드 경로에서 무효화. UX 골격은 가치 있으나 핵심 전제 재설계 필수 |
| **합의된 강점** | Touch FSM 롱터치/COOLDOWN UX 구조 견고 · 웜부트 WARM 분기가 이슈 1~4 실질 해소(타이밍 분리) · 이슈 5~9 Full ATI 복원으로 해소 · 데이터시트 인용 충실 |
| **합의된 약점** | 콜드부트 CHECK_FINGER 원리적 불가(실측 반례) · COLD/WARM 구분 근거 데이터시트 부재 · Release UI 부호 정합 미검증 · 롱터치 중 Activation LTA 수렴 vs STUCK 회복 단일 자유도 충돌 · FSM 13상태+ 복잡도 폭증 |

### 치명 blocking 요약 (3렌즈 교차 수렴)
1. **콜드부트 CHECK_FINGER 가드 원리적 불가** — C 렌즈 실측 반례. 'MCLR 직후 auto-ATI 선행'으로 호스트가 ATI 타이밍 개입 불가→이슈 1~3 콜드 경로 재현. (A·B 렌즈의 '터치된 채 부팅 미방어'와 수렴)
2. **Release UI 해제 공식 self-cap 부호 정합 미검증** — A·B 렌즈 공통 지목. Linearise+Invert 비트가 펌웨어 분석이 경고한 충돌 비트필드 위에 섬. 깨지면 release 먹통.
3. **Boot FSM COLD/WARM 구분 근거 부재 + 롱터치 중 Activation LTA 수렴(단일 Beta 자유도 충돌)** — 구조적 미해결.

### 이 안이 살아남으려면 (해결 필수)
1. **선결 PoC 게이트**: (a)콜드부트 손가락 부착 시 host 가드 가능성 (b)Release UI 부호 방향·Delta Snapshot 기록 실칩 측정. 두 게이트 미통과 시 콜드 분기·Release UI 의존 전면 재설계.
2. **Boot FSM을 E8300 비휘발성 플래그 기반으로 재설계** — reset_event는 보조 신호로만. 미명시 외부 의존을 설계화.
3. **Release UI 강등 + 표준 LTA·finger-guard FSM 본안화** — 부호 정합·beta 윈도우 검증 전까지 STUCK/reseed가 Release UI 단독 의존 금지.
4. **FSM 최소화 + 진단 강화** — SUSPECT를 COLD 흡수, write_and_verify 실값 비교, ati_error 노출, 0x30 LSB 충돌 선해소.
