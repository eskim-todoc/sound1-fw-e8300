---
name: adversary-assumption
purpose: retry-and-count 전략과 "결합 권고" 결론을 적극 공격 — 가정 허점·누락 대안·영구먹통 경로·전력 과소평가 검증
type: 에이전트-로그
maturity: stable
tags: [touch, iqs323, retry-and-count, ati-full, adversarial, failure-mode, power, ati-error, fixed-ati]
---

# 06 adversary — 가정 공격 반증가

> **TL;DR**: "ATI_Active 폴링 + retry-count 결합이 최적"이라는 결론에 4개 허점이 존재한다. (1) 결합 자체가 ATI_Error 영구먹통 경로를 해결하지 못한다. (2) ATI Full 전제가 필수인지 검토되지 않았다 — FIXED 유지가 더 단순한 원천 해법이다. (3) retry-count 단독으로는 ATI_Error 정지 상태에서 자동 복구 주체가 없어 영구먹통이 된다. (4) 폴링 주기 100ms 단축은 방전 측정 주기 교란이라는 과소평가된 부수 피해가 있다.

---

## 공격 과제별 반증

### 공격 1: "결합이 최적" — 정말 둘 다 필요한가?

#### 1-A. ATI_Active 폴링만으로 충분하고 retry-count는 사족 아닌가?

05_firmware-impl이 권고한 결합 구조:

```
[상위] ATI_Active 폴링 (드라이버 레이어) — ATI 중 대기
   ↓
[하위] retry-count (기능 레이어) — 예외 실패 안전망
```

**반론**: ATI_Active 폴링이 제대로 동작한다면 ATI 중 I2C 실패 자체가 발생하지 않는다. 드라이버가 ATI_Active=1을 감지하는 순간 상위 read를 막아버리기 때문이다. 그 상태에서 기능 레이어에 도달하는 I2C 실패는 오직 "진짜 하드웨어 고장"뿐이다.

그렇다면 retry-count가 추가되는 근거는 "ATI_Active 폴링이 실패할 수 있다"는 가정이다. 그런데 ATI_Active 폴링이 실패하는 경로는 곧 ATI_Active bit 자체를 읽지 못하는 상황 — 즉 I2C가 이미 죽어있다는 뜻이다. 이 경우 retry-count도 똑같이 I2C read를 통해 감지하므로, 두 계층이 동시에 무너진다. **ATI_Active 폴링이 살아있으면 retry-count는 불필요하고, ATI_Active 폴링이 죽으면 retry-count도 같이 죽는다.** 결합의 상보성 주장이 성립하지 않는 조건이 존재한다.

#### 1-B. retry-count만으로 충분한가?

05_firmware-impl 표(§4 엣지케이스)에서 "auto-ATI 1회가 100ms 초과 시 → 카운터 누적, 10회면 오판 고장"이 명시되어 있다. 04_timing-probability는 t_ati가 최대 1초(펌웨어 타임아웃 기준)이고 POR는 1.5초라고 한다. 100ms×10=1초이므로 **단 한 번의 정상 POR auto-ATI가 10회 카운터를 채울 수 있다**. 즉 retry-count 단독은 타이밍 함정으로 인해 정상을 고장으로 오판한다.

**결론**: 두 방식 모두 단독으로 충분하지 않다는 주장은 동의하지만, 그 이유가 "서로를 보완하기 때문"이 아니라 **ATI Full 자체가 I2C 타이밍을 예측불가능하게 만들기 때문**이다. 뿌리가 ATI Full에 있다면 결합보다 더 근본적인 질문이 있다.

---

### 공격 2: ATI Full 전제 자체를 공격 — FIXED 유지가 더 단순한 원천 해법 아닌가?

#### 2-A. 현 상태 확인

현재 펌웨어는 `ATI Mode=Disabled`(`0x36` LSB=`0x08`, bits[2:0]=000, `tdc_drv_iqs323.c:651`)이며, FIXED 보상값을 직접 쓴다(`docs/참고/touch/이슈해결/2026-06_CRX1-ESD-더미채널.md` 근거). I2C 통신 실패 문제(retry-count 논의의 출발점)는 오직 **ATI Full을 다시 켤 때** 발생한다.

03_touch-ati-interaction §2.2가 명시한다:

> "현 펌웨어: ATI Mode=Disabled(0x36 LSB=0x08, bits[2:0]=000=Disabled). 결론: 현 펌웨어에서는 Re-ATI가 자동으로 발동하지 않는다. §5.10의 'LTA drift → Re-ATI' 경로는 비활성 상태."

따라서 **retry-count·ATI_Active 폴링 논의 전체가 ATI Full을 다시 켠다는 전제 위에 서 있다.**

#### 2-B. ATI Full을 안 켜면 어떻게 되나?

FIXED 모드의 단점은 환경 변화(온도·습도)에 따른 감도 열화다. 그러나 Sound1 사용 환경(인공와우 외부기 — 귀 뒤 고정 착용)에서 LTA가 ATI Band를 이탈할 만큼 급격한 환경 변화가 발생하는 빈도는 검토되지 않았다.

만약 **FIXED 모드 + 노터치 게이트**로 충분하다면:
- I2C 블로킹 문제가 원천 제거된다.
- retry-count 임계 설계 논의 자체가 불필요해진다.
- ATI_Active 폴링 ~73줄 추가 자체가 불필요해진다.
- ATI_Error 핸들러 설계 불필요.

**1단계 분석 5개가 ATI Full 도입을 기정사실로 받아들이고 "그 전제 아래서 최선이 무엇인가"를 물었다. 그러나 "ATI Full 도입이 필요한가"는 물어보지 않았다.** 이것이 가장 큰 누락 대안이다.

#### 2-C. FIXED 유지의 구체적 리스크

공정한 공격을 위해 FIXED 유지의 리스크도 명시한다:
- LTA가 환경 drift를 추적하지 못해 감도 열화가 발생할 수 있다.
- `ATI Mode=Disabled`인 경우에도 LTA IIR 갱신은 동작하므로(`docs/참고/touch/데이터시트/02_proxfusion동작.md §5.6`), MULT/COMP 재산출 없이 LTA만 갱신된다. 이 상태에서 장기 drift가 터치 판정 임계를 벗어날 수 있다.

그러나 이 리스크가 실증된 필드 데이터가 있는지, 아니면 이론적 우려인지가 ATI Full 도입 결정의 핵심 근거여야 한다. 1단계 분석 어디에도 이 데이터가 없다.

**결론**: ATI Full 도입은 전제이지 결론이 아니다. FIXED 유지가 필드에서 충분하다면 retry-count 논의는 처음부터 필요 없다.

---

### 공격 3: ATI_Error 정지 상태에서 영구먹통 경로

01_datasheet-ati §5.11 원문:

> "A re-ATI is not automatically triggered when ATI Error occurs. The ATI Error bit is set and it is up to the master to manually trigger a re-ATI by setting the Re-ATI bit in System Control."

즉 ATI_Error가 발생하면 IQS323은 **자동 복구하지 않는다.** 마스터가 `0xC0 bit2`를 1로 써야 재시도가 된다.

#### 3-A. retry-count가 ATI_Error를 "고장"으로 오판하는 시나리오

```
[상황] ATI Full 동작 중 → ATI_Error 발생(bit6 set) → IQS323 정지
[retry-count 거동]
  → force_window_open() 타임아웃(ATI 완료 안 됐지만 ATI_Active≠1인 ATI_Error 상태)
  [주의] ATI_Error 상태에서는 ATI_Active bit(0x10 bit5)가 0이 된다
  → read_register()가 0xEE를 반환하거나 정상 반환할 수도 있다
  [문제] is_auto_ati_done_single_read은 ATI_Active==0이면 "완료"로 판정(L330)
  → tdc_drv_iqs323_read_status가 false 반환하는가 여부가 핵심
```

`wait_re_ati_done` (L581~623)을 보면 ATI_Error 발생 시 `return false`를 반환한다 (L608~610). 그러나 retry-count가 감시하는 것은 **`tdc_touch_get_state()`의 반환값**(`tdc_touch.c:341`)이고, 이는 `tdc_drv_iqs323_read_status()` (L1166~1183)의 반환값에 의존한다.

#### 3-B. ATI_Error 상태에서 read_status는 무엇을 반환하는가?

`tdc_drv_iqs323_read_status` (L1166~1183)는 내부적으로 `read_register(0x10, &lsb, &msb)`를 호출한다. ATI_Error 상태에서 ATI_Active=0이면 `force_window_open()`이 성공할 수도 있다 — ATI가 끝났기 때문에 RDY window가 다시 열릴 수 있다.

그렇다면:
- `read_register()` → 성공(true)
- `tdc_drv_iqs323_read_status()` → 성공(true)
- `tdc_touch_get_state()` → 성공(true)
- **retry-count 카운터가 오르지 않는다**

ATI_Error 상태를 retry-count가 인식하지 못한 채, 시스템은 잘못 보정된 MULT/COMP 값을 가진 IQS323에서 계속 터치를 read한다. 고장 판정도 발생하지 않고, 자동 복구(`0xC0 bit2`)도 발행되지 않는다.

현재 `tdc_touch.c:389~393`에서 `ati_error`를 `(void) ati_error`로 무시하고 있다. **ATI_Error가 set된 상태를 감지하는 코드가 없으므로, 수동 Re-ATI 트리거(`0xC0 bit2`)를 누가 발행할지 정해진 주체가 없다.** retry-count도 아니고, ATI_Active 폴링도 아니다(ATI_Active는 0이므로 폴링 통과). ATI_Error는 읽히지만 무시된다.

**영구먹통 경로**:

```
ATI Full 활성 → LTA drift 심함 → Re-ATI 발동 → ATI_Error 발생
→ ATI_Active=0(ATI 완료, 단 실패) → RDY window 재개방
→ read_status 성공(true) → ati_error bit 수신하지만 (void) 처리
→ retry-count 카운터 증가 없음 → 고장 판정 없음
→ 0xC0 bit2 미발행 → Re-ATI 재시도 없음
→ IQS323은 잘못 보정된 채로 계속 동작 or 감도 열화 영구화
```

이 경로는 retry-count와 ATI_Active 폴링이 **둘 다 있어도 해결되지 않는다.** `ati_error` 무시 정책이 바뀌지 않는 한.

#### 3-C. 결합 권고가 이 경로를 막는가?

05_firmware-impl §3 ATI_Error 핸들러 (~20줄)가 있으면 막힌다. 그러나 이 핸들러는 "ATI_Active 폴링 안"의 구성 요소이지 결합 권고의 핵심 결론이 아니다. 05의 결합 권고(`§5`)는 ATI_Active 폴링(1차)과 retry-count(2차)를 계층화하자는 것이지, ATI_Error 핸들러가 필수라는 것을 명시하지 않는다.

**결론**: 결합 권고는 영구먹통 경로를 닫지 않는다. ATI_Error를 감지하고 `0xC0 bit2`를 발행하는 명시적 핸들러가 없으면 이 경로는 열려있다.

---

### 공격 4: 100ms 폴링 단축의 과소평가된 악영향

05_firmware-impl §1-1에서 다음이 언급되었다:

> "방전 주기도 `get_state` 호출 단위이므로 함께 100ms로 빨라짐 — 방전 열화·측정 cycle 교란 여부는 별도 실측 필요(실측1 게이트)"

이것이 "별도 실측 게이트"로 처리되었지만 공격 관점에서는 과소평가다.

#### 4-A. 방전 측정 주기 교란

Sound1의 방전(discharge) 측정은 `tdc_touch_get_state()` 경로와 연동되어 있다. 폴링 주기가 200ms → 100ms로 줄면:
- 방전 cycle이 2배 빨라진다.
- 방전 측정 누적 카운트가 동일 시간 내 2배 쌓인다.
- 방전량을 절대값으로 계산하는 로직이 있다면 수치가 틀어진다.
- 방전 열화 판단 임계(있다면)가 잘못 트리거될 수 있다.

현재 코드에서 방전 측정이 `get_state` 호출에 직결되어 있는지 여부가 실측 전에는 불명이다. 이것이 "별도 실측 게이트"가 되는 이유다. 그러나 게이트를 통과 못 하면 100ms 단축 자체가 불가능해지고, 그렇다면 retry-count 임계 10회의 전제(100ms×10=1초 블라인드)도 재설계되어야 한다.

#### 4-B. 전력 소비 영향

100ms 폴링에서 매 주기마다 `force_window_open()` + I2C 트랜잭션 2회가 추가된다. 초당 10회 I2C 트랜잭션이 초당 20회로 늘어난다. 04_timing-probability §3(응답성 영향)은 계산했지만 소비 전력 증가량을 계산하지 않았다.

IQS323이 각 comm window 처리에 소비하는 전류와 CM3의 I2C 버스 활성 전류를 고려하면, 배터리 구동 기기(Sound1은 인공와우 외부기)에서 주기 단축이 미치는 실 영향은 무시 불가 수준일 수 있다. 이 수치가 검증되지 않았다.

#### 4-C. 롱터치 판정 시간 기준 미변경 확인

05_firmware-impl §1-1:

> "롱터치 누적 카운트 타이밍, `s_boot_5s_warned` 기준 등 시간 계산에는 영향 없음(절대 tick 비교이므로)"

이 부분은 정확하다 — `ULP_LONG_TOUCH_MS`(2200ms)는 tick 차이로 계산하므로 poll 주기와 무관하다. 그러나 04_timing-probability §3이 계산한 "10회×200ms=2초 블라인드(롱터치의 91%)" 문제는 **poll을 100ms로 낮춰도 "10회×100ms=1초 블라인드(롱터치의 45%)"로만 개선**된다. 45%도 여전히 큰 비율이다.

**결론**: 100ms 단축은 방전 주기 교란 리스크와 전력 증가를 수반하며, 이것이 해결되지 않으면 retry-count의 핵심 전제(10회=1초)도 흔들린다.

---

## 종합 반증표

| 허점 번호 | 공격 대상 | 반증 핵심 | 심각도 |
|---|---|---|---|
| **허점_1** | 결합의 상보성 | ATI_Active 폴링이 살아있으면 retry-count는 불필요. 둘이 동시에 죽으면 결합도 무의미. 상보 조건이 성립하지 않는 케이스 존재. | 중 |
| **허점_2** | ATI Full 전제 | FIXED 유지가 원천 해법. ATI Full 도입 필요성 검증 없음. 결합 논의 전에 이 질문이 선행되어야 한다. | **높음** |
| **허점_3** | ATI_Error 영구먹통 | ATI_Error 발생 시 retry-count도 ATI_Active 폴링도 감지 못하고 `ati_error`는 무시(`void` 처리). `0xC0 bit2` 발행 주체 없음 → 영구 감도 열화 또는 정지. 결합 권고로 해결 안 됨. | **높음** |
| **허점_4** | 100ms 단축 | 방전 측정 주기 교란·전력 증가 미검증. 실측 게이트 통과 실패 시 10회 임계 전제 재설계 필요. | 중 |

---

## 누락된 대안

| 대안 | 설명 | 결합 권고 대비 장점 |
|---|---|---|
| **FIXED 유지 + 노터치 게이트** | ATI Mode=Disabled 현 상태 유지. I2C 블로킹 문제 원천 제거. | 복잡도 0, ATI_Error 경로 없음, 전력 증가 없음 |
| **ATI_Error 전용 핸들러만** | ATI_Active 폴링·retry-count 결합 대신, ATI_Error bit 감지 → 0xC0 bit2 자동 발행 1가지 경로만 구현 | 영구먹통 경로 명시 차단, 코드 단순 |

---

## 영구먹통 경로 유무

**존재한다.**

```
ATI Full 활성 → ATI_Error set(bit6) → ATI_Active=0 → RDY window 재개방
→ read_status 성공(I2C 통신 정상) → ati_error 수신 후 (void) 처리(tdc_touch.c:393)
→ retry-count 증가 없음, ATI_Active 폴링 통과
→ 0xC0 bit2 미발행 → ATI_Error 영구 유지 → 잘못 보정된 MULT/COMP 값으로 동작 지속
```

이 경로를 막으려면 결합 권고에 **ATI_Error 핸들러(0xC0 bit2 자동 발행)** 를 명시적으로 포함시켜야 하며, 이는 현재 결합 권고 ~19줄 + ~73줄 외에 추가 설계가 필요하다.
