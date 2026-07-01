---
name: 에이전트로그 01 — 분석_절전ATI정합
purpose: 절전 진입(func_sleep)→ULP 루프→롱터치 재부팅 전체 시퀀스를 Full ATI 관점에서 규명. threshold=255 무감 잔재 제거 후 절전 전용 설계 도출.
type: tasks/에이전트-로그
applies_to: [Sound1]
maturity: stable
tags: [touch, iqs323, full-ati, sleep, ulp, threshold, reseed, analysis, 상태:완료]
---

# 에이전트 로그 01 — 분석_절전ATI정합

**TL;DR**: threshold=255는 ATI를 끄지 않고 임계를 무한대로 만들어 절전 중 터치를 완전 차단한다. ULP 루프의 롱터치 2.2s 경로는 현재 코드에서 실질적으로 동작 불가(무감 255로 CH0 Touch bit가 절대 set 안 됨). reseed()는 절전 직후 LTA를 저속 클럭 환경 counts로 재동기한다. 무감 255 제거 후 절전 중에도 운용 임계(threshold=16) 그대로 유지하면 추가 Full ATI 재실행 없이 터치 감지·롱터치 재부팅이 가능하다. 단, 절전 환경 드리프트 대비 reseed 타이밍이 핵심이다.

---

## 1. 현재 apply_sleep_settings()의 레지스터 의미 규명

### 1.1 0x62 = 0xFFFF (threshold=255, hysteresis=255)

코드 근거: `tdc_touch_iqs323.c:413` — `touch_settings(255, 255)`

데이터시트 A.17 (06_레지스터레퍼런스.md:392~399):
- `Touch Settings (0x62)`:
  - `TOUCH_THRESHOLD` (bits[7:0] = LSB) = 255
  - `TOUCH_HYSTERESIS` (bits[15:12] = MSB bits[7:4]) = 0xFF → 상위 4비트(bits[15:12])만 유효 → 0xF = 15

**절대 터치임계 공식** (데이터시트 A.17):
```
절대 터치임계 = TOUCH_THRESHOLD × LTA / 256
             = 255 × LTA / 256
             ≈ LTA (거의 동일)
```

즉 터치로 인한 delta(LTA − Counts)가 LTA 자체만큼 커야 터치 진입 조건 `(LTA − Counts) > 절대임계`가 충족된다. 자기 정전용량 방식에서 손가락 터치로 발생하는 delta는 수십 counts 수준이며 LTA 전체(수백 counts)에 달하는 delta는 실질적으로 불가능하다.

**결론**: 0x62=0xFFFF는 ATI Mode(0x36)를 건드리지 않는다. IC는 Full ATI 모드(`ATI Mode bits[2:0]=100`)를 유지한 채로 MULT/COMP를 그대로 보존한다. 그러나 threshold=255로 인해 LTA에 근접한 delta 없이는 터치가 감지되지 않아 **사실상 무감** 상태가 된다.

**코드 주석과의 대조**: `tdc_touch_iqs323.c:413` 주석 "절전 최둔감 (오탐 방지) — 단일 적용". 원의도는 오탐 방지였으나, 결과적으로 롱터치 깨움 경로까지 차단하는 모순이 발생한다.

### 1.2 0xC0 = 0x0700 (Power Normal, CH timeout disable)

코드 근거: `tdc_touch_iqs323.c:418` — `write_register(REG_SYSTEM_CONTROL, 0x00, 0x07)`

데이터시트 A.30 (06_레지스터레퍼런스.md:505~520):
- LSB 0x00:
  - bits[6:4] = 000 → **Power Mode: Normal**
  - bit3 = 0 (Reseed 없음)
  - bit2 = 0 (Re-ATI 없음)
- MSB 0x07:
  - bit8 = 1 → CH0 Timeout Disable
  - bit9 = 1 → CH1 Timeout Disable
  - bit10 = 1 → CH2 Timeout Disable

**의미**: 절전 진입 시 IQS323의 Power Mode를 Normal(000)로 고정한다. 즉 IC 자체는 NP 고속 샘플링 모드로 계속 동작한다. 이는 CM3(EZ8300)가 절전(SYSCLK 2.56MHz 저하)에 들어가는 것과 별개다. IQS323은 3.3V로 계속 구동되며 보통 속도로 측정한다.

**ATI 모드 확인**: 0xC0 write는 `REG_SYSTEM_CONTROL`이다. `REG_SENSOR0_ATI_SETUP`(= `0x36`)은 건드리지 않는다. `apply_settings()`에서 기록한 `0x36 = 0x040C` (Full ATI Mode, bits[2:0]=100)는 `apply_sleep_settings()` 호출 후에도 **변경되지 않고 Full 유지**된다.

---

## 2. ULP 루프에서 터치 카운트 동작 분석 및 모순 규명

### 2.1 ULP 루프의 터치 감지 경로

코드 근거: `main.c:904~956` (func_sleep 내 ULP 루프)

```c
// main.c:911~930 핵심 로직
tdc_touch_state_t state = TDC_TOUCH_STATE_RESET;
if (tdc_touch_get_state(&state))
{
    if (state == TDC_TOUCH_STATE_TOUCH)
    {
        touch_cnt++;
        if (touch_cnt >= TDC_TOUCH_ULP_LONG_TOUCH_CNT)  // = 11
        {
            SYS_WATCHDOG_RESET();
        }
    }
    else
    {
        touch_cnt = 0;
    }
}
```

`tdc_touch_get_state()` 경로 (`tdc_touch.c:219~229`):
```c
bool tdc_touch_get_state(tdc_touch_state_t *p_state)
{
    tdc_touch_iqs323_status_t st;
    if (!tdc_touch_iqs323_read_status(&st)) { return false; }
    *p_state = st.pressed ? TDC_TOUCH_STATE_TOUCH : TDC_TOUCH_STATE_NOT_TOUCH;
    return true;
}
```

`tdc_touch_iqs323_read_status()` (`tdc_touch_iqs323.c:313~332`):
```c
out->pressed = (msb & (1u << 1)) != 0;   /* bit9 = MSB bit1, CH0 Touch */
```

즉 `pressed` 판정은 `System Status(0x10)` bit9(CH0 Touch)이다. 이 bit는 **IC 내부 터치 판정 결과**로, `TOUCH_THRESHOLD × LTA / 256`을 초과하는 delta가 있어야 set된다.

### 2.2 threshold=255 무감 상태에서의 모순

현재 `apply_sleep_settings()`가 `0x62 = 0xFFFF`를 쓴 후 ULP 루프가 시작되므로:
- 절대 터치임계 ≈ LTA (수백 counts)
- 실제 터치 delta ≈ 수십 counts
- **조건 `(LTA - Counts) > (LTA × 255/256)` 불충족 → CH0 Touch bit 절대 set 안 됨**

따라서 ULP 루프에서 `tdc_touch_get_state()`는 항상 `TDC_TOUCH_STATE_NOT_TOUCH`를 반환하고, `touch_cnt`는 절대 증가하지 않는다. **롱터치 2.2s → 재부팅 경로는 현재 코드에서 실질적으로 동작 불가**하다.

이것이 요구사항_1 "Fixed 잔재 제거"의 근거다.

---

## 3. reseed()의 역할과 절전 클럭 영향

### 3.1 절전 직전 터치 해제 대기

코드 근거: `main.c:834~843`
```c
// func_sleep 진입부
tdc_touch_iqs323_status_t st;
while (tdc_touch_iqs323_read_status(&st) && st.pressed)
{
    ci_printv("[TOUCH] SLEEP: WAIT TOUCH RELEASE \r\n");
    delay_ms(100);
}
```

손이 놓인 것을 확인한 후 절전 진입 시퀀스를 진행한다. 이 시점에 LTA는 "운용 환경(노터치 상태)"의 counts를 반영하고 있다.

### 3.2 절전 환경에서의 클럭 강하와 counts 변화

코드 근거: `main.c:885~889`
```c
ci_power_sleep(); /* SYSCLK 30.72M → 2.56M, SLOWCLK 유지 */
i2c_set_master_prescale(I2C_MASTER_PRESCALE_6);
tdc_touch_iqs323_reseed(); /* 클럭 강하 후 LTA를 절전 counts로 재동기(순수 bit3) */
```

**SYSCLK 강하의 영향**: CM3 측 클럭이 30.72MHz → 2.56MHz로 낮아지면 I2C SCL 주파수도 변한다. `i2c_set_master_prescale(I2C_MASTER_PRESCALE_6)`로 SCL을 약 122kHz로 유지하도록 보정한다. IQS323 자체 클럭은 독립이므로 IC의 측정 주기(NP report rate)는 영향받지 않는다.

**LP report rate**: `apply_settings()`에서 `0xC2 = 0x0064`(LP report rate = 100ms)를 기록했다. 그러나 `apply_sleep_settings()`에서 Power Mode를 Normal(000)로 쓰므로 절전 중 IC는 NP 모드로 동작한다. `0xC1`(NP report rate)은 `apply_settings()`에서 명시적으로 기록되지 않았으므로 기본값 0 또는 PM_TIMEOUT(5000ms) 내 자동 LP 전환 가능성이 있다. 단, `apply_sleep_settings()`의 Power Mode=Normal 고정으로 인해 자동 모드 강하도 억제된다(0x0000 bits[6:4]=000 = Normal 고정).

**counts 변화 원인**: 온도 변화, CM3 측 디지털 노이즈 감소(SYSCLK 저하), IQS323 전극 주변 열팽창 등으로 절전 환경의 noTouch counts가 운용 환경과 수 counts ~ 수십 counts 다를 수 있다.

### 3.3 reseed()의 LTA 재동기 역할

`tdc_touch_iqs323_reseed()` (`tdc_touch_iqs323.c:380~383`):
```c
bool tdc_touch_iqs323_reseed(void)
{
    return write_register(REG_SYSTEM_CONTROL, 0x58, 0x07);
    /* 0x58 = bit3(Reseed=1) + bit4(Power 101, No ULP 중간) ... */
}
```

실제 LSB=0x58 = 0101 1000:
- bits[6:4] = 101 → Power Mode: **Automatic No ULP** (운용 설정과 동일)
- bit3 = 1 → **Reseed 트리거**

데이터시트 §5.5.1 (02_proxfusion동작.md:351~353):
> Reseed는 최신 측정 counts를 취해 LTA를 그 값으로 seed → 외부 환경 최신 조건에 LTA 일치. `System Control`의 `Reseed` bit set으로 명령, 완료 시 **자동 clear**.

**역할**: SYSCLK 강하 및 주변 환경 변화로 인해 절전 진입 직후 noTouch counts가 운용 환경 LTA와 미세하게 달라질 수 있다. reseed()는 현재(절전 환경) counts를 LTA로 즉시 동기화하여 **delta = 0 상태를 만든다**. 이후 손이 닿을 때 counts가 내려가면 delta = LTA - Counts > 0이 되어 터치로 판정된다.

**주의**: `reseed()`는 `REG_SYSTEM_CONTROL`에 Power Mode도 같이 쓰는데, `0x58` = bits[6:4]=101 = Automatic No ULP다. 반면 `apply_sleep_settings()`는 `0x00` = bits[6:4]=000 = Normal로 썼다. 즉 현재 코드에서 절전 시퀀스는:
1. `apply_sleep_settings()` → 0xC0 LSB=0x00 (Power Normal)
2. `reseed()` → 0xC0 LSB=0x58 (Power Automatic No ULP)

**결과**: reseed 직후 IQS323의 Power Mode가 Automatic No ULP로 덮인다. 이는 의도하지 않은 부작용이지만, ULP 루프 동안 IQS323이 LP 또는 ULP(No ULP 이므로 최대 LP)로 전환될 수 있어 소비 전류 관점에서 오히려 유리할 수 있다. 그러나 LP 모드에서는 report rate가 100ms(0xC2 설정값)로 느려질 수 있다. ULP 루프의 200ms 웨이크업 주기보다 report rate가 느리면 매 웨이크업마다 최신 데이터를 읽지 못할 수 있다.

---

## 4. 무감 255 제거 후 절전 전용 설계

### 4.1 핵심 판단: 절전 전용 임계값 필요 여부

**운용 임계 (threshold=16) 절전 유지 방안**:
- `apply_sleep_settings()`에서 0x62 write 자체를 제거 → apply_settings() 시 기록한 threshold=16, hysteresis=8 그대로 유지
- 절전 Full ATI 상태에서 reseed() 후 LTA = 절전 환경 counts → 절대임계 = 16 × LTA / 256 ≈ LTA/16
- 절전 중 손 터치 delta ≈ 수십 counts → 충분히 threshold 초과 가능

**검토**: 운용 감도(threshold=16)를 절전 중에도 유지하면 절전 모드에서의 오탐(전자기 간섭, 케이스 내 이물) 가능성이 있다. 그러나 요구사항.md §2(요구사항_1)에서 "오탐 방지"를 위한 별도 완화는 언급되지 않았고, 확정 답변(인터뷰)에서도 "Full ATI 적응임계 유지"로 확정되었다.

**절전 전용 임계 분리 방안 (선택지, 실측 게이트)**:
- `apply_sleep_settings()`에서 threshold를 운용 16보다 높은 값(예: 32~48)으로 설정 → 절전 중 오탐 완화, 롱터치는 충분히 감지
- 단, 절전 후 운용 복귀 시 `apply_settings()` 전체 재호출 또는 threshold만 재기록 필요
- 이번 범위(요구사항_1)에서는 운용 임계 그대로 유지를 권장하고, 실측 후 조정을 별도 작업으로 분리

**권장**: threshold=255 write를 제거하고 **운용 임계 threshold=16/hysteresis=8을 그대로 유지**한다. Full ATI가 산출한 MULT/COMP는 유지되므로 추가 ATI 재실행 불필요.

### 4.2 Full ATI 재실행 필요 여부

`apply_sleep_settings()` 현재 코드는 `REG_SENSOR0_ATI_SETUP(0x36)`을 건드리지 않는다. Full ATI 모드(bits[2:0]=100)와 ATI가 산출한 MULT/COMP(0x38/0x39)는 절전 진입 후에도 변경 없이 보존된다.

reseed()는 LTA만 재동기하며 MULT/COMP를 건드리지 않는다. 따라서:
- **절전 진입 시 Full ATI 재실행 불필요**
- reseed() 후 LTA가 절전 counts로 맞춰지면 IC의 터치 판정이 정상 동작

### 4.3 reseed 타이밍 최적화

현재 코드 순서 (`main.c:881~889`):
```
1. apply_sleep_settings()  ← 0x62=255 write (제거 대상), 0xC0=Normal
2. ci_power_sleep()        ← CM3 SYSCLK 강하
3. i2c_set_master_prescale() ← I2C 속도 보정
4. reseed()                ← LTA ← 절전 counts, Power=AutoNoULP 덮음
5. ci_timer_init_prescaled() ← ULP 200ms 타이머 설정
```

**최적 시퀀스**: reseed()를 SYSCLK 강하 후, ULP 루프 직전에 수행하는 현재 순서가 옳다. 강하 이전에 reseed하면 운용 환경의 counts로 LTA가 동기화되어 절전 환경과 delta가 생길 수 있다. 현재 순서 유지.

**제거 후 최소 변경 시퀀스**:
```c
void tdc_touch_iqs323_apply_sleep_settings(void)
{
    SYS_WATCHDOG_REFRESH();

    // [제거] touch_settings(255, 255) ← 이 줄만 삭제
    // threshold=16/hysteresis=8은 apply_settings()에서 기록된 채로 유지

    /* CH0~CH2 timeout disable (SW 타임아웃 사용). Full 이라 ATI MULT/COMP 재쓰기 없음. */
    if (!write_register(REG_SYSTEM_CONTROL, 0x00, 0x07))
    {
        ci_printe("[TOUCH] FAIL: SLEEP CH TIMEOUT DISABLE \r\n");
    }

    tdc_touch_iqs323_set_ulp();
    SYS_WATCHDOG_REFRESH();
}
```

### 4.4 구체 레지스터 write 시퀀스 (제안)

절전 진입 시 최종 레지스터 상태:

| 레지스터 | 주소 | 기록값 | 출처 | 설명 |
|---|---|---|---|---|
| CH0 Touch Settings | 0x62 | 0x0810 (threshold=16, hyst=8) | apply_settings() 유지 | **무감 255 제거, 운용 임계 그대로** |
| ATI Setup | 0x36 | 0x040C (Full ATI) | apply_settings() 유지 | Full 모드 유지 |
| System Control | 0xC0 | 0x0007 (Normal, CH timeout off) | apply_sleep_settings() | 절전 중 Normal 모드 유지 |
| System Control | 0xC0 | 0x0758 (AutoNoULP + Reseed) | reseed() | SYSCLK 강하 후 LTA 재동기 |

**최종 상태**: threshold=16, ATI Full, Power=AutoNoULP, LTA=절전counts

### 4.5 절전 중 터치 감지·해제·롱터치 동작 보장

제거 후 ULP 루프 동작:
1. `tdc_touch_get_state()` → `read_status()` → `System Status bit9` 읽기
2. 손 터치 시: Counts 감소 → (LTA - Counts) > 16×LTA/256 → CH0 Touch bit set → pressed=true → state=TOUCH
3. 손 해제 시: Counts 복귀 → delta 감소 → (LTA - Counts) < (16-8)×LTA/256 → CH0 Touch clear → state=NOT_TOUCH
4. 롱터치 유지: touch_cnt 11회(≈2.2s) → `SYS_WATCHDOG_RESET()` → 재부팅

**롱터치 카운트 계산 확인** (tdc_touch_time.h:36~38):
```
TDC_TOUCH_ULP_LONG_TOUCH_MS  = 2200
TDC_TOUCH_ULP_WAKE_MS        = 200
TDC_TOUCH_ULP_LONG_TOUCH_CNT = ceil(2200/200) = 11
```

main.c ULP 루프(main.c:930): `if (touch_cnt >= 11)` → WATCHDOG RESET.

---

## 5. 위험 및 미확정 사항

### 5.1 절전 중 드리프트로 LTA band 이탈 → 오탐/미탐

**위험**: 절전 중 온도 변화 등으로 noTouch counts가 천천히 drift하면 LTA(reseed 시점의 counts로 고정)와 현재 counts 간 delta가 쌓일 수 있다.

- **오탐 방향**: 온도 상승으로 정전용량 증가 → counts 감소 → (LTA - Counts) > 0 방향으로 증가 → threshold 초과 가능 → 잘못된 터치 감지
- **미탐 방향**: 온도 하강으로 정전용량 감소 → counts 증가 → delta 감소 → 실제 터치에도 threshold 미달

**완화**: IQS323의 LTA fast filter (0xB2)는 counts가 LTA로부터 fast filter band(0xB4=10) 이상 sensing **반대** 방향으로 drift할 때 빠른 LTA 갱신을 적용한다(02_proxfusion동작.md:362). 즉 counts가 LTA 위로 올라가면(오탐 반대 방향) fast filter가 LTA를 따라잡아 준다. 오탐 방향(counts가 LTA 아래로 drift)은 fast filter가 개입하지 않아 위험 실재한다.

**대응**: 절전 기간이 짧거나(수분) 온도 변화가 적으면 실용 범위 내. 장시간 절전(수시간) 환경 변화가 큰 경우는 실측 확인 필요. → **[실측 게이트]**

### 5.2 Re-ATI 자동 발동 가능성

데이터시트 §5.10 (02_proxfusion동작.md:642~648):
> re-ATI는 채널 LTA가 `ATI Band`(ATI Target 중심) 밖으로 drift할 때 실행.
> ATI Band = Large(1/8) 설정 → band = 1/8 × ATI Target

IQS323은 ATI Full 모드이므로 LTA가 ATI Target ± 1/8×ATI_Target를 벗어나면 자동 Re-ATI를 발동한다. 절전 중 Re-ATI가 발동되면:
1. Re-ATI 중 ati_active=1 → 이 기간 터치 판정 일시 중단 가능
2. Re-ATI 완료 후 새로운 MULT/COMP로 counts가 ATI Target 근처로 재정규화
3. LTA도 새 counts 기반으로 재동기(Re-ATI 완료 후 RESEED 없이도 LTA 자동 갱신 시작)

**위험**: Re-ATI 진행 중(ati_active=1) ULP 루프가 터치를 놓치는 순간이 생길 수 있다. 단, IC 내부 Re-ATI는 수십 ms 이내에 완료되므로(데이터시트 원문: "ATI 알고리즘은 짧은 시간에 실행되어 사용자가 인지 못 함") ULP 200ms 웨이크업 주기보다 짧으면 1 tick만 놓칠 가능성이 있다. 롱터치 11회 조건 특성상 1 tick 누락은 카운터 리셋(else: touch_cnt=0)을 유발하므로 롱터치 타이머가 리셋될 위험은 있다.

**완화**: 절전 중 Re-ATI는 드리프트가 클 때만 발생한다. 절전 기간이 짧으면 LTA가 ATI Band 경계를 벗어나기 어렵다. 실측으로 절전 중 Re-ATI 발생 빈도를 확인해야 한다. → **[실측 게이트]**

### 5.3 0xB2 LP Beta와 ULP 루프의 관계

`apply_settings()`에서 `0xB2 = 0x0202` (NP/LP LTA Fast Beta = 2). 데이터시트 §5.6:
> LP filter betas: 'Low Power'·'Ultra Low Power'일 때 사용.

`reseed()` 직후 Power Mode가 AutoNoULP(101)로 설정되므로 IQS323은 LP로 강하할 수 있다. LP 모드에서는 NP Beta(0xB1 상위 0x08)가 아닌 LP Beta(0xB1 하위 0x08)를 사용한다. 현재 NP/LP 모두 동일값(0x08)으로 설정되어 있어 모드 전환에 의한 LTA 추적 속도 변화는 없다.

### 5.4 Hysteresis 비트 매핑 [실측 게이트] (기존 잔존)

요구사항.md §3 비목표:
> Hysteresis 비트 매핑 불일치([실측 게이트]) 확정 — 계측으로 관측만, 코드 정정은 별도

0x62 write 시 `hysteresis` 인자는 MSB로 전달된다. A.17에서 TOUCH_HYSTERESIS는 bits[15:12](상위 4비트). `write_register(REG_CH0_TOUCH, threshold, hysteresis)`에서 hysteresis=8(0x08=0000 1000)이 MSB로 전달되면 bits[15:8] = 0x08 = 0000 1000이 된다. 실제 TOUCH_HYSTERESIS는 bits[15:12]이므로 상위 4비트 = 0000 → hysteresis 계수 = 0?

이 불일치는 기존부터 [실측 게이트]로 표기된 항목이며, 본 분석 범위에서는 현상 확인에 그치고 코드 정정은 별도 작업으로 분리한다.

---

## 6. 요약 결론

| 항목 | 현재 상태 | 제안 |
|---|---|---|
| threshold=255 의미 | ATI 비활성 아님, 절대임계를 LTA 수준으로 끌어올려 사실상 무감 | 0x62 write 제거, threshold=16 유지 |
| ULP 롱터치 동작 가능성 | 현재 불가 (CH0 Touch bit 절대 set 안 됨) | 제거 후 정상 동작 |
| ATI Mode 보존 | 절전 후에도 Full ATI 유지됨 (0x36 미변경) | 추가 ATI 재실행 불필요 |
| reseed() 역할 | SYSCLK 강하 후 LTA를 절전 counts로 재동기, delta=0 초기화 | 현재 시퀀스(강하 후 reseed) 유지 |
| 절전 전용 임계 | 필요 없음, 운용 임계 그대로 유지 권장 | threshold=16, hysteresis=8 그대로 |
| 드리프트 오탐 위험 | 실재, 온도 변화 시 counts drift → 오탐 가능 | 실측 게이트, 장시간 절전 추가 관측 필요 |
| Re-ATI 자동 발동 | 드리프트 크면 가능, ULP 중 1 tick 롱터치 카운터 리셋 위험 | 실측 게이트 |
| Power Mode 의도치 않은 덮임 | reseed()가 AutoNoULP로 덮음 (apply_sleep_settings Normal과 충돌) | 의도적 결과로 수용 가능 (소비 관점 유리), 주석 명시 권장 |
