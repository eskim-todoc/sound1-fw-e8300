---
name: 코드 포맷 규칙 (clang-format + 수동 규칙)
purpose: 은수님이 정한 코드 포맷 조건 11개를 clang-format 자동 적용분과 작성자 책임분으로 갈라 정리
type: reference/규칙
applies_to: [Sound1]
tags: [clang-format, 코드포맷, 스타일, 컨벤션, 작성규칙]
---

# 코드 포맷 규칙

**TL;DR**: 은수님이 [`조건.md`](조건.md) 에 적은 **11개 조건 중 clang-format 이 자동으로 처리하는 것은 5개**이고, 2개는 부분, **4개는 작성자가 지켜야 한다.** 특히 `case` 블록 사이 공백 · 제어문 앞 공백줄 · 주석 형태는 도구가 만들어 주지 않는다. 본보기는 [`example.c`](example.c) = **`src/2__cm3/source/main.c` 와 바이트 동일**하다. **2026-08-06 에 `src/2__cm3/source/` 전체(149파일)에 일괄 적용을 마쳤다.**

## 1. 본보기

| 파일 | 정체 |
|---|---|
| [`example.c`](example.c) | **`src/2__cm3/source/main.c` 와 바이트 동일**. 은수님이 «이렇게 써라» 는 기준으로 두신 것 |
| [`조건.md`](조건.md) | 조건 11개 원문 |
| `src/.clang-format` | 도구 설정 |

## 2. 조건 11개 - 자동 / 수동 구분 (clang-format 22 기준)

> [!IMPORTANT]
> **clang-format 을 돌렸다고 조건이 다 지켜지지는 않는다.** 아래 «수동» 4개와 «부분» 2개는 처음 쓸 때 맞춰야 한다.

| # | 조건 | 처리 | 근거 설정 |
|---|---|---|---|
| 1 | 변수 여러 줄 나열 시 자료형·변수명 줄 맞춤 | **자동** | `AlignConsecutiveDeclarations: true` |
| 2 | `if`·`for`·`while`·`switch`·`case`·`return` 등 제어문은 **모두 `{}`** | **자동** | **`InsertBraces: true`** (v15+) |
| 3 | `case` 라벨은 `{}` 블록 안에, **`break` 와 다음 `case` 사이 공백 줄** | **수동** | 해당 옵션 없음 |
| 4 | `{}` 의 시작·끝은 항상 새 줄 | **자동** | `BreakBeforeBraces: Allman` |
| 5 | `{` 시작에 의미 없는 줄바꿈 금지 | **자동** | **`KeepEmptyLines: {AtStartOfBlock: false}}`** (§4) |
| 6 | 함수 호출 인자가 길면 각 인자 새 줄 + 시작 위치 정렬 | **자동** | `BinPackArguments: false` + **`ColumnLimit: 160`** (§5) |
| 7 | 주석 1줄 `//`, 여러 줄 `/* */`, 시작 위치를 코드와 맞춤 | **부분** | 위치는 `AlignTrailingComments`. **형태 선택은 수동** |
| 8 | `/* */` 안에 `/* */` 중첩 금지 (이 경우 `//` 사용) | **수동** | C 문법상 중첩 불가 |
| 9 | 배열 초기화가 길면 각 요소 새 줄 + 시작 위치 정렬 | **부분** | 나눔은 `ColumnLimit`. **내부 정렬은 안 한다** (§6) |
| 10 | 제어문 **바로 위에 코드**가 있으면 사이에 공백 줄 | **수동** | 해당 옵션 없음 |
| 11 | 제어문 **바로 위에 주석**이 있으면 사이에 공백 줄 | **수동** | 해당 옵션 없음 |

**자동 5 (1·2·4·5·6) · 부분 2 (7·9) · 수동 4 (3·8·10·11).**

## 3. 작성 시 체크 (수동 4개)

```c
// 조건_11 : 제어문 위 주석과 제어문 사이에 공백 줄

// 조건_10 : 제어문 위 코드와 제어문 사이에도 공백 줄
if (condition)
{
    do_something();
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

## 4. 조건_5 - 옛 옵션 이름은 v22 에서 조용히 무시된다

> [!CAUTION]
> **`KeepEmptyLinesAtTheStartOfBlocks` 는 clang-format 22 에 없는 이름이다.** 써도 에러가 나지 않고 **그냥 무시된다.**

v19 부터 아래로 옮겨졌다.

```yaml
KeepEmptyLines:
  AtEndOfFile:     false
  AtStartOfBlock:  false   # <- 조건_5
  AtStartOfFile:   true
```

**이 문서의 이전 판이 옛 이름을 처방하고 있었다.** 그대로 넣었어도 조건_5 는 안 지켜졌을 것이다. 옵션을 추가할 때는 `--dump-config` 로 **그 키가 실제로 존재하는지 먼저 확인한다.**

## 5. 조건_6 - `ColumnLimit` 이 유한해야 작동한다

**`BinPackArguments: false` 는 «한 줄에 안 들어갈 때» 만 발동한다.** 예전 설정의 `ColumnLimit: 320` 은 사실상 «줄을 절대 나누지 않는다» 여서, 조건_6 이 나누라고 하는 호출을 오히려 **한 줄로 도로 붙이고 있었다.**

`ColumnLimit: 0`(무제한) 도 마찬가지다. 실측으로 확인했다 — 손으로 2줄로 나눠 둔 매크로 호출을 똑같이 한 줄로 합쳤다.

**폭 160 의 근거**는 본보기다.

| 파일 | 최대 | >120 | >140 | >160 |
|---|---|---|---|---|
| `main.c` (본보기) | 181 | 27 | 7 | **1** |

본보기가 160 을 넘는 줄은 **1개**뿐이었고, 그것은 인자 4개짜리 `TDC_PRINTF_D` 호출로 **조건_6 이 나누라고 하는 바로 그 형태**였다. 은수님이 폭 160 을 선택했다(2026-08-06).

## 6. 조건_9 - 내부 정렬(`AlignArrayOfStructures`)은 넣지 않는다

넣으면 본보기와 어긋난다.

```c
// AlignArrayOfStructures: Left 를 켰을 때
{A_LONG_NAME,        B_NAME},
{A_MUCH_LONGER_NAME, BB    },   // 닫는 중괄호 앞을 패딩한다
```

본보기 `main.c` 의 `s_tdc_batt_led_table[]` 은 내부를 정렬하지 않는다. **실물이 기준이다.**

## 7. 도구가 못 지키는 것 - 후행 `/* */` 주석 앞 공백

`SpacesBeforeTrailingComments: 2` 를 걸어도 **블록 주석에는 안 걸린다.**

```c
return 0;  // 줄 주석      -> 2칸 (설정대로)
return 0; /* 블록 주석 */  -> 1칸 (설정 무시)
```

**설정으로 고칠 수 없다.** 조건 문서가 칸 수를 규정하지 않으므로 조건 위반은 아니다.

## 8. 실행 방법

> [!WARNING]
> **Eclipse 의 clang-format 플러그인으로 돌리지 말 것.** stdin/stdout 인코딩을 명시하지 않아 CP949 왕복이 일어나고 `—`·`≈`·`µ`·`►` 가 영구 손실된다([`CLAUDE.md` §소스 인코딩 규칙](../../../CLAUDE.md)). **CLI 로 돌리면 그 경로를 타지 않는다.**

```sh
CF="/c/Program Files/LLVM/bin/clang-format.exe"
SRC=src/2__cm3/source

# 어긋난 파일 확인 (고치지 않는다)
find "$SRC" -path "$SRC/lib" -prune -o \( -name "*.c" -o -name "*.h" \) -print \
  | xargs -I{} "$CF" --dry-run -Werror {}

# 일괄 적용
find "$SRC" -path "$SRC/lib" -prune -o \( -name "*.c" -o -name "*.h" \) -print \
  | xargs -I{} "$CF" -i {}
```

`lib/SEGGER_RTT` · `lib/tiny-AES-c` 는 **벤더 코드라 제외**한다.

## 9. 새 코드에 적용하는 범위

| 상황 | 적용 |
|---|---|
| **새로 쓰는 함수·파일** | 조건 11개 **전부** |
| 기존 함수에 몇 줄 추가 | 조건을 지킨다. 파일이 이미 포맷돼 있다 |
| **`isd/` · `ble/` 등 기존 코드** | **2026-08-06 일괄 적용 완료** ([작업 기록](../../tasks/cm3/20260806_1450_clang-format-일괄적용/)) |
| `0__bootloader` · `1__cfx` · `3__hear` · `5__calibration` | **미적용.** 여기서 빌드 검증이 되지 않거나 도구 생성 코드다 |

## 10. 일괄 적용을 다시 할 때 알아야 할 것

| 항목 | 내용 |
|---|---|
| **`__LINE__` 이 밀린다** | 줄이 밀리면 `TDC_PRINTF_E` 와 명시 `__LINE__` 이 싣는 즉치가 바뀌어 `.text` 가 달라진다. **실기 대기가 쌓인 시점에는 피한다** |
| **검증 자** | 구조 지표·줄 수는 못 쓴다(`InsertBraces` 가 `{}` 를 늘린다). **함수별 심볼 크기 표**를 쓴다 |
| **개행** | 도구가 파일을 통째로 다시 쓰면 CRLF 가 섞일 수 있다. `LineEnding: LF` 로 고정했고, `git status` 는 정규화 때문에 이것을 **안 보여준다** |
| **`example.c`** | 포맷 후 `main.c` 를 다시 복사해 바이트 동일을 유지한다 |
