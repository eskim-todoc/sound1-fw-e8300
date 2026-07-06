---
name: iqs323-전력모드-rdy통신-정밀확인
purpose: IQS323 NP/LP/ULP/Halt 전력모드별 RDY·Force Communication·Report Rate 레지스터 동작을 데이터시트 원문과 Sound1 실제 코드로 교차검증
type: tasks/에이전트-로그
maturity: experimental
tags: [touch, iqs323, power-mode, rdy, force-communication, report-rate, np, lp, ulp, halt, 2026-07-06]
---

# 그라운드_2 — 전력모드별 RDY/통신 동작 정밀 확인

**TL;DR**: Force Communication(§8.13)은 원문상 전력모드와 무관하게(Halt 탈출조차 동일 기전) 항상 통신 윈도우를 강제로 열 수 있다 — 은수님의 "ULP 평상시 통신 실패" 가설은 강한 형태로는 원문에 반박된다. 다만 t_wait는 원문 자체가 "application specific"이라 명시해 고정 45ms 상한이 LP/ULP의 느린 Report Rate 하에서 실제로도 유효한지는 [미확인]이며, 이 지점이 과거 회귀의 더 정밀한 재해석 후보다. 코드 교차검증 결과 Sound1은 현재 IC를 항상 NP에 고정하고 있고 Interface Selection도 Streaming(0)으로 남아있어, LP/ULP/Event mode는 전부 "아직 실제로 가동해본 적 없는" 가정 영역이다. Force Comm 관련 실리콘 errata(I2C Lock Up)도 신규 확인.

---

## 검증 방법

- 원문: `docs/참고/touch/iqs323_datasheet.pdf`를 `pdftotext -layout` 변환한 `docs/참고/touch/iqs323_datasheet_layout.txt`(기존 산출물, 3999줄)를 직접 grep·Read로 대조. 인용은 전부 실제 줄 번호·"Page NN of 68" 원문 페이지 표기를 함께 표기한다.
- 코드: `src/2__cm3/Cortex-M3-src/systemControl/tdc_touch_iqs323.c`(499줄)·`.h`(113줄) 전체 Read, `tdc_touch.c`·`main.c`는 grep으로 관련 부분만 확인.
- 보조: `docs/참고/touch/IQS323 예제코드/02_드라이버_API.md`(Azoteq 공식 Arduino 레퍼런스 드라이버 `IQS323.cpp` 정리본) — Force Comm의 실제 구현 형태를 원문 도표와 교차검증하는 데 사용.
- 과거 S3/V3 문서는 배경 파악 목적으로만 참조했고, 결론은 본 문서에서 원문·코드로 독립 재도출했다(무비판적 재인용 금지 지시 준수).

---

## 항목_1 — Force Communication(§8.13)의 파워모드 무관성

### 원문 확정 사실

§8.13 원문 전체(줄 1893~1899, Page 33 of 68):

> "Ideally, communication with the IQS323 should only be initiated in a RDY window. In event mode RDY windows are only provided when an event is reported. In event mode it may be required to change device settings or query the device immediately. A communication request described in the figure below will force a RDY window to open. The minimum and maximum time between the communication request and the opening of a RDY window (twait) is application specific. The typical values of twait are 0.1ms ≤ twait ≤ 45ms."

이 문단은 Force Comm의 필요성을 오직 **인터페이스 모드(Streaming vs Event)** 축으로만 설명한다. **전력모드(NP/LP/ULP/Halt)에 대한 어떠한 조건절·예외절도 원문에 없다.** "특정 전력모드에서는 Force Comm이 안 되거나 다르게 동작한다"는 진술은 §8.13 어디에도 없다.

같은 결론을 강화하는 두 개의 별도 원문 근거:

1. **§8.7(줄 1768~1771, Page 30)**: "Transfer of data between the master and slave must occur during the communications window (RDY is low). If the master wishes to initiate communication outside of a communications window (RDY is high), a force communications request must be made. Section 8.13 describes the force communications request sequence." — "RDY가 high인 모든 경우"에 대해 일반적으로 서술, 역시 전력모드 무관.

2. **§5.2 Halt 탈출 규정(줄 905~908, Page 15)**: "In Halt mode, the IQS323 will remain in a deep sleep state. To exit Halt Mode, a force communications request must be made (see Section 8.13), and the power mode must be changed in the following communications window." — Halt는 "conversion·processing가 전혀 없는" 4모드 중 가장 극단적인 상태(§5.2, 줄 900~901: "Deep sleep in which no conversions or processing are done")인데, 이 상태에서 벗어나는 **유일한 명시적 절차가 바로 Force Comm**이다. 가장 제한적인 상태에서도 작동해야 하는 기전이라면, 그보다 덜 제한적인 NP/LP/ULP에서 작동하지 않을 이유가 원문 구조상 없다.

**판정**: 은수님의 가설("ULP는 터치 발생시에만 통신 가능, 평상시 실패")은 **강한 형태로는 원문에 의해 반박된다.** Force Comm은 "이벤트가 없어 RDY가 계속 high인 상태"를 깨기 위해 존재하는 기능이며, 그 상태가 ULP이든 NP이든 Halt이든 구분하지 않는다.

### 실제 메커니즘 재확인 (기존 참고문서 오류 정정)

기존 `05_i2c인터페이스.md`의 §8.13 해설 NOTE는 Force Comm을 "SCL 없이 SDA만 최소 4클럭 분량 토글하는 특수 웨이크업 패턴"으로 설명한다. 이번에 원문 Figure 8.1/8.2 텍스트 레이어와 Azoteq 공식 레퍼런스 드라이버를 직접 대조한 결과, **이 설명은 뒷받침되지 않는다.**

- 원문 Figure 8.2(줄 1901~1924)와 Figure 8.1(줄 1795~1811, §8.9 Terminate Communication용)은 동일한 라벨 구조("Setup to write" → "0x44 + ACK" → "0xFF + ACK")를 보인다. `SCL`행에는 "CLOCK"이라는 라벨이 붙어 있어 — 클럭이 없는 것이 아니라 **정상적으로 클럭이 뛰는 표준 I²C 전송**임을 시사한다.
- `IQS323 예제코드/02_드라이버_API.md`(Azoteq 공식 Arduino 드라이버 `IQS323.cpp` 정리)가 결정적이다: **"12.1. `void force_I2C_communication(void)` — 동작: `iqs323_deviceRDY`가 false(RDY가 HIGH)일 때만 `beginTransmission` → `write(0xFF)` → `endTransmission(STOP)` 수행"**(문서 506~508행). Arduino `Wire` 라이브러리의 `beginTransmission/write/endTransmission`은 항상 표준 클럭 기반 I²C이며 SCL 없는 특수 신호를 보낼 수단이 없다. 즉 **Azoteq 자신의 레퍼런스 구현조차 "RDY가 열려있지 않을 때 단일 바이트 0xFF를 표준 I²C로 쓴다"가 전부**다.
- Sound1의 `force_window_open()`(`tdc_touch_iqs323.c:145~175`)도 정확히 이 형태다: RDY가 이미 OPEN이면 즉시 반환(줄 149~152), 아니면 `uint8_t force_comm = 0xFF; i2c_write(&force_comm, 1);`(줄 147~158)로 표준 I²C 1바이트 쓰기 후 최대 45ms 폴링. 헤더의 `TDC_TOUCH_IQS323_MAX_WAIT_OPEN_MS = 45`(`tdc_touch_iqs323.h:32`)는 §8.13이 명시한 typical 상한 45ms와 정확히 일치 — 이 상수가 데이터시트 §8.13에서 직접 도출됐음을 강하게 뒷받침한다.

**정리**: Sound1의 `force_window_open()`은 이미 Azoteq 공식 레퍼런스와 동형인, 올바른 Force Communication(§8.13) 구현이다. 그리고 이 함수는 새로 설계해야 할 미래 기능이 아니라 **현재 모든 I²C 레지스터 접근(`write_register`/`read_register`/`tdc_touch_iqs323_read_debug`) 앞단에 이미 무조건 걸려 있는, 상시 가동 중인 경로**다.

### 남는 진짜 리스크 — t_wait의 조건부 성격

§8.13은 t_wait를 "application specific"이라 명시하고 각주로 "Contact Azoteq for an application specific value of twait"(줄 1899, 1936)를 달았다. **즉 0.1~45ms는 "typical" 수치일 뿐, 모든 Report Rate 설정에서 보장되는 상한이라는 명시적 진술은 원문에 없다.**

이 여백은 과거 회귀(코드 주석: LP report rate 100ms 전환 시 45ms 폴링이 window를 놓쳐 timeout)에 대한 더 정밀한 재해석의 실마리다 — 항목_4에서 이어간다.

---

## 항목_2 — 전력모드별 Report Rate 레지스터·POR 기본값

### 원문 확정 (§9 Memory Map, 줄 2148~2156, Page 36)

| 주소 | 레지스터명 | POR 기본값 | 원문 부가 조건 |
|---|---|---|---|
| 0xC0 | System Control | 0x0000 | Appendix A.30(모드 선택 필드 포함) |
| 0xC1 | Normal Power Mode Report Rate | 0x0000 | 16-bit value (ms) — 별도 Range 명시 없음(필드폭상 0~65535 가능) |
| 0xC2 | Low Power Mode Report Rate | 0x0000 | 16-bit value (ms) — 별도 Range 명시 없음 |
| 0xC3 | Ultra Low Power Mode Report Rate | 0x0000 | **Range: 0-3000**(원문에 명시된 유일한 Report Rate 상한) |
| 0xC4 | Halt Mode Report Rate | 0x0000 | 16-bit value (ms) |
| 0xC5 | Power Mode Timeout | 0x0000 | Range: 0-65000 |

**세 모드(NP/LP/ULP)는 완전히 독립된 개별 레지스터이며, ULP가 NP·LP로부터 파생 계산되는 구조가 아니다.** 단, ULP만 예외적으로 원문에 변환 공식이 붙는다.

### ULP만의 특수 공식 (§8.11.1, 줄 1835~1848, Page 32)

> "In I2C streaming mode data is constantly reported at the relevant power mode report rate specified in milliseconds by the Normal Power Report Rate, Low Power Report Rate and Ultra Low Power Report Rate registers. In ULP power mode the report rate is: **(Auto Prox Cycle Select × Ultra Low Power Report Rate) ms**. Where Auto Prox Cycle Select is defined in the Prox Input and Control register."

`Auto Prox Cycle Select`는 §9 Memory Map상 `Prox Input and Control`(0x33/0x43/0x53, 채널별 개별 레지스터, A.9) bit3-2 필드다: "00=4·01=8·10=16·11=32"(줄 3108~3113, "Number of conversions before each interrupt is generated in Auto Mode"). 즉 **ULP의 실제 보고 주기는 0xC3 레지스터값 단독이 아니라 "채널별 Auto Prox Cycle Select × 0xC3" 곱셈값**이다. NP/LP/Halt에는 이런 곱셈 공식이 원문 어디에도 없다 — 이 셋은 레지스터값이 곧 ms로 해석되는 것으로 보인다(단, "0xC1/0xC2/0xC4는 곱셈 없이 직접 ms"라고 못박는 명시 문장은 원문에서 찾지 못해 이 부분은 구조적 추론이며 **[일부 추론, 배제법에 의한 정황 근거]**로 표시한다).

한 가지 배치상 주의점: 이 공식은 §8.11.**1 "I2C Streaming"** 절 아래 있고, §8.11.2 "I2C Event Mode" 절에는 별도로 반복되지 않는다. Event mode에서도 동일 공식이 그대로 내부 샘플링 주기를 결정하는지 원문이 명시적으로 재확인하지는 않는다 — **[미확인, 단 강한 정황 근거 있음]**: §3.4 전력소비표 자체가 "Interface Selection: Event mode"(줄 661) 조건에서 측정됐음에도 NP/LP/ULP 각 행이 "Report rate [ms]" 열(16/60/160ms 등, 줄 665~677)을 그대로 갖고 있다 — Streaming/Event 구분과 무관하게 Report Rate가 내부 변환 주기를 결정한다는 방증이다(선행 `20260705_touch-event-mode-adoption` 라운드의 "Report Rate는 Streaming 전용이 아니다" 결론과 부합).

### Sound1 실제 코드 대조 — 중요한 괴리 발견

`tdc_touch_iqs323.c` 상단 레지스터 정의(줄 16~34)를 보면 **0xC1·0xC3·0xC4는 코드에 상수조차 정의돼 있지 않다.** 실제로 쓰는 것은:

- `REG_LP_REPORT_RATE = 0xC2`(줄 32) — `beta_power_settings()`에서 `write_register(REG_LP_REPORT_RATE, 0x64, 0x00)`(줄 319)로 100ms 기록.
- `REG_PM_TIMEOUT = 0xC5`(줄 33) — 같은 함수에서 `write_register(REG_PM_TIMEOUT, 0x00, 0x00)`(줄 320)로 0 기록.

**0xC1(NP Report Rate)·0xC3(ULP Report Rate)·0xC4(Halt Report Rate)는 전체 코드베이스에서 단 한 번도 write되지 않는다** — POR 기본값 0x0000 그대로 방치된다. 즉:
- 현재 IC의 NP Report Rate는 "0"이다(코드가 설정 안 함 = 하드웨어 기본값). 0이 "가능한 한 빠르게"를 의미하는지 원문에 명시적 정의 문장은 못 찾았다 — **[미확인]**. 다만 이 값이 사실이라면, `force_window_open()`의 "RDY가 이미 열려있으면 즉시 반환"(줄 149~152) 경로가 NP 운용 중 거의 항상 성립해 실제 0xFF 강제쓰기 코드 경로 자체가 지금까지 드물게만 실행됐을 가능성이 있다 — **[추론, 미확인]**.
- ULP를 실제로 가동할 경우 사용될 0xC3와 CH0의 Auto Prox Cycle Select(0x33)도 둘 다 미설정 상태라 POR 기본값(각각 0, 32)이 그대로 적용된다. 항목_3에서 계속.

---

## 항목_3 — ULP "wake-up 채널 + 저속 채널" 이원화가 단일채널(CH0) 구성에 적용되는지

### 원문 확정 (§5.2, 줄 889~903, Page 14~15)

> "Ultra-low power mode (ULP) — Optimized firmware setup — Intended for rapid wake-up from deep sleep on **a single channel** (e.g. distributed proximity event), enabling immediate button response for an approaching user — **Other sensor channels are sampled at a slower rate** in order to optimize power consumption"

원문은 명확히 "빠른 채널 1개 + 그 외 채널(들)은 저속"이라는 **복수-채널 전제**의 구조로 ULP의 이득을 설명한다.

### Sound1 실제 채널 구성 (코드 대조)

`sensor_setup()`(`tdc_touch_iqs323.c:266~282`):

```c
write_register(REG_SENSOR2_SETUP, 0x00, 0x00); /* CH2 disable */
write_register(REG_SENSOR1_SETUP, 0x00, 0x00); /* CH1 disable (단일채널) */
write_register(REG_SENSOR0_SETUP, 0x01, 0x01); /* CH0: enable_channel + ctx0 */
```

CH1·CH2는 `Enable Channel` bit(A.5 bit0)까지 0으로 꺼져 있다 — "느리게 샘플되는 다른 채널"이 아니라 **아예 존재하지 않는 채널**이다.

### 판정

Sound1의 CH0 단일채널 구성에서는 ULP의 헤드라인 이득 구조("빠른 파수꾼 1개 + 느린 보조원 여럿")가 **적용할 대상 자체가 없다.** "다른 채널"이 원천적으로 없으므로 그 채널들을 저속화해서 아끼는 전력이 애초에 존재하지 않는다 — 작업 지시에서 제기된 가설("이원화 이득이 성립하지 않을 수 있다")은 **원문·코드 교차검증으로 확인(지지)된다.**

다만 이것이 "ULP가 Sound1에 무의미하다"는 결론까지 가는 것은 과확대다. ULP는 이원화 혜택과 별개로, **활성 채널 자체(CH0)의 리포트 주기를 LP보다 더 느리게 만드는 축**은 여전히 갖고 있다(§8.11.1 공식: Auto Prox Cycle Select × ULP Report Rate). 이원화 이득은 없어도, 단일채널 자체의 저속화 이득은 이론상 남아있다 — 다만 그 절감폭(§3.4 표는 3채널 self-cap 조건이라 1채널 근사는 그라운드_1 소관)은 본 항목 범위 밖이다.

### 레지스터 수준 이원화 기전 재구성 — [부분 추론]

원문 어디에도 "이 채널을 wake 채널로 지정" 같은 전용 selector 비트는 없다(Appendix A.1~A.37 전체 재확인 결과, Channel Setup·Prox Input and Control·System Control 어디에도 "ULP wake channel select" 필드 없음). 대신 `Auto Prox Cycle Select`가 **채널(센서 블록)별로 개별 설정되는 4비트 필드**(0x33=CH0, 0x43=CH1, 0x53=CH2, 각 bit3-2)이고, ULP 리포트 주기 공식이 이 값을 곱셈 인자로 사용한다는 점에서: 실제 이원화는 **채널마다 다른 Auto Prox Cycle Select 값을 줘서(빠른 채널=4, 느린 채널=32) 공유되는 단일 0xC3 베이스에 서로 다른 배율을 곱하는 방식으로 구현될 것**으로 추정된다. 이는 원문이 명시적으로 서술한 문장이 아니라 레지스터 구조로부터의 재구성이므로 **[추론]**으로 표기한다.

Sound1은 CH0의 Auto Prox Cycle Select(0x33)를 아예 write하지 않아(코드에 `0x33` 레지스터 정의 자체가 없음) POR 기본값 `11=32`(가장 느린 배율, 줄 3113·A.9 기본값 해설)로 남아있다. ULP를 실제 가동한다면 CH0을 "빠른 wake 채널"로 쓰고 싶을 경우 이 필드를 명시적으로 낮은 값(예: 4)으로 재설정해야 한다는 뜻이며, 현재는 그렇게 되어 있지 않다.

---

## 항목_4 — Power Mode Timeout(0xC5) 강하 시퀀스·이벤트 NP 복귀

### 원문 확정 (§5.3 전체, 줄 917~939, Page 15~16)

> "The power mode is selected by writing the appropriate value to the Power Mode field in the System Control register.
> In order to optimize power consumption, power modes are stepped when the power mode is set to 'Automatic'. This moves the device to more power efficient modes when no interaction has been detected for a certain configurable time specified by the Power Mode Timeout register. Setting the power mode timeout to '0x00' will prevent the chip from lowering the power mode.
> In addition to 'Automatic' power mode, the IQS323 power mode switching can also be set to 'Automatic No ULP'. This functions identically to 'Automatic' mode except the device will never enter Ultra Low Power (ULP) mode.
> While the power mode switching is set to either 'Automatic' or 'Automatic No ULP', the IQS323 will return to normal power mode regardless of the current power mode if any events are triggered. Thereafter, the automatic power mode switching will take effect."

확정되는 사실:

1. **PM Timeout=0 → 강하 완전 차단**은 원문에 명시적으로 못박혀 있다(위 인용 3번째 문장). Sound1의 현재 설정(`REG_PM_TIMEOUT, 0x00, 0x00`)은 이 기능을 정확히, 의도한 그대로 사용하고 있다.
2. **NP→LP→ULP "stepped"(단계적) 강하**: 원문은 "more power efficient modes"(복수)라고만 하고 NP→LP→ULP 순서를 문자 그대로 나열하지는 않는다. 다만 (a) Power Mode 필드의 개별 선택지가 000=NP/001=LP/010=ULP 순으로 정의되어 있고(A.30), (b) Halt(011)는 이 "stepped" 서술 문단에 전혀 등장하지 않으며, (c) §5.2가 Halt를 Force Comm으로만 드나드는 별도 상태로 취급한다는 점에서 — **"Automatic/Automatic No ULP의 단계적 강하는 NP→LP→(ULP)까지만이고 Halt는 포함되지 않는다"**는 것이 구조적으로 타당한 추론이다. 이를 "Halt는 자동 강하 목적지가 아니다"라고 명시하는 문장은 원문에 없으므로 **[강한 정황 추론, 완전한 명시 확인은 아님]**으로 표기한다.
3. **타임아웃 값의 반복 적용**: Power Mode Timeout 레지스터는 0xC5 하나뿐이다(NP→LP용, LP→ULP용이 별도로 없음). 따라서 "같은 시간값이 각 단계 전이마다 반복 적용된다"(마지막 이벤트로부터 T 경과 시 NP→LP, 그 뒤로도 상호작용 없이 다시 T가 경과하면 LP→ULP)는 것이 유일하게 일관된 해석이다 — 원문이 "per-step 별도 타임아웃"을 언급하지 않는다는 배제법에 근거하며 **[정황 추론]**이다.
4. **이벤트 발생 시 NP 복귀 조건**: "any events are triggered"(위 인용 마지막 문단) — Table 8.1(줄 1866~1883)의 6종 이벤트(ATI Error·ATI Event·Power·Slider·Prox·Touch) **전부**가 이 "any event" 범주에 든다. **터치 이벤트로 국한되지 않는다.** 다만 실제로 어떤 이벤트가 "트리거"로 카운트되려면 `Events Enable`(0xD3, A.32/A.33)에서 해당 비트가 켜져 있어야 한다(§8.12: "Enabled events are reported... Global events can be individually enabled by setting the relevant bit"). Sound1의 `events_enable()`(`tdc_touch_iqs323.c:300~304`)은 `0x52` = **ATI Error·ATI Event·Touch Event만 활성화**, Prox Event·Slider·Power Event는 비활성 상태다. 따라서 **Sound1이 실제로 Automatic 계열을 가동한다면, "복귀 트리거"는 사실상 ATI 이상 또는 Touch뿐이며 Prox 단독으로는 NP 복귀가 걸리지 않는다** — 이는 원문(범용) + 코드(Sound1 한정) 교차검증으로 도출한 확정 사실이다.
5. **Automatic No ULP의 정확한 차이**: "identically to Automatic... except never enter ULP" — LP까지는 동일하게 단계 강하하되 ULP만 배제. Sound1의 현재 Power Mode 선택값은 바로 이 "Automatic No ULP"(101, A.30 bit6-4)이지만 PM Timeout=0으로 강하 자체가 막혀 있어 **실질적으로는 "Automatic No ULP를 선택은 해뒀지만 강하 자체가 잠겨 있어 결과적으로 항상 NP"** 상태다.

### §5.8과의 상호작용 — Sound1은 채널 타임아웃 비충돌 확인

§5.8 각주(줄 1096~1097): "If channel prox and touch timeouts are used then ULP mode should not be used. For automatic power mode switching set the mode to 'Automatic No ULP'." Sound1의 `REG_SYSTEM_CONTROL` MSB=0x07(줄 318,447,453)은 CH0~2 Timeout Disable **전부 1(비활성화)**로 세팅돼 있다(A.30 bit10-8). 즉 **Sound1은 채널 타임아웃 자체를 쓰지 않으므로, "Automatic No ULP"를 택할 §5.8상의 강제 이유는 없다** — 현재 "No ULP" 선택은 이 각주 때문이 아니라 별도의(아마 과거 회귀 대응) 판단으로 보인다. 이 부분의 "왜"는 그라운드_3의 회귀 재구성 범위와 맞닿아 있어 본 항목에서는 사실 확인까지만 하고 판단은 넘긴다.

---

## 추가 발견 — 코드 교차검증에서 드러난 사실 (할당 항목 밖이지만 직접 관련)

작업 지시에 따라 코드를 실제로 대조하는 과정에서, 4개 항목 검증에 직접 영향을 주는 사실 세 가지를 추가로 확인했다. 원래 배경 설명이 전제한 것과 다를 수 있어 명시적으로 보고한다.

### 추가발견_1 — Interface Selection이 현재 Streaming(0)이다

`REG_SYSTEM_CONTROL`(0xC0)에 대한 전체 write는 코드베이스에 3곳뿐이다(전체 `src/2__cm3/Cortex-M3-src/` grep 결과, `tdc_touch_iqs323.c` 외 어디서도 0xC0을 쓰지 않음을 확인):

| 위치 | LSB | 16진 분해 (bit7=Interface Selection) |
|---|---|---|
| `beta_power_settings()` 줄 318 | `0x50` = `0101_0000` | bit7=**0**(Streaming), bit6-4=101(Automatic No ULP) |
| 부팅 Re-ATI 트리거 줄 447 | `0x54` = `0101_0100` | bit7=**0**(Streaming), bit2=1(Re-ATI) |
| RESEED 트리거 줄 453 | `0x58` = `0101_1000` | bit7=**0**(Streaming), bit3=1(Reseed) |

세 곳 모두 bit7이 0이다. A.30 원문(줄 3613~3615): "Bit 7: Interface Selection • 0: I2C Streaming • 1: I2C Events". **즉 현재 작업 트리 코드는 IQS323의 Interface Selection을 Event mode로 전환하는 write를 어디에도 갖고 있지 않다 — POR 기본값인 Streaming이 그대로 유지된다.** `events_enable()`(0xD3)은 ATI Error/ATI Event/Touch Event 플래그를 활성화하지만, 이것이 §8.12 문서화 위치와 달리 Streaming 모드에서도 System Status 래칭에 동일하게 영향을 주는지, 아니면 사실상 무의미한지는 원문에서 명시적으로 가르지 않는다 — **[미확인]**.

이 사실은 배경 메모("20260705_touch-event-mode-adoption에서 이미 확정된 사실")가 전제한 "Sound1은 event mode"라는 가정과 어긋난다. 이 작업 시점(2026-07-06)의 실제 작업 트리 코드가 그렇다는 것만 보고하며, `20260705` 라운드가 문서 단계(요구사항/분석/계획)에 머물렀는지, 다른 곳에 구현이 있는지에 대한 판단은 본 역할(그라운드_2) 범위 밖이라 [미확인]으로 남긴다.

### 추가발견_2 — IC는 현재 항상 하드웨어 NP에 고정되어 있고, SW의 "ULP"는 IC와 무관한 CM3 전용 개념이다

`tdc_touch_iqs323.c` 꼬리 주석(줄 497~499): "절전 전용 settings는 제거됨 — 절전도 노말과 동일한 IQS323 설정(운용 임계·Full ATI·PM timeout 0 → 항상 NP)을 그대로 사용한다. CM3 클럭만 ci_power_sleep(main.c)로 절감한다." `main.c` 확인 결과(줄 846, 1216~1218): "is_ulp 플래그 미사용... CM3 클럭만 ci_power_sleep로 절감" + `ci_power_sleep(); /* SYSCLK 30.72M → 2.56M, SLOWCLK 유지 */`. 그리고 `tdc_touch_iqs323_set_ulp()`(SW ULP 플래그 setter)는 코드베이스 어디서도 호출되지 않는다(정의만 있고 미사용, grep 확인).

**즉 Sound1이 현재 말하는 "ULP"는 IQS323 IC의 Power Mode 필드를 010(Ultra Low Power)으로 바꾸는 것이 전혀 아니고, CM3 MCU 클럭 저감 + CFX/DSP 코어 자체 ULP 진입 커맨드일 뿐이다.** IQS323은 시스템이 "ULP"라고 부르는 상태에서도 계속 하드웨어 NP로 동작한다(PM Timeout=0이 이를 보장). 따라서 본 문서가 다루는 "IQS323 하드웨어 LP/ULP/Halt 모드"는 **현재 가동 중인 상태가 아니라 전부 미래 가정(만약 IC의 Power Mode 필드를 실제로 LP/ULP로 바꾼다면)**이라는 점을 명확히 해둔다. 이는 C1/C2(후보 페르소나)의 설계 범위에 직접적인 전제 조건이 된다.

### 추가발견_3 — I2C Lock Up 실리콘 errata가 Force Comm을 명시적으로 지목한다

원문 Appendix C "Known Issues"(줄 3911~3923, Page 67):

> "**I2C Lock Up** — Versions Affected: IQS323-00x v1.3 and below. IQS323-A0x v1.4 and below. Issue Description: In certain cases the IQS323 can enter a state in which all bytes read over I2C return a constant value. **There is a higher likelihood of this occuring when using the force communications method described in Section 8.13.** Recommended Workaround: At the end of every I2C communication, read one byte from a register that does not exist. If the returned value is not 0xEE, hard reset the IQS323 by following the guidelines in Section 6.6."

이것은 항목_1의 결론("Force Comm은 모드 무관하게 항상 동작해야 한다")과 배치되지 않는 별도 차원의 리스크다 — **아키텍처상 유효한 기능이지만, 구 실리콘 리비전에서는 이 기능을 쓸수록 I2C 응답이 고정값에 락업될 확률이 올라간다는 제조사 공식 errata**다. Sound1은 `force_window_open()`을 모든 I²C 트랜잭션 앞단에 예외 없이 걸어두고 있어(줄 145~175, 모든 `write_register`/`read_register`/`tdc_touch_iqs323_read_debug` 경유), 만약 실제 탑재 칩이 영향 리비전이라면 이 errata에 가장 크게 노출되는 사용 패턴이다.

- Sound1이 실제 사용 중인 칩의 실리콘 리비전(Major/Minor Version, 0x01/0x02 레지스터)은 코드에서 전혀 read하지 않는다 — **[미확인]**.
- errata가 권장하는 워크어라운드(트랜잭션 말미에 존재하지 않는 레지스터를 읽어 0xEE 확인, 아니면 하드리셋)는 현재 코드에 구현되어 있지 않다. `confirm_reset()`/`tdc_touch_iqs323_read_status()`의 기존 `lsb==0xEE && msb==0xEE` 체크(줄 251, 357)는 **정상 레지스터 read가 우연히 0xEEEE로 돌아오는 글리치를 잡는 것**이지, errata가 말하는 "고정값에 락업"(0xEE가 아닌 다른 상수에 고정)까지 잡아내지는 못한다.
- 주문 코드는 `TDC_TOUCH_IQS323_SLAVE_ADDR 0x44`(`.h:28`)로 보아 IQS323-001(Release UI, 3-button self-cap, 주소 0x44, `07_오더링·패키지.md` Table 10.1)과 일치한다 — 이는 errata의 "IQS323-00x" 계열에 해당하나, 구체 리비전(v1.3 이하 여부)은 미확인이다.

이 발견은 할당 항목 밖이지만 "Force Comm이 항상 통신을 열 수 있는가"라는 최우선 검증 질문과 직결되므로 누락하지 않고 보고한다.

---

## 은수님 가설에 대한 종합 판정

| 가설 요소 | 판정 | 근거 |
|---|---|---|
| "ULP는 원리적으로 통신 자체가 불가능/차단된다" | **반박됨** | §8.13이 전력모드 무관하게 서술, §5.2가 Halt(최극단)조차 동일 기전으로 탈출 규정 |
| "ULP는 터치 발생 시에만 통신 가능" | **반박됨(단, 부분적으로 재구성 필요)** | Force Comm은 이벤트 유무와 무관하게 언제든 호출 가능한 요청이지 "이벤트 대기"가 아니다. 다만 §5.3의 "NP 복귀" 조건은 터치만이 아니라 활성화된 이벤트 전부(Sound1 기준: ATI Error/ATI Event/Touch)다 |
| "고정 45ms 폴링이 LP/ULP의 느린 Report Rate에서 실패할 수 있다" | **반박되지 않음, 오히려 가장 유력한 정밀 원인 후보** | §8.13의 t_wait "application specific" 문구 자체가 고정 상한을 보증하지 않음을 시사. 원문이 명시적으로 "t_wait는 Report Rate에 비례"라고 쓰지는 않으나 부정하지도 않는다 — [미확인이나 배제되지 않음] |
| "지금 당장 LP/ULP를 켜면 문제가 재현된다" | **판정 불가(범위 밖)** | 현재 IC는 PM Timeout=0으로 항상 NP 고정 — LP/ULP는 가동된 적이 없어 실측 재현은 별도 실험이 필요 |

**한 줄 요약**: Force Comm의 "작동 가능 여부"는 전력모드와 무관하다는 것이 원문상 명확하지만, "고정된 짧은 타임아웃이 모든 전력모드에서 똑같이 신뢰할 수 있는가"는 원문이 스스로 열어둔 질문이며 — 과거 회귀는 전자(불가능)가 아니라 후자(타임아웃 부정합)로 재해석하는 것이 더 정밀하다.

---

## 미확인 목록 (정직성 체크리스트)

- Report Rate 레지스터값 "0"이 하드웨어적으로 무엇을 의미하는지(무제한/최고속 vs 다른 의미) — 원문에서 명시 문장 못 찾음.
- ULP Report Rate 공식(Auto Prox Cycle Select × 0xC3)이 Event mode에서도 문자 그대로 재확인되는지 — §8.11.1(Streaming 절)에만 명시, 강한 정황 근거는 있으나 원문 명시 재확인 문장 없음.
- "Automatic/Automatic No ULP의 stepped 강하가 Halt를 포함하지 않는다" — 구조적 추론이며 원문의 직접 부정문은 없음.
- PM Timeout 레지스터 하나가 각 단계 전이마다 반복 적용되는지 — 배제법에 의한 추론.
- Sound1 실장 IQS323의 정확한 실리콘 리비전(Major/Minor Version 레지스터 미독취) — I2C Lock Up errata 해당 여부 판정 불가.
- `20260705_touch-event-mode-adoption` 작업이 실제 구현으로 이어졌는지, 이어졌다면 어느 커밋/파일인지 — 현재 작업 트리의 `tdc_touch_iqs323.c`에는 Interface Selection=Events로의 전환 write가 없음을 확인했을 뿐, 그 이유(미구현/다른 위치/계획 단계)는 판단 범위 밖.
- 0xD3(Events Enable)가 Interface Selection=Streaming 상태에서도 System Status 래칭에 실질적 영향을 주는지.
- AZD004 application note 내용 전반 — 데이터시트가 NP/LP/ULP 상세 설명을 위임하는 문서이나 본 작업 범위(iqs323_datasheet.pdf + 코드) 밖이라 열람하지 않음.
