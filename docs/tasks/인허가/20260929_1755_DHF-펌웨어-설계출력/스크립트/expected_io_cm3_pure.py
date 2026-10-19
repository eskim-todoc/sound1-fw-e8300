"""Read-only CM3 expected-I/O documentation checks; no build/device/source writes."""
from __future__ import annotations

import ast
import copy
import json
from pathlib import Path
import re
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[5]
TASK = ROOT / "docs/tasks/인허가/20260929_1755_DHF-펌웨어-설계출력"
IDS = [1, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
       21, 22, 23, 24, 27, 28, 29, 30, 31, 32, 33, 41, 44]


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def expected_projection(rows: list[dict]) -> list:
    """Never use actual values to supply or repair expected values."""
    return [(r.get("scenario"), r["unit"], r["case"], r.get("field"), r["expected"])
            for r in rows]


def checker_functions() -> dict:
    """Load only pure validator declarations; do not execute the test runner."""
    source = ROOT / "tests/unit/tdc_ut_cm3_extra_run.py"
    tree = ast.parse(source.read_text(encoding="utf-8"))
    selected = []
    for node in tree.body:
        if isinstance(node, ast.Assign):
            if any(isinstance(t, ast.Name) and t.id == "EXPECTED_RECORD_COUNTS"
                   for t in node.targets):
                selected.append(node)
        if isinstance(node, ast.FunctionDef) and node.name in (
                "expected_record_keys", "validate_records"):
            selected.append(node)
    namespace: dict = {}
    exec(compile(ast.Module(body=selected, type_ignores=[]), str(source), "exec"), namespace)
    return namespace


def mutation_checks(rows: list[dict]) -> int:
    """Reject 175 in-memory damaged evidence inputs, without altering files."""
    checks = checker_functions()
    validate = checks["validate_records"]
    scenarios = checks["EXPECTED_RECORD_COUNTS"]
    rejected = 0
    for scenario in scenarios:
        original = [r for r in rows if r["scenario"] == scenario and r["pass"] == 1]
        assert not validate(scenario, original), scenario
        for kind in ("drop", "duplicate", "reorder", "field", "case", "unit",
                     "unexpected_actual", "missing_expected"):
            bad = copy.deepcopy(original)
            if kind == "drop":
                bad.pop()
            elif kind == "duplicate":
                bad.append(copy.deepcopy(bad[-1]))
            elif kind == "reorder":
                bad[0], bad[1] = bad[1], bad[0]
            elif kind == "field":
                bad[0]["field"] += "_damaged"
            elif kind == "case":
                bad[0]["case"] += 10000
            elif kind == "unit":
                bad[0]["unit"] = 999
            elif kind == "unexpected_actual":
                bad[0]["actual"][0] += 1
            else:
                del bad[0]["expected"]
            assert validate(scenario, bad), (scenario, kind)
            rejected += 1
        for index, row in enumerate(original):
            if row["expected"] is None:
                bad = copy.deepcopy(original)
                bad[index]["expected"] = [0]
                assert validate(scenario, bad)
                rejected += 1
    original = [r for r in rows if r["scenario"] == "caller-dac-fail" and r["pass"] == 1]
    for field, value in (("known_failure", False), ("actual", [0]), ("expected", [1])):
        bad = copy.deepcopy(original)
        bad[5][field] = value
        assert validate("caller-dac-fail", bad)
        rejected += 1
    bad = copy.deepcopy(original)
    bad[7]["known_failure"] = True
    assert validate("caller-dac-fail", bad)
    return rejected + 1


def main() -> None:
    manifest = read_json(TASK / "qa/expected-io/cm3-pure.json")
    logs = read_json(TASK / "qa/expected-io/cm3-pure-logs.json")
    assert [u["id"] for u in manifest["units"]] == [f"CM3-{i:03}" for i in IDS]
    vectors = read_json(TASK / "qa/source-preserving-tests/cm3-vectors/checks.json")["records"]
    extra = read_json(TASK / "qa/source-preserving-tests/cm3-extra/checks.json")["records"]
    for directory, records in (("cm3-vectors", vectors), ("cm3-extra", extra)):
        current = read_json(ROOT / f"tmp/unit-tests/{directory}/checks.json")["records"]
        assert expected_projection(current) == expected_projection(records), directory
    all_rows = vectors + extra
    poisoned = copy.deepcopy(all_rows)
    for row in poisoned:
        row["actual"] = [-123456789] * len(row["actual"])
    assert expected_projection(poisoned) == expected_projection(all_rows)
    vector_count = extra_count = links = 0
    for unit in manifest["units"]:
        path = ROOT / unit["file"]
        text = path.read_text(encoding="utf-8")
        assert text.startswith(f'# {unit["id"]} — {unit["title"]}'.replace('·', '/'))
        assert "## 근거 소스와 기록" in text
        assert "주 작업 담당" not in text and "서브에이전트" not in text
        assert "\ufffd" not in text
        for target in re.findall(r"\]\(([^)]+)\)", text):
            if target.startswith(("http:", "https:", "#")):
                continue
            link = unquote(target.split("#")[0])
            assert (path.parent / link).exists(), (path, link)
            links += 1
        for case in unit["cases"]:
            if case["key"].startswith("vector-"):
                record = case["record"]
                expected = [f["value"] for f in case["expected_fields"]]
                if all(v is None for v in expected):
                    expected = None
                matched = [r for r in vectors if r["unit"] == record["unit"]
                           and r["case"] == record["case"]]
                assert len(matched) == 2
                assert all(r["expected"] == expected for r in matched), case["key"]
                assert matched[0]["actual"] == matched[1]["actual"]
                count = len(case["expected_fields"])
                verdict = "OBSERVED" if expected is None else "PASS"
                summary = f'[UT-{unit["id"]}] C{record["case"]} {verdict} N={count} F=0'
                assert summary in text
                vector_count += 1
            elif case["key"].startswith("extra-"):
                record = case["record"]
                value = case["expected_fields"][0]["value"]
                expected = None if value is None else [value]
                matched = [r for r in extra if r["scenario"] == case["scenario"]
                           and all(r[k] == v for k, v in record.items())]
                assert len(matched) == 2
                assert all(r["expected"] == expected for r in matched), case["key"]
                assert matched[0]["actual"] == matched[1]["actual"]
                extra_count += 1
    assert (vector_count, extra_count) == (48, 614)
    for unit in logs["units"]:
        n = int(unit["id"][-3:])
        for row in unit["records"]:
            if row.get("variant") == "legacy_current":
                assert n == 27 and row["expected"] == ([1, 24] if row["case"] == 1 else [2, 12])
            elif row["mode"] == "target" or row.get("variant") == "pc-address-layout":
                ref = next(r for r in vectors if r["unit"] == n and r["case"] == row["case"])
                assert row["expected"] == ref["expected"]
            else:
                ref = next(r for r in extra if r["unit"] == n and r["case"] == row["case"]
                           and r["scenario"] == row["scenario"])
                assert row["expected"] == ref["expected"]
    # Independent numerical anchors: no actual or recorded expected inputs.
    assert [int((2 * p + 14) / 41) + 1 for p in (13, 14)] == [1, 2]
    assert [300 * p for p in (249, 250, 251)] == [74700, 75000, 75300]
    assert (100 * 14435) // 32768 == 44
    assert (200 * 20871) // 32768 == 127
    rejected = mutation_checks(extra)
    assert rejected == 175
    print(json.dumps(dict(role="cm3-pure",documents=len(IDS),links=links,
                          pure_unique_records=vector_count,extra_unique_records=extra_count,
                          pure_raw_records=len(vectors),extra_raw_records=len(extra),
                          extra_scenarios=21,expected_actual_poison_invariant=True,
                          actual_used_for_expected=False,evidence_mutations_rejected=rejected,
                          device_executed=False,firmware_test_rerun=False), ensure_ascii=False))


if __name__ == "__main__":
    main()
