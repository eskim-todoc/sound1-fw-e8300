---
name: 비판-P6-HW협업
purpose: 설계안 P6(HW협업·근본개선파)에 대한 3렌즈(가능성·요구이슈·복잡도리스크) 교차 비판 종합
type: 개선
maturity: experimental
tags: [touch, iqs323, redesign, critique, hw-revision, reference-ui, guard, ferrite, persona-hw]
---

# 비판 · 설계안 P6 (HW협업·근본개선파)

> **TL;DR**: 3렌즈 모두 **조건부**. 회로를 고쳐 IC 자동기능(autoATI·Reference UI·Release UI)을 정식 복원하는 진단 자체는 타당하다. 그러나 ① **웜부트 'autoATI 미발생' 전제가 자기 프로젝트 실측 로그와 충돌**한다('MCLR 직후 auto-ATI가 sensor_setup보다 먼저 자율 실행'). ② **HW 4건(페라이트·guard·470Ω·reference 전극)이 임계경로의 직렬 선행조건**이고 guard 능동구동·reference 전극은 데이터시트로 검증 불가+레이아웃 의존→'얇은 SW 즉시 적용'의 정반대인 다개월 직렬 의존. ③ **전력 근거 수치가 틀린 표 행 인용**(Mutual 2ch 9/50µA를 Self 1ch에 적용, 실제 4/37µA로 페널티 9배). ④ **Reference UI 드리프트 차감이 터치 중 무력화**(표준 LTA frozen+Follower Event Mask)—전원버튼은 본질이 장기터치라 차감이 가장 필요한 구간에 억제됨. ⑤ 주소 오기(CH2 0x72→0x80, CH0 0x52→0x60) 구현 시 먹통 위험. 살아남으려면 웜부트=IC reset 여부를 선검증하고, HW를 위험도순 분리해 ①③(저비용)을 SW 무관 선행하며 ②④를 별도 트랙으로 디커플해야 한다.

> [!NOTE]
> 본 문서는 워크플로우가 생성한 18개 비판 중 P6 관련 3개를 종합한 것이다. 비판 원문을 왜곡하지 않되 구조화했다. 설계안 본문은 [1_설계안_P6_HW협업](1_설계안_P6_HW협업.md) 참조.

---

## 렌즈 A — 가능성 (HW·데이터시트 실현가능성) · verdict: 조건부

### 핵심 findings
- **[위반 없음] order code 제약** — Release UI·Reference UI 두 축 모두 데이터시트 보장. 001=Release UI, Movement UI(A01 전용) 명시 회피. 0xD3/0xD4/0x20~0x25가 'Release UI order code 한정'으로 001 가용.
- **[명문] Reference UI/Follower Event Mask** — CH2=Reference·CH0=Follower + Follower Event Mask=0x03이 §7.3.1 Table 7.1과 정확히 일치. **단 주소 오기**: 표 'Channel Setup CH2(0x72)'는 실제 0x80(0x72는 Channel1 Touch Settings).
- **[치명적 모순] 웜부트 'autoATI 미발생' 전제 충돌** — 설계 2중 안전장치 핵심은 '웜부트는 IC 레지스터 보존→재ATI 불필요→터치 중 ATI 미발생'. 그러나 같은 칩 실측 로그가 '**MCLR 리셋 직후 IC가 auto-ATI를 자동 수행하며 sensor_setup보다 먼저 돈다**' 확인. 웜부트가 IC soft reset/MCLR을 수반하면 auto-ATI가 터치 중 돌아 전제 붕괴. **Follower Event Mask는 reference(CH2) ATI만 막을 뿐 primary(CH0) 부팅 직후 auto-ATI는 못 막음.** 2중 안전장치 중 1중(웜부트 ATI 생략)이 데이터시트/실측상 미보장.
- **[잔존] ATI Error 전역 비트 리스크** — autoATI(Full)+CH2 신설 구성에서 CH0/CH2 중 하나라도 ATI 수렴 실패 시 ati_error는 전역 비트(any channel). 이미 동일 칩에서 'CH1 더미 ATI 실패가 CH0 터치 판정을 막음' 실측. 다채널 autoATI는 ATI Error 표면적을 오히려 늘림→'얇은 SW' 철학과 충돌.
- **[미보장] guard electrode** — CRX1 능동 guard 구동은 데이터시트에 전용 모드 없음(설계 자인). guard는 데이터시트 기능 아닌 회로 기법→AZD125 의존, 본 데이터시트로 미검증. degrade(floating) 경로는 있으나 ESD 차폐 효용 불확실.
- **[부정확] Automatic No ULP 전류 근거** — '9µA vs 50µA(2ch)'는 Mutual 2채널 값, 본 설계는 Self-cap. Self-cap 실값은 NP 125/LP 37/ULP 4µA. **절전 예산 비교 기준 수치 자체가 틀림.** ULP 금지(§5.8)는 정확하나 전류 상쇄 논거('페라이트·guard로 재측정 감소')는 정량 근거 없음.
- **[일부만 보장] HW 4장** — ①페라이트: 데이터시트 무관, 일반 EMI 설계. ③CRX0 100→470Ω: 데이터시트 권장 470Ω 정합(단 fc 감도영향 동시평가). ④CRX2 reference 전극: §7.3 '사용자 비접촉+동일 환경 노출' 두 조건 동시 만족 전극 배치가 인공와우 외형상 미검증(설계도 '공간 부족 시 무력화' 인정).
- **[일반 명시뿐] Release UI 전원버튼 적용** — 'long term touch 해제' 일반 용도. 전원버튼 변화율 reseed가 '의도적 롱터치'를 '해제'로 오판 안 할지 데이터시트 미보장→실칩 튜닝 의존.

### blocking issues
| # | blocking |
|---|---|
| A-1 | **웜부트 auto-ATI 전제 붕괴** — 이슈 1~4 해결 핵심인 '웜부트는 재ATI 미실행→터치 중 ATI 미발생'이 실측 로그('MCLR 직후 auto-ATI가 sensor_setup보다 먼저 자동 실행')와 모순. Follower Event Mask는 reference(CH2) ATI만 disable, primary(CH0) 부팅 직후 auto-ATI는 못 막음. '웜부트가 IC reset을 수반하는가' 미확정인 채 ATI Full 복원 전제하면 터치 중 모드전환에서 autoATI가 CH0 터치 정전용량을 기준으로 잡는 이슈 1~3 재현. **반드시 실측으로 '웜부트=IC soft reset 여부' 및 'reset 시 CH0 auto-ATI 발생 여부' 선검증.** |

### suggestions 요약
- 선검증 게이트: '웜부트 시 IQS323이 soft reset/MCLR 받는가', 받으면 'CH0 auto-ATI가 터치 중 실행되는가'를 RTT/I²C 로그로 먼저 측정. 미발생 확증 전 ATI Full 복원 보류
- ATI Error 전역 비트 대응을 SW 상태머신에 명시(수동 Re-ATI 트리거 로직 필수 분기)
- **주소 오기 정정: CH2 0x72→0x80, CH0 0x52→0x60** (0x52/0x72는 전혀 다른 레지스터→그대로 구현 시 먹통)
- Automatic No ULP 전류 근거를 Self-cap 실값(NP125/LP37µA)으로 교체, '재측정 감소→상쇄' 주장 보류 표기
- guard(CRX1)와 reference 전극(CRX2) 분리 우선순위화(reference 전극 확보 가능성 레이아웃 먼저, guard는 floating degrade 기본안)
- Release UI 전원버튼 적합성을 '의도적 롱터치 vs 해제' 오판 시나리오로 별도 검증, Touch threshold 2차 판정을 1차 안전망으로 승격 검토

---

## 렌즈 B — 요구·이슈 충족 · verdict: 조건부

### 핵심 findings
- **[정합] 레지스터 매핑** — 인용 핵심 레지스터(bit6 Release UI Enable·Follower Event Mask·Follower Weight·0xD4·0xD3) 데이터시트 정의와 일치. order code 001=Release UI 검증됨.
- **[검증된 사실] 웜부트 요구 충족** — '웜부트=E8300만 워치독 리셋, IC 전원유지·MCLR 미수행→레지스터 보존'은 펌웨어 분석 §11로 검증(현재도 그렇게 동작). '웜부트 시 재ATI 불필요→터치 중 ATI 미발생'은 현 구조 위에서 성립, 이슈 1~4 모드전환 경로를 구조적으로 끊는 진단 타당. (단 렌즈 A의 'MCLR 수반 시' 충돌과 교차 검토 필요)
- **[견고] 이슈 5·8·9 해소** — autoATI(Full)로 ATI Base 정규화→'RESEED 카운트 340~660 변동(이슈8)'·'고정 threshold 들쭉날쭉(이슈9)'·'FIXED 채택(이슈5)' 근원 소멸. 이 세 이슈는 데이터시트 메커니즘 근거 분명.
- **[결함] 이슈 6~7 해소 근거 결함** — 설계는 'Reference UI가 표준 LTA 드리프트를 터치 중에도 능동 차감'이라 주장하나, **§5.5 표준 LTA는 touch 중 frozen + §7.3.1 Follower Event Mask는 follower가 touch면 reference ATI disable**. 즉 2~3초 롱터치 동안 reference의 CH0 LTA 보정 기여가 억제/동결→'터치 중 드리프트 차감'이 데이터시트상 미보장. 차감은 노터치 구간 한정으로 해석.
- **[부작용] Release UI 변화율 reseed** — 해제 판정 충족 시 '채널 reseed + 상태 이탈'. 모드전환(웜부트)이 '손가락 댄 채' 트리거되는 빈번 케이스에서 reseed 시점에 손가락 남아 있으면 reseed가 터치 정전용량을 baseline 흡수→이슈 1~4 변형 재발 가능.
- **[미검증] 전력모드 정량** — Automatic No ULP는 §5.8로 필수이나 ULP 대비 LP 전력차가 인공와우 절전예산에 허용되는지 '확인 필요'. 9/50µA가 Mutual 2ch 값이라 Self 1ch(125/37/4µA)와 다름. 예산 초과 시 절전판정 경로가 SW fallback degrade.
- **[큰 미검증 폭] HW 4장** — guard 능동구동·CRX2 비접촉 전극·페라이트 비드값 모두 HW 리비전+실측 종속.

### blocking issues
| # | blocking |
|---|---|
| B-1 | **Reference UI 드리프트 차감의 터치-중 무력화.** 이슈 6~7 해소 근거가 'Reference UI가 표준 LTA 드리프트 능동 차감→200ms 방전 폐기'인데, §5.5(표준 LTA touch 중 frozen)+§7.3.1(Follower touch 시 reference ATI disable)에 따르면 2~3초 롱터치 동안 그 차감이 억제됨. **전원버튼은 본질이 장기 터치라 드리프트 누적 구간이 곧 터치 구간**→폐기하려는 200ms 방전이 막던 'self-cap 드리프트→일정 터치 후 먹통(이슈6)'이 재발 가능. 설계가 이 freeze 상호작용을 전혀 안 다룸. |
| B-2 | **콜드부트 손가락-닿음 미해결(설계 자인).** 설계는 'autoATI 후에도 콜드부트 손가락 닿으면 ATI 오염 잔존'을 '콜드부트 1회 한정'으로 인정+‘보드 최초 전원=손 안 닿음 가정'으로 회피. 그러나 충전 채터링 재부팅(페라이트로 '격감'할 뿐 '제거' 못 함)은 콜드부트 경로를 타며 그 순간 손가락 닿아 있을 수 있음(충전 케이블 연결은 기기 쥔 상태에서 빈번)→'콜드부트=무접촉' 가정이 채터링 시나리오에서 깨짐→이슈 1~3 재발. **9개 중 가장 핵심 부류의 재발 경로 미차단.** |

### suggestions 요약
- Reference UI 터치-중 freeze 명시 검토: 드리프트 차감이 노터치 구간에만 유효함 반영, 2~3초 롱터치 드리프트는 Release UI Activation LTA가 단독 감당 가능한지 실칩 튜닝. Reference UI 역할을 '노터치 환경 드리프트' 보정으로 한정
- 충전-채터링-재부팅=콜드부트 경로에 '손가락 닿음 방어' 명시(s_boot_touch_ignore류 or ATI 완료 후 해제 변화율 감지 시 강제 re-ATI), '콜드부트=무접촉 가정' 단정 제거
- 전력예산을 설계 진입 전 정량 게이트로(Self-cap 1~2ch 실측 NP/LP/ULP), ULP 필수 판명 시 핵심 주장 약화를 리스크로 승격
- HW 의존 4장을 요구충족 판정에서 분리(①③ 저비용 vs ②④ 레이아웃·미명시), ②④ 미확보 시 잔존 이슈(드리프트) 명시
- write_and_verify 강화 + autoATI 복원 후 ATI Error(0x10 bit6) 감시·수동 재트리거 로직을 얇은 상태머신에 포함

---

## 렌즈 C — 복잡도·리스크 · verdict: 조건부

### 핵심 findings
- **['얇은 SW' 과장] 튜닝 파라미터 표면 폭증** — FIXED+200ms 방전 폐기 대신 autoATI+Reference UI+Release UI+이벤트모드 4개 IC 서브시스템 동시 가동. SW 줄 수는 줄어도 시스템 튜닝 파라미터 폭증(Follower Weight·Release Delta%·Settling Threshold·Sample Delay·Activation LTA Beta·ATI Base/ResFactor/Band·채널 배선). 이 중 6개가 '실칩 튜닝/확인 필요' 미검증.
- **[근거 오류] 전력·의존성 수치** — '9µA vs 50µA(2ch)'는 Mutual 2ch, 본 회로는 Self-cap 1ch. Self-cap 실제 ULP 4µA/LP 37µA/NP 125µA. **'Automatic No ULP' 실제 페널티는 4→37µA(~9배)로 설계 가정(9→50, 5.5배)보다 비율 더 나쁨.** 전력 상쇄 논거가 틀린 기준 산정.
- **[강한 모순] Reference UI 양립 요건** — §7.3은 reference 채널이 '센싱과 같은 조건 노출 + 사용자 불가촉' 동시 만족 요구. 인공와우 외부기(귀걸이형 소형)에서 손가락 패드와 동일 온습도·신체근접 받으며 손이 절대 안 닿는 전극은 기하학적으로 강한 모순. fallback(Reference UI 제거)은 이슈 6·7(드리프트·200ms방전)을 그대로 재개방→핵심 카드 빠지면 P6 차별점 절반 증발.
- **[일부 퇴행] 진단 가시성** — 현재는 단일 활성채널+평탄한 RTT 마진 로그로 직관적. P6는 3개 UI 상호작용→'터치 미인식' 시 원인이 ATI인지·follower 발산인지·Release UI 조기 reseed인지·Follower Event Mask 오마스킹인지 귀속 어려움.
- **[미검증] 개체편차·timing** — Reference UI는 CH0+CH2 2채널 동시 변환 필요→변환 시간·전류 증가. 전류표는 고정 채널수 기준만, 본 구성 실측은 '가정'.
- **[잔존] 콜드부트-터치동시 구멍** — autoATI(Full) 복원 시 콜드부트에 손가락 닿으면 정전용량 baseline latch→이슈 1~4 근원이 콜드부트 1회 한정 잔존. 완화책이 '보드 최초 전원=손 안 닿음 가정'이라 보장 아닌 가정.

### blocking issues
| # | blocking |
|---|---|
| C-1 | **HW 4건이 임계경로 직렬 선행조건이고 일부는 데이터시트로 검증 불가.** (a)guard 능동구동: 데이터시트에 전용 모드 없음→AZD125+실측 없이 설계 확정 불가. (b)reference 전극 레이아웃·페라이트 비드값: 보드 리비전 필요. SW는 HW 리비전 안착 전 검증 불가→**'얇은 SW 즉시 적용'의 정반대인 다개월 직렬 의존.** 복잡도 렌즈 기준 가장 무거운 결함. |
| C-2 | **전력 예산 판정의 근거 수치 오류**(Mutual 2ch 9/50µA를 Self 1ch에 적용). 실제 Self-cap ULP 4µA를 No ULP(LP 37µA)로 포기하는 비용이 9배→설계가 든 fallback 트리거 조건(④ 초과 시 ULP+SW롱터치)이 오히려 default가 될 공산. 그 경우 Release UI·Event Timeout(§5.8 ULP 금지)이 무력화→P6 두 핵심 축 중 Release UI 운용 전제 흔들림. |

### suggestions 요약
- 전력 재산정: 'ULP 9µA vs LP 50µA'를 Self-cap 행(ULP 4/LP 37/NP 125µA)으로 교체, 본 구성(1~2ch) 실측으로 No ULP 예산 재검증. ULP 필수면 Release UI를 절전구간에서 끄고 SW 롱터치로 대체하는 분기를 default 가정으로
- HW 의존을 위험도순 분리·단계화: 저비용·저위험(①페라이트 R28, ③CRX0 470Ω)을 SW 무관 선행 실험(채터링/ESD 격감만 먼저 측정). guard·reference 전극(②④)은 별도 트랙 디커플. SW는 ②④ 없이도 동작하는 모드(autoATI+Release UI only)를 1차 타깃
- Reference UI 의존 제거 가능성 우선 검증(§7.3 양립이 외형상 가능한지 레이아웃 먼저, 불가 시 드리프트를 autoATI Re-ATI Band+prox 기반 LTA freeze 방지로 대체 가능한지 평가)
- 3개 UI 동시 가동 전 단계 게이트: ①autoATI+Follower Event Mask만으로 이슈 1~5·8·9 해소 검증 → ②Release UI → ③Reference UI, 각 단계 RTT 진단 포인트(ATI Event·Error·Activation LTA·Delta Snapshot 0x20~0x25 덤프)
- 콜드부트-터치동시 잔존 구멍을 실측 확인, 막지 못하면 콜드부트 한정 ATI Error→수동 Re-ATI 재시도 루프 추가

---

## 종합 판정

| 항목 | 내용 |
|---|---|
| **3렌즈 verdict** | A 조건부 · B 조건부 · C 조건부 |
| **생존성** | **조건부, 단 시간축 리스크 최대** — 진단·방향은 타당하나 HW 4건 직렬 의존으로 '즉시 적용' 불가(다개월). HW를 위험도순 분리하면 ①③+autoATI 부분은 생존 |
| **합의된 강점** | autoATI 복원으로 이슈 5·8·9 근원 소멸(데이터시트 근거 분명) · 웜부트 레지스터 보존이 현 구조서 검증된 사실 · '회로가 IC를 못 받쳐 SW가 부채 떠안음' 진단 타당 · 저비용 HW(페라이트·470Ω)는 SW 무관 선행 가능 |
| **합의된 약점** | 웜부트 'autoATI 미발생' 전제가 실측 로그와 충돌 · HW 4건 직렬 선행(guard·reference 전극 데이터시트 미보장+레이아웃 의존) · 전력 근거 수치 오류(Mutual 2ch를 Self 1ch에) · Reference UI 드리프트 차감이 터치 중 무력화 · 주소 오기(CH2 0x72·CH0 0x52) |

### 치명 blocking 요약 (3렌즈 교차 수렴)
1. **웜부트 'autoATI 미발생' 전제가 자기 실측 로그와 충돌** — A 렌즈 핵심. 'MCLR 직후 auto-ATI 선행' + Follower Event Mask가 CH0 못 막음→이슈 1~3 재현. (P1·P2·P3과 동일 'MCLR auto-ATI 선행' 함정에 수렴)
2. **HW 4건 임계경로 직렬 의존(guard·reference 전극 검증 불가)** — C 렌즈 최대 결함. '얇은 SW 즉시 적용'의 정반대.
3. **전력 근거 수치 오류 + Reference UI 운용 전제 흔들림** — Self-cap 실값으로 No ULP 페널티 9배, fallback이 default 될 공산.
4. **Reference UI 드리프트 차감 터치-중 무력화** — B 렌즈. 전원버튼=장기터치라 가장 필요한 구간에 억제됨.

### 이 안이 살아남으려면 (해결 필수)
1. **웜부트 선검증**: '웜부트=IC soft reset/MCLR 수반 여부' 및 'reset 시 CH0 auto-ATI 발생 여부'를 RTT/I²C 로그로 먼저 측정. 발생 시 CH0 Timeout Disable·수동 Re-ATI 게이팅 등 별도 차단책.
2. **HW 위험도순 분리·디커플**: ①페라이트·③470Ω을 SW 무관 선행 실험(채터링/ESD 격감 측정), guard·reference 전극(②④)은 별도 트랙. SW는 ②④ 없이도 동작하는 autoATI+Release UI only를 1차 타깃.
3. **전력 재산정**: Self-cap 실값(ULP 4/LP 37/NP 125µA)으로 No ULP 예산 재검증, ULP 필수 시 절전구간 Release UI 끄고 SW 롱터치 대체를 default로.
4. **Reference UI 역할 한정**: '노터치 환경 드리프트' 보정으로만, 터치 중 드리프트는 Release UI Activation LTA 단독 감당 검증. 외형상 §7.3 양립 불가 시 제거판을 base로.
5. **주소 오기 정정 + ATI Error 처리 복원** + 콜드부트-터치동시 방어 명시.
