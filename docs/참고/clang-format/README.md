---
name: 코드 포맷 규칙 (clang-format + 수동 규칙)
purpose: 은수님이 정한 코드 포맷 조건 11개를 clang-format 자동 적용분과 작성자 책임분으로 갈라 정리
type: reference/규칙
applies_to: [Sound1]
tags: [clang-format, 코드포맷, 스타일, 컨벤션, 작성규칙]
---

# 코드 포맷 규칙

**TL;DR**: 은수님이 [`조건.md`](조건.md) 에 적은 **11개 조건 중 clang-format 이 자동으로 처리하는 것은 4개뿐**이고 **7개는 작성자가 지켜야 한다.** 특히 `{}` 강제 · `case` 블록 사이 공백 · 제어문 앞 공백줄은 도구가 만들어 주지 않는다. 본보기는 [`example.c`](example.c) = **`src/2__cm3/source/main.c` 와 바이트 동일**하다. 새 코드는 그 파일처럼 쓴다.

## 1. 본보기

| 파일 | 정체 |
|---|---|
| [`example.c`](example.c) | **`src/2__cm3/source/main.c` 와 바이트 동일**(1,121줄, `diff` 0). 은수님이 «이렇게 써라» 는 기준으로 두신 것 |
| [`조건.md`](조건.md) | 조건 11개 원문 |
| `src/.clang-format` | 도구 설정 |

## 2. 조건 11개 - 자동 / 수동 구분

> [!IMPORTANT]
> **clang-format 을 돌렸다고 조건이 다 지켜지지는 않는다.** 아래 «수동» 7개는 처음 쓸 때 맞춰야 한다.

| # | 조건 | 처리 | 근거 설정 |
|---|---|---|---|
| 1 | 변수 여러 줄 나열 시 자료형·변수명 줄 맞춤 | **자동** | `AlignConsecutiveDeclarations: true` |
| 2 | `if`·`for`·`while`·`switch`·`case`·`return` 등 제어문은 **모두 `{}`** | **수동** | clang-format 이 중괄호를 만들지 않는다 |
| 3 | `case` 라벨은 `{}` 블록 안에, **`break` 와 다음 `case` 사이 공백 줄** | **수동** | 해당 옵션 없음 |
| 4 | `{}` 의 시작·끝은 항상 새 줄 | **자동** | `BreakBeforeBraces: Allman` |
| 5 | `{` 시작에 의미 없는 줄바꿈 금지 | **미설정** | `KeepEmptyLinesAtTheStartOfBlocks` 가 `.clang-format` 에 **없다** (§4) |
| 6 | 함수 호출 인자가 길면 각 인자 새 줄 + 시작 위치 정렬 | **자동** | `BinPackArguments: false` · `AlignAfterOpenBracket: Align` |
| 7 | 주석 1줄 `//`, 여러 줄 `/* */`, 시작 위치를 코드와 맞춤 | **수동** | 형태 선택은 도구가 못 한다 |
| 8 | `/* */` 안에 `/* */` 중첩 금지 (이 경우 `//` 사용) | **수동** | C 문법상 중첩 불가 |
| 9 | 배열 초기화가 길면 각 요소 새 줄 + 시작 위치 정렬 | **부분** | `ColumnLimit: 320` 이라 웬만해선 안 걸린다 → 실질 **수동** |
| 10 | 제어문 **바로 위에 코드**가 있으면 사이에 공백 줄 | **수동** | 해당 옵션 없음 |
| 11 | 제어문 **바로 위에 주석**이 있으면 사이에 공백 줄 | **수동** | 해당 옵션 없음 |

**자동 4 (1·4·6 + 부분적으로 9) · 수동 7.**

## 3. 작성 시 체크 (수동 7개)

```c
// 조건_11 : 제어문 위 주석과 제어문 사이에 공백 줄
// 조건_10 : 제어문 위 코드와 제어문 사이에도 공백 줄

if (condition)          // 조건_2 : 한 줄짜리라도 반드시 {}
{
    do_something();     // 조건_5 : { 바로 다음에 빈 줄을 두지 않는다
}

switch (value)
{
    case A:             // 조건_3 : case 는 {} 블록 안
    {
        handle_a();
        break;
    }
                        // 조건_3 : break 와 다음 case 사이에 공백 줄
    case B:
    {
        handle_b();
        break;
    }
}
```

## 4. 미설정 항목 - `KeepEmptyLinesAtTheStartOfBlocks`

> [!WARNING]
> **조건_5 를 강제하는 설정이 `.clang-format` 에 없다.** LLVM 기본값이 `true`(빈 줄 유지)라, 지금은 도구가 조건_5 를 지켜 주지 않는다.

실측으로 확인된다.

| 파일 | `{` 다음 줄이 빈 줄인 경우 |
|---|---|
| `example.c` (= `main.c`) | **0건** |
| `isd/tdc_isd_map_ecap.c` | **44건** |
| `isd/tdc_isd_map_data.c` | 29건 |
| `ble/remote/tdc_ble_remote_sp_para.c` | 15건 |
| `isd/tdc_isd_map_live.c` | 9건 |
| `ble/remote/tdc_ble_remote.c` | 8건 |
| 그 외 6파일 | 각 1~3건 |

**총 113건** (벤더 코드 `SEGGER_RTT` 제외, `lib/tiny-AES-c` 1건 포함).

### 왜 지금 고치지 않는가

`KeepEmptyLinesAtTheStartOfBlocks: false` 를 넣고 전체 포맷을 돌리면 **113곳에서 줄이 밀린다.** 그러면 `util/tdc_printf.h:36` 이 모든 `TDC_PRINTF_*` 에 심는 `__LINE__` 즉치가 대량으로 바뀌어 **`.text` 가 크게 달라진다**(세션 인수인계 자산_1). 실기 미검증 병합이 5건 쌓인 지금은 위험 대비 이득이 없다.

**착수 조건**: 실기를 한 번 몰아서 확인한 뒤. 그때는 «순수 포맷» 이므로 불변량을 **동작 무변경 + 심볼 크기 동일**로 잡고, `.text` 차이가 `__LINE__` 즉치뿐임을 `cmp -l` 로 입증한다.

## 5. 새 코드에 적용하는 범위

| 상황 | 적용 |
|---|---|
| **새로 쓰는 함수·파일** | 조건 11개 **전부** |
| 기존 함수에 몇 줄 추가 | 주변 스타일을 따르되, **새로 쓰는 줄은 조건을 지킨다** |
| 기존 코드 일괄 정리 | **하지 않는다** — §4 참조. 별도 작업으로 승인 후 |

> [!NOTE]
> **[소스 인코딩 규칙](../../../CLAUDE.md)과 함께 지킨다.** `.c`/`.h` 주석에 `—`(em dash) · `≈` · `µ` · `►` 를 쓰면 Eclipse clang-format 플러그인이 CP949 왕복에서 영구 손실시킨다. 한글 · `·` · `→` · `×` 는 안전하다.
