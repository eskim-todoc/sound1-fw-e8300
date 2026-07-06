---
name: touch-이벤트모드-reportrate-결합안
purpose: 이벤트 모드(0xC0 bit7) 전환 시 Report Rate(0xC1)가 여전히 유효한 개념인지 데이터시트 원문(§8.11.1·§8.11.2·§3.4·§8.4·§6.6.1)과 현재 코드를 직접 대조해 판정하고, 유효하다면 결합 설계·구체 값·SW 폴링(100ms)과의 에일리어싱 회피안을 제시
type: tasks/에이전트-로그
maturity: experimental
tags: [touch, iqs323, event-mode, report-rate, aliasing, force-communication, ati-invariant, datasheet, heisenberg]
---

# 후보_3 — Events 모드 + Report Rate(0xC1) 결합안

**TL;DR**: §8.11.1은 Report Rate를 스트리밍 문맥에서만 서술하나, §3.4 전류소비표는 Event 모드 조건에서도 Report Rate별 전류 차이를 보여 결합안이 무의미하지 않음을 확인했다. §8.4·§6.6.1로 ATI 버스트 간섭 우려도 해소했고, 0xC1=70ms(SW 폴링 100ms보다 빠르게) 제안과 참고문서의 미검증 서술 정정을 담았다.

---

## 0. 조사 방법과 원문 대조 근거

모든 핵심 주장은 `docs/참고/touch/iqs323_datasheet.pdf`를 `pdftotext -layout`으로 변환한 원문에서 직접 확인했다(페이지 번호는 원문 하단 "Page N of 68" 푸터 위치로 직접 산정, 목차 페이지 표기와 교차 일치 확인). 코드 사실은 `tdc_touch_iqs323.c`(전체 499줄) 직접 통독으로 확인했다.

| 절 | 원문 페이지 | 확인 내용 |
|---|---|---|
| §8.11.1/8.11.2 | p.32 | Report Rate의 서술 위치가 Streaming 문맥에 한정되는지 |
| §3.4 Current Consumption | p.11 | Event 모드 조건에서 Report Rate별 전류 차이 실측표 |
| §8.4 Communication During ATI | p.30 | ATI 진행 중 I²C 통신 비활성화 규정 |
| §6.6.1 Reset Indication | p.22 | Reset Event bit 설정 상태에서 통신창 연속개방→ATI 지연 인과 |
| §9 Memory Map(0xC1행) | p.37 | 0xC1 범위·기본값 |
| A.30 System Control | p.61 | 0xC0 bit7 Interface Selection 비트 정의 |
| `tdc_touch_iqs323.c` 전체 | - | 0xC1 미사용 확인, force_window_open()·write 사이트 재확인 |

---

## 1. 핵심 판정 — Report Rate는 Streaming 전용 개념인가

### 1.1 §8.11.1·§8.11.2 원문 직접 인용

**§8.11.1 I²C Streaming(원문)**:
> "In I2C streaming mode data is constantly reported at the relevant power mode report rate specified in milliseconds by the Normal Power Report Rate, Low Power Report Rate and Ultra Low Power Report Rate registers."

**§8.11.2 I²C Event Mode(원문, 전문)**:
> "In event mode the RDY line will only go low when one or more of the enabled events are triggered or if the device resets. This is usually enabled since the master does not want to be interrupted unnecessarily during every cycle if no activity occurred."

**표면적 사실(정직한 1차 확인)**: §8.11.1의 문장은 명시적으로 "streaming mode" 문맥에서만 Report Rate를 언급한다. §8.11.2(Events)는 Report Rate라는 단어를 **한 번도 사용하지 않는다**. 이 두 문단만 놓고 보면 "Report Rate=Streaming 전용, Events에서는 무관"이라는 결론도 나올 수 있다 — 이것이 이 결합안 자체를 무의미하게 만들 수 있다는 우려의 근거다.

### 1.2 반례 발견 — §3.4 전류소비표(원문 p.11)

전류소비표 상단 시험조건(원문, 직접 확인):
> "Self-capacitive Mode Setup: ATI Target = 512, Fxfer = 500 kHz ... **Interface Selection: Event mode**"

그리고 표 본문(원문, 발췌):

| Power mode | Active channels | Report rate [ms] | Typical Current [µA] (1.8V/3.3V) |
|---|---|---|---|
| Normal Power | Self-capacitive (3 channels) | 16 | 125/125 |
| Low Power | Self-capacitive (3 channels) | 60 | 37.0/37.5 |
| Ultra Low Power | Self-capacitive (3 channels) | 160 | 4.00/4.00 |
| Halt | NA | 3000 | 2.00/2.00 |

이 표는 시험조건 전체가 **"Interface Selection: Event mode"로 고정**된 상태에서 측정됐다. 그런데도 "Report rate [ms]" 열은 전력모드별로 다른 값(16/60/160/3000)을 가지며, 이 값에 정확히 비례해 전류가 달라진다(16ms일 때 125µA, 3000ms일 때 2µA). **Interface Selection이 Event로 고정된 채인데도 Report Rate 값이 바뀌면 전류가 바뀐다는 것은, Report Rate가 IC 내부의 실제 변환(conversion) 주기를 계속 게이팅하고 있다는 뜻이다** — 만약 Event 모드에서 Report Rate가 정말 "무관"해진다면 이 표의 네 행은 Interface Selection이 동일하므로 전류도 동일해야 하는데, 실제로는 그렇지 않다.

### 1.3 판정

**§8.11.1/8.11.2의 문장 자체는 Report Rate의 역할을 Streaming 문맥에서만 명문화**하지만(데이터시트에 "Report Rate가 Event 모드의 내부 주기에도 적용된다"고 못박은 단일 문장은 없음 — 이 점은 정직하게 [미확인]으로 남긴다), **§3.4의 실측조건표는 별도의 근거로 같은 결론에 도달한다**: Report Rate는 통신 인터페이스 종류(Streaming/Events)와 무관하게 IC의 내부 변환·감지 주기를 결정하는 레지스터이고, Streaming/Events의 차이는 오직 "그 결과를 RDY로 매번 알릴지, 상태변화(이벤트) 시에만 알릴지"에 있다.

**결론: 이 결합안은 무의미해지지 않는다.** 오히려 Events 모드로 전환하면 RDY 자체 토글이라는 관측 지표가 사라지므로, Report Rate가 "내부 감지 주기가 얼마나 빠른가"를 판단할 수 있는 **거의 유일한 남은 레버**가 된다는 점에서 결합의 의의가 더 커진다(§3 참조).

---

## 2. 부가 발견 — ATI 버스트 간섭 위험 해소 (V3 §2 신규위험_1 재검증)

V3(`13_V3_S3검증.md` §2)는 "Report Rate가 ATI 버스트 내부 주기에도 적용되는지 미확인 — 적용된다면 `wait_ati_done_blocking()`·`try_finish_init()` 타임아웃 초과 위험"을 신규위험으로 남겼다. 본 절에서 이를 재검증한다.

### 2.1 §8.4 원문(직접 확인, p.30)

> "**8.4 Communication During ATI** — Provided the Reset Event bit in the System Status register is not set, I2 C communications are disabled for the duration of the ATI process."

이 문장은 조건부다: Reset Event bit가 **set이 아닌** 정상 운용 상태(우리 코드는 `ack_reset()`+`confirm_reset()`으로 매 부팅 이 조건을 만족시킨 뒤에만 이후 시퀀스를 진행)에서는, **ATI가 진행되는 동안 I²C 통신 자체가 통째로 비활성화**된다. Report Rate가 규정하는 것은 어디까지나 "통신(보고)의 주기"이므로(§8.11.1), 통신이 통째로 꺼져 있는 구간에는 Report Rate 값이 무엇이든 개입할 여지가 없다.

### 2.2 §6.6.1 원문(보조 근거, p.22) — 조건 차이 명시

> "While the Reset Event bit is set: ... ATI will take much longer to complete, since communication windows are continuously being opened"

이 문장은 §8.4와 **반대 조건**(Reset Event bit가 아직 set인 상태, 즉 부팅 초입 ACK Reset 이전)을 다룬다. 이 구간에서는 통신창이 계속 열려있고, 이것이 ATI를 오히려 **지연시킨다**고 데이터시트가 직접 인과관계를 명시한다. 이는 "통신 활동이 잦을수록 ATI가 느려진다"는 메커니즘을 제조사 스스로 인정하는 근거이지만, 이 문장이 다루는 상황은 우리 코드의 부팅 Re-ATI(447행)·ati_error Re-ATI(465행)가 실행되는 시점(이미 ACK Reset 완료 후, Reset Event bit clear 상태)과는 **다른 상태**다. 따라서 이 문장을 "Report Rate 값이 크면 런타임 Re-ATI가 느려진다"는 일반 규칙으로 직접 확장하는 것은 과잉해석이며, 어디까지나 유사 메커니즘의 정황 증거로만 취급한다.

### 2.3 결론

우리 시스템의 모든 ATI/Re-ATI 트리거(부팅 Full ATI, `tdc_touch_iqs323_re_ati()`)는 §8.4가 규정하는 "Reset Event bit clear" 상태에서 실행되므로, **ATI가 진행되는 동안은 Report Rate 설정값과 무관하게 통신 자체가 비활성화된다** — Report Rate는 ATI 완료 후 정상 런타임 구간에서만 의미를 갖는 별개의 축이다. **V3의 신규위험_1(ATI 버스트-Report Rate 간섭)은 기각한다.** `wait_ati_done_blocking()`(iqs323.c:328, 25회 폴링)·`try_finish_init()`(tdc_touch.c:153, 2500ms 타임아웃)에 대한 회귀 위험은 Report Rate 도입으로 인해 새로 생기지 않는다.

---

## 3. 결합 메커니즘 — 어디까지 의미가 있는가

### 3.1 force_window_open() 상시강제개방 아키텍처와의 상호작용

V3(`13_V3_S3검증.md` §3, 본 노드도 `tdc_touch_iqs323.c:145-159` 직접 재확인)가 발견한 사실을 그대로 계승한다: `force_window_open()`은 RDY가 이미 열려있지 않으면(149-152행) 0xFF 강제통신을 전송(154-159행)한다. Streaming에서는 Report Rate마다 IC가 자체적으로 RDY를 열어주므로 이 생략 분기가 자주 히트되지만, **Events 모드에서는 이벤트가 없는 한 RDY가 계속 닫혀있어**, 100ms마다 도는 `tdc_touch_process()`의 정기 폴링(`tdc_touch.c:296`, `TDC_TOUCH_POLL_INTERVAL_MS=100`, `tdc_touch_time.h:18` 확인)이 사실상 매번 강제개방(Force Comm) 경로를 타게 된다.

**함의**: 현재 아키텍처(force_window_open 그대로) 위에 Events 모드를 얹으면, "호스트가 이벤트를 통지받는 시점"은 여전히 100ms SW 폴링 주기가 지배한다 — RDY의 이벤트 기반 비동기 알림이라는 Events 모드 본연의 이점은 이 아키텍처에서 **아직 실현되지 않는다**(그 실현은 C4의 force_window_open 재설계 스코프).

### 3.2 그렇다면 Report Rate가 실제로 담당하는 역할

3.1의 제약 위에서, Report Rate가 실제로 좌우하는 것은 두 가지로 좁혀진다:

1. **읽어오는 데이터의 신선도(staleness bound)** — 매 100ms 강제개방으로 `read_status()`/`read_debug()`를 호출할 때, 그 시점 System Status/Counts/LTA 값이 "얼마나 최근에 갱신됐는가"는 §1.3에서 확인했듯 Report Rate가 결정한다. Report Rate를 정의하지 않고 POR 기본값 0으로 방치하면 이 신선도가 [미확인] 상태로 남는다.
2. **NP 고정 상태의 소비전류** — `beta_power_settings()`(320행)의 `PM Timeout=0`은 Automatic No ULP를 사실상 무력화하고 항상 NP를 유지시킨다(§5.3 원문 "Setting the power mode timeout to '0x00' will prevent the chip from lowering the power mode" 확인). 이 상태에서 실제 평균전류를 결정하는 유일한 변수가 NP Report Rate(0xC1)이며, 현재 이 값이 POR 기본값(0x0000, 의미 미확인)으로 방치되어 있다.

### 3.3 스코프 경계 — C4 의존성 명시

Touch/Prox 이벤트 자체는 "상태 진입/이탈"(Table 8.1)로 정의되므로, 그 진입/이탈을 IC가 알아채는 시점도 결국 내부 변환 주기(=Report Rate가 게이팅하는 그 주기, §1.3)에 종속된다고 보는 것이 §3.4의 근거와 정합적이다. 따라서 C4에서 force_window_open()이 진짜 RDY 인터럽트 기반으로 재설계되어 "이벤트 없으면 통신 없음"이 실현되더라도, **터치 감지 자체의 지연 하한은 여전히 Report Rate가 결정**한다 — 이 결합안(C3)은 현재 아키텍처(§3.1)에서는 "데이터 신선도+전류 정의"라는 제한된 효과를, C4 재설계 이후에는 "감지 지연 하한"이라는 더 본질적인 효과를 갖는, **C4와 독립적으로 유효하되 C4와 결합할 때 가치가 커지는** 레버다.

---

## 4. 값 제안

### 4.1 후보값과 근거

**제안: 0xC1(Normal Power Mode Report Rate) = 70ms** (LSB=0x46, MSB=0x00, little endian — §8.5 원문 "16-bit data is sent in little endian byte order" 확인, 기존 `write_register(addr, lsb, msb)` 호출 관례와 동일).

근거:
- SW 폴링 주기(100ms)보다 **의도적으로 빠르게** 설정 — §5의 에일리어싱 회피 논리와 직결.
- 레지스터 범위 0-3000ms(§9 원문, p.37) 내에서, Azoteq 자체 특성화 지점인 16ms(§3.4, self-cap NP)보다는 느리므로 미검증 극단 영역이 아니라 **문서화된 동작 범위 안쪽**에 위치한다. 다만 우리 채널 구성(CRX0 단일채널)과 §3.4 표 조건(3채널)이 다르므로 정확한 전류 수치는 표 값을 직접 대입할 수 없고 [미확인] — "16ms/3채널=125µA보다는 낮을 것"이라는 방향성 추정까지만 가능하다.
- 0xC1은 0xC0(System Control, 다른 6개 기능이 비트로 뒤섞여 5개 write 사이트마다 보존해야 하는 A.30 레지스터)과 달리 **전용 단독 레지스터**다(부록 A 목차 A.29→A.30으로 직행, 0xC1~0xC4·0xC5에 대한 별도 비트필드 부록 없음 확인) — 부팅 시 **한 번만** 쓰면 되고, V3가 지적한 "5곳 write 중 하나라도 누락하면 조용한 회귀"류의 위험이 구조적으로 없다.

### 4.2 구현 스케치(제안 — 본 단계는 분석, 코드 변경은 미착수)

```c
/* tdc_touch_iqs323.c 상단 매크로 영역, 기존 REG_LP_REPORT_RATE(0xC2) 옆에 추가 */
#define REG_NP_REPORT_RATE    0xC1   /* 신규: Normal Power Mode Report Rate */

/* beta_power_settings() 내부, 기존 0xC2 write(319행) 인접 위치에 추가 */
ok &= write_register(REG_NP_REPORT_RATE, 0x46, 0x00); /* NP report 70ms (신규, [실측 게이트]) */
```

기존 0xC2(LP Report Rate=100ms, 319행)는 PM Timeout=0으로 LP에 도달하지 못해 이미 죽은 값(S3 §4 기존 발견 재확인)이므로, 신규 0xC1과 값을 일치시켜야 할 이유는 없다 — 두 레지스터는 서로 다른 전력모드에 속해 독립적으로 결정한다.

### 4.3 "0"의 의미 미확인 — 실측 게이트

부록 A 전체(A.1~A.37)를 목차 대조까지 마쳤으나 0xC1~0xC4에 대한 전용 비트필드 설명은 **존재하지 않는다**(A.29 System Control 직전에서 A.30으로 바로 이어짐, 원문 p.60~61 확인). 따라서 "0=최대속도 연속변환"인지 "0=미정의/파행 동작"인지는 여전히 [미확인]이다. 이 문서가 제안하는 70ms는 이 불확실성을 명시적 값으로 대체해 없애는 것이 목적이며, 채택 전에 오실로스코프로 CTx0 duty cycle을 직접 관측하거나 Azoteq에 "0"의 정의를 문의하는 것을 실측 게이트로 권고한다.

---

## 5. 에일리어싱(맥놀이) 회피 방안

### 5.1 V3가 제기한 우려 재확인

V3(§2, 신규위험_2)는 "SW 폴링(CM3 클럭 기준)과 IQS323 Report Rate(IQS323 자체 발진기 기준)는 서로 다른 독립 발진기이므로, 주기를 일치시키면(100ms=100ms) 위상이 서서히 표류하는 느린 맥놀이가 생긴다"고 지적했다. 두 발진기 모두 공통 기준에 위상 고정되어 있지 않으므로, 주기가 같으면(또는 낮은 정수비 관계면) "SW가 매번 갓 갱신된 값을 읽는 국면"과 "매번 갱신 직전의 stale 값을 읽는 국면"이 느리게 번갈아 나타날 수 있다는 것이 핵심 우려다.

### 5.2 기존 참고문서 서술 정정 — 할루시네이션 감사 기여

`docs/참고/touch/데이터시트/05_i2c인터페이스.md` §8.13 NOTE 박스는 다음과 같이 서술한다:

> "(report rate + 20% 여유 권장, report rate 배수 타이밍 피할 것)" / "retry 시 report rate 배수 피할 것"

이 서술을 원문 §8.13 전체(p.33, Force Communication 절 전문)와 대조한 결과, **"20%"·"multiple"이라는 단어는 원문 전체(68페이지) 어디에도 등장하지 않는다**(전체 텍스트 grep 결과 0건). §8.13 원문은 다음이 전부다:

> "The minimum and maximum time between the communication request and the opening of a RDY window (twait) is application specific. The typical values of twait are 0.1ms ≤ twait ≤ 45ms." (각주: "Contact Azoteq for an application specific value of twait")

**정정**: 참고문서의 "report rate+20% 여유", "report rate 배수 피할 것" 서술은 **데이터시트 원문 근거가 없는, 참고문서 자체의 부가 해설(출처 미상)**로 확인된다. 이 서술이 우연히 V3의 맥놀이 우려와 방향이 같아 그럴듯해 보이지만, 데이터시트가 실제로 그렇게 권고한다고 인용해서는 안 된다. 이하 §5.3의 회피 논리는 이 정정된 사실 위에서, 데이터시트가 아닌 **일반 신호처리 원리(V3의 논리를 계승)**로만 뒷받침한다.

### 5.3 제안 근거 — "회피"가 아니라 "위상 무관 신선성 보장"

두 독립 발진기는 주기를 다르게 설정해도 공차(ppm 수준)로 인해 결국 위상이 서서히 표류한다 — "회피"란 표현은 엄밀히는 성립하지 않는다. 그러나 **주기 자체를 크게 다르게, 그것도 SW 폴링보다 확실히 빠르게(70ms < 100ms) 설정**하면 훨씬 강한 성질을 얻는다:

- 100ms=100ms(1:1)로 맞추면, 위상이 우연히 나쁜 정렬에 들어간 구간에서는 **매 폴링이 전부 1주기 stale**일 수 있고, 이 정렬이 깨지는 데 걸리는 시간은 오직 두 발진기의 공차 차이(전형적으로 매우 느림, 초 단위 이상)에 좌우된다 — 정렬이 나쁠 때 벗어날 방법이 없다.
- Report Rate=70ms(<100ms)로 설정하면, 임의의 위상에서도 SW의 연속 두 폴링 사이(100ms 경과)에 **최소 1회의 IQS323 내부 갱신이 반드시 일어난다**(100ms 동안 70ms 주기는 최소 1회, 평균 1.43회 완료). 즉 최악의 경우에도 "이번 폴링에서 읽은 데이터가 직전 폴링 이후 단 한 번도 갱신되지 않았다"는 상황 자체가 발생하지 않는다 — 이는 발진기 공차나 위상 표류와 무관하게 **주기 대소 관계만으로 결정론적으로 성립**하는, 앞의 "언젠가 정렬이 풀리길 기다린다"는 논리보다 훨씬 강한 보장이다.

따라서 핵심 권고는 "100과 다른 값을 고르라"가 아니라 **"SW 폴링 주기보다 유의미한 여유를 두고 빠르게 고르라"**이며, 70ms(margin 30ms)는 이 조건을 만족한다. 낮은 정수비 배수(50/33/25ms 등)를 추가로 피하는 것은 부차적인 안전판일 뿐, 위 결정론적 보장에 필수는 아니다.

---

## 6. 3대 불변식(INV-CTRL-1~3) 영향

| 불변식 | 영향 | 근거 |
|---|---|---|
| INV-CTRL-1 (Full ATI) | 무손상 — 오히려 확증 강화 | §2에서 확인한 대로 ATI 진행 중 통신 자체가 비활성화(§8.4)되므로 Report Rate 값이 ATI 소요시간에 개입할 경로가 없다 |
| INV-CTRL-2 (ATI 에러→Re-ATI) | 무손상 | 동일 근거(§2) — `tdc_touch_iqs323_re_ati()`(465행)가 트리거하는 Re-ATI도 §8.4의 "통신 비활성화" 규정을 그대로 따른다 |
| INV-CTRL-3 (첫 터치 해제 감지) | 무손상, 잠재적 개선 | 터치 상태 읽기의 신선도 하한이 방치된 0(미확인)에서 70ms(확정)로 바뀌므로, `boot_ignore`/`sleep_ignore` 게이트가 참조하는 터치 비트가 최악의 경우에도 70ms 이내 갱신값임이 보장된다 — 다만 debounce 카운터는 이미 폴링 단위(100ms) 여유를 두고 설계되어 있어 개선 체감은 크지 않을 것으로 예상 |

---

## 7. 남은 미확인 · 리스크

| 항목 | 상태 |
|---|---|
| Report Rate=0(POR 기본값)의 정확한 의미 | **미확인** — 부록 A 전체(A.1~A.37) 대조로 전용 비트필드 설명이 없음을 재확인. §3.4 조차 최소 시험값이 16ms이지 0이 아니다 |
| 70ms 설정 시 실제 소비전류 수치 | **미확인** — §3.4 표는 3채널 조건이라 1채널(CRX0 단독)인 우리 구성에 직접 대입 불가, 방향성(125µA보다 낮음)만 추정 |
| Force Comm twait(0.1~45ms)가 Report Rate 값에 실제로 비례하는지 | **미확인** — §8.13 원문은 "application specific, contact Azoteq"라고만 명시. 참고문서의 "report rate에 따라 달라진다"는 서술도 원문 미검증(§5.2에서 정정) |
| Events 모드의 실질적 지연 단축 효과 | force_window_open() 재설계(C4) 전까지는 제한적(§3.1~3.3) — C4 결과에 따라 본 결합안의 가치 평가가 달라질 수 있음 |
| 70ms이 touch 반응성·전류의 최적점인지 | **미확인** — 실측 게이트, 60~80ms 대역 내 벤치 튜닝 권고 |

---

## 핵심 결론 (3~5줄)

1. §8.11.1/8.11.2 원문만 보면 Report Rate는 Streaming 전용처럼 서술되어 있으나, §3.4 전류소비표가 Interface Selection=Event 조건에서도 Report Rate별 전류 차이를 실측 조건으로 보여준다 — **이 결합안은 무의미해지지 않는다**. 다만 이 결론은 단일 명시 문장이 아닌 교차확인 추론이라는 점을 분명히 밝힌다.
2. §8.4("ATI 진행 중 통신 비활성화")·§6.6.1 원문으로 V3가 남긴 "Report Rate-ATI 버스트 간섭" 신규위험을 기각했다 — INV-CTRL-1·2에 대한 회귀 우려가 해소된다.
3. 현재 force_window_open() 아키텍처(매 100ms 강제개방이 사실상 상시화)에서는 이 결합안의 효과가 "데이터 신선도 보장+전류 정의"로 제한되며, 이벤트 기반 저지연이라는 본연의 이점은 C4(force_window_open 재설계) 완료 이후에야 온전히 실현된다.
4. 0xC1=70ms를 제안한다 — SW 폴링(100ms)보다 확실히 빠르게 설정하면 두 독립 발진기의 위상 관계와 무관하게 "최소 1회 갱신 보장"이라는 결정론적 성질을 얻으며, 이는 참고문서의 미검증 서술("report rate+20%")보다 원문에 안전하게 근거한 논리다.
5. 참고문서 `05_i2c인터페이스.md` §8.13 NOTE의 "report rate+20% 여유", "report rate 배수 피할 것" 서술은 데이터시트 원문에 없는 출처 미상 부가 해설임을 확인해 정정했다 — 향후 이 문서를 인용할 때는 이 점을 반영해야 한다.
