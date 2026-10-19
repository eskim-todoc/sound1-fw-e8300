"""Split a saved RTT capture without changing the raw file or declaring acceptance.

Checks observed record framing, index continuity and BEGIN/END pairs. Full
criteria coverage and physical evidence still require the test manifest.
"""
from __future__ import annotations
import argparse
from collections import defaultdict
from datetime import datetime
import hashlib
import json
from pathlib import Path
import re

TAG = re.compile(r"\[UT-(CFX|CM3|HEAR|BOOT)-(\d{3})\] C(\d+) (.*)")
VALUES = re.compile(r"A\[(\d+):(\d+)\]=(-?\d+(?:,-?\d+)*)$")
DIFF = re.compile(r"I(\d+) A=(-?\d+) E=(-?\d+)$")
FINAL = re.compile(r"(PASS|FAIL|OBSERVED) N=(\d+) F=(\d+)$")
GROUP = re.compile(r"\[UT-BATCH\] CORE=(CFX|CM3|HEAR|BOOT) (BEGIN|END) N=(\d+)$")
RAW = re.compile(r"\[UT-BATCH\] CORE=(CFX|HEAR) RAW=(NUMERIC|FFT|VMAG|EXPONENT) U=(\d+) C=(\d+)$")


def parse(text: str) -> dict:
    units = defaultdict(list)
    records, groups, problems = [], [], []
    pending = None
    active = None
    raw_label = None
    numeric_unit = None
    all_lines = []
    # Even a syntactically complete END may be a partially written RTT line.
    # Only a received LF commits the last line; never accept a truncated file.
    if text and not text.endswith("\n"):
        problems.append("파일 마지막 줄의 줄바꿈이 없어 전송 완료를 확인할 수 없음")
    for lineno, raw in enumerate(text.splitlines(), 1):
        # The GUI raw log normally has no prefix, but preserve timestamped
        # captures by extracting only from the literal test tag onward.
        start = raw.find("[UT-")
        if start < 0:
            continue
        line = raw[start:].rstrip()
        all_lines.append(line)
        if not line.isascii() or len(line.encode("utf-8")) + 2 > 60:
            problems.append(f"{lineno}: 시험 로그 길이/ASCII 형식 오류")
        group = GROUP.fullmatch(line)
        if group:
            core, event, count_text = group.groups()
            count = int(count_text)
            if event == "BEGIN":
                numeric_unit = None
                if not count:
                    problems.append(f"{lineno}: 빈 묶음 BEGIN")
                if active:
                    problems.append(f"{lineno}: 앞 묶음이 끝나기 전 BEGIN")
                if pending or raw_label:
                    problems.append(f"{lineno}: 앞 조건의 종료 줄 누락")
                active = {"core": core, "requested": count, "line": lineno,
                          "records": 0, "ended": False}
                groups.append(active)
            elif not active or active["core"] != core:
                problems.append(f"{lineno}: 대응 BEGIN 없는 END")
            else:
                active["completed"] = count
                active["ended"] = True
                if count != active["requested"] or not active["records"] or pending or raw_label:
                    problems.append(f"{lineno}: 묶음 개수 불일치/결과 누락")
                active = None
                numeric_unit = None
            continue
        if line.startswith("[UT-BATCH]"):
            if any(word in line for word in ("ABORT", "CANCEL", "ERROR", "TIMEOUT")):
                problems.append(f"{lineno}: {line}")
                if active:
                    active["aborted"] = True
                    active = None
            else:
                raw_marker = RAW.fullmatch(line)
                if not raw_marker or not active or pending or active["core"] != raw_marker[1]:
                    problems.append(f"{lineno}: 알 수 없거나 위치가 잘못된 묶음 줄")
                elif raw_label:
                    problems.append(f"{lineno}: 앞 원시 배열이 누락됨")
                else:
                    raw_key = f"{raw_marker[1]}-{int(raw_marker[3]):03d}"
                    raw_label = (raw_key, int(raw_marker[4]), raw_marker[2])
                    numeric_unit = raw_key if raw_marker[2] == "NUMERIC" else None
                    # Keep section identity even when filtering to one unit;
                    # raw FFT/VMAG may reuse the same case number as verdicts.
                    units[raw_key].append(line)
            # Recognized raw-array markers are retained, never interpreted PASS.
            continue
        if line.startswith("[UT-CONSOLE]"):
            if any(word in line for word in ("ERROR", "REJECTED", "WAITING_RESTORE")):
                problems.append(f"{lineno}: {line}")
            continue
        match = TAG.fullmatch(line)
        if not match:
            if not line.startswith("[UT-CONSOLE]"):
                problems.append(f"{lineno}: 해석할 수 없는 시험 줄")
            continue
        core, unit, case, payload = match.groups()
        if not active:
            problems.append(f"{lineno}: 묶음 BEGIN/END 밖의 시험 결과")
        key = f"{core}-{unit}"
        units[key].append(line)
        identity = (key, int(case))
        values, difference, final = VALUES.fullmatch(payload), DIFF.fullmatch(payload), FINAL.fullmatch(payload)
        if values:
            first, last = int(values[1]), int(values[2])
            data = list(map(int, values[3].split(",")))
            if first == 0:
                if pending:
                    problems.append(f"{lineno}: 앞 조건의 종료 줄 누락")
                if numeric_unit != key:
                    numeric_unit = None
                pending = {"unit": key, "case": int(case), "values": [], "differences": [],
                           "line": lineno, "errors": [], "array_kind": "NUMERIC" if numeric_unit else "RESULT"}
                if raw_label:
                    multi_numeric = raw_label[2] == "NUMERIC" and raw_label[1] == 0
                    if raw_label[0] != key or (not multi_numeric and raw_label[1] != int(case)):
                        pending["errors"].append("원시 배열 제목과 ID/조건이 다름")
                    else:
                        pending["array_kind"] = raw_label[2]
                    raw_label = None
            if not pending or (pending["unit"], pending["case"]) != identity:
                problems.append(f"{lineno}: 배열 시작 줄 누락")
                continue
            if first != len(pending["values"]) or last < first or len(data) != last - first + 1:
                pending["errors"].append("배열 인덱스 누락/중복")
            if pending["differences"]:
                pending["errors"].append("불일치 줄 다음에 배열 출력")
            pending["values"].extend(data)
        elif difference or final:
            if not pending or (pending["unit"], pending["case"]) != identity:
                problems.append(f"{lineno}: 결과 배열 없는 판정/불일치 줄")
                continue
            if difference:
                idx, actual, expected = map(int, difference.groups())
                if (idx >= len(pending["values"]) or actual == expected or
                        pending["values"][idx] != actual or
                        any(d["index"] == idx for d in pending["differences"])):
                    pending["errors"].append("불일치 인덱스/값 오류")
                pending["differences"].append({"index": idx, "actual": actual, "expected": expected})
            else:
                status, n, failed = final[1], int(final[2]), int(final[3])
                pending.update(status=status, count=n, failed=failed)
                if n != len(pending["values"]) or n == 0 or failed != len(pending["differences"]):
                    pending["errors"].append("판정 개수와 실제 줄 불일치")
                if (status in ("PASS", "OBSERVED") and failed) or (status == "FAIL" and not failed):
                    pending["errors"].append("판정과 불일치 개수 모순")
                for error in pending["errors"]:
                    problems.append(f"{lineno}: {key} C{case} {error}")
                records.append(pending)
                if active:
                    active["records"] += 1
                    if active["core"] != core:
                        problems.append(f"{lineno}: 묶음과 다른 코어의 결과")
                pending = None
        else:
            problems.append(f"{lineno}: 알 수 없는 결과 형식")
    if pending:
        problems.append(f"{pending['line']}: 마지막 조건의 종료 줄 누락")
    if raw_label:
        problems.append("마지막 원시 배열 제목에 대응하는 결과가 없음")
    if active:
        problems.append(f"{active['line']}: 묶음 END 누락")
    if not records:
        problems.append("완료된 시험 조건이 없음")
    if not groups:
        problems.append("묶음 BEGIN/END가 없어 일괄 실행 완료 여부를 확인할 수 없음")
    return {"units": dict(units), "records": records, "groups": groups,
            "problems": problems, "lines": all_lines}


def export(source: Path, destination: Path) -> dict:
    # Refuse overwriting an earlier result directory, including an empty one.
    data = source.read_bytes()
    result = parse(data.decode("utf-8-sig", errors="strict"))
    destination.mkdir(parents=True, exist_ok=False)
    (destination / "원본.log").write_bytes(data)
    for key, lines in result["units"].items():
        (destination / f"{key}.log").write_bytes(("\r\n".join(lines) + "\r\n").encode("ascii",errors="backslashreplace"))
    (destination / "전체-시험.log").write_text("\n".join(result["lines"]) + "\n", encoding="utf-8")
    result.update(source=str(source.resolve()), sha256=hashlib.sha256(data).hexdigest(),
                  checks="received record framing only; not all-vector coverage or product acceptance")
    (destination / "수집검사.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    summary = ["# 유닛 시험 로그 수집 결과", "",
               "원본은 수정하지 않았다. 아래 PASS/FAIL은 수신한 조건의 출력이며 제품 전체 판정이 아니다.",
               "배열 인덱스와 종료 줄을 검사했다. 실행하지 않았거나 통째로 빠진 유닛/벡터는 매뉴얼의 묶음 목록 및 예상 로그와 별도로 대조한다.", "",
               f"원본 SHA256: `{result['sha256']}`", "",
               f"수신한 유닛: {len(result['units'])}개, 종료된 조건: {len(result['records'])}개", "",
               "## 먼저 확인할 사항", ""]
    summary += [f"- {p}" for p in result["problems"]] or ["기록 안에서 배열/종료 줄 형식 오류는 발견하지 못했다. 시험 통과를 뜻하지 않는다."]
    summary += ["", "## 유닛별 수신 결과", "", "| 유닛 | PASS 조건 | FAIL 조건 | 관찰 조건 |", "|---|---:|---:|---:|"]
    for key in sorted(result["units"]):
        rows = [r for r in result["records"] if r["unit"] == key]
        counts = [sum(r["status"] == status for r in rows) for status in ("PASS", "FAIL", "OBSERVED")]
        summary.append(f"| [{key}]({key}.log) | {counts[0]} | {counts[1]} | {counts[2]} |")
    summary += ["", "## 기대값과 다른 조건", ""]
    failed = [r for r in result["records"] if r["status"] == "FAIL"]
    summary += [f"- {r['unit']} C{r['case']}: {r['failed']}개 값 불일치" for r in failed] or ["수신한 종료 줄 중 FAIL은 없다. 누락된 시험이나 물리 출력은 이 사실로 판정하지 않는다."]
    (destination / "결과 요약.md").write_text("\n".join(summary) + "\n", encoding="utf-8")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    dest = args.out or args.capture.parent / ("시험결과_" + datetime.now().strftime("%Y%m%d_%H%M%S_%f"))
    result = export(args.capture, dest)
    print(dest.resolve())
    print(f"records={len(result['records'])}; framing_problems={len(result['problems'])}; not product acceptance")
    raise SystemExit(2 if result["problems"] else 0)


if __name__ == "__main__":
    main()
