#!/usr/bin/env bash
#
# 구조 지표 측정 - 리팩토링 전후의 "조건 분기 총수 보존" 검증용.
#
#   bash tests/metrics.sh <파일...>                     지표 출력
#   bash tests/metrics.sh --save <이름> <파일...>        기준선 저장
#   bash tests/metrics.sh --diff <이름> <파일...>        기준선과 대조
#   bash tests/metrics.sh --func <함수명> <파일>          함수 범위만 측정
#
# 왜 스크립트인가
#   손으로 grep 을 칠 때마다 패턴이 조금씩 달라져 거짓 판정이 났다.
#   2026-07-30 이월_D 에서 `grep -o "case "` 가 주석 속 "case" 를 세어
#   19 -> 21 거짓 FAIL 을 냈고, 기준선 19 자체가 실제 레이블 17 + 주석
#   언급 2 였다. 2026-08-05 이월_2 에서는 `grep -c "tdc_ble_reply"` 가
#   #include 줄까지 세어 50 이 나왔다(실제 호출 49).
#
#   지표는 "같은 방식으로 두 번 재는 것" 이 전부다. 그 방식을 코드로
#   고정한다.
#
# 측정 규칙 (이월_I)
#   구조 지표(case·switch·if·else·for)  행 앵커 `^[[:space:]]*`
#     - 주석 안의 단어를 세지 않는다
#     - `} else {` 같은 형태는 이 프로젝트에 없다(clang-format 이 분리)
#   연산자(||·&&)                        주석 «행» 제외 후 개수
#     - 행말 주석에 연산자를 쓰지 않는 관례를 전제로 한다
#   호출(reply 등)                       `이름[a-z0-9_]*(` 로 여는 괄호까지
#     - #include 줄과 주석 언급을 배제한다
#
# 한계 (알고 쓸 것)
#   - 행말 주석에 `||` 를 쓰면 연산자 계수가 오염된다. 쓰지 말 것.
#   - 여러 줄에 걸친 조건식은 `if (` 가 한 번만 나오므로 정상 계수된다.
#   - 매크로 안에 숨은 분기는 세지 못한다. 이 프로젝트는 분기 매크로를
#     쓰지 않으므로 현재는 문제가 없다.
#   - 지표 불변은 필요조건이지 충분조건이 아니다. 분기를 서로 맞바꾸면
#     총수는 같다. 테스트와 실기가 나머지를 덮는다.
#
# 함께 알아야 할 것 - `.elf` 바이트 비교의 함정 (2026-08-06 실측)
#   이 프로젝트에서 **줄이 밀리면 `.text` 가 거의 항상 바뀐다.**
#
#     util/tdc_printf.h:36
#       tdc_printf_file_func_line(__SHORT_FILE__, __func__, __LINE__);
#
#   즉 **모든 TDC_PRINTF_* 호출이 __LINE__ 을 갖는다.** 소스에서
#   `grep __LINE__` 을 해도 보이지 않는다. 주석 몇 줄만 넣어도 그 뒤의
#   TDC_PRINTF 개수만큼 즉치가 바뀐다(실측 - 주석 6줄 추가에 .text 11바이트).
#
#   또 main.c:115 의 __DATE__ 가 빌드 날짜를 문자열로 박으므로,
#   **날짜 경계를 넘겨 빌드하면 그것만으로 1바이트가 달라진다**(실측).
#
#   따라서
#     - "바이트 동일" 을 주장하려면 `cmp -l` 로 전수 비교할 것.
#       디스어셈블리에서 특정 즉치 하나를 찾은 것으로는 부족하다.
#     - 줄 수를 보존하는 편집(선언 이동 등)은 이 문제를 피한다.
#     - 소량 바이트가 어긋나면 __DATE__ 를 먼저 의심할 것.

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="$HERE/.metrics"

usage() {
    sed -n '3,20p' "$0" | sed 's/^# \{0,1\}//'
    exit 2
}

# 한 파일(또는 표준입력 텍스트)의 지표를 "이름=값" 으로 출력한다.
measure() {
    local text="$1"

    local code
    code="$(printf '%s\n' "$text" | grep -vE '^[[:space:]]*(//|/\*|\*)')"

    printf 'case=%s\n'    "$(printf '%s\n' "$text" | grep -cE '^[[:space:]]*case ')"
    printf 'switch=%s\n'  "$(printf '%s\n' "$text" | grep -cE '^[[:space:]]*switch')"
    printf 'default=%s\n' "$(printf '%s\n' "$text" | grep -cE '^[[:space:]]*default[[:space:]]*:')"
    printf 'if=%s\n'      "$(printf '%s\n' "$text" | grep -cE '^[[:space:]]*if \(')"
    printf 'else=%s\n'    "$(printf '%s\n' "$text" | grep -cE '^[[:space:]]*else')"
    printf 'for=%s\n'     "$(printf '%s\n' "$text" | grep -cE '^[[:space:]]*for \(')"
    printf 'while=%s\n'   "$(printf '%s\n' "$text" | grep -cE '^[[:space:]]*while \(')"
    printf 'return=%s\n'  "$(printf '%s\n' "$text" | grep -cE '^[[:space:]]*return')"
    printf 'or=%s\n'      "$(printf '%s\n' "$code" | grep -oE '\|\|' | wc -l | tr -d ' ')"
    printf 'and=%s\n'     "$(printf '%s\n' "$code" | grep -oE '&&' | wc -l | tr -d ' ')"
    printf 'lines=%s\n'   "$(printf '%s\n' "$text" | wc -l | tr -d ' ')"
}

# 함수 하나의 본문을 뽑는다. "여는 중괄호가 다음 줄" 규약을 쓴다.
# (시그니처 줄에 행말 주석이 붙어 있어도 잡힌다 - 2026-08-05 이월_2 에서
#  step() 을 놓쳤던 원인이 정규식이 그 형태를 못 잡은 것이었다.)
extract_func() {
    local name="$1" file="$2"
    awk -v fn="$name" '
        $0 ~ ("(^|[^a-zA-Z0-9_])" fn "[[:space:]]*\\(") && $0 !~ /;[[:space:]]*$/ { cand = NR; sig = $0 }
        /^\{/ { if (cand == NR - 1) { inside = 1; depth = 0 } }
        inside {
            print
            n = gsub(/\{/, "{"); m = gsub(/\}/, "}")
            depth += n - m
            if (depth == 0 && NR > cand + 1) exit
        }
    ' "$file"
}

MODE="show"
NAME=""
FUNC=""

while [ $# -gt 0 ]; do
    case "$1" in
        --save) MODE="save"; NAME="$2"; shift 2 ;;
        --diff) MODE="diff"; NAME="$2"; shift 2 ;;
        --func) FUNC="$2"; shift 2 ;;
        -h|--help) usage ;;
        *) break ;;
    esac
done

[ $# -gt 0 ] || usage

if [ -n "$FUNC" ]; then
    [ $# -eq 1 ] || { echo "오류: --func 는 파일 하나만 받습니다." >&2; exit 2; }
    TEXT="$(extract_func "$FUNC" "$1")"
    [ -n "$TEXT" ] || { echo "오류: 함수 '$FUNC' 를 '$1' 에서 찾지 못했습니다." >&2; exit 2; }
    LABEL="$FUNC() in $1"
else
    TEXT="$(cat "$@")"
    LABEL="$*"
fi

RESULT="$(measure "$TEXT")"

case "$MODE" in
    show)
        echo "대상: $LABEL"
        echo "$RESULT" | while IFS='=' read -r k v; do printf '  %-8s %s\n' "$k" "$v"; done
        ;;
    save)
        mkdir -p "$BASE_DIR"
        printf '%s\n' "$RESULT" > "$BASE_DIR/$NAME"
        echo "기준선 저장: $BASE_DIR/$NAME ($LABEL)"
        echo "$RESULT" | while IFS='=' read -r k v; do printf '  %-8s %s\n' "$k" "$v"; done
        ;;
    diff)
        BASE="$BASE_DIR/$NAME"
        [ -f "$BASE" ] || { echo "오류: 기준선 '$BASE' 가 없습니다. 먼저 --save 하세요." >&2; exit 2; }
        echo "대조: $LABEL  vs  기준선 '$NAME'"
        fail=0
        while IFS='=' read -r k v; do
            b="$(grep "^$k=" "$BASE" | cut -d= -f2)"
            if [ "$k" = "lines" ]; then
                printf '  %-8s %s -> %s  (참고)\n' "$k" "$b" "$v"
            elif [ "$b" = "$v" ]; then
                printf '  %-8s %s  불변\n' "$k" "$v"
            else
                printf '  %-8s %s -> %s  ** 변화 **\n' "$k" "$b" "$v"
                fail=$((fail + 1))
            fi
        done <<< "$RESULT"
        echo ""
        if [ "$fail" -eq 0 ]; then
            echo "구조 지표 전부 불변 (lines 제외)"
            exit 0
        fi
        echo "$fail 개 지표가 변했습니다. 의도한 변화인지 확인하세요."
        exit 1
        ;;
esac
