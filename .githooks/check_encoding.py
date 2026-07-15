#!/usr/bin/env python3
"""pre-commit: CP949 왕복 시 손실되는 문자를 커밋 전에 차단한다.

배경
----
Eclipse clang-format 플러그인은 clang-format 을 외부 프로세스로 실행하며
stdin/stdout 인코딩을 명시하지 않으면 플랫폼 기본값(한국어 Windows = CP949)을
쓴다. 이때 CP949 에 매핑이 없는 문자는 '?'(0x3F) 로 대체되어 영구 손실된다.
파일은 UTF-8 을 유지한 채 특정 문자만 죽기 때문에 알아채기 어렵다.

실제 피해 사례(2026-07-15): U+2014(—) 148건이 시한폭탄으로 남아 있었고
그중 75건이 이미 '?' 로 손상된 상태였다.

대책
----
소스 주석에는 ASCII 만 쓴다(한글은 CP949 에 있으므로 안전). 이 훅은 CP949
왕복이 불가능한 문자가 staged 파일에 들어오면 커밋을 막는다.

설치: git config core.hooksPath .githooks
"""
import subprocess
import sys
from pathlib import Path

# Windows 에서 stdout 은 기본 CP949 로 잡힌다. 문제 문자를 그대로 출력하면
# 훅 자신이 UnicodeEncodeError 로 죽으므로 UTF-8 로 재설정한다(이 훅이 막으려는
# 바로 그 사고를 훅이 겪는 것을 방지).
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

TARGET_EXT = (".c", ".h")
_cache = {}

# 자주 쓰이지만 CP949 에 없는 문자 -> 권장 대체
SUGGEST = {
    "—": "-",   # — EM DASH
    "–": "-",   # – EN DASH
    "≈": "~",   # ≈ ALMOST EQUAL TO
    "µ": "u",   # µ MICRO SIGN  (us / uA)
    "►": ">",   # ► BLACK RIGHT-POINTING POINTER
    "…": "...",  # … HORIZONTAL ELLIPSIS
}


def cp949_safe(ch):
    """CP949 왕복 후에도 원래 문자가 보존되는가."""
    if ch in _cache:
        return _cache[ch]
    try:
        ok = ch.encode("cp949").decode("cp949") == ch
    except (UnicodeEncodeError, UnicodeDecodeError):
        ok = False
    _cache[ch] = ok
    return ok


def staged_files():
    out = subprocess.run(
        ["git", "diff", "--cached", "--name-only", "--diff-filter=ACM"],
        capture_output=True, text=True, encoding="utf-8",
    ).stdout
    return [f for f in out.split("\n") if f.strip().endswith(TARGET_EXT)]


def main():
    problems = []
    for f in staged_files():
        p = Path(f)
        if not p.exists():
            continue
        try:
            text = p.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            problems.append((f, 0, "파일 인코딩이 UTF-8 이 아님", ""))
            continue
        for ln, line in enumerate(text.split("\n"), 1):
            for ch in line:
                if ord(ch) > 127 and not cp949_safe(ch):
                    hint = SUGGEST.get(ch)
                    desc = f"U+{ord(ch):04X} '{ch}'"
                    if hint:
                        desc += f" -> '{hint}' 로 바꾸세요"
                    problems.append((f, ln, desc, line.strip()[:70]))
                    break   # 라인당 1건만 보고

    if not problems:
        return 0

    print("")
    print("=" * 72)
    print(" 커밋 차단: CP949 왕복 시 손실되는 문자가 있습니다.")
    print(" Eclipse clang-format 이 이 문자를 '?' 로 영구 파괴합니다.")
    print("=" * 72)
    for f, ln, desc, src in problems[:20]:
        loc = f"{f}:{ln}" if ln else f
        print(f"  {loc}")
        print(f"      {desc}")
        if src:
            print(f"      | {src}")
    if len(problems) > 20:
        print(f"  ... 외 {len(problems) - 20}건")
    print("")
    print(" 소스 주석에는 ASCII 를 쓰세요 (한글은 CP949 에 있어 안전합니다).")
    print(" 의도적으로 통과시키려면: git commit --no-verify")
    print("")
    return 1


sys.exit(main())
