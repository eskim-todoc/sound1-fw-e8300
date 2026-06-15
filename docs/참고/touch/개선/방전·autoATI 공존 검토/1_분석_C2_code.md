---
name: 방전autoATI-1-분석-C2-code
purpose: auto-ATI Full 도입 시 discharge_crx0·CalCap·apply_settings 경로와의 코드 레벨 충돌·공존 지점 검증 (페르소나 C2 코드 통합)
type: 개선
maturity: experimental
tags: [touch, iqs323, auto-ati, discharge, calcap, code-review, coexistence]
---

# 1 · 코드 통합 검증 (C2) — discharge/CalCap/ATI 경로와 auto-ATI Full 공존

> **TL;DR**: 현재 코드는 CH0·CH1 모두 ATI Mode=Disabled(0x36/0x46=0x0408)로, 200ms마다 `discharge_crx0()`가 Sensor0 Setup(0x30)에 `0x02`(CH0 disable+CRX0 VSS)→`0x01`(enable+ctx0)을 verify 없이 토글한다. ATI Full로 바꾸면 **(1)** 0x30 enable 비트 토글이 채널을 끄는 순간 IC가 채널 재진입 시 재캘리브레이션(auto-Re-ATI)을 트리거할 수 있고 **(2)** 현 코드엔 Re-ATI 수렴 대기·완료 게이트가 200ms 폴링 경로에 전혀 없어(완료 대기는 init·dump·sleep 경로에만 존재) 방전과 ATI가 시간축에서 비동기 충돌한다. **CH1만 ATI Disabled + CH0만 Full**은 레지스터가 채널별(0x36 vs 0x46)이라 코드상 분리 가능하나, `discharge_crx0`가 0x30(CH0)을 토글하는 한 CH0 Full과는 충돌. **결론: 조건부불가** — discharge를 현 형태로 두고 CH0 Full을 켜는 단순 공존은 코드상 안전 게이트 부재로 불가. CH0 Full을 쓰려면 방전을 ATI 비활성 구간으로 시간분리하거나 0x30 토글이 아닌 경로로 바꿔야 한다.

> [!IMPORTANT]
> 코드 사실은 `tdc_drv_iqs323.c`/`.h`를 직접 Read해 확정. 데이터시트 비트 해석·실칩 거동은 "확인 필요"로 명시. 입력: [`00_입력.md`](00_입력.md), [`펌웨어 구현·시퀀스 분석.md`](../펌웨어%20구현·시퀀스%20분석.md), [`생각정리/4_타당성검증.md`](../생각정리/4_타당성검증.md), [`은수님 안 평결·하이브리드 권고.md`](../은수님%20안%20평결·하이브리드%20권고.md).

---

## 1. 현재 3경로의 코드 사실 (출발점)

### 1.1 ATI 설정 — 현재 Disabled (채널별 레지스터)

- **CH0**: `write_ati_compensation()`(drv.c:683) → `0x36`(SENSOR0_ATI_SETUP) = LSB `0x08`·MSB `0x04`. `ATI_SETUP_LSB 0x08`의 bits[2:0]=000 → **ATI Mode=Disabled**(drv.c:644·651). 이후 `0x38`(MULT)·`0x39`(COMP)에 FIXED 값을 직접 write.
- **CH1**: `sensor_setup()` DUMMY 분기(drv.c:500~505) → `0x46`(SENSOR1_ATI_SETUP) = `DUMMY_ATI_SETUP_LSB 0x08`·`MSB 0x04` → **CH1도 ATI Disabled**. 주석(drv.c:500): "CalCap 부하 auto-ATI 수렴 실패로 인한 전역 ATI_ERROR 방지".
- **핵심**: ATI Setup은 **채널마다 별도 레지스터**(CH0=0x36, CH1=0x46). → **CH0 Full + CH1 Disabled는 레지스터 구조상 독립 설정 가능** (Q2 전반부 코드적 답).

### 1.2 CalCap 더미 — CH1 활성 유지 (measurement cycle 보존)

`sensor_setup()` DUMMY 분기(drv.c:479~510):
- `0x44`(SENSOR1_PATTERN_DEF) LSB `PATTERN_CALCAP_SIZE_1PF_LSB`·MSB `PATTERN_DEF_MSB` (CalCap 1pF)
- `0x40`(SENSOR1_SETUP) `enable_channel=1` + `cal_cap_tx=1`·`cal_cap_rx=1` (CalCap을 변환 부하로, 외부 CTx0 끔)
- `0x46` ATI Disabled (위)
- `0x70`(CHANNEL1_SETUP) = `CH_MODE_INDEPENDENT`
- **목적(drv.c:481)**: "CH1 활성 유지 → CH0 discharge 시 measurement cycle 보존". CH0를 끄는 순간(방전) 유일 활성 채널이 사라지면 cycle 정지·RDY 먹통 → 이를 CH1 CalCap이 막는 구조.

### 1.3 수동방전 — 0x30(CH0 Sensor Setup) 토글

`tdc_drv_iqs323_discharge_crx0()`(drv.c:938~952):
```c
write_register_discharge(0x30, INACTIVE_RXS_CRX0_VSS /*0x02*/, 0x00); // CH0 disable + CRX0 VSS
write_register_discharge(0x30, 0x01, 0x01);                          // CH0 복원 enable+ctx0
```
- `INACTIVE_RXS_CRX0_VSS = 0x02`(drv.h:89). 200ms 폴링마다 `get_state()` 말미 호출.
- **verify 없음**(drv.c:941 주석: best-effort, 통신부하 절감). write 성공만 확인, 값 미검증.
- **건드리는 레지스터 = 0x30 자체** = `sensor_setup`이 CH0를 구성하는 바로 그 레지스터(drv.c:528). 즉 방전 = CH0 채널 구성 레지스터를 매 200ms 두 번 재기록.

### 1.4 auto-ATI 완료 대기 — 200ms 폴링 경로엔 없음

- `is_auto_ati_done_single_read()`(drv.c:312): System Status(0x10) `ati_active` 비트 1회 읽기. **init 단계**(`try_finish_init`)에서만 사용.
- `re_ati_trigger()`(drv.c:569)·`wait_re_ati_done()`(drv.c:581): **dump 모드**(`apply_settings` ATI_DUMP 분기)·sleep 측정 경로에서만 호출.
- **운용 200ms 폴링 루프(`get_state`→`discharge_crx0`)에는 ATI 진행 여부를 보는 코드가 일절 없다.** 현재 ATI Disabled라 불필요했기 때문. → Full 도입 시 이 부재가 직접적 공존 위험원.

---

## 2. ATI Disabled → Full 전환 시 충돌·공존 지점 (코드 라인)

### 충돌점 ①: discharge_crx0의 0x30 enable 토글 ↔ auto-Re-ATI (Q1·Q3 코드 핵심)

- 방전 1단계(drv.c:942)가 0x30 `enable_channel=0`으로 CH0를 **끈다**. CH0가 ATI Full이면, 채널을 껐다 켜는(drv.c:947 복원) 행위는 데이터시트상 채널 재진입 시 재캘리브레이션을 유발할 수 있다(**확인 필요** — DS의 채널 enable 토글 시 ATI 재실행 조건은 C1 데이터시트 페르소나 영역). 현 코드는 복원 후 ATI 수렴을 기다리지 않고 즉시 다음 폴링으로 진행 → **200ms마다 미수렴 상태에서 측정·판정**할 위험.
- 더 직접적: 방전 직후 enable 시 카운트 튐 → LTA 교란 → (CH timeout이 켜져 있거나 ATI Band 이탈 시) auto-Re-ATI 재트리거 가능. 단, 현 코드는 `apply_settings`에서 **CH timeout을 disable**(drv.c:1142, 0xC0 MSB `0x07`)해 "auto-reATI 트리거 경로 차단"(drv.c:1139 주석)을 의도. **Full로 가면 이 차단을 풀어야 ATI가 의미 있고**, 풀면 방전 유발 카운트 튐이 곧장 Re-ATI 진동 위험으로 연결(adversarial C3 영역).

### 충돌점 ②: 0x30 LSB 비트 의미 — enable 외 비트 동시 오염 (Q4)

- 코드는 0x30 LSB에 `0x02`(`INACTIVE_RXS_CRX0_VSS`)를 쓰는데, 데이터시트 [06 A.5] Sensor Setup LSB는 **bit3 Invert·bit2 Dual Direction·bit1 Linearise·bit0 Enable**. 데이터시트 정의대로면 `0x02`=**Linearise=1, enable=0**, 복원 `0x01`=enable=1·나머지 0. (펌웨어 분석 §8·B2 — 코드 주석은 이를 INACTIVE_RXS 핀상태로 해석, **실칩 미확정**.)
- 함의: 방전 토글이 매 200ms CH0의 **Linearise/Invert 비트까지 0↔1 토글**한다면(데이터시트 해석 기준), self-cap+Release UI에서 카운트 부호처리에 영향 → auto-ATI가 보는 raw 신호를 흔든다. ATI Disabled인 현재는 LTA만 추적해 무증상이지만 **Full에서는 ATI 입력 자체를 교란**할 수 있다(확인 필요, B2 유형 충돌). **이 비트 해석 불확실성이 Full 공존의 가장 큰 미지수.**

### 충돌점 ③: ATI 수렴 대기 부재 ↔ 200ms 주기 (Q1)

- §1.4대로 폴링 경로에 ATI gate 없음. Full로 바꿔도 `discharge_crx0`는 무조건 200ms마다 enable 토글. ATI 수렴이 200ms 안에 끝난다는 보장이 코드에 없다 → **수렴 중 방전이 끼어들어 수렴을 리셋**하는 무한 비수렴 가능(확인 필요: ATI 수렴 시간 vs 200ms).
- 추가: §1.3 verify 없음 → ATI 진행 중 0x30 write가 RDY 윈도우와 겹치면 I²C 무응답(00 §4: "Full auto-ATI 발동 시 I²C 무응답 사례") 재현 위험. 현 `write_register_discharge`는 통신부하 절감용 경량 경로라 핸드셰이크 보강이 선행돼야.

### 공존 가능점: CH1 Disabled + CH0 Full (Q2 후반부)

- ATI Setup이 채널별 레지스터(0x36 vs 0x46)이므로 **CH1 CalCap을 ATI Disabled로 둔 채 CH0만 0x36에서 ATI Mode=Full로 설정하는 것은 코드상 양립 가능**. `sensor_setup`의 CH1 DUMMY 분기(0x46 Disabled 유지)는 그대로 두고, `write_ati_compensation`을 CH0 0x36에 Full(예: reset값 0x040C 계열) write로 교체하면 된다.
- **단 전제**: 전역 ATI_ERROR(System Status bit6)는 채널 합산이라(분석 §13 이슈9) CH1 CalCap이 Full이면 ATI_ERROR가 SET되지만, CH1을 Disabled로 유지하므로 이 문제는 회피됨. 즉 **CH1 Disabled 유지가 CH0 Full 도입의 필수 전제** — 이건 현 코드가 이미 충족.
- **그러나** CH0 Full을 켜는 순간 §충돌①②③이 모두 CH0에 적용된다. 따라서 "CH1 Disabled+CH0 Full" 양립은 *ATI 설정 레지스터 차원에선 가능*, *discharge_crx0가 CH0 0x30을 토글하는 한 동작 차원에선 충돌*.

---

## 3. 종합 — 공존 판정과 코드 변경 최소 조건

| 질문 | 코드 레벨 답 |
|---|---|
| Q1 (방전 토글이 ATI 수렴 교란) | **교란 가능** — 폴링 경로에 ATI gate 부재(drv.c §1.4), 200ms마다 0x30 enable 토글 무조건 실행 |
| Q2 (CH1 Disabled+CH0 Full 양립) | **레지스터 분리 가능**(0x36 vs 0x46), 단 discharge 0x30 토글과 CH0 Full은 충돌 |
| Q3 (방전후 카운트튐 Re-ATI 재트리거) | **가능** — Full 쓰려면 CH timeout disable(drv.c:1142)을 풀어야 하고 풀면 진동 위험 부상 |
| Q4 (0x30 LSB 비트 충돌) | **확인 필요** — 데이터시트 해석상 Linearise/Invert 동시 토글 가능(B2), ATI 입력 교란 우려 |
| Q5 (방전≠ATI 레이어) | 코드도 분리 — ATI=0x36/0x38/0x39+LTA, 방전=0x30 물리 토글. 대체 관계 아님 (C5 영역) |

**코드상 단순 공존(discharge 현 형태 + CH0 Full)은 불가에 가깝다.** 안전한 CH0 Full 도입을 위한 최소 코드 변경:
1. **ATI gate 추가**: 폴링 루프에서 `is_auto_ati_done_single_read()` 확인 → ATI 진행 중이면 `discharge_crx0` 보류(Q6-① 시간분리). 현 코드엔 이 분기 없음.
2. **0x30 토글 경로 재검토**: enable 비트 토글 대신 Inactive Rxs(Pattern Def 0x34) 등 ATI를 안 깨우는 경로로 방전(Q6-⑥, 확인 필요).
3. **I²C 통신관리 선행**: `write_register_discharge` 경량 경로에 RDY 핸드셰이크 보강(하이브리드 권고 V3 "I²C 통신관리").
4. **CH timeout/Re-ATI 정책 결정**: Full + timeout disable 유지(auto-Re-ATI 억제, Q6-③) 조합이 코드 변경 최소.

---

## 4. 권장 대안 (코드 관점)

**Q6-③ + Q6-①의 조합**을 권장: ATI Mode를 부팅 1회 Full 수렴(init `try_finish_init` 경로 재활용) 후 **운용 중에는 auto-Re-ATI 억제(CH timeout disable 유지, drv.c:1142)** + 방전은 ATI 비활성 구간으로 한정. 이러면 ① 현 코드 구조(init 단계 ATI 대기 존재) 재활용 가능 ② 200ms 폴링 경로에 ATI gate를 새로 안 넣어도 됨 ③ 방전 0x30 토글이 진행 중 ATI를 깰 일이 없음. 단 0x30 LSB 비트 의미(충돌②)와 I²C 무응답은 **실측 선행 필수**. ESD 물리누적 여부 미확정이므로 방전 자체는 유지(하이브리드 권고 §3: "ESD 물리누적 실측 후 폐기 검토").
