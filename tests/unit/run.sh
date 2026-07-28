#!/usr/bin/env bash
#
# ble/mapping 파싱 함수의 오프라인 단위 테스트를 빌드하고 실행한다.
#
#   bash tests/unit/run.sh
#
# 호스트(PC)에서 실제 펌웨어 소스를 그대로 컴파일한다. 사본이 아니다.
# 하드웨어·공유 메모리 의존은 stub_ble.c 가 대신한다.
#
# 필요한 것
#   - C 컴파일러 (MinGW-w64 gcc 등)
#   - Ezairo 8300 SDK 헤더
#
# 환경 변수로 지정할 수 있다
#   CC           컴파일러 경로
#   EZAIRO_SDK   SDK 루트 (include/ 를 담고 있는 디렉터리)

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SRC="$ROOT/src/2__cm3/source"
BUILD="$HERE/build"

# ---------------------------------------------------------------- 컴파일러
find_compiler() {
    if [ -n "${CC:-}" ] && command -v "$CC" >/dev/null 2>&1; then
        echo "$CC"
        return 0
    fi

    if command -v gcc >/dev/null 2>&1; then
        command -v gcc
        return 0
    fi

    # winget 으로 설치한 WinLibs (MinGW-w64)
    local pkg
    for pkg in "$LOCALAPPDATA/Microsoft/WinGet/Packages"/BrechtSanders.WinLibs.*/mingw64/bin/gcc.exe; do
        if [ -x "$pkg" ]; then
            echo "$pkg"
            return 0
        fi
    done

    return 1
}

# ---------------------------------------------------------------- SDK
find_sdk() {
    if [ -n "${EZAIRO_SDK:-}" ] && [ -d "$EZAIRO_SDK/include/cm3" ]; then
        echo "$EZAIRO_SDK"
        return 0
    fi

    local cand
    for cand in \
        "/c/Program Files (x86)/ON Semiconductor/Ezairo 8300 SDK" \
        "/c/Program Files/ON Semiconductor/Ezairo 8300 SDK"; do
        if [ -d "$cand/include/cm3" ]; then
            echo "$cand"
            return 0
        fi
    done

    return 1
}

CC_BIN="$(find_compiler)" || {
    echo "오류: C 컴파일러를 찾지 못했습니다." >&2
    echo "" >&2
    echo "  MinGW-w64 를 설치하세요:" >&2
    echo "    winget install --id BrechtSanders.WinLibs.POSIX.UCRT --exact" >&2
    echo "" >&2
    echo "  또는 CC 환경변수로 직접 지정하세요:" >&2
    echo "    CC=/path/to/gcc bash tests/unit/run.sh" >&2
    exit 2
}

SDK_DIR="$(find_sdk)" || {
    echo "오류: Ezairo 8300 SDK 헤더를 찾지 못했습니다." >&2
    echo "  EZAIRO_SDK 환경변수로 SDK 루트를 지정하세요." >&2
    exit 2
}

echo "컴파일러 : $CC_BIN"
echo "SDK      : $SDK_DIR"
echo ""

# ---------------------------------------------------------------- 빌드 설정
INCLUDES=(
    -I "$HERE"
    -I "$SRC/ble"
    -I "$SRC/ble/mapping"
    -I "$SRC/sys"
    -I "$SRC/hal"
    -I "$SRC/cfx_link"
    -I "$SRC/isd"
    -I "$SRC/stim"
    -I "$SRC/board"
    -I "$SRC/util"
    -I "$SRC/lib/SEGGER_RTT"
    -I "$SRC"
    -I "$SDK_DIR/include/cm3"
    -I "$SDK_DIR/include/shared"
)

CFLAGS=(-std=gnu11 -Wall -Wextra -O0 -g)

# 테스트가 함께 컴파일하는 실제 펌웨어 소스.
# tdc_ble_reply.c 는 스텁이 아니라 실물이다. 응답 조립 결과까지 검증한다.
COMMON_SRC=(
    "$HERE/stub_ble.c"
    "$SRC/ble/tdc_ble_reply.c"
)

# 테스트 이름 -> 대상 소스
declare -A TEST_TARGETS=(
    [map_measure]="$SRC/ble/mapping/tdc_ble_map_measure.c"
    [map_flash]="$SRC/ble/mapping/tdc_ble_map_flash.c"
    [map_stim]="$SRC/ble/mapping/tdc_ble_map_stim.c"
)

mkdir -p "$BUILD"

fail_total=0
run_total=0
src_warn_total=0

for name in map_measure map_flash map_stim; do
    test_src="$HERE/test_$name.c"
    [ -f "$test_src" ] || continue

    exe="$BUILD/test_$name.exe"
    log="$BUILD/build_$name.log"

    if ! "$CC_BIN" "${CFLAGS[@]}" "${INCLUDES[@]}" \
            -o "$exe" "$test_src" "${COMMON_SRC[@]}" ${TEST_TARGETS[$name]} > "$log" 2>&1; then
        echo "빌드 실패: test_$name" >&2
        cat "$log" >&2
        fail_total=$((fail_total + 1))
        continue
    fi

    # 경고는 빌드를 막지 않는다. 다만 어느 쪽에서 났는지 구분해 보고한다.
    # tests/ 경고는 우리 책임이라 0 이어야 하고,
    # src/ 경고는 프로덕션 코드의 것이라 이 작업에서 고치지 않는다(원칙_4).
    warn_test=$(grep "warning:" "$log" | grep -c "/tests/" || true)
    warn_src=$(grep "warning:" "$log" | grep -vc "/tests/" || true)

    if [ "$warn_test" -ne 0 ]; then
        echo "!! 테스트 코드 경고 $warn_test 건 (0 이어야 함)" >&2
        grep "warning:" "$log" | grep "/tests/" >&2
        fail_total=$((fail_total + 1))
    fi
    if [ "$warn_src" -ne 0 ]; then
        src_warn_total=$((src_warn_total + warn_src))
    fi

    echo "======== test_$name ========"
    if "$exe"; then
        :
    else
        fail_total=$((fail_total + 1))
    fi
    run_total=$((run_total + 1))
    echo ""
done

if [ "$run_total" -eq 0 ]; then
    echo "실행된 테스트가 없습니다." >&2
    exit 2
fi

if [ "$src_warn_total" -ne 0 ]; then
    echo "참고: src/ 컴파일 경고 $src_warn_total 건 (프로덕션 코드 - 이 러너는 고치지 않는다)"
    echo "      상세는 $BUILD/build_*.log"
    echo ""
fi

if [ "$fail_total" -ne 0 ]; then
    echo "########  $fail_total 개 테스트 실패  ########"
    exit 1
fi

echo "########  전체 통과  ########"
exit 0
