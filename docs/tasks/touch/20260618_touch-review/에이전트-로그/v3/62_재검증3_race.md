---
name: 재검증3_race — 59_종합-재검증2 adversarial 공격 (race 관점)
purpose: 59_종합-재검증2를 race·타이밍·원자성·미검증 주장 관점에서 공격해 새 약점·놓친 시나리오·할루시·미검증 주장을 발굴
type: 에이전트-로그
maturity: in-progress
tags: [touch, iqs323, adversarial, race, timing, atomicity, revalidation, round3, v3]
---

# 62 — 재검증 라운드3 (race 관점) adversarial 공격: 59_종합-재검증2 대상

**TL;DR**: 59_종합-재검증2를 race·타이밍·원자성 관점에서 공격한 결과, 수렴 선언된 항목 3건이 실제로 새 race 경로를 내포하거나 정정이 불완전하고, 신규 치명 3건·고위험 2건·중간 1건을 발굴했다. 특히 재검증2가 "수렴"으로 선언한 "사후 검증 방향 명확화"와 "3단계 비블로킹 R3_CHARGING 명세"가 각각 새 race 경로와 미검증 전제를 내포한다. 전체 16+8=24개 adversarial 노드를 통과한 아키텍처 원리 자체는 기각하지 않으나, 구현 전 해소 필수 블로킹이 추가로 발굴된다.

---

## 0. 공격 방법론

**페르소나**: race 관점 adversarial — 동의 없음, 결함만 발굴.

**입력**: 59_종합-재검증2.md 전체 + 코드 ground truth(iqs323.c·main.c·tdc_touch_config.h) + 42_재검증1_race·52_재검증2_race 이전 공격 내용.

**공격 기준**:
- 59가 "수렴"으로 선언한 항목이 실제 코드 또는 데이터시트 미확인 전제 위에서 새 race를 은닉하는가
- 59가 도입한 새 설계(정제된 gate_check·3단계 비블로킹 R3_CHARGING·사후 검증 방향 정정)가 기존에 없던 race 경로를 생성하는가
- 59가 블로킹으로 처리한 항목 이외에 숨겨진 race 전제가 있는가

**공격 축**:
- **축_R**: 해소됐다고 선언한 race가 코드 순서에서 여전히 열려 있는가
- **축_G**: 59가 도입한 새 설계 자체가 기존에 없던 race 경로를 만드는가
- **축_U**: 확정됐다고 처리했으나 실제로는 추정인 전제를 근거로 race가 없다고 단정하는가
- **축_S**: 삽입점 순서(apply_settings 내 레지스터 write·RESEED·gate_check 순서)가 race 없이 정합하는가

---

## 1. 공격_1 — 59의 "사후 검증 방향 명확화 수렴" 선언이 새로운 counts 방향 race를 생성한다 [치명, 축_G]

### 1-1. 59의 처리

59 §4 Phase 1 "사후 검증 정제":
```
RESEED 발행 → counts 1회 재읽기
  → counts < lo (터치 중 = counts 감소 방향): 정상 터치 진행 중, RTT 경보 불필요
  → counts > hi (상한 이탈): 이상 상태, RTT 경보 발행 ("race 추정, LTA IIR 자기치유 위임")
  → 재RESEED 없음
```

59 §9 수렴 확정 표: "사후 검증 재시도 3회 제거 + RTT 경보 방향 정정(counts 하한 ≠ race 경보 방향) — 재검증2 방향 명확화 후 수렴"

### 1-2. 신규 race 경로 (축_G)

59의 사후 검증 정제는 "counts > hi = 이상 상태 = race 추정"이라는 판정을 도입했다. 그런데 **이 판단이 성립하는 시나리오를 코드 순서로 추적하면 새 race 경로가 드러난다**:

```
[시나리오 A] 정상 Re-ATI 완료 후 LTA가 noTouch로 수렴 중
  → RESEED 발행(LTA = 현재 noTouch counts ≈ FACTORY_NOTOUCH)
  → 사후 counts 읽기 시점에 ESD 전하 누적으로 counts가 FACTORY_NOTOUCH + MARGIN_UP 초과
  → counts > hi → RTT 경보 발행 "race 추정"
  → 실제로는 ESD 누적이 원인, race가 아님 — 오경보
```

```
[시나리오 B] 복수 채널 펌웨어 확장 시 (미래 위험)
  → counts > hi 임계값이 고정 FACTORY_NOTOUCH_COUNTS + MARGIN_UP
  → 드리프트 방향이 양수(noTouch counts 증가)인 환경에서 Re-ATI 후 정상 counts가 hi 초과
  → 매 부팅마다 오경보 발행 — 현장 대응 불가
```

더 결정적인 race: **사후 검증에서 "counts > hi → race 추정" RTT 경보를 발행하고 LTA IIR 자기치유에 위임하는 설계에서, LTA IIR이 수렴하는 방향이 올바른지 검증이 없다.** RESEED 완료 후 LTA = FACTORY_NOTOUCH(올바름). 이후 counts가 hi를 초과한 상태는 self-cap 기준 "counts 증가 = capacitance 감소 = 손 완전 탈착 + ESD 방전"이다. LTA IIR은 이 증가된 counts를 추적해 LTA를 상향 조정한다. 결과적으로 다음 터치 시 delta = LTA(증가) - counts(터치 시 감소) → delta 과대 → false touch 위험.

**59의 미처리**: §4 사후 검증 정제에 "counts > hi 시 RTT 경보만 발행, LTA IIR 위임"이라고 썼지만 LTA IIR이 hi 초과 counts를 추적하면서 생기는 **false touch 경보 위험 경로**를 분석하지 않았다. 자기치유가 "counts 감소 방향(터치)"에서만 올바르고, "counts 증가 방향(ESD 방전 후 과도 값)"에서는 LTA를 위험한 값으로 수렴시키는 경로가 있다.

**요구 조치**: "counts > hi → RTT 경보 + LTA IIR 위임" 설계에 "LTA가 hi를 초과한 값으로 수렴하는 경우 후속 false touch 가능성" 분석을 §4 Phase 1 사후 검증 항목에 추가. ESD 방전 트랙(블로킹_B 실측)이 이 경로의 결정 게이트로 명시 연결되어야 한다.

---

## 2. 공격_2 — 59의 "3단계 비블로킹 R3_CHARGING 명세 수렴" 선언이 ATI 진행 중 RESEED 게이트 재진입 race를 은닉한다 [치명, 축_R+축_G]

### 2-1. 59의 처리

59 §4 Phase 3 R3_CHARGING 명세:
```
① re_ati_trigger() 발행 → s_ati_in_progress=true
② 폴링 중 s_ati_in_progress=true이면 터치 판정 결과 NOT_TOUCH 강제
③ tdc_drv_iqs323_is_ati_active() 확인 → false이면:
   iii-a. 현재 counts로 게이트 재확인
   iii-b. 통과 시 정상 ATI 완료, s_ati_in_progress=false
   iii-c. 이탈 시 ATI 오염 → RTT 경보 + 다음 주기 R3_CHARGING 재시도
```

59 §9 수렴 확정 표에는 이 3단계 구조가 수렴 항목으로 명시되지 않았으나, §4 Phase 3에서 "명세가 있어야 한다"는 표현으로 설계 확정처럼 기술됐다.

### 2-2. 신규 race 경로 (축_R+축_G)

**race 경로_1: s_ati_in_progress 플래그와 노터치 게이트 RESEED의 동시 도달**

```
[정상 부팅 경로] apply_settings() 진행 중
  → 노터치 게이트 통과 → RESEED 발행 → s_boot_touch_ignore 처리
[R3_CHARGING 경로] 충전 중 30초 타이머 도달
  → re_ati_trigger() → s_ati_in_progress=true
```

이 두 경로가 동시에 활성화될 수 있는 시나리오가 59에 없다. 충전 중 부팅(배터리 장착 직후 충전기 연결)이면 부팅 시퀀스 중 apply_settings()가 실행되는 타이밍과 R3_CHARGING 30초 타이머가 충돌하지 않지만, **Phase 1 노터치 게이트 RESEED와 Phase 3 R3_CHARGING이 동시에 활성화된 환경에서 두 경로의 상호 배제 보장이 없다**.

`s_ati_in_progress=true` 기간 동안 `write_ati_compensation()` 재진입 차단(§4 Phase 3 코드 스니펫)이 적용된다면, 노터치 게이트 RESEED 경로가 apply_settings() 내에서 `write_ati_compensation()`을 호출하는 순서와 충돌하는지 확인이 필요하다 [확정 필요: apply_settings()와 R3_CHARGING 트리거 경로의 상호 배제 구조].

**race 경로_2: iii-c 재시도가 노터치 게이트를 우회한다**

iii-c에서 "다음 주기 R3_CHARGING 재시도"라고 명세했다. 재시도는 30초 타이머를 기다리는가, 아니면 즉시 재발행인가? 59 §4 Phase 3에 "ATI_Error 후 re_ati_trigger() 재발행 시 30초 타이머 리셋 여부 명시(확인_13 신규)"가 있으나, iii-c 재시도가 게이트 재확인을 포함하는지 명시되지 않았다.

```
iii-c 재시도 시나리오:
  ATI 오염(iii-c) → RTT 경보 → 다음 주기 R3_CHARGING 재시도
  → 재시도 트리거 시 노터치 게이트 재확인 없이 re_ati_trigger() 발행?
  → 이 시점 터치 중이면 또 Re-ATI 오염
  → iii-c 재시도 루프
```

재시도마다 게이트 재확인을 수행하도록 명세됐는지 59 §4에서 명확하지 않다. "다음 주기 도달 시 현재 counts로 게이트 통과 확인 후 트리거"는 최초 발행 명세이고, iii-c 재시도 경로에서도 동일하게 적용되는지 별도 명시가 없다.

**요구 조치**: R3_CHARGING 3단계 구조에 (1) 부팅 apply_settings()와 동시 실행 시 상호 배제 보장 경로 명시, (2) iii-c 재시도 시 게이트 재확인 포함 여부 명시, (3) s_ati_in_progress 플래그 범위(노터치 게이트 RESEED 경로와의 충돌 여부)를 §4 Phase 3에 추가.

---

## 3. 공격_3 — gate_check() 런타임 언더플로 가드가 컴파일 타임 #error와 이중 체크 시 논리 비일관이다 [치명, 축_G]

### 3-1. 59의 처리

59 §4 Phase 1 `tdc_notouch_gate_check()` 정제 코드:
```c
if (TDC_TOUCH_FACTORY_NOTOUCH_COUNTS <= TDC_TOUCH_NOTOUCH_MARGIN_STRICT) {
    return false;  /* 비정상 설정 — RTT 경보 */
}
uint32_t lo = (uint32_t)TDC_TOUCH_FACTORY_NOTOUCH_COUNTS
              - TDC_TOUCH_NOTOUCH_MARGIN_STRICT;
```

동시에 §2-2에서 컴파일 타임 #error:
```c
#if TDC_TOUCH_NOTOUCH_GATE_ENABLE && \
    (TDC_TOUCH_NOTOUCH_MARGIN_STRICT >= TDC_TOUCH_FACTORY_NOTOUCH_COUNTS)
#error "NOTOUCH_MARGIN_STRICT must be < FACTORY_NOTOUCH_COUNTS (uint32 underflow risk)"
#endif
```

59 §9 수렴 확정 표: "gate_check() 언더플로 #error 추가 — 수렴"

### 3-2. 논리 비일관 race 경로 (축_G)

컴파일 타임 #error가 "MARGIN_STRICT >= FACTORY_NOTOUCH_COUNTS면 빌드 불가"를 보장한다면, 런타임 `if (TDC_TOUCH_FACTORY_NOTOUCH_COUNTS <= TDC_TOUCH_NOTOUCH_MARGIN_STRICT) { return false; }` 분기는 **도달 불가 코드(dead code)**다.

**도달 불가 dead code가 존재하는 것은 두 가지를 의미한다**:

가설_1: 컴파일 타임 #error가 GATE_ENABLE=0 환경에서 비활성화되는 경우가 있고, 런타임 가드가 실제로 필요한 경우가 있다.

```c
#if TDC_TOUCH_NOTOUCH_GATE_ENABLE && \
    (TDC_TOUCH_NOTOUCH_MARGIN_STRICT >= TDC_TOUCH_FACTORY_NOTOUCH_COUNTS)
#error ...
#endif
```

이 #error는 `GATE_ENABLE=0`이면 비활성화된다. 그런데 `tdc_notouch_gate_check()`는 `GATE_ENABLE=0`이어도 함수로 존재할 수 있다. **GATE_ENABLE=0 환경에서 누군가 gate_check()를 직접 호출하면 #error로 차단되지 않고 런타임 가드만 남는다.** 이 경로에서는 MARGIN_STRICT >= FACTORY_NOTOUCH_COUNTS 조합이 런타임에 도달할 수 있다.

가설_2: `FACTORY_NOTOUCH_COUNTS`가 런타임 EEPROM 로드값이라면 컴파일 타임 #error가 의미 없다.

블로킹_J 해소 후 Phase 2에서 EEPROM 로드가 활성화되면 `FACTORY_NOTOUCH_COUNTS`의 실효값이 런타임에 변경될 수 있다 [추정: EEPROM 로드가 매크로 상수를 변경하는지, 런타임 변수를 사용하는지 설계 미확정]. 런타임 로드값이 컴파일 타임 상수와 다를 경우, #error 체크는 초기 상수 기준이고 런타임 체크는 로드값 기준으로 서로 다른 기준을 보는 **이중 기준 race**가 발생한다.

**59의 미처리**: "컴파일 타임 #error + 런타임 가드 이중 체크"가 GATE_ENABLE=0 경로와 EEPROM 런타임 로드 경로에서 서로 다른 기준을 보는 잠재 불일관을 분석하지 않았다.

**요구 조치**: gate_check() 런타임 가드가 dead code인지 아닌지를 GATE_ENABLE=0 경로와 EEPROM 런타임 로드 경로 각각에서 명시. EEPROM 로드값이 MARGIN 상수와 비교되는 경우 컴파일 타임 #error가 의미 없어지므로 런타임 범위 확인을 EEPROM 로드 직후에 추가하는 방향 검토 필요.

---

## 4. 공격_4 — wait_re_ati_done() WDT 수정이 ATI_CALIB_MODE 경로와 Phase 3 R3_CHARGING 비블로킹 경로 사이 실행 흐름 충돌을 만든다 [고위험, 축_S]

### 4-1. 59의 처리

59 §2-1 Phase −1 즉시 수정:
```c
/* iqs323.c:581~623 wait_re_ati_done() 루프 본체 내 Sys_Delay 직전 삽입 */
SYS_WATCHDOG_REFRESH();
Sys_Delay(TDC_DRV_IQS323_DEFAULT_DELAY_MS * 100);
```

59 §9 수렴 확정 표: "wait_re_ati_done() WDT 갱신 미삽입 즉시 수정 필요 — 수렴 (Phase −1 긴급 항목)"

### 4-2. 실행 흐름 충돌 (축_S)

`wait_re_ati_done()` 루프 본체에 `SYS_WATCHDOG_REFRESH()` + `Sys_Delay(×100ms)` 삽입은 Phase −1 긴급 항목으로 수렴됐다. 그런데 이 수정은 Phase 3 R3_CHARGING 비블로킹 설계와 실행 흐름 충돌이 있다:

59 §4 Phase 3 R3_CHARGING 명세에서 "비블로킹 구조 — re_ati_trigger() 발행 후 다음 폴링에서 ATI_Active 확인"으로 `wait_re_ati_done()`을 사용하지 않는 경로를 설계했다. 그런데 **ATI_CALIB_MODE=1 경로(`iqs323.c:860`)에서는 wait_re_ati_done()을 blocking으로 호출**한다.

```
ATI_CALIB_MODE=1 활성 환경 + R3_CHARGING 비블로킹 활성 환경:
  R3_CHARGING → re_ati_trigger() → s_ati_in_progress=true
  동시에 ATI_CALIB_MODE=1 경로 → wait_re_ati_done() blocking 호출
  → wait_re_ati_done()이 ~2.3초 blocking 중 s_ati_in_progress=true
  → 200ms 폴링 루프에서 s_ati_in_progress=true → NOT_TOUCH 강제 (~10회)
  → wait_re_ati_done() 내 SYS_WATCHDOG_REFRESH()가 있어 WDT는 OK
  → 그러나 ATI_CALIB_MODE와 R3_CHARGING이 동시 활성일 때 어느 쪽이 "진짜" ATI 완료를 소비하는가?
```

R3_CHARGING이 `re_ati_trigger()`를 발행하면 IC에서 Re-ATI가 시작된다. 동시에 ATI_CALIB_MODE 경로에서 `wait_re_ati_done()`이 polling을 시작하면, 이 폴링이 R3_CHARGING이 발행한 Re-ATI를 "완료"로 소비하고 s_ati_in_progress=true를 해제하지 못한 채 blocking에서 빠져나올 수 있다.

**59의 미처리**: ATI_CALIB_MODE 경로와 R3_CHARGING 비블로킹 경로의 동시 활성 시나리오가 분석되지 않았다. Phase −1 WDT 수정이 ATI_CALIB_MODE와 Phase 3의 동시 활성 충돌을 무시한 채 "독립 함수 수정 1회로 두 경로 모두 커버"라고 단순화했다.

**요구 조치**: ATI_CALIB_MODE와 R3_CHARGING 동시 활성 빌드가드(`#if ATI_CALIB_MODE && REATI_ON_NOTOUCH_ENABLE → #error "동시 활성 불가"` 또는 실행 시 플래그 상호 배제) 또는 어느 경로가 Re-ATI 완료를 소비하는지 소유권 명시가 Phase −1 수정에 추가돼야 한다.

---

## 5. 공격_5 — 절전 RESEED 오염→ULP 즉시 재부팅 루프 §8 미해결 수록이 해소 경로를 제시하지 않아 구현자에게 미전달된다 [고위험, 축_U]

### 5-1. 59의 처리

59 §8 미해결 표:
> "절전 RESEED 오염→ULP 즉시 재부팅 루프 — GATE_ENABLE=0 환경 현재 코드에서 재현 가능 — 블로킹_G 해소 후 절전 RESEED 게이트화 여부 결정"

59 §9 수렴 확정 표: "절전 RESEED→ULP 즉시 재부팅 루프를 §8 미해결에 명시 — 수렴"

### 5-2. 해소 경로 미전달 race (축_U)

52_재검증2_race 공격_1에서 이 경로를 치명으로 발굴했고, 59가 §8에 수록하는 것으로 "수렴"을 선언했다. 그러나 **§8 수록이 해소가 아니라는 점이 구현 단계에서 문제가 된다**:

```
현재 코드 경로 (GATE_ENABLE=0 기본값):
  func_sleep() pressed==false 대기(수백 ms)
  → apply_sleep_settings() + ci_power_sleep() 실행 중 터치 재개
  → RESEED 발행 → LTA = 터치 counts (오염)
  → ULP 루프 → 즉시 SLEEP_THRESHOLD 초과 → WDT 리셋 재부팅
  → 재부팅 후 GATE_ENABLE=0 → RESEED 무조건 → 오염 LTA 유지
  → 절전 재진입 시도 → 같은 루프 반복
```

59는 "블로킹_G(MCLR POR 실측) 해소 후 절전 RESEED 게이트화 여부 결정"이라고 했다. 그런데 이 루프는 **MCLR이 LTA를 POR 초기화하는지 여부와 무관하게 GATE_ENABLE=0 환경에서 현재 코드에서 재현 가능하다**:

- MCLR이 완전 POR이면: 재부팅 후 auto-ATI → RESEED 무조건(GATE_ENABLE=0) → LTA = 현재 counts. 여전히 손을 올려놓은 상태면 터치 counts = LTA → RESEED 오염.
- MCLR이 부분 POR이면: LTA 보존 + RESEED 재오염으로 더 나쁜 상태.

**블로킹_G 결과와 무관하게 절전 진입 RESEED 게이트화가 필요한 경로가 있다.** 59는 이 경로의 해소를 블로킹_G에 귀속시켜 미룬 채 §8 "수록"으로 수렴을 선언했다. 이것은 기각되지 않은 것이 아니라 **해소 경로 없이 묶어둔 것**이다.

**59의 미처리**: 절전 진입 시 pressed==false 확인 후 RESEED 발행 직전 게이트 확인(공장값 기준 noTouch 범위 체크)을 Phase 1 계획에 추가하는 것이 블로킹_G와 무관하게 즉시 설계 가능하다. 59 §4 Phase 1에 "절전 진입 RESEED 게이트화"가 명시적으로 없다.

**요구 조치**: Phase 1 노터치 게이트 설계에 "절전 진입 RESEED에도 동일 게이트 적용(FACTORY_NOTOUCH_COUNTS 기준 체크)" 명시. 블로킹_G 결과 대기와 무관하게 게이트 구조 자체는 Phase 1 착수 시 포함. §8 미해결 표의 해소 경로를 "블로킹_G 해소 후" 조건 없이 "Phase 1 게이트 설계에 포함" 방향으로 수정.

---

## 6. 공격_6 — 확인_14(R3_CHARGING 게이트 기준 정책)가 "공장 정적 기준 시대착오"라는 실제 운용 race를 과소평가한다 [중간, 축_U]

### 6-1. 59의 처리

59 §5 확인_14: "R3_CHARGING 게이트 기준: 공장값(정적) vs 마지막 Re-ATI 후 noTouch counts(동적) 정책 결정 필요 — 고위험, Phase 3 착수 전"

### 6-2. 운용 race 분석 (축_U)

59가 "확인 필요"로만 처리한 이 항목은 실제 운용 시나리오에서 결정론적 race를 만든다:

```
[시나리오] Phase 3 Re-ATI N회 성공 후
  → 각 Re-ATI 후 MULT/COMP가 해당 시점 환경 기준으로 갱신됨
  → 1년 후 noTouch counts가 공장 기준 대비 ±30 이탈 (드리프트 누적)
  → R3_CHARGING 트리거 시 게이트 기준 = 공장값(FACTORY_NOTOUCH_COUNTS)
  → 현재 정상 noTouch counts = FACTORY_NOTOUCH_COUNTS ± 30
  → 게이트 MARGIN_STRICT < 30이면 → 게이트 영구 실패
  → R3_CHARGING Re-ATI 완전 차단 → 드리프트 보정 불가 → 감도 열화
```

이것은 "공장 절대 기준 → 노터치 게이트 → Re-ATI" 아키텍처 원리가 장기 운용에서 자기 모순에 빠지는 경로다. **공장 절대 기준을 게이트로 쓰는 한, Re-ATI가 환경 드리프트를 보정할수록 공장 기준 게이트는 더 자주 실패한다.** Re-ATI 성공 = 게이트 기준 이탈 = 다음 Re-ATI 차단의 순환이 발생한다.

59 §9 수렴 확정 표의 "단일 아키텍처 원리(공장 절대 기준 → 노터치 게이트 → Re-ATI) — 16개 adversarial 노드 기각 없음"이 이 순환 모순을 검토하지 않았을 가능성이 있다.

**59의 미처리**: 확인_14를 "정책 결정 필요"로만 처리하고, "공장 정적 기준 게이트 + 주기 Re-ATI" 조합이 장기 운용에서 자기 모순 순환에 빠지는 경로를 정식 결함으로 수록하지 않았다. 이것이 §8 미해결 표에 없다.

**요구 조치**: 확인_14 정책 결정에 "Re-ATI 누적 후 공장 기준 게이트 이탈 시 게이트 기준 업데이트 정책" 선택지 추가. §8 미해결 표에 "공장 기준 게이트의 장기 드리프트 자기 모순 경로" 수록.

---

## 7. 수렴 여부 판정

### 수렴 확정 (59 선언 이후 이번 공격에서도 기각되지 않은 항목)

| 항목 | 판정 |
|---|---|
| 단일 아키텍처 원리(공장 절대 기준 → 노터치 게이트 → Re-ATI) | 수렴 유지 (단, 공격_6의 장기 자기 모순 경로는 별도 추적 필요) |
| wait_re_ati_done() WDT 갱신 즉시 수정 필요 | 수렴 (단, 공격_4에서 ATI_CALIB_MODE 충돌 추가 분석 필요) |
| RTT_TUNING × FACTORY_CALIB 동시 활성 #error | 수렴 |
| gate_check() 언더플로 #error 필요성 | 수렴 (단, 공격_3에서 dead code 불일관 추가 확인 필요) |
| EEPROM 로드 경로 필요성 + 인자 전달 설계 방향 | 수렴 |
| SATURATE_DETECT_ENABLE 기본값 0u | 수렴 |
| ATI_Error 재시도 상한 N회 + FIXED 기저 유지 RTT 경보 명세 | 수렴 |
| 사후 검증 재시도 제거 방향 | 수렴 (단, 공격_1에서 counts > hi ESD 오경보 경로 추가 확인 필요) |

### 신규 약점 (3차 재검증에서 발굴)

| # | 약점 | 축 | 심각도 |
|---|---|---|---|
| 공격_1 | 사후 검증 counts > hi → RTT 경보 설계가 ESD 방전 시 LTA 상향 수렴 → false touch 위험 경로를 분석하지 않음 | G | **치명** |
| 공격_2 | R3_CHARGING 3단계 비블로킹과 부팅 apply_settings() 동시 실행 상호 배제 미명시 + iii-c 재시도의 게이트 재확인 포함 여부 미명시 | R+G | **치명** |
| 공격_3 | gate_check() 런타임 언더플로 가드가 컴파일 타임 #error와 GATE_ENABLE=0 경로·EEPROM 런타임 로드 경로에서 이중 기준 불일관을 형성 | G | **치명** |
| 공격_4 | ATI_CALIB_MODE blocking 경로와 R3_CHARGING 비블로킹 경로 동시 활성 시 Re-ATI 완료 소비 충돌 — Phase −1 수정이 이 충돌을 분석하지 않음 | S | **고위험** |
| 공격_5 | 절전 RESEED 오염→ULP 재부팅 루프 해소 경로를 블로킹_G에 귀속해 미룬 것이 MCLR POR 결과와 무관한 즉시 해소 가능 경로를 묻어버림 | U | **고위험** |
| 공격_6 | R3_CHARGING 게이트 기준(공장 정적) + 주기 Re-ATI가 장기 드리프트 후 자기 모순 순환에 빠지는 운용 race 경로 — §8 미해결에 없음 | U | **중간** |

---

## 8. 핵심 6줄

1. **[치명] 사후 검증 counts>hi 경로 ESD 오경보 + LTA false touch 위험**: 59 §4의 "counts > hi → RTT 경보 + LTA IIR 위임" 설계에서 ESD 방전 후 counts가 hi를 일시 초과하면 오경보가 발행되고, LTA IIR이 증가된 counts를 추적해 LTA를 공장값보다 높게 수렴시킨 후 다음 터치 시 delta 과대 → false touch 위험 경로가 있다 — 블로킹_B(ESD 실측) 해소 전 이 경로 차단 설계가 Phase 1에 필요하다.

2. **[치명] R3_CHARGING 3단계 설계의 상호 배제·재시도 게이트 재확인 미명시**: 59 §4 Phase 3 R3_CHARGING 3단계 비블로킹 명세에 (a) 부팅 apply_settings()와 동시 실행 시 s_ati_in_progress 플래그 상호 배제 구조, (b) iii-c 재시도 시 게이트 재확인 포함 여부가 없어, 구현자가 ATI 오염 루프에 빠질 수 있는 경로가 열려 있다.

3. **[치명] gate_check() 컴파일 타임 #error와 런타임 가드의 이중 기준 불일관**: 컴파일 타임 #error는 GATE_ENABLE=0이면 비활성화되므로 GATE_ENABLE=0 환경에서 gate_check()를 직접 호출하면 런타임 가드만 남는다. EEPROM 런타임 로드 설계 확정 후 FACTORY_NOTOUCH_COUNTS가 런타임 변수가 되면 컴파일 타임 #error 자체가 의미 없어지는 이중 기준 race가 발생한다.

4. **[고위험] ATI_CALIB_MODE와 R3_CHARGING 비블로킹 동시 활성 시 Re-ATI 완료 소유권 충돌**: Phase −1 wait_re_ati_done() WDT 수정이 "독립 함수 수정 1회로 두 경로 모두 커버"라고 단순화했지만, ATI_CALIB_MODE blocking과 R3_CHARGING 비블로킹이 동시에 활성화된 환경에서 Re-ATI 완료 소비 주체가 결정되지 않았다 — 상호 배제 빌드가드 또는 런타임 플래그 소유권 명세 필요.

5. **[고위험] 절전 RESEED 게이트화 해소 경로를 블로킹_G에 귀속해 즉시 가능한 설계를 미룸**: 절전 RESEED 오염→ULP 재부팅 루프는 MCLR POR 완전성과 무관하게 GATE_ENABLE=0 환경에서 현재 재현 가능하다 — Phase 1 노터치 게이트 설계에 절전 진입 RESEED 게이트화를 블로킹_G 대기 없이 포함하는 것이 가능하며 이를 §4 Phase 1에 명시해야 한다.

6. **[중간] 공장 정적 게이트 + 주기 Re-ATI의 장기 자기 모순 경로 §8 미수록**: 환경 드리프트 누적 후 Re-ATI가 MULT/COMP를 갱신할수록 실제 noTouch counts가 FACTORY_NOTOUCH_COUNTS에서 멀어져 R3_CHARGING 게이트가 영구 실패하는 순환이 발생한다 — 이것이 아키텍처 원리의 장기 운용 취약점이며 §8 미해결 표에 추가와 확인_14 정책 결정에 "게이트 기준 동적 업데이트 정책" 선택지 포함이 필요하다.
