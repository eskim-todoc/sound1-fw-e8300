# LED 패턴 디버깅용 UI 명령 구현계획

작성자: 김은수
작성일: 2026-04-23
상태: 리뷰 대기
관련:
- [`[요구사항] LED pattern UI 명령 Rev.0`]([요구사항]%20LED%20pattern%20UI%20명령%20Rev.0%20by%20김은수.md)
- [`[현상분석] LED pattern UI 명령 Rev.0`]([현상분석]%20LED%20pattern%20UI%20명령%20Rev.0%20by%20김은수.md)

---

## 1. 결정

| # | 결정 |
|---|---|
| Q1 | ERROR (#7) → LED_ST_ERROR_MCU 대표 |
| Q2 | N=0 → 모든 src NONE + override 유지 (해제는 `--led clr` 사용) |
| Q3 | 잘못된 N → error msg + return -1 |
| Q4 | help 한 줄 갱신 |

---

## 2. 코드 변경 — `tdc_ui_command.c` 만 수정

### 2.1 패턴 매핑 테이블 (file-local const)

`handle_led()` 위에 추가:

```c
/* 사진 "패턴 (디버깅)" 칼럼 N (0~14) → (src, state) 매핑.
 * N=0 은 sentinel — 모든 src NONE 만 적용. */
static const struct {
    led_src_t   src;
    led_state_t st;
    const char *desc;
} k_tdc_pattern_table[] = {
    /*  0 */ { LED_SRC__MAX,   LED_ST_NONE,                       "all off"                  },
    /*  1 */ { LED_SRC_POWER,  LED_ST_POWER_ON,                   "POWER_ON"                 },
    /*  2 */ { LED_SRC_POWER,  LED_ST_POWER_OFF,                  "POWER_OFF"                },
    /*  3 */ { LED_SRC_MAPPING,LED_ST_MAPPING_ISD_BATT_READY,     "MAPPING_ISD_BATT_READY"   },
    /*  4 */ { LED_SRC_MAPPING,LED_ST_MAPPING_NO_ISD_BATT_READY,  "MAPPING_NO_ISD_BATT_READY"},
    /*  5 */ { LED_SRC_MAPPING,LED_ST_MAPPING_ISD_BATT_LOW,       "MAPPING_ISD_BATT_LOW"     },
    /*  6 */ { LED_SRC_MAPPING,LED_ST_MAPPING_NO_ISD_BATT_LOW,    "MAPPING_NO_ISD_BATT_LOW"  },
    /*  7 */ { LED_SRC_ERROR,  LED_ST_ERROR_MCU,                  "ERROR_MCU (대표)"         },
    /*  8 */ { LED_SRC_BATTERY,LED_ST_BATT_CRITICAL,              "BATT_CRITICAL"            },
    /*  9 */ { LED_SRC_BATTERY,LED_ST_BATT_MID,                   "BATT_MID"                 },
    /* 10 */ { LED_SRC_BATTERY,LED_ST_BATT_READY,                 "BATT_READY"               },
    /* 11 */ { LED_SRC_ISD,    LED_ST_IN_USE,                     "IN_USE"                   },
    /* 12 */ { LED_SRC_BLE_IND,LED_ST_PAIR,                       "PAIR"                     },
    /* 13 */ { LED_SRC_BLE_IND,LED_ST_OTA_QCC,                    "OTA_QCC"                  },
    /* 14 */ { LED_SRC_BLE_IND,LED_ST_OTA_EZAIRO,                 "OTA_EZAIRO"               },
};
#define TDC_PATTERN_TABLE_LEN  (int)(sizeof(k_tdc_pattern_table) / sizeof(k_tdc_pattern_table[0]))
```

### 2.2 `handle_led()` 의 새 서브

기존 `--led pair` 분기 다음에 추가:

```c
/* --led pattern <0~14>  — 사진 디버깅 칼럼 매핑 */
if (ci_strcasecmp(argv[1], "pattern") == 0)
{
    if (argc < 3) return -1;

    int n = atoi(argv[2]);
    if (n < 0 || n >= TDC_PATTERN_TABLE_LEN)
    {
        output_printf("invalid pattern (0~%d)\r\n", TDC_PATTERN_TABLE_LEN - 1);
        return -1;
    }

    /* 모든 src 강제 NONE + override 활성 — 단일 패턴만 보이도록 */
    for (int src = 0; src < LED_SRC__MAX; src++)
    {
        s_tdc_led_override[src] = true;
        led_request((led_src_t) src, LED_ST_NONE);
    }

    /* N>0 인 경우 해당 패턴 요청 */
    if (n > 0)
    {
        led_request(k_tdc_pattern_table[n].src, k_tdc_pattern_table[n].st);
    }

    output_printf("OK: pattern %d — %s\r\n", n, k_tdc_pattern_table[n].desc);
    return 0;
}
```

### 2.3 help 문자열 갱신

```c
{"led", handle_led, "--led show|req|clr|user|pair|burst|pattern"},
```

---

## 3. 단계 / 커밋

단일 커밋:

> Add : --led pattern N (0~14) UI 명령 — 사진 디버깅 칼럼 매핑

영향 파일: `tdc_ui_command.c` 만.

문서 3 건 별도 커밋:

> Docs : --led pattern N UI 명령 요구·분석·구현계획

---

## 4. 검증

### 4.1 정적

- 빌드 성공 (Eclipse, ENABLE_UI_CMD 정의 시)
- `Grep "k_tdc_pattern_table"` 정의 1 회

### 4.2 실기

요구사항 §2.4 + 현상분석 §6.2 시나리오 확인.

---

## 5. 위험 / 잔여

- (R1) 패턴 진입 후 모든 src override 활성 — 디버깅 끝나면 `--led clr <src>` 또는 `--led clr` 일괄 해제 명령이 필요할 수 있음. 현재 `--led clr` 는 src 1 개만 해제. 일괄 해제 명령 신설은 본 작업 범위 외 (후속).
- (R2) 사진 매핑이 변경되면 테이블 갱신 필요. 사진과 디스크립터 (`k_led_patterns[]`) 가 따로 정의되므로 동기화 책임은 사용자.
- (R3) ERROR 5 종 중 MCU 대표만 표시. 다른 ERROR 패턴은 동일 빨강 패턴이라 시각 구분 무의미.

---

## 6. 작업 순서

1. 본 문서 + 요구사항/현상분석 작성 — 완료
2. `tdc_ui_command.c` 수정 (테이블 + 핸들러 + help)
3. 단일 커밋 (코드)
4. 사용자 확인 후 `claude_develop` 병합
