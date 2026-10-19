"""Read-only CM3 I/O expected-I/O document validator.

Reads the manifest, Markdown, source anchors and archived PC results.
Does not compile/run firmware, access a device, write results or alter baselines.
The numeric source oracle is intentionally bounded: T/C formulas, criterion
arrays and known mismatches. It is not a full C interpreter.
"""
from __future__ import annotations

import copy
import argparse
import json
import re
from collections import Counter
from pathlib import Path
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[5]
TASK = Path(__file__).resolve().parents[1]
MANIFEST = TASK / "qa/expected-io/cm3-io.json"
IDS = [2, 10, 25, 26, 34, 35, 36, 37, 38, 39, 40, 42, 43, 45, 46, 47, 48, 49, 50, 51]
PREFIX = {"host": "H", "ram": "R", "host_extension": "E"}
EXPECTED_TOTALS = {"host": (81, 377), "host_extension": (83, 579), "ram": (79, 456)}
SOURCE = "src/2__cm3/Cortex-M3-src/test/tdc_ut_io_cm3.c"
EXT = "tests/unit/tdc_ut_io_cm3_extended.c"


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def require(condition: bool, detail: str) -> None:
    if not condition:
        raise AssertionError(detail)


def expected_values(case: dict) -> list:
    return [field["expected"] for field in case["expected_fields"]]


def indexed(manifest: dict) -> dict:
    return {(unit["number"], case["mode"], case["case_id"]): case
            for unit in manifest["units"] for case in unit["cases"]}


def md_escape(value: object) -> str:
    return str(value).replace("|", "\\|").replace("\n", "<br>")


def source_oracle(manifest: dict) -> dict:
    """Independent source formulas/constants, never report actual values."""
    cases = indexed(manifest)
    ext = (ROOT / EXT).read_text(encoding="utf-8")
    base = (ROOT / SOURCE).read_text(encoding="utf-8")
    for expression in ["100*slot+10*map+i", "500*slot+10*map+i",
                       '"stored-T"', "300+i", '"stored-C"', "800+i",
                       '"unselected-map-kept"', "120+i"]:
        require(expression in ext, "source formula changed: " + expression)
    formula_fields = 0
    for case_id, (slot, map_number) in enumerate([(1, 1), (2, 1), (1, 2), (4, 4)], 1):
        values = expected_values(cases[42, "host_extension", case_id])
        oracle = [value for i in range(32)
                  for value in (100*slot + 10*map_number + i,
                                500*slot + 10*map_number + i)]
        require(values[1:] == oracle, f"T/C read formula case {case_id}")
        formula_fields += len(oracle)
    oracle = [value for i in range(32) for value in (300+i, 800+i, 120+i)]
    require(expected_values(cases[42, "host_extension", 9])[2:] == oracle,
            "T/C write/unselected map formula")
    formula_fields += len(oracle)

    # Explicit expected constants from known(...) / put(...) in current tests.
    failure_oracles = [
        (39, "host_extension", case, 0, 0) for case in (6, 8, 10, 12)
    ] + [
        (42, "host_extension", 10, 0, 240),
        (43, "host_extension", 5, 0, 240),
        (46, "host_extension", 5, 0, 0),
        (46, "host_extension", 7, 2, 74),
        (49, "host_extension", 3, 0, 0),
        (49, "host_extension", 9, 2, 240),
        (49, "host_extension", 9, 3, 1),
    ] + [
        (51, mode, case, field, value)
        for mode in ("host", "ram")
        for case, values in ((17, (240, 3)), (18, (240, 4)))
        for field, value in enumerate(values)
    ]
    for marker in ['known("failed-I2C-must-not-report-success", operations[i](), 0, 1)',
                   'known("failed-save-normal-ACK",tx[0],en__ERROR_Response,0x6D)',
                   'known("failed-save-normal-ACK",tx[0],en__ERROR_Response,0x6F)',
                   'known("busy-command-retained",getRemoteCommand(),0x4A,en__remoteControl_IDLE)',
                   'known("busy-error-detail-overwritten",tx[3],en__PreviouCommnadIsNotCompleted,0)']:
        require(marker in ext, "known expectation source anchor changed: " + marker)
    require("put(tx[3], 3)" in base and "put(tx[3], 4)" in base,
            "OTA source expected error codes missing")
    for unit, mode, case_id, field, value in failure_oracles:
        require(expected_values(cases[unit, mode, case_id])[field] == value,
                f"source expected mismatch: {unit}/{mode}/{case_id}/{field}")

    criteria = read_json(ROOT / "tests/unit/tdc_ut_io_cm3_criteria.json")
    criterion_fields = 0
    for unit in criteria["added_target_ram_tests"]:
        if unit["unit"] != 50:
            continue
        for case in unit["cases"]:
            require(expected_values(cases[50, "ram", case["case"]]) == case["expected_fields"],
                    f"RAM 50 criterion {case['case']}")
            criterion_fields += len(case["expected_fields"])
    return {"tc_formula_fields": formula_fields,
            "known_failure_expected_fields": len(failure_oracles),
            "criteria_numeric_fields": criterion_fields}


def mismatch_field(record: dict) -> int:
    name = record["field"]
    if name.startswith("field="):
        return int(re.match(r"field=(\d+)", name).group(1))
    return {"busy-command-retained": 2, "busy-error-header-overwritten": 2,
            "busy-error-detail-overwritten": 3}.get(name, 0)


def validate(manifest: dict) -> dict:
    require(manifest["role"] == "cm3-io", "role")
    require([u["number"] for u in manifest["units"]] == IDS, "owned unit IDs/order")
    cases = indexed(manifest)
    require(len(cases) == 243, "duplicate/missing mode/case")
    totals = {}
    for mode, expected in EXPECTED_TOTALS.items():
        selected = [case for (_, m, _), case in cases.items() if m == mode]
        actual = (len(selected), sum(len(c["expected_fields"]) for c in selected))
        require(actual == expected, f"mode counts {mode}: {actual}")
        totals[mode] = {"cases": actual[0], "expected_fields": actual[1]}
    observations = [(u, m, n) for (u, m, n), c in cases.items() if not c["expected_fields"]]
    require(observations == [(2, "host_extension", 6), (2, "host_extension", 12)],
            "observation-only scope")
    links = 0
    table_fields = 0
    rtt_lines = 0
    source_text = {}
    archive = {}
    for name in ("checks.json", "ram-checks.json"):
        report = read_json(TASK / "qa/source-preserving-tests/cm3-io" / name)
        tmp = read_json(ROOT / "tmp/unit-tests/cm3-io" / name)
        for key in ("cases", "observation_only_cases", "payload_comparisons", "units",
                    "failed_comparisons", "observations"):
            require(report[key] == tmp[key], f"archive/tmp {name}: {key}")
        archive[name] = report
        for row in report["units"]:
            selected = [c for (u, m, _), c in cases.items()
                        if u == row["unit"] and m == row["runner"]]
            require(len(selected) == row["cases"], f"case count {row}")
            require(sum(len(c["expected_fields"]) for c in selected) == row["payload_comparisons"],
                    f"payload count {row}")
            if row["runner"] != "host_extension":
                require(sum(4 + len(c["expected_fields"]) for c in selected) == row["compared_words"],
                        f"framing count {row}")
        for mismatch in report["failed_comparisons"]:
            key = mismatch["unit"], mismatch["runner"], mismatch["case"]
            value = expected_values(cases[key])[mismatch_field(mismatch)]
            require(value == mismatch["expected"] and value != mismatch["actual"],
                    f"actual copied into expected: {key}/{mismatch['field']}")

    for unit in manifest["units"]:
        path = ROOT / unit["file"]
        require(path.name == unit["id"] + ".md", "filename")
        doc = path.read_text(encoding="utf-8")
        require(doc.startswith("# " + unit["id"] + " / " + unit["title"]), "ID/title")
        require("\ufffd" not in doc and "서브에이전트" not in doc and "주 작업 담당" not in doc,
                "damaged text or work-assignment prose")
        blocks = re.split(r"(?=^### )", doc, flags=re.M)
        counts = Counter(c["mode"] for c in unit["cases"])
        for mode, count in counts.items():
            require(sorted(c["case_id"] for c in unit["cases"] if c["mode"] == mode)
                    == list(range(1, count+1)), "case sequence")
        for case in unit["cases"]:
            title_token = case["key"]
            block = next((b for b in blocks if title_token in b.split("\n", 1)[0]), None)
            require(block is not None, f"missing MD case: {unit['id']}/{title_token}")
            require(case["input"] in block, f"missing input: {unit['id']}/{title_token}")
            for index, field in enumerate(case["expected_fields"]):
                require(field["index"] == index and field["tolerance"] == 0, "field index/tolerance")
                require("actual" not in field, "mixed actual/expected field object")
                row = f"| {index} | {md_escape(field['name'])} | {md_escape(field['expected'])} |"
                require(row in block, f"missing expected row: {unit['id']}/{title_token}/{index}")
                table_fields += 1
            require(case["expected_origin"].startswith("시험 코드"), "expected provenance")
            if not case["expected_fields"]:
                require(case["status"] == "OBSERVED_JUDGMENT_PENDING", "OBSERVED promoted to PASS")
                require("판정을 보류" in block, "missing pending judgment")
            source_path, symbols = case["source"].split(":", 1)
            if source_path not in source_text:
                source_text[source_path] = (ROOT / source_path).read_text(encoding="utf-8")
            for symbol in symbols.split("/"):
                require(re.search(r"\b" + re.escape(symbol) + r"\s*\(", source_text[source_path]) is not None,
                        "source symbol missing: " + case["source"])

        for summary in unit["rtt_summaries"]:
            case = cases[unit["number"], "ram", summary["case_id"]]
            line = (f"[UT-CM3-{unit['number']:03d}] C{case['case_id']} PASS "
                    f"N={len(case['expected_fields'])} F=0")
            require(summary["line"] == line and line in doc, "RTT C/N mismatch")
            rtt_lines += 1
        require(len(unit["rtt_summaries"]) == counts["ram"], "unexpected target RTT")
        observer = unit["observer"]
        require(not observer["expected_fields"], "observer expected must be absent")
        line = f"[UT-CM3-{unit['number']:03d}] C100 OBSERVED N={len(observer['fields'])} F=0"
        require(observer["rtt_summary"] == line and line in doc, "observer C/N")
        require([f["index"] for f in observer["fields"]] == list(range(len(observer["fields"]))),
                "observer indices")
        for mode, filename in (("host", "checks.json"), ("ram", "ram-checks.json")):
            for line in unit["saved_logs"][mode]:
                require(line in archive[filename]["stdout"].splitlines(), "invented PC log")
                require(line in doc, "missing saved PC log")
        expected_findings = [r for report in archive.values() for r in report["failed_comparisons"]
                             if r["unit"] == unit["number"]]
        require(unit["known_mismatches"] == expected_findings, "known findings lost")
        for target in re.findall(r"(?m)^- \[[^\n]+\]\(([^)\n]+)\)$", doc):
            require((path.parent / unquote(target)).resolve().is_file(), "broken link: " + target)
            links += 1
    require(table_fields == 1412 and rtt_lines == 79, "document totals")
    log_source = (ROOT / "src/test/tdc_ut_log.c").read_text(encoding="utf-8")
    require('[UT-%s-%03u] C%u %s N=%u F=%u' in log_source, "RTT format changed")
    oracle = source_oracle(manifest)
    coverage = read_json(ROOT / "tests/unit/tdc_ut_io_cm3_coverage.json")
    require(set(IDS).issubset({u["id"] for u in coverage["units"]}), "coverage ID scope")
    return {"status": "PASS", "documents": len(IDS), "all_mode_cases": len(cases),
            "all_mode_expected_fields": table_fields, "mode_counts": totals,
            "observation_only_cases": len(observations), "valid_source_links": links,
            "rtt_pass_summaries": rtt_lines, "rtt_observed_summaries": len(IDS),
            "known_mismatches_preserved": {"pc": 15, "ram": 4}, "source_oracle": oracle,
            "product_tests_executed": False, "device_run": False, "baseline_modified": False}


def self_check_oracle(manifest: dict) -> int:
    """In-memory negative tests of this document validator; no file mutation."""
    checked = 0
    for key, index, injected in [((51, "host", 17), 0, 195),
                                  ((42, "host_extension", 1), 63, -999)]:
        changed = copy.deepcopy(manifest)
        indexed(changed)[key]["expected_fields"][index]["expected"] = injected
        try:
            source_oracle(changed)
        except AssertionError:
            checked += 1
        else:
            raise AssertionError("oracle accepted deliberately corrupted expectation")
    return checked



def build_pc_output_blocks(unit: dict) -> list[dict]:
    """Exact printf summary lines, conditional on all contract checks matching.

    The CHECK_KNOWN_FINDINGS assertions intentionally require known bad actuals;
    these all-pass lines therefore describe only the ordinary contract mode.
    """
    blocks = []
    known_bad = {39: 4, 42: 1, 43: 1, 46: 2, 49: 3}
    for mode, label in (
            ("host", "기본 PC / --ram 없음"),
            ("ram", "PC RAM 분기 / --ram"),
            ("host_extension", "PC 전용 확장 / --ram 없음")):
        cases = [case for case in unit["cases"] if case["mode"] == mode]
        if not cases:
            continue
        payload = sum(len(case["expected_fields"]) for case in cases)
        notes = ("--check-known-findings를 사용하지 않는 정상 계약 모드에서 "
                 "이 유닛의 사전 비교가 모두 일치했을 때의 조건부 stdout 문자열이다. "
                 "현재 실행에서 수집한 결과나 실장치 PASS가 아니다. "
                 "사례별 A/EXPECTED 배열 줄은 PC stdout에 없으며 입력 문서에서만 확인한다. "
                 "text의 마지막 LF는 printf의 \\n을 나타낸다.")
        if mode == "host_extension":
            line = (f"CM3 EXT unit={unit['number']} cases={len(cases)} comparisons={payload} "
                    "mismatches=0 known=0 HOST_ONLY\n")
            notes += (" known은 known() 호출 횟수가 아니라 actual!=expected인 경우만 증가한다. "
                      "따라서 전부 일치하는 정상 모드에서는 0이다.")
            if unit["number"] in known_bad:
                notes += (f" 현재 알려진 불일치는 {known_bad[unit['number']]}필드다. "
                          "--check-known-findings의 known()는 알려진 오류 actual과의 일치를 "
                          "assert하므로 기대값으로 고쳐진 상태를 그 모드의 통과로 간주할 수 없다.")
            if unit["number"] == 2:
                notes += (" cases=12에는 기대값 없는 E-06/E-12 관찰 2건이 포함된다. "
                          "comparisons=26에는 E-02의 반복 관계 비교도 포함된다. "
                          "이 요약의 mismatches=0은 관찰 2건을 PASS로 승격하지 않는다. "
                          "관찰 printf 형식과 변동값은 variable_records에 별도로 둔다.")
        else:
            words = payload + 4 * len(cases)
            line = (f"CM3 unit={unit['number']} cases={len(cases)} words={words} "
                    "mismatches=0 done=1 overflow=0\n")
            notes += " words는 각 사례의 framing 4원소를 포함하며 done=1은 기록 완료이지 제품 전체 합격이 아니다."
            if mode == "ram":
                notes += " --ram도 PC __WFE/MMIO 모형이며 타깃 장치 실행이 아니다."
            if unit["number"] == 51:
                notes += (" 현재 PC 모형에는 오류 응답 4필드 불일치가 있다. "
                          "--check-known-findings는 mismatch==4를 요구하므로 여기의 0행은 "
                          "그 모드의 통과 로그가 아니다. 시험용 SPI 완료 처리 보완과 별개로 "
                          "PC 출력 완료를 타깃 완료/복원 증거로 사용할 수 없다.")
        blocks.append({"label": label + " / 정상 계약 전 비교 일치 시", "text": line, "notes": notes})
    return blocks


def build_log_export(manifest: dict) -> dict:
    """Export only pre-existing expected fields; never read saved actual arrays."""
    units = []
    # The manifest preserves the earlier audit. Current batch readiness comes
    # from the test-only completion boundary, not from changing expected data.
    io_source = (ROOT / SOURCE).read_text(encoding="utf-8")
    spi_completion = all(anchor in io_source for anchor in (
        "if (phase == 1 && s_tdc_spi_auto_ack) clear_ReadCommandForSPI_Master();",
        "s_tdc_spi_auto_ack = unit == 51;", "s_tdc_spi_auto_ack = saved_auto_ack;"))
    for unit in manifest["units"]:
        target = [case for case in unit["cases"] if case["mode"] == "ram"]
        blocked = bool(unit.get("target_blocked", False))
        if unit["number"] == 51 and spi_completion:
            blocked = False
        out = {"id": unit["id"], "file": unit["file"],
               "target_supported": bool(target), "target_blocked": blocked,
               "target_blocked_reason": unit.get("target_blocked_reason", "") if blocked else "",
               "records": [], "variable_records": [], "pc_only_records": [],
               "pc_output_blocks": build_pc_output_blocks(unit)}
        if blocked:
            out["target_log_classification"] = unit["target_log_classification"]
            out["target_blocked_evidence"] = unit["target_blocked_evidence"]
        for case in unit["cases"]:
            values = expected_values(case)
            deterministic = bool(values) and all(type(value) is int for value in values)
            record = {"case": case["case_id"], "key": case["key"],
                      "mode": "target" if case["mode"] == "ram" else "pc",
                      "source_mode": case["mode"], "source": case["source"],
                      "title": case["title"],
                      "notes": case["input"] + (" " + case["note"] if case["note"] else "")}
            if case["mode"] == "ram":
                require(deterministic, "noninteger target expectation")
                record["expected"] = values
                record["fields"] = [f["name"] for f in case["expected_fields"]]
                record["log_type"] = "target_rtt_report"
                record["notes"] += (" 전체 유닛 출력은 통과 시 비교용이며 현재 타깃 일괄 실행은 차단됨. "
                                     "unit-level target_blocked_reason은 C1..C23 전체에 적용됨." if blocked else
                                     " 타깃 RAM 전제 조건에서 report 지원. 실장치 실행 결과가 아닌 통과 시 비교용.")
                out["records"].append(record)
                continue
            record["log_type"] = ("pc_host_unit_summary" if case["mode"] == "host"
                                  else "pc_extension_summary")
            record["rtt_supported"] = False
            record["expected_kind"] = "deterministic" if deterministic else "variable_or_observation"
            record["notes"] += " 배열은 판정 필드 순서의 사전 기대값이며 PC가 A배열 줄을 출력한다는 뜻이 아님."
            if deterministic:
                # These arrays are contract inputs, not printed PC output.
                # Keep them only in the original manifest/unit input document.
                continue
            else:
                variable = {"case": case["case_id"], "key": case["key"], "mode": "pc",
                            "source_mode": case["mode"], "source": case["source"],
                            "fields": [], "fixed_expected_fields": [], "notes": ""}
                if values:
                    for field in case["expected_fields"]:
                        if type(field["expected"]) is int:
                            variable["fixed_expected_fields"].append(
                                {"index": field["index"], "name": field["name"],
                                 "expected": field["expected"]})
                        else:
                            variable["fields"].append(
                                {"index": field["index"], "name": field["name"],
                                 "range": "0..128회; trace 저장 공간의 범위이며 통과값 범위가 아님",
                                 "reason": "첫 정상 초기화의 쓰기 횟수와 같아야 하는 관계식. "
                                           "사전 절대 횟수는 비교 코드에 없으며 기존 actual에서 가져오지 않음.",
                                 "expected_relation": field["expected"]})
                    variable["notes"] = ("E-02의 7개 비교 중 field5만 관계값. 고정 6필드는 "
                                         "fixed_expected_fields에 원래 인덱스로 보존. RTT 미지원.")
                else:
                    names = ["writes", "reads", "ulp"] if case["case_id"] == 6 else ["ready", "writes"]
                    for name in names:
                        variable["fields"].append(
                            {"name": name, "range": "0 또는 1" if name in ("ulp", "ready") else "0 이상의 횟수",
                             "reason": "오류 입력 후 소프트웨어 관찰값. 사전 성공/안전 판정식이 없어 "
                                       "기존 PC 숫자를 expected로 고정하지 않음."})
                    variable["log_type"] = "pc_observation"
                    variable["output_format"] = (
                        "CM3 OBS unit=2 case=6 all-bus-errors writes=%d reads=%d ulp=%d NOT_PRODUCT_PASS"
                        if case["case_id"] == 6 else
                        "CM3 OBS unit=2 case=12 timeout-with-all-bus-errors ready=%d writes=%d NOT_PRODUCT_PASS")
                    variable["format"] = variable["output_format"]
                    variable["notes"] = ("관찰 전용/판정 보류. PC printf 형식만 지원하며 RTT PASS로 만들지 않음. "
                                         "format의 %d는 소스 포맷 지정자이며 실제 stdout에는 관찰한 정수가 들어간다. "
                                         "관찰값을 고정한 전문이나 literal %d 출력으로 대조하지 않는다.")
                out["variable_records"].append(variable)
                record["variable_record_key"] = case["key"]
            # No per-case PC success/array line exists; the unit-level blocks
            # above are the only deterministic PC stdout exported here.

        observer = unit["observer"]
        out["variable_records"].append(
            {"case": 100, "key": "O-100", "mode": "target_observation",
             "source": observer["source"], "log_type": "target_rtt_observed",
             "fields": [{"index": f["index"], "name": f["name"],
                         "range": f["meaning"] + "; 고정 입력 미지정 시 관찰 당시 int 값",
                         "reason": "읽기 전용 snapshot의 상태값. 관찰 직전 입력과 장치/버퍼 상태에 따라 변하며 "
                                   "기능 expected가 정의되지 않음."} for f in observer["fields"]],
             "payload_count": len(observer["fields"]), "summary": observer["rtt_summary"],
             "notes": "expected=NULL로 관찰만 보고. expected 자리 채움 0을 PASS 기준으로 사용하지 않음. "
                      "O-100은 별도 관찰 API이며 run_unit의 C1..C23 완료를 의미하지 않음."})
        units.append(out)
    return {
        "schema_version": 1, "role": "cm3-io",
        "expected_source": "cm3-io.json의 source-derived expected_fields만 export. actual/실행 로그 미사용.",
        "array_convention": "records.expected는 payload 전 원소이며 [-999,unit,case,N] framing 4원소는 포함하지 않음. "
                            "배열 순서는 비교 field 인덱스 0부터. 모든 정수는 원문 그대로이며 누락/축약 없음.",
        "target_preconditions": "TDC_UNIT_TEST, CFX 실행 허용0, 다른 코어/간섭 IRQ 정지, 시작 시 I2C idle/"
                                "SPI 응답 없음, 기존 결과 해제. 출력 전에 run_unit의 완료/복원 필요. "
                                "CM3-051은 시험용 SPI 완료 처리로 두 응답 사이 RAM 대기를 해소했으나 실제 송수신은 별도 검증.",
        "log_types": {
            "target_rtt_report": {"source": "src/test/tdc_ut_log.c",
                                  "summary_format": "[UT-CM3-%03u] C%u PASS N=%u F=0",
                                  "expected_array_output": True,
                                  "notes": "실제 formatter로 A줄 분할. 숫자를 actual에서 채우지 않음."},
            "target_rtt_observed": {"source": "src/test/tdc_ut_log.c",
                                    "expected_array_output": False,
                                    "notes": "실제 관찰값 없이는 A줄을 정수 전문으로 미리 만들 수 없음. F=0도 무비교이며 PASS 아님."},
            "pc_host_unit_summary": {"source": "tests/unit/tdc_ut_io_cm3_host.c",
                                     "format": "CM3 unit=%d cases=%d words=%d mismatches=%d done=1 overflow=0",
                                     "scope": "unit", "expected_array_output": False,
                                     "notes": "case별 PASS/A줄 없음. words는 framing 포함. 불일치 시 stderr 필드 행만 출력."},
            "pc_extension_summary": {"source": EXT,
                                      "format": "CM3 EXT unit=%d cases=%d comparisons=%d mismatches=%d known=%d HOST_ONLY",
                                      "scope": "unit", "expected_array_output": False,
                                      "notes": "case별 정상 출력 없음. comparisons는 framing 없는 검사 수. 기대값이 모두 일치하면 "
                                               "mismatches=0/known=0 형식이나 known-findings 모드는 기존 실패까지 요구함."},
            "pc_observation": {"source": EXT, "expected_array_output": False,
                               "notes": "CM3-002 E-06/E-12의 구현된 printf 형식. NOT_PRODUCT_PASS 유지."}
        },
        "units": units,
        "counts": {"target_records": 79, "target_payload_integers": 456,
                   "target_blocked_units": [u["id"] for u in units if u["target_blocked"]],
                   "blocked_unit_records": sum(len(u["records"]) for u in units if u["target_blocked"]),
                   "pc_cases": 164, "pc_deterministic_records": 0,
                   "pc_deterministic_integers": 0,
                   "pc_output_blocks": 33, "pc_stdout_array_lines": 0,
                   "pc_deterministic_input_cases_in_unit_docs": 161,
                   "pc_deterministic_input_fields_in_unit_docs": 949,
                   "pc_relational_cases": 1,
                   "pc_relational_fixed_fields": 6, "pc_relational_fields": 1,
                   "pc_observation_cases": 2, "observer_snapshots": 20},
        "limitations": ["PC 사례별 기대배열은 원래 cm3-io.json/유닛 입력 문서에만 보존한다. PC 출력에 없는 배열을 로그로 내보내지 않음.",
                        "pc_output_blocks는 --check-known-findings를 제외한 정상 계약 모드의 조건부 전 비교 일치 요약행이다. 해당 모드의 현재 실행/합격 기록이 아님.",
                        "C17/C18의 [195,0]은 PC 모형의 관찰이지 타깃 완료 결과가 아님.",
                        "CM3-051 전체 C1..C23 전문은 통과 시 비교용. 현재 시험 경계 보완은 실제 장치 통과 결과가 아님.",
                        "다른 유닛의 target_blocked=false는 정적 한계 미발견 표기이며 실장치 검증 완료가 아님."]
    }


def validate_pc_output_blocks(manifest: dict, exported: dict) -> dict:
    host = (ROOT / "tests/unit/tdc_ut_io_cm3_host.c").read_text(encoding="utf-8")
    ext = (ROOT / EXT).read_text(encoding="utf-8")
    host_format = re.search(r'printf\("(CM3 unit=%d cases=%d words=%d mismatches=%d done=1 overflow=0\\n)"', host)
    ext_format = re.search(r'printf\("(CM3 EXT unit=%d cases=%d comparisons=%d mismatches=%d known=%d HOST_ONLY\\n)"', ext)
    require(host_format is not None and ext_format is not None, "source summary printf format changed")
    host_format = host_format.group(1).replace("\\n", "\n")
    ext_format = ext_format.group(1).replace("\\n", "\n")
    compact = re.sub(r"\s+", "", ext)
    require("if(actual!=expected)s_tdc_known++;" in compact, "known counter semantics changed")
    require("assert(actual==known_actual&&known_actual!=expected);" in compact, "known-mode oracle changed")
    counts = Counter()
    for original, output in zip(manifest["units"], exported["units"]):
        blocks = output["pc_output_blocks"]
        block_index = 0
        for mode in ("host", "ram", "host_extension"):
            cases = [c for c in original["cases"] if c["mode"] == mode]
            if not cases:
                continue
            payload = sum(len(c["expected_fields"]) for c in cases)
            args = (original["number"], len(cases), payload, 0, 0) if mode == "host_extension" else (
                original["number"], len(cases), payload + 4 * len(cases), 0)
            text = (ext_format if mode == "host_extension" else host_format) % args
            require(blocks[block_index]["text"] == text, "PC block differs from source printf")
            require("--check-known-findings를 사용하지 않는" in blocks[block_index]["notes"], "mode unspecified")
            require(text.endswith("\n") and len(text.splitlines()) == 1, "PC summary line boundary")
            require("PASS" not in text and "A[" not in text and "%d" not in text, "invented PC output")
            block_index += 1
            counts[mode] += 1
        require(len(blocks) == block_index, "missing/duplicate PC output blocks")
        require(output["pc_only_records"] == [], "unprinted PC case arrays still exported")
    require(dict(counts) == {"host": 13, "ram": 12, "host_extension": 8}, "PC block mode counts")
    observed = [r for u in exported["units"] for r in u["variable_records"] if r.get("log_type") == "pc_observation"]
    require(len(observed) == 2, "PC observation formats")
    for record in observed:
        require(record["format"] == record["output_format"], "observation format aliases")
        source_format = record["format"].replace(f"case={record['case']} ", "case=%d ")
        require(source_format in ext, "invented observation printf")
        require("%d" in record["format"] and "NOT_PRODUCT_PASS" in record["format"], "observation made static/pass")
    return {"blocks": sum(counts.values()), "modes": dict(counts), "source_printf_exact_match": True,
            "known_counts_only_mismatches": True, "pc_case_array_stdout_lines": 0, "observed_formats": 2}


def validate_log_export(manifest: dict, exported: dict) -> dict:
    require(exported == build_log_export(manifest), "log export differs from source expectations")
    units = exported["units"]
    records = [r for u in units for r in u["records"]]
    pc_inputs = [c for u in manifest["units"] for c in u["cases"] if c["mode"] != "ram"]
    deterministic_pc = [c for c in pc_inputs if c["expected_fields"]
                        and all(type(v) is int for v in expected_values(c))]
    require(len(records) == 79 and sum(len(r["expected"]) for r in records) == 456, "target export count")
    require(len(pc_inputs) == 164 and len(deterministic_pc) == 161, "PC input scope preserved")
    require(sum(len(c["expected_fields"]) for c in deterministic_pc) == 949, "PC input fields preserved")
    require(all(type(n) is int for r in records for n in r["expected"]), "noninteger export")
    pc_output_check = validate_pc_output_blocks(manifest, exported)
    unit51 = next(u for u in units if u["id"] == "CM3-051")
    require(len(unit51["records"]) == 23, "unit51 records must cover whole run")
    require([r["expected"] for r in unit51["records"] if r["case"] in (17, 18)] == [[240, 3], [240, 4]],
            "OTA contract expectation replaced by PC actual")
    # No observed actual values feed any exported expected array.
    changed_actual = copy.deepcopy(manifest)
    for unit in changed_actual["units"]:
        unit["saved_logs"] = {"host": ["DO_NOT_USE_ACTUAL"], "ram": ["DO_NOT_USE_ACTUAL"]}
        for mismatch in unit["known_mismatches"]:
            mismatch["actual"] = -2147483648
    require(build_log_export(changed_actual) == exported, "export depends on actual/logs")
    source_oracle(manifest)
    source = (ROOT / SOURCE).read_text(encoding="utf-8")
    target_guard = source.split("int tdc_ut_io_cm3_run_unit(int unit)", 1)[1].split("#if", 1)[0]
    supported = {int(n) for n in re.findall(r"unit == (\d+)", target_guard)}
    require({int(u["id"].split("-")[1]) for u in units if u["records"]} == supported, "target source support")
    return {"status": "PASS", **exported["counts"], "all_expected_values_match_manifest": True,
            "pc_output_validation": pc_output_check,
            "actual_log_mutation_does_not_change_export": True, "product_or_device_run": False}

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--export-logs", action="store_true", help="Print expected log JSON; writes no files")
    parser.add_argument("--export-unit", type=int, help="With --export-logs, print one unit for bounded transfer")
    parser.add_argument("--check-logs", action="store_true", help="Verify saved cm3-io-logs.json")
    args = parser.parse_args()
    manifest = read_json(MANIFEST)
    result = validate(manifest)
    result["in_memory_negative_checks_rejected"] = self_check_oracle(manifest)
    if args.export_logs:
        exported = build_log_export(manifest)
        validate_log_export(manifest, exported)
        if args.export_unit is not None:
            require(args.export_unit in IDS, "unknown export unit")
            exported["units"] = [u for u in exported["units"] if u["id"] == f"CM3-{args.export_unit:03d}"]
        print(json.dumps(exported, ensure_ascii=False, separators=(",", ":")))
        return
    if args.check_logs:
        result["log_export"] = validate_log_export(manifest, read_json(MANIFEST.with_name("cm3-io-logs.json")))
    print(json.dumps(result, ensure_ascii=True, indent=2))


if __name__ == "__main__":
    main()
