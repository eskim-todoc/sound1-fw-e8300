---
name: 부팅ATI게이트 분석 (v3 페르소나 01)
purpose: 부팅 Auto-ATI 결과를 버리지 않고 노터치일 때만 baseline으로 채택하는 게이트 전략 설계
type: 에이전트-로그
maturity: in-progress
tags: [touch, iqs323, auto-ati, boot-gate, reseed, baseline, noTouch-gate, v3]
---

# 01 — 부팅 ATI 게이트 (v3 페르소나)

**TL;DR**: 현 펌웨어는 부팅 Auto-ATI 결과를 무조건 버리고 고정값(FIXED MULT/COMP)을 덮어쓴다. 핵심 개선 방향은 Auto-ATI 완료 직후 현재 counts를 읽어 "노터치 윈도우(공장값 ± 허용)" 안에 있으면 Auto-ATI 산출 MULT/COMP를 그대로 채택하고 RESEED를 허용, 윈도우 밖(터치 의심)이면 기존 고정값으로 폴백하고 RESEED를 보류하는 조건부 채택 게이트다.

---

## 1. 현 부팅 시퀀스 (코드 확정)

`tdc_drv_iqs323.c` 기준:

```
MCLR 리셋 (initialize.c:449)
  → Auto-ATI 진행 (IC 자율, POR 시 자동)
  → wait_auto_ati_done() 또는 is_auto_ati_done_single_read() 폴링 (L334~378 / L312~331)
      └─ 터치 중이면 TIMEOUT → 주석 "정상" (L376)
  → ack_reset_event() (L1104) — ACK 후 Auto-ATI 결과(MULT/COMP)는 레지스터에 존재
  → write_ati_compensation() (L1141) — 고정 MULT/COMP 덮어쓰기 → Auto-ATI 결과 **완전 폐기**
  → RESEED 무조건 발행 (L1147, 0xC0 0x08) — LTA ← 현재 counts (게이트 없음)
  → CH timeout 비활성 (L1155, 이중 방어)
```

**핵심 문제**: `write_ati_compensation()` 시점에 Auto-ATI가 산출한 MULT/COMP가 덮어써진다. 그 이전에 "지금 노터치인가"를 확인하는 코드가 없다.

---

## 2. 왜 Auto-ATI 결과를 채택하면 이득인가

`개념정리_터치동작모델.md §3-A` 확정 사항:

- MULT(게인)는 noTouch counts 절대값과 터치 시 Δcounts 진폭을 **동시에** 결정한다.
- Auto-ATI가 **노터치 상태에서** 완료하면 MULT/COMP는 그 보드의 실제 noTouch 정전용량에 맞춰 최적화된다 — 보드 편차가 흡수된 최적 게인.
- 현 고정값은 사전 측정 보드 기준이므로 해당 보드와 편차가 있으면 감도 오차(둔감/과민)가 발생한다 [추정].
- 따라서 **노터치 확인 조건부로 Auto-ATI 결과를 채택**하면 보드 편차 보정이 무료로 달성된다.

---

## 3. 게이트 전략 설계

### 3-1. 노터치 판별 기준

Cold-Start Ambiguity (`브레인스토밍_부팅터치모호성.md §2`): 부팅 시 미지수 ① 보드 기준 ② 터치 여부가 모두 미지. 미지수 ①을 공장값으로 고정하면 ② 판별 가능.

```
노터치 게이트 조건:
  현재 Counts ∈ [공장 noTouch 기준 − Δ_허용, 공장 noTouch 기준 + Δ_허용]
```

- 공장 noTouch 기준: EEPROM 저장 또는 대표 보드 실측값 [확정 필요: 실측5]
- Δ_허용: 환경 드리프트 최대폭 + 보드 편차 상한 합산 [확정 필요: 실측5]

> [!IMPORTANT]
> **현재 공장값/절대 noTouch 기준이 EEPROM에 없으면 게이트가 동작할 절대 잣대가 없다.** 단기 대안: 기존 고정 MULT/COMP로 대략적 noTouch counts를 역산하거나, RTT 실측으로 현 보드 대표값을 구해 하드코딩. 장기 대안: 공장 캘리브레이션 작업(`공장캘리` 페르소나 02 담당).

### 3-2. 게이트 흐름도

```mermaid
flowchart TD
    mclr["MCLR 리셋"] --> autoati["Auto-ATI 진행 (IC 자율)"]
    autoati --> poll["is_auto_ati_done_single_read() 폴링"]
    poll -->|ATI_Active=0 (완료)| readcounts["Counts 읽기 (현재 상태)"]
    poll -->|TIMEOUT (터치 중 정상)| fallback["고정 MULT/COMP 폴백"]
    readcounts --> gate{"Counts ∈ noTouch 윈도우?"}
    gate -->|YES — 노터치 확인| adopt["Auto-ATI MULT/COMP 채택 (write 생략 또는 검증 후 유지)"]
    gate -->|NO — 터치 의심| fallback
    adopt --> reseed_ok["RESEED 허용 (LTA ← 노터치 counts)"]
    fallback --> reseed_hold["RESEED 보류 → counts 회복 대기"]
    reseed_hold --> recover{"counts 노터치 윈도우 복귀?"}
    recover -->|YES| reseed_ok
    recover -->|TIMEOUT| degraded["저신뢰 폴백 RESEED (기존 동작)"]
    reseed_ok --> fixed_phase["ATI Mode=Disabled + CH timeout 비활성 (기존 유지)"]
    degraded --> fixed_phase
```

### 3-3. Auto-ATI 채택 시 MULT/COMP 처리

`write_ati_compensation()`은 MULT/COMP + `ATI_SETUP_LSB = 0x08`(ATI Mode=Disabled)을 한 번에 write한다.

- **채택 경로**: ATI_SETUP(Mode=Disabled 부분)만 write하고, MULT/COMP는 Auto-ATI 산출값 그대로 유지. 즉 `write_register(ATI_SETUP, 0x08, msb)` 만 실행, MULT/COMP write 생략.
- **폴백 경로**: 현행 `write_ati_compensation()` 전체 실행 (MULT/COMP 고정값 덮어쓰기).

> [!NOTE]
> Auto-ATI 완료 후 MULT/COMP 레지스터(0x38·0x3A)에는 IC가 산출한 값이 이미 기록돼 있다. 채택 경로에서는 그 값을 read해 로그로 남기고(RTT 디버깅), Mode=Disabled만 추가로 write하면 된다. 별도 write가 필요 없어 I2C 통신 횟수도 최소화된다.

### 3-4. RESEED 보류 시 counts 회복 대기

- 대기 중 counts를 폴링. 윈도우 진입 시 즉시 RESEED.
- timeout: `s_boot_touch_ignore` 기존 5초 가드와 유사하게 설정 [확정 필요].
- timeout 후에는 기존 동작(고정값 + RESEED 무조건)으로 폴백 — 운용 연속성 보장.
- 이 대기 구간에는 `s_boot_touch_ignore = true`를 유지해 상위 레이어에서 터치 판정을 억제한다.

---

## 4. 게이트 race 및 한계

`브레인스토밍_부팅터치모호성.md §6-B(2)` 확정:

| 위험 | 설명 | 완화 |
|---|---|---|
| **게이트 race** | 윈도우 진입 확인 ~ RESEED 실행 사이 터치 시작 | μs~수ms 간격으로 확률 극히 낮음 [추정]. 사후검증(RESEED 직후 counts 재확인) 가능 |
| **애매한 부분 터치** | counts가 윈도우 경계 근처 → 노터치 오판 | Δ_허용을 보수적으로 설정. 여전히 100% 불가 |
| **공장값 부재** | 절대 기준 없으면 게이트 작동 불가 | 단기: 기존 고정 MULT/COMP 기준 계산. 장기: 공장 캘리브레이션(02 페르소나) |
| **ATI_Error** | Auto-ATI 실패 시 0x10 bit6 set | 현재 `(void) ati_error` 무시. 채택 경로에선 ATI_Error=1이면 무조건 폴백 |
| **TIMEOUT 경로** | Auto-ATI가 timeout되면 Auto-ATI 결과 신뢰 불가 | TIMEOUT → 무조건 폴백(현 동작 유지) — 구현 변경 불필요 |

---

## 5. 구현 위치 (코드 레이어)

변경 대상: `tdc_drv_iqs323.c` — `tdc_drv_iqs323_apply_settings()` 또는 이에 준하는 초기화 함수 내 `write_ati_compensation()` 호출 직전.

```c
/* [제안] 게이트 삽입 위치 — write_ati_compensation() 호출 전 */

uint8_t counts_lsb, counts_msb;
bool use_auto_ati_result = false;

if (!auto_ati_timeout && !auto_ati_error) {
    /* Auto-ATI 완료 + 에러 없음 → Counts 읽어 노터치 윈도우 확인 */
    if (read_counts_ch0(&counts_lsb, &counts_msb)) {
        uint16_t counts = ((uint16_t)counts_msb << 8) | counts_lsb;
        if (is_in_no_touch_window(counts)) {  /* 공장값 ± Δ_허용 */
            use_auto_ati_result = true;
        }
    }
}

if (use_auto_ati_result) {
    /* ATI_SETUP (Mode=Disabled)만 write, MULT/COMP는 Auto-ATI 결과 유지 */
    write_register(ATI_SETUP_ADDR, TDC_DRV_IQS323_ATI_SETUP_LSB, TDC_DRV_IQS323_ATI_SETUP_MSB);
    ci_printi("[TOUCH] BOOT-GATE: Auto-ATI 채택 (counts=%u)\r\n", counts);
} else {
    /* 폴백: 기존 고정값 전체 write */
    write_ati_compensation();
    ci_printw("[TOUCH] BOOT-GATE: Fixed 폴백\r\n");
    /* RESEED 보류 → 회복 대기 로직 (별도 구현) */
}
```

> [!IMPORTANT]
> `is_in_no_touch_window()` 구현에 필요한 **공장 noTouch 기준값·Δ_허용**은 실측5(환경 드리프트·보드 편차 측정) 이후 확정된다. 현재는 `[확정 필요]`.

---

## 6. 현 `s_boot_touch_ignore`와의 관계

기존 `tdc_touch.c:46`의 `s_boot_touch_ignore`는 READY 전이 후 이미 RESEED가 완료된 상태에서 동작하는 **사후 방어**다. 본 게이트는 RESEED 직전(초기화 함수 내)에서 동작하는 **사전 방어**로, 두 방어가 상호 보완한다:

```
[사전] RESEED 게이트 (본 문서) — 오염된 LTA 생성 자체를 막음
[사후] s_boot_touch_ignore      — RESEED 후에도 터치 상태면 상위 판정 억제
```

두 레이어를 모두 유지하는 것이 권장된다.

---

## 7. 정리 — 핵심 결론 (7줄)

1. **현 문제**: `write_ati_compensation()` 호출로 Auto-ATI 결과(보드 맞춤 MULT/COMP)가 무조건 폐기되고, RESEED도 게이트 없이 실행 → 터치 부팅 시 LTA 오염.
2. **게이트 원리**: Auto-ATI 완료 후 counts가 공장 noTouch 윈도우 안에 있으면 노터치 확정 → Auto-ATI MULT/COMP 채택 + RESEED 허용.
3. **폴백 조건**: Auto-ATI timeout, ATI_Error, 또는 counts 윈도우 이탈(터치 의심) → 기존 고정값 write + RESEED 보류 → counts 회복 대기.
4. **채택 경로 구현**: ATI_SETUP(Mode=Disabled)만 write하고 MULT/COMP write 생략 — Auto-ATI 산출값을 레지스터에 그대로 보존.
5. **선행 필요**: 공장 noTouch 기준값과 Δ_허용 확정(실측5). 단기 대안: RTT 실측으로 현 보드 대표값 하드코딩.
6. **game race**: 윈도우 확인~RESEED 사이 race는 제거 불가이나 확률 극히 낮음 + 사후검증 + 자기치유(LTA IIR)로 완화 가능.
7. **기존 `s_boot_touch_ignore` 유지**: 본 게이트(사전 방어) + 기존 가드(사후 방어) 이중 구조가 권장.
