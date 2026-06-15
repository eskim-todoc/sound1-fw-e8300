---
name: 은수님의견-A6-절전전력실측검증
purpose: 은수님 제안 Q6·Q7(M7·M8) 절전 auto-ATI/Re-ATI 동작·환경대응·추가세팅 불필요·드리프트 해결 주장을 코드·데이터시트로 검증 + 실측 필요 항목 도출
type: 개선
maturity: stable
tags: [touch, iqs323, auto-ati, re-ati, beta, power, ulp, drift, sleep, review, A6]
---

# A6 · 절전·전력·실측 검증 (Q6·Q7 / M7·M8)

> **TL;DR**: 절전 검증자 관점에서 은수님 M7(절전도 auto-ATI로 추가세팅 불필요)·M8(auto-ATI가 드리프트 해결→수동방전 불필요)을 코드·데이터시트로 검증. **결론: 두 명제 모두 현 코드 위에서 성립하지 않으며, 성립시키려면 대규모 SW 변경+실측이 선행**한다. 결정적 발견 3건 — (1) **BETA 레지스터(0xB0~0xB3)는 코드 어디에서도 write되지 않아 reset 0x0000(alpha=0)** 상태다. 은수님 6단계 "빠른 BETA"는 미구현일 뿐 아니라, 데이터시트 해석상 0x0000은 LTA 추적을 사실상 멈추는 값이라 M8과 정면 모순(부호·세만틱 실측 확인 필요). (2) **IC Power Mode 필드·Report Rate(0xC1~0xC5)도 한 번도 write되지 않는다** — 펌웨어의 "ULP 모드"는 IC 전력모드 전환이 아니라 마진읽기 게이팅용 SW 플래그(`g_tdc_iqs323_in_ulp_mode`)일 뿐. 실제 IC 절전은 E8300 SYSCLK 강하(30.72M→2.56M)에만 의존. (3) **auto-ATI/Re-ATI는 운용·절전 경로 모두에서 ATI Mode=Disabled로 차단** — Re-ATI는 LTA가 ATI Band 밖일 때만 도는데 ATI Disabled에선 자동 트리거 안 됨(B6 상호배타 ULP 금지와도 무관하게 애초 비활성). 절전 1ch 전류는 데이터시트 부재(3ch만)로 실측 필수(B6).

> [!IMPORTANT]
> 근거: 공통입력 [00](00_은수님-의견-입력.md), 데이터시트 [01 §3.4](../../데이터시트/01_개요·전기·타이밍.md)·[02 §5.2/5.3/5.6/5.8/5.10/5.11](../../데이터시트/02_proxfusion동작.md)·[06 A.26~A.30](../../데이터시트/06_레지스터레퍼런스.md), 펌웨어 [구현·시퀀스 분석](../펌웨어%20구현·시퀀스%20분석.md), 이전검증 [4_타당성검증 B6](../생각정리/4_타당성검증.md), 코드 `tdc_drv_iqs323.{c,h}`. **코드 사실은 직접 Read로 확정, 추정은 "확인 필요" 표기.**

---

## 1. 검증 범위 — 담당 명제·질문

| 명제 | 내용 | 질문 | 판정(본 분석) |
|---|---|---|---|
| **M7** | 절전 진입도 동일 개념(노터치 확인 후 auto-ATI), 추가 터치센서 세팅 불필요 | **Q6** | **반증** — 절전에 추가 세팅이 오히려 다수 존재, auto-ATI 비활성 |
| **M8** | auto-ATI 모드라 드리프트 이슈 해결 → 수동 방전 불필요 | **Q7** | **반증(조건부)** — auto-ATI/Re-ATI가 코드상 차단, BETA 미설정. 드리프트 방어는 현재 200ms 방전이 전담 |
| (M5 연계) | 빠른 BETA로 LTA 수렴 가속 | Q4(A3 주담당) | **미구현 사실 추가** — BETA 레지스터 자체가 write 안 됨 |

---

## 2. 코드 현황 — 절전·전력·ATI·BETA 사실 (ground truth)

### 2.1 BETA 레지스터: 코드 어디에서도 write 안 됨 (reset 0x0000)

`grep`로 `src/2__cm3/.../systemControl/` 전체에서 `0xB0`·`0xB1`·`0xB2`·`0xB3`·`*_FILTER`·`BETA` write 0건 확인.

- Filter Betas 4종(`Counts`/`LTA`/`LTA Fast`/`Activation·Movement`)은 데이터시트 [06 A.26~A.29] **reset 기본값 0x0000**.
- 데이터시트 [02 §5.6] 해설: `alpha = Beta/256`, Beta 클수록 빠른 추적. **Beta=0 → alpha=0 → 현재 입력 0% 반영 = LTA가 raw counts를 추적하지 않음**.

> [!WARNING]
> **M5/M8 직격 모순(세만틱 확인 필요)**: 은수님 6단계 "LTA가 카운트를 최대한 빠르게 추적하도록 BETA 조정"은 (a) **현재 미구현**(0xB1 LTA Beta = 0)이며, (b) 본 데이터시트 해설을 그대로 적용하면 reset값 0x0000은 *가장 느린*(추적 정지) 쪽이다. 즉 "빠른 BETA"를 원하면 0xB1을 명시적으로 큰 값으로 **새로 write하는 코드 추가가 선행**한다. 단, Azoteq 일부 제품군은 Beta를 "damping shift(클수록 느림)"로 정의하기도 하므로, **0xB1의 0→큰값 변경이 추적을 빠르게/느리게 하는지는 실칩 RTT 실측으로 부호 확정 필요**(02 §5.6 해설과 06 A.27 원문 비트 의미 교차검증 대상).

### 2.2 IC Power Mode·Report Rate: 한 번도 write 안 됨

- System Control(0xC0)의 Power Mode 필드[06 A.30 bits6-4: 000 NP·001 LP·010 ULP·011 Halt·100 Automatic·**101 Automatic No ULP**]에 대한 write 0건.
- Report Rate(0xC1 NP·0xC2 LP·0xC3 ULP·0xC4 Halt·0xC5 Power Mode Timeout) write 0건 — 모두 reset 0x0000.
- 코드의 `tdc_set_iqs323_in_ulp_mode()`/`tdc_is_iqs323_in_ulp_mode()`는 **순수 SW 플래그** `g_tdc_iqs323_in_ulp_mode`(drv.c:13). 용도는 `apply_tuning()`(1248)에서 sleep vs normal 재초기화 분기 + 마진읽기 게이팅뿐. **IC 전력모드를 바꾸지 않는다.**

> [!IMPORTANT]
> **결정적 함의**: 펌웨어가 말하는 "절전(ULP)"은 IQS323의 ULP 전력모드가 아니다. 실제 절전 효과는 **E8300 CM3 SYSCLK 강하(30.72MHz→2.56MHz, `ci_power_sleep`)와 폴링 유지**에서 온다. IQS323 자체는 Power Mode 미설정 → **POR 기본값(0x0000 = Power Mode 000 = Normal Power 고정)으로 계속 동작**(확인 필요: 0x0000이 NP 고정인지 Automatic 디폴트인지 — 06 A.30 reset 0x0000이면 bits6-4=000=NP). 즉 **B6의 "Auto No ULP 상호배타" 쟁점은 현 코드에선 발생조차 안 한다**(애초 ULP·Channel Timeout 둘 다 미사용·비활성). 은수님 M7이 전제하는 "절전 모드의 auto-ATI"는 **물리적으로 존재하지 않는 모드**를 가리킨다.

### 2.3 auto-ATI / Re-ATI: 운용·절전 모두 ATI Mode=Disabled로 차단

- 운용: `write_ati_compensation()`이 ATI Setup(0x36)=0x0408(bits[2:0]=000 Disabled) + FIXED MULT/COMP(0x5C82/0x63EF) write [펌웨어분석 §7].
- 절전: `apply_sleep_settings()`(drv.c:973~1007)도 `SLEEP_ATI_SETUP`(동일 Disabled) + 동일 FIXED MULT/COMP를 다시 write. **auto-ATI 미수행**.
- Re-ATI[02 §5.10]는 "LTA가 ATI Band 밖으로 drift 시 자동 실행"이나, **ATI Disabled 상태에선 ATI Error 시에도 자동 트리거 안 됨**[02 §5.11 IMPORTANT: master가 Re-ATI bit 수동 set 필요]. 코드의 `re_ati_trigger()`/`calib_re_ati()`(569·829)는 **개발/측정 경로(`SLEEP_MEASURE_MODE`·`ATI_DUMP`)에서만** 호출, 운용 경로엔 없음.

### 2.4 절전 진입의 "추가 세팅" 실재 — M7 직접 반증

`func_sleep`→`apply_sleep_settings`→`ci_power_sleep`→`reseed` 경로[펌웨어분석 §11]는 추가 세팅 덩어리다:

| 단계 | 세팅 | 코드 |
|---|---|---|
| 터치 해제 대기 | pressed 동안 100ms 폴링 | main.c:797~ |
| 둔감화 | touch_settings **255/255** | apply_sleep_settings 965 |
| ATI 재기록 | SLEEP ATI Setup + FIXED MULT/COMP | 973~1007 |
| CH Timeout Disable | 0xC0 MSB 0x07 | 1011 |
| 클럭 강하 후 복원 | touch_settings **100/80** + **RESEED(0xC0 0x08)** | reseed 921·932 |

→ "절전에서 추가 터치센서 세팅 불필요"(M7)는 **현 코드와 정반대**. 절전 전환은 둔감→클럭강하→reseed라는 다단계 세팅으로 LTA를 새 환경(주변 OFF·저속 클럭)에 다시 깐다.

---

## 3. 명제 검증

### M7 (Q6) — 절전 auto-ATI·추가세팅 불필요 → **반증**

| 은수님 주장 | 코드·DS 근거 | 판정 |
|---|---|---|
| 절전도 "동일 개념"(auto-ATI)으로 노터치 확인 | 절전 경로 auto-ATI 미수행(ATI Disabled), Power Mode 미설정 | 전제 불성립 |
| 노터치 확인되면 추가 세팅 불필요 | §2.4 다단계 세팅(255/255→reseed 100/80) 실재 | 반증 |
| 환경변화 자동 대응 | LTA 추적이 환경대응 수단이나 BETA=0(추적 정지)·노터치 freeze 방지 prox 비활성(PROX_ENABLE=0) | 자동대응 약함 |

추가 위험(절전 특화):
- **저속 클럭(2.56MHz)에서 auto-ATI/Re-ATI 동작 보장 미확인**(확인 필요). auto-ATI는 내부 측정 사이클·타이머 의존 — 저속 클럭·report rate 변동 시 수렴 시간·정확도 영향 가능. 데이터시트 전류표[01 §3.4]도 모드별 report rate 상이(NP 16ms vs LP 60ms vs ULP 160ms)임을 명시.
- 절전 중 auto-ATI를 켜면(은수님 안 채택 시) **Re-ATI 진동(Q5)·ATI 중 I²C 무응답**(02 §5.10 해설 "Full Mode autoATI 발동 시 I²C 무응답 사례"가 ATI Disabled 채택 이유) 위험이 절전에서 더 치명적(웜부트 watchdog과 상호작용).

### M8 (Q7) — auto-ATI가 드리프트 해결 → 수동방전 불필요 → **반증(조건부)**

| 은수님 주장 | 코드·DS 근거 | 판정 |
|---|---|---|
| auto-ATI 모드라 드리프트 해결 | 운용·절전 모두 ATI Disabled. auto-ATI 미동작 | 전제 불성립 |
| → 200ms 수동 방전 불필요 | 현재 드리프트 방어는 **CRX0 VSS 200ms 방전이 전담**(펌웨어분석 §8, 이슈 "드리프트→먹통" 대응) | 방전 제거 시 방어 공백 |

- 드리프트의 본질: self-cap CRX0에 ESD/전하 누적 → counts 편이[펌웨어분석 §14.1]. **auto-ATI/Re-ATI는 LTA를 재캘리브레이션할 뿐 누적 전하를 물리적으로 빼주지 않는다** — 방전과 메커니즘이 다르다.
- 설령 auto-ATI를 켜도(은수님 안), Re-ATI는 ATI Band 밖 drift 시에만 도는 **이산 보정**이라 연속적 전하 누적을 200ms 단위로 막는 방전을 대체한다고 단정 불가. **수동방전 제거 가부는 실측 게이트**.
- B1(진짜 오염원=Reseed)·B6과 정합: 드리프트 방어 핵심은 auto-ATI 복원이 아니라 **방전 유지 + Reseed 시점 노터치 게이팅**.

---

## 4. 발견 (Findings)

1. **BETA 미구현(0xB0~0xB3=0x0000)** — M5의 "빠른 BETA"는 미구현. reset값은 본 데이터시트 해설상 추적 정지(alpha=0)이라 M8과도 모순. 부호 세만틱 실측 확정 필요.
2. **IC Power Mode·Report Rate 미설정(0xC0 PM·0xC1~0xC5=0)** — 펌웨어 "ULP"는 SW 플래그뿐, IC는 사실상 NP 고정 추정. 절전 효과는 E8300 SYSCLK 강하에만 의존. B6 상호배타 쟁점은 현 코드에서 미발생.
3. **auto-ATI/Re-ATI 운용·절전 모두 차단(ATI Disabled)** — M7·M8의 "auto-ATI" 전제 자체가 현 펌웨어에 부재. 복원하려면 코드 추가+I²C 무응답 리스크 재검토 선행.
4. **절전 추가세팅 다수 실재** — 255/255 둔감→클럭강하→reseed 100/80. M7 직접 반증.
5. **수동방전(200ms CRX0 VSS)이 드리프트 방어 단독 담당** — auto-ATI는 대체 메커니즘 아님(재캘리 vs 전하방전).

---

## 5. 실측 필요 항목 (게이트)

| 순위 | 항목 | 검증 내용 | 게이트 대상 | (B6 연계) |
|---|---|---|---|---|
| 1 | **BETA 부호·효과** | 0xB1 LTA Beta를 0→큰값 write 시 LTA 추적이 빨라지는가/느려지는가(RTT로 LTA 수렴 시간 측정) | M5/M8 "빠른 BETA" 채택 가부 | — |
| 2 | **절전 1ch 전류** | self-cap **1ch** NP/LP 실측(Event mode, 실 report rate) + 인공와우 절전 예산 사양 확보 | auto-ATI/ULP/Channel Timeout 채택 가부 | **B6 순위2** |
| 3 | **저속 클럭 auto-ATI 동작** | SYSCLK 2.56MHz·절전 report rate에서 auto-ATI/Re-ATI 수렴 시간·ATI Error·I²C 응답성 캡처 | 절전 auto-ATI(M7) 채택 가부 | — |
| 4 | **수동방전 vs auto-ATI 드리프트** | 200ms 방전 OFF + auto-ATI ON 조건에서 장시간 counts drift·먹통 재현 여부 | M8 방전 제거 가부 | — |
| 5 | **IC Power Mode 실제값** | 0xC0 PM 필드/0x10 Current Power Mode를 RTT로 read해 현재 IC가 NP 고정인지 확인 | M7 전제(절전 모드 존재) 확인 | — |

---

## 6. 결론

은수님 M7·M8은 **"auto-ATI가 켜져 있고 절전 전력모드가 존재한다"는 전제 위에 서 있으나, 현 펌웨어는 ① auto-ATI를 운용·절전 모두 Disabled로 끄고 ② IC Power Mode를 설정하지 않으며 ③ BETA도 write하지 않는다.** 따라서 두 명제는 현 코드 위에서 **성립하지 않는다(반증)**. 은수님 안을 채택하려면 (BETA write + ATI Mode Full 복원 + Power Mode 설정) 코드 추가가 선행하고, 그때 비로소 **저속클럭 auto-ATI 동작·1ch 절전 전류·I²C 무응답·드리프트 대체성**이 모두 실측 게이트로 등장한다. 절전·전력 관점에서 은수님 안은 **조건부반대** — 방향(auto-ATI 자기정상화)은 매력적이나, 절전 추가세팅 불필요·수동방전 불필요 주장은 현 사실관계와 정면 충돌하며 ④건의 실측 없이는 회귀 위험이 크다.
