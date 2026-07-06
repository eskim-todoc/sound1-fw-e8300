---
name: 12-V4-MEAS-FILT검증
purpose: C-MEAS(측정모드 재구성)·C-FILT(공격적 필터·ATI·임계) 두 후보의 인용을 데이터시트 원문(pdftotext 직접 재추출)·코드·과거 실측 로그와 전면 재대조한 adversarial 검증 — 할루시네이션 3건 확정, 조기 기각 대부분 재확인, Lever C만 조건부 생존
type: tasks/에이전트-로그
maturity: experimental
tags: [touch, iqs323, heisenberg, adversarial-verify, c-meas, c-filt, hallucination-audit, threshold, debounce, beta]
---

# 12 · V4 — C-MEAS·C-FILT 검증 (adversarial, 원문 직접 재대조)

**TL;DR**: `iqs323_datasheet.pdf`를 `pdftotext -layout`으로 직접 재추출해 두 후보의 모든 핵심 인용을 원문과 바이트 단위로 재대조했다. C-MEAS M1(평균화 무력론)은 기각 재확인. C-FILT Lever A(Touch Debounce)는 후보가 "미확인"으로 놔둔 것보다 **더 강하게 기각**(3중 확인: §5.7 본문·A.17 표·코드 버그이력 모두 "없음"으로 수렴). Lever C(Threshold 상향)의 "158 도달난" 근거는 실제 이력 문서로 확인됐으나 후보가 계수와 절대임계를 혼동했고, 결정적으로 **k=100도 이미 실패("도달난")했다는 사실을 새로 발견** — 후보의 1순위 권고(80→100)가 자기모순. 할루시네이션 3건 확정(§5 참고).

## 0. 핵심 판정표

| 검증항목 | 후보 주장 | 원문/코드 재대조 결과 | 판정 |
|---|---|---|---|
| M1 평균화 무력론 | "average" DS 0건 + 이미 최댓값 | **확인**(전문 grep 0건, 0x33 미기입 코드 확인, 0x01CF bit[3:2]=11=32 직접 재계산 일치) | 기각 재확인 |
| Lever A 존재여부 | "근거 상충"(미확인) | **불일치 아님 — 3중 일치로 부재 확정**(§2) | **기각 강화**(중간→기각) |
| Lever C "158" 이력 | "더 높은 k(158 부근) 실패 이력" | 실제 이력: **k=102**가 절대158 생성, 실패. **k=100도 별도 실패**(신규 발견) | 부분 오염 정정 + 헤드룸 재축소 |

---

## 1. C-MEAS M1 — 평균화 무력론 (원문 재대조)

**"average" 어휘 재확인**: `pdftotext -layout` 전문(68p) grep 결과 `average`/`averaging`/`averaged` 계열은 오직 "Long Term Average"(LTA 명칭) 및 "slowly updated to track"(§5.5 본문) 문맥 2곳뿐 — **다회 변환 평균화를 서술하는 문장은 0건**, 후보 주장 확인.

**Auto Prox Cycle Select(A.9, p.54) 원문**: "Number of conversions before each interrupt is generated in Auto Mode" — 00=4·01=8·10=16·11=32. 이 필드의 **유일한 정량적 사용처**는 p.32 §8.11.1 "In ULP power mode the report rate is: (Auto Prox Cycle Select × Ultra Low Power Report Rate)ms" 한 문장뿐 — grep으로 "Auto Prox Cycle"·"Auto Mode" 전체 5회 등장 위치 전수 확인, ULP 밖 서술 0건. 후보 주장 확인(NP/LP 효과는 데이터시트가 규정 안 함 — 미확인 유지).

**코드 미기입 확인**: `tdc_touch_iqs323.c` 레지스터 매크로 목록(L18-34)에 0x33/0x43/0x53 없음, `write_register(` 전체 grep에도 해당 주소 0건 — 부팅마다 reset 기본값 0x01CF 유지. **직접 재계산**: 0x01CF = 0000 0001 1100 1111 → bit[3:2]="11"=32(최댓값) — 후보의 비트 분해와 정확히 일치(재현 성공). **M1 기각 재확인**.

**§1 공통 논거 정정 (할루시네이션 확정, §5-2 참고)**: "Beta 필드가 4비트(0~15)라 damping factor 최대 5.9%로 상한"이라는 근거는 **오류** — 실제 A.26/A.27/A.28(p.60-61) 원문은 Beta를 NP/LP 각각 **8비트**(bits[15:8]/bits[7:0], 0~255)로 정의하며 4비트 제한 서술은 어디에도 없음. 단, AZD004 원문(p.13, `a1=(2^β-1)/2^β`, `b0=1/2^β`)의 지수형 공식상 β>20 근방부터 사실상 포화(2^20≈100만)되어 "실무적으로 작은 정수만 의미 있다"는 **결과**는 우연히 비슷하게 맞다 — 다만 원인은 "4비트 하드웨어 상한"이 아니라 "지수 공식의 실질 포화"다. M1 결론(기각) 자체는 이 정정과 무관하게 유지.

---

## 2. C-FILT Lever A — Touch Debounce Enter 존재 여부 (원문 직접 재대조)

후보는 "A.17 표와 §5.7 서술이 상충"이라며 "중간(비트 미확인)"으로 판정했다. **원문 재대조 결과, 두 원본은 상충하지 않고 서로 일치한다** — 상충은 제3의 출처(편집 노트)에서 왔다.

**§5.7 본문 원문(p.17, 직접 추출)**:
> "...for more than the number of consecutive samples specified by the **Prox Debounce Enter** field... exit... **Prox Debounce Exit** field... A channel will enter the touch state if: (LTA−Counts) > Touch Threshold **and** exit the touch state if: (LTA−Counts) > (Touch Threshold − Touch Hysteresis). The Touch Threshold and Touch Hysteresis are set in the Touch Settings register."

Prox 진입·이탈에는 "number of consecutive samples" 문구가 **명시**되지만, Touch 진입·이탈 두 공식 어디에도 연속조건 언급이 **전혀 없다**. 이는 우연한 누락이 아니라 대비 서술이다.

**A.17 원문(p.57, 직접 추출)**: "> Bit 15-12: Touch Hysteresis ... > Bit 7-0: Touch Threshold" — **딱 2개 필드만** 서술. 대조군 A.16(Prox Settings, p.57)은 "> Bit 15-12: Prox Debounce Exit ... > Bit 11-8: Prox Debounce Enter ... > Bit 7-0: Prox Threshold" **3개 필드**로 bits[11:8]까지 명시 서술. Touch Settings에는 그 대응 항목 자체가 없다(공백이 아니라 **서술 부재**).

**코드 이력(제3의 독립 확인)**: `tdc_touch_iqs323.c:286-289` 주석 — "이전엔 MSB 통째로 써 **bits[11:8](미정의)**에 들어가 실제 hysteresis=0 이었던 버그 수정." Sound1 개발 이력 자신이 이 니블을 "미정의"로 명명했다.

**결론**: §5.7 본문·A.17 표·코드 버그이력 **3개 독립 출처가 전부 "없음"으로 수렴** — 이건 "상충"이 아니라 만장일치다. 후보가 "상충"이라 부른 근거는 실제로는 `02_proxfusion동작.md` §5.7 "SW 구현 관점" NOTE(원문 인용이 아닌 편집 해설)의 "Debounce는... Touch Settings의 **대응 필드**에서 설정한다"는 문장인데, 이 문장 자체가 원문 어디에도 근거가 없는 **과잉 일반화(할루시네이션, §5-3)**다. Lever A는 "중간(비트 미확인)"이 아니라 **기각**이 맞다 — 후보보다 한 단계 더 단호한 결론.

---

## 3. C-FILT Lever C — "158 도달난" 이력 재확인 (실제 이력 문서 발굴)

`docs/tasks/touch/20260622_touch-instrumentation-sleep-ati/이력 및 결과.md` L63 + `테스트 로그2.md`(원 실측 로그, 코드 아님)를 직접 대조했다.

**실제 기록(2026-06-23)**: "Threshold 하향: 수렴 시간은 적절하나 임계(**계수102**, 절대~158)가 높아 수렴에 막혀 도달난. 계수 **102→80**... **60은 위험·100은 도달난** → 80 절충(은수님 제안)."

`테스트 로그2.md`(핀셋 약결합 실측)는 이를 직접 뒷받침: `THR=158 (k=102)` 고정 상태에서 delta가 최대 **D=90**(CNT=309)까지만 올라가고 158 문턱을 넘지 못해 터치 미검출(`.` 표시 유지) — "도달난"의 실체가 정확히 이것.

**할루시네이션 확정**: 후보는 "더 높은 **k(약 158 부근)**를 이미 한 차례 시도했다가 실패"라 썼다. 이는 **계수(k)와 절대임계(THR=k×LTA/256)를 혼동**한 오류다 — 실패한 것은 k=**102**(및 별도로 k=**100**)이며, 158은 그 결과로 나온 **절대 카운트 값**이다. k=158을 실제 시도했다면 절대임계는 ~246(k=158×399/256)이 되어 전혀 다른 심각도다.

**신규 발견(후보 미확인 항목 해소)**: 후보 §8 "1순위 권고"는 "레버 C(80→100 부근)"이었다. 그러나 실제 이력은 **k=100이 이미 "도달난"으로 실패했음을 명시**한다 — 후보 자신의 최우선 권고가 프로젝트 자체 실측 이력과 정면충돌한다. k=60은 "위험"으로 별도 표기(사유 미상, 아마 과민 오탐 방향 — 본 노드 범위 밖). 안전 범위는 최대 k≈84~86 부근(별도 자매 문서 `20260703_touch-heisenberg-sw-mitigation/에이전트-로그/04_S2`의 독립 추정과도 방향 일치)으로 좁혀야 한다.

**잔여 불확실성**: `iqs323.h:46` 주석의 "실터치 D 160~190"과 이 실측 로그의 "약결합 시도 D_max=90"은 서로 다른 결합조건(핀셋 약결합 vs 정상 파지)일 가능성이 높으나 원문·코드만으로 확정 불가 — 신규 실측 전 두 수치 모두 맹신 금지.

---

## 4. 레버별 최종 분류 — 노이즈 저감 vs 완화/방어

| 레버 | 원 판정 | 재검증 후 판정 | 분류 |
|---|---|---|---|
| M1 다중변환 | 기각 | **기각 재확인** | 해당없음(SW액션 자체 없음) |
| M2 변환주파수 | 조건부생존 | (본 노드 스코프 외, 변경 없음) | 실측전제 |
| M3 레지스터 명시화 | 채택(하드닝) | **확인**(0x32=0x1290 bit12 재계산 일치) | 방어(노이즈무관) |
| A. Debounce Enter | 중간 | **기각**(3중 확인) | 해당없음(필드 부재) |
| B. Counts Beta | 중간 | 결론 유지(추론 근거만 정정, §1) | 완화(버스트성 한정) |
| C. Threshold 상향 | 강함·1순위 | **메커니즘은 강함 유지, 헤드룸 대폭 축소**(k≤~85) | **진짜 저감**(단 마진 희박) |
| D. Hysteresis | 무관 확인 | 변경없음 | 방어(release 전용) |
| E. Fast Filter Band | 약함 | §5.6 원문 "opposite direction to sensing" 재확인 → 방향불일치 재확인 | 무관(오탐 방향 미적용) |
| F. ATI Band | 헤드룸0 | 재확인(A.12 원문 2택1, 이미 Large) | 해당없음 |
| G. ATI Target | 미확인·비권고 | 유지 + **CH1더미 이력 정황 발견**(§5 참고) | 미확인 |

---

## 5. 추가 확인 사실 (스코프 인접, 참고용)

1. **Fast Filter Band 방향**: §5.6 원문 "Fast filtering is applied to the LTA if the channel counts drift away from the LTA **in the opposite direction to the sensing direction**" — 후보 인용 그대로 확인.
2. **ATI Band 2택1**: A.12 원문 "0: Small(1/16) · 1: Large(1/8)" — 옵션 2개뿐, 현재 bit3=1(Large) 확인(0x040C 직접 분해).
3. **read_status() bit4 미추출**: `tdc_touch_iqs323_read_status()`(L346-364) 실제 확인 — ati_active(bit5)·ati_error(bit6)·pressed(bit9)·prox(bit8)만 추출, ATI Event(bit4) 추출 코드 없음. 후보 주장 확인(라인번호만 약간 드리프트, 내용은 일치).
4. **"Full Mode I2C 무응답" 주석 — 정황상 구버전 확정 강화**: `02_proxfusion동작.md L658`이 인용한 오래된 리스크는 실제로 `20260619_touch-fullati-control/분석.md` L57("과거 80%는 CH1 더미 수렴 실패 → **CH1 폐기로 소멸**")로 프로젝트 자체가 이미 해소 판정을 내린 바 있다 — 현재 코드는 실제로 Full Mode 가동 중(L437 확인). 후보의 "구버전 주석으로 추정" 의심이 정황 근거로 강화됨(단, Lever G가 건드리는 "ATI Target 확대"는 CH1과 무관해 이 해소가 직접 적용되진 않음 — 잔여 캐지션 유지).
5. **핀 목록·fOSC 공차**: Table 2.4(p.7) 11개 신호명 중 클럭 입력 핀 0건, Table 4.3(p.12) fOSC 13.23~14.77MHz 모두 원문 그대로 확인.
6. **메모리맵 기본값 전수 대조**: §9(p.35-37) 원문과 `06_레지스터레퍼런스.md` 표를 전수 대조 — 0x30~0xE1 전 항목 일치, 오류 0건(이 문서의 신뢰도는 높음).

---

## 6. 종합 결론

C-MEAS는 자평("전부 근거 약하거나 실체없음")이 **정확했다** — M1은 재검증으로도 기각. C-FILT는 7개 레버 중 6개가 무관/기각/헤드룸0으로 재확인되거나 오히려 더 단호히 기각됐고(Lever A), **유일하게 살아있는 레버는 C(Threshold)** 인데 이마저 후보가 제안한 구체적 수치범위(80→100)는 프로젝트 자체 실측 이력으로 **이미 반증**된 상태였다. "AC 클럭/SPI 노이즈를 실제 저감하는가"라는 질문에 대해 두 후보 전체에서 **원리와 실측 근거가 모두 확립된 것은 하나도 없다** — Threshold 상향은 노이즈의 물리적 성격(코히런트/버스트)과 무관하게 판정식을 직접 미는 유일한 "진짜" 레버이지만, 안전 여력이 k≈80→85 수준의 좁은 구간뿐이며 이조차 신규 실측 없이는 재실패 위험이 실재한다.

## 근거

- `iqs323_datasheet.pdf` 본 세션 `pdftotext -layout` 전문 재추출(`/tmp/iqs323_full.txt`) 직접 grep·페이지별 재추출: p.7(Table 2.4)·p.12(Table 4.3)·p.17-19(§5.6-5.11 본문)·p.32(§8.11.1)·p.35-37(§9 메모리맵)·p.52-61(A.1-A.30)
- `azd004_azoteq_sensing_v1.1.pdf` §5.1-5.3(p.13-14, IIR 공식·typical beta 값)
- `tdc_touch_iqs323.c`(L18-34 레지스터 매크로, L284-320 touch/beta settings, L346-364 read_status, L437 ATI Setup write) · `tdc_touch_iqs323.h`(L45-53 THRESHOLD/HYSTERESIS 주석)
- `docs/tasks/touch/20260622_touch-instrumentation-sleep-ati/이력 및 결과.md` L40-63, `테스트 로그2.md`(전체 65줄)
- `docs/tasks/touch/20260619_touch-fullati-control/분석.md` L57(CH1더미 폐기 이력)
- `06_C-MEAS_측정모드재구성.md`·`07_C-FILT_공격적필터ATI임계.md`(검증 대상 원문)
- `데이터시트/02_proxfusion동작.md`·`04_부가기능·UI.md`·`06_레지스터레퍼런스.md`, `터치 임계·계수 레지스터 동작.md`(교차 대조 대상)
