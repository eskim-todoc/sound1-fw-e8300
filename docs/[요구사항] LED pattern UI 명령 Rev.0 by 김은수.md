# LED 패턴 디버깅용 UI 명령 (`--led pattern N`) 요구사항

작성자: 김은수
작성일: 2026-04-23
상태: 리뷰 대기
근거: 사용자 지시 — 스펙 사진의 패턴 번호 (0~14) 매칭

---

## 1. 배경

LED 패턴 14 종 (사진 표) 을 일일이 확인하려면 매핑/배터리/충전기 상태를 만들어야 하는 번거로움이 있다. 디버깅 UI 명령으로 패턴 번호 (0~14) 를 입력하면 해당 LED 패턴이 즉시 표시되도록 한다.

---

## 2. 요구사항

### 2.1 명령 형식

```
--led pattern <N>
```

`N`: 0 ~ 14 정수

### 2.2 패턴 번호 매핑 (사진 디버깅 칼럼)

| N | 표시 LED 상태 | 색·패턴 |
|--:|---|---|
| 0 | (LED all off) | OFF |
| 1 | LED_ST_POWER_ON | 하늘색 ON 80 / OFF 220 × 5 |
| 2 | LED_ST_POWER_OFF | 파랑  ON 100 / OFF 200 × 4 |
| 3 | LED_ST_MAPPING_ISD_BATT_READY | 파랑  ON 200 / OFF 800 |
| 4 | LED_ST_MAPPING_NO_ISD_BATT_READY | 파랑  지속 ON |
| 5 | LED_ST_MAPPING_ISD_BATT_LOW | 보라  ON 100 / OFF 900 |
| 6 | LED_ST_MAPPING_NO_ISD_BATT_LOW | 보라  지속 ON |
| 7 | LED_ST_ERROR_MCU (대표) | 빨강  ON 180 / OFF 180 |
| 8 | LED_ST_BATT_CRITICAL | 노랑  ON 1100 / OFF 1100 |
| 9 | LED_ST_BATT_MID | 노랑  지속 ON |
| 10 | LED_ST_BATT_READY | 녹색  지속 ON |
| 11 | LED_ST_IN_USE | 흰색  지속 ON |
| 12 | LED_ST_PAIR | 파랑  ON 500 / OFF 500 |
| 13 | LED_ST_OTA_QCC | 녹색  ON 1100 / OFF 1100 |
| 14 | LED_ST_OTA_EZAIRO | 녹색  ON 180 / OFF 180 |

### 2.3 동작

- 모든 LED src (POWER / ERROR / BLE_IND / MAPPING / BATTERY / ISD) 를 강제 NONE 으로 만든 후, 해당 패턴의 src 에만 state 요청 → Arbiter 가 다른 src 영향 없이 단일 패턴만 표시
- override 플래그 (`s_tdc_led_override[]`) 활성화 — 다음 main loop iteration 에서 자동 src 가 덮어쓰지 않도록
- N=0 은 모든 src NONE 만 → IDLE (LED OFF)
- 잘못된 N (음수, > 14, 비숫자) 는 에러 메시지 + exit code -1

### 2.4 수용 기준

- [ ] `--led pattern 7` 입력 시 빨강 점멸 표시
- [ ] `--led pattern 0` 입력 시 LED 모두 꺼짐
- [ ] 다른 src 의 자동 요청이 와도 override 활성으로 패턴 유지
- [ ] `--led clr error` 등으로 override 해제 가능
- [ ] `--help` 출력에 `--led pattern` 안내 포함

---

## 3. 비목표

- 패턴 시간 / 색 직접 변경 (별도 패턴 디스크립터 수정 필요)
- ERROR 5 종 개별 선택 (대표 MCU 만 — 나머지는 같은 빨강 패턴이라 시각 구분 무의미)
- 패턴 번호 자동 발견 (사진 매핑 고정)

---

## 4. 제약

- 기존 `--led req` / `--led clr` / `--led pair` / `--led burst` 명령과 공존
- override 활성 시 자동 src 무시 — 디버깅 끝나면 `--led clr <src>` 로 해제 필요

---

## 5. 참고

- 스펙 사진: `docs/led_ind_state.JPG` 의 "패턴 (디버깅)" 칼럼
