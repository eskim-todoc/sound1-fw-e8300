# 노드 02 — AGC 처리 분석가

## 핵심 발견

### cfx_pwr_to_dB / cfx_dB_to_pwr Q 포맷
- `cfx_pwr_to_dB()` : 입력 Q1.47 (frac48), 출력 Q8.16
  - 호출 전 `NORMALIZE_SHIFT_BIT_NUM_FOR_10DB_LIBRARY = 5` 비트 좌측 시프트 필요
  - 입력값 최소 23 이상이어야 정상 계산
- `cfx_dB_to_pwr()` : 입력 Q8.16, 출력 Q1.47 (frac48)
  - 출력에 `>> 24` 적용 → **Q1.23** 변환 확인됨 (agc.c L285)

### AGC gain 최종 반환 포맷
- Q1.23 × Q12.12 = Q13.35 → `>> 23` → **Q12.12** (24비트 int) 반환 (agc.c L289-304)

### 오디오 × gain 연산 (apply_agc_gain)
- 입력: agc_gain (Q12.12) × 오디오 (Q24.0) = Q36.12
- `>> 12` → Q36.0 → 하위 24비트 = **Q24.0** 출력
- 클리핑: INT24_MAX = +8,388,607 / INT24_MIN = -8,388,608 (definitionsForAlgorithm.h L39-40)
- 클리핑은 agc.c 내 4군데에서 각 단계마다 적용

### vAbsolute / MaxElement
- HEAR 마이크로코드가 오디오 믹스 절대값 최대값을 HW 주소 `HEAR_ADDR_AUDIO_MIX_ABS_MAX_VALUE`에 자동 기록
- CFX가 해당 주소를 직접 읽어 AGC 판단 기준으로 사용 (Q24.0 정수)

## 요약 테이블

| 단계 | 포맷 | 비트 |
|---|---|---|
| cfx_pwr_to_dB 입력 | Q1.47 | 48 |
| cfx_pwr_to_dB 출력 | Q8.16 | 24 |
| cfx_dB_to_pwr 출력 (>>24 후) | Q1.23 | 24 |
| AGC gain 최종 | Q12.12 | 24 |
| 오디오 입력 | Q24.0 | 24 |
| AGC 적용 후 오디오 출력 | Q24.0 | 24 |
