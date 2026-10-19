"""Run the unchanged audit-log source against RAM FatFs/time boundaries.

Checks storage failure reporting for SW-FW-RID-73/74 and record formation.
Does not validate QCC authentication, clock trust, encryption or actual NVM.
"""
from __future__ import annotations
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import traceback

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "tmp/unit-tests/event-log"
FS = ROOT / "src/2__cm3/source/fs"
SOURCES = [FS / "tdc_fs_event_log.c", FS / "tdc_fs_event_log.h",
           ROOT / "src/2__cm3/source/hal/tdc_hal_timer.h",
           ROOT / "src/2__cm3/source/sys/tdc_sys_error.h",
           ROOT / "tests/unit/tdc_ut_event_log_host.c"]

# 입력 조건에서 정한 전체 비교 목록이다. 출력 레코드가 누락되거나
# 중복되거나, fixture의 기대값 자체가 바뀌어도 통과시키지 않는다.
EXPECTED = {
    1: dict(return_=0, ident=1, count=0, index=0, file_bytes=4096, close_count=1),
    2: dict(return_=0, no_rewrite=0, preserved=1),
    3: dict(return_=0, index=0, count=0, ident=1),
    4: dict(return_=0, index=0, count=0, ident=1),
    5: dict(return_=0, index=0, count=0, ident=1),
    6: dict(return_=-1, no_close_unopened=0),
    7: dict(return_=-1, close_count=1),
    8: dict(return_=-1, error_major=10, error_minor=1, close_count=0),
    9: dict(return_=-1, error_major=10, error_minor=3, close_count=1, no_write=0),
    10: dict(return_=-1, error_major=10, error_minor=3, close_count=1, no_write=0),
    11: dict(return_=-1, error_major=10, error_minor=2, close_count=1),
    12: dict(return_=-1, error_major=10, error_minor=2, close_count=1),
    13: dict(return_=-1, error_major=10, error_minor=4, close_count=1, no_write=0),
    14: dict(return_=-1, error_major=10, error_minor=4, close_count=1, no_write=0),
    15: dict(return_=-1, error_major=10, error_minor=4, close_count=1, no_write=0),
    16: dict(return_=0, count=1, index=1, event=64, time=1, address=1, close_count=1, no_error=0),
    17: dict(return_=0, count=2, index=2, first_event=64, second_event=50),
    18: dict(return_=0, index_wrap=0, count_saturates=255, last_event=240, first_preserved=64),
    19: dict(return_=0, index=1, count=255, oldest_replaced=50),
    20: dict(address_zero=1),
    21: dict(return_=0, no_write=0),
    22: dict(return_=-1, close_count=1),
    23: dict(return_=-1, no_write=0),
    24: dict(return_=-1),
    25: dict(return_=-1),
}
EXPECTED_FIELDS = {(case, field.removesuffix("_")): expected
                   for case, fields in EXPECTED.items() for field, expected in fields.items()}


def check_records(records: list[dict]) -> list[dict]:
    indexed = {(r["case"], r["field"]): r for r in records}
    assert len(indexed) == len(records) == len(EXPECTED_FIELDS), "Missing/duplicate comparison"
    assert indexed.keys() == EXPECTED_FIELDS.keys(), "Changed comparison fields"
    for key, expected in EXPECTED_FIELDS.items():
        assert indexed[key]["expected"] == expected, (key, "Changed oracle")
    failures = [r for r in records if r["actual"] != r["expected"]]
    assert failures == [{"case": case, "field": "return", "actual": 0, "expected": -1}
                        for case in (24, 25)], failures
    return failures


def declaration(text: str, kind: str, name: str) -> str:
    match = re.search(r"typedef\s+" + kind + r"\s*\{[^}]*\}\s*" + name + r"\s*;", text)
    if not match:
        raise ValueError("Missing original declaration: " + name)
    return match[0]


def without_includes(text: str) -> str:
    return re.sub(r"^\s*#include[^\n]*", "", text, flags=re.MULTILINE)


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    # A failed new run must not leave an earlier successful result as evidence.
    (OUT / "checks.json").write_text(json.dumps({"status": "RUNNING", "device_execution": False}), encoding="utf-8")
    raw = {p: p.read_bytes() for p in SOURCES}
    source, header, timer, tdc_sys_error = [raw[p].decode("utf-8-sig") for p in SOURCES[:4]]
    sdk = Path(os.environ.get("TDC_E8300_SDK", "C:/Program Files (x86)/ON Semiconductor/Ezairo 8300 SDK"))
    config = sdk / "include/cm3/ffconf.h"
    config_bytes = config.read_bytes()
    sector = re.search(rb"^#define\s+FF_MAX_SS\s+(\d+)", config_bytes, re.MULTILINE)
    assert sector and int(sector[1]) == 4096, "Review changed FatFs sector/record capacity"
    types = "#define FF_MAX_SS 4096\n" + declaration(timer, "struct", "tdc_hal_timer_time_t") + "\n"
    for name in ("tdc_sys_error_data_logging_error_t", "tdc_sys_error_major_t"):
        types += declaration(tdc_sys_error, "enum", name) + "\n"
    types += without_includes(header)
    (OUT / "event_log_types.inc").write_text(types, encoding="utf-8")
    (OUT / "event_log_product.inc").write_text(without_includes(source), encoding="utf-8")
    runs = []
    for optimization in ("-O0", "-O2"):
        exe = OUT / ("event_log_" + optimization[1:] + ".exe")
        command = ["gcc", "-std=c11", optimization, "-Wall", "-Wextra", "-Wno-unused-variable",
                   "-Wno-sign-compare", "-Wno-type-limits", "-I", str(OUT), str(SOURCES[4]), "-o", str(exe)]
        build = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, encoding="utf-8", timeout=60)
        (OUT / (optimization[1:] + "-build.log")).write_text(build.stdout + build.stderr, encoding="utf-8")
        if build.returncode:
            raise RuntimeError(build.stderr)
        result = subprocess.run([str(exe)], cwd=OUT, capture_output=True, text=True, encoding="utf-8", timeout=20)
        (OUT / (optimization[1:] + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        records = [json.loads(line[7:]) for line in result.stdout.splitlines() if line.startswith("RECORD ")]
        failures = check_records(records)
        if runs:
            assert records == runs[0]["records"], "Optimization-dependent result"
        runs.append({"optimization": optimization, "command": command, "records": records,
                     "known_product_failures": failures})
    for path, data in raw.items():
        assert path.read_bytes() == data, "Source changed during audit-log test"
    report = {"status": "HARNESS_OK_WITH_KNOWN_FAILURES", "device_execution": False, "acceptance_report": False,
              "scope": "actual audit-log function bodies; RAM FatFs and fixed time; host only",
              "requirements": ["SW-FW-RID-73", "SW-FW-RID-74"],
              "cases": 25, "comparison_records": len(runs[0]["records"]),
              "known_failure_records": 2, "runs": runs,
              "sources": {p.relative_to(ROOT).as_posix(): hashlib.sha256(b).hexdigest() for p, b in raw.items()},
              "sdk_sector_config": {"path": str(config), "sha256": hashlib.sha256(config_bytes).hexdigest()},
              "remaining": ["real FatFs flush/NVM durability", "clock origin/range validation",
                            "connection/disconnection caller reachability", "QCC authorization/transport"]}
    (OUT / "checks.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"EVENT_LOG_HOST 25 cases, {report['comparison_records']} comparisons; "
          "2 known failure records; O0/O2 agree; no product writes/device execution")


if __name__ == "__main__":
    try:
        main()
    except Exception:
        OUT.mkdir(parents=True, exist_ok=True)
        (OUT / "checks.json").write_text(json.dumps({"status": "HARNESS_ERROR", "device_execution": False,
                                                    "tdc_sys_error": traceback.format_exc()}, indent=2), encoding="utf-8")
        raise
