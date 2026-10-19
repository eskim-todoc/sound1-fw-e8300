"""Run host regressions and optionally isolated normal/test SDK links.

Exit zero means the harness behaved as expected, including reported known
criteria mismatches. It does NOT mean all product criteria passed.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "tmp/unit-tests/suite"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", action="store_true", help="also link all three cores with/without test macro")
    parser.add_argument("--preserve-source", type=Path,
                        help="pre-work snapshot; require product source unchanged before and after tests")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    # Invalidate a previous successful run before starting this one. Persist
    # partial progress too, so a timeout/interrupt cannot look like old success.
    results = []
    def save_progress(status):
        (OUT / "checks.json").write_text(json.dumps({
            "device_execution": False, "meaning": "harness checks, not product acceptance",
            "status": status, "results": results,
        }, ensure_ascii=False, indent=2), encoding="utf-8")
    save_progress("RUNNING")
    guard = ([sys.executable, "tests/unit/check_source_preservation.py", str(args.preserve_source),
              "--output", str(OUT / "source-preservation.json")] if args.preserve_source else None)
    if guard:
        # A stale/mismatched baseline stops execution before any build/test.
        subprocess.run(guard, cwd=ROOT, check=True)
    commands = [
        ("suite-runner-checker", [sys.executable, "tests/unit/check_suite_runner_test.py"]),
        ("source-preservation-checker", [sys.executable, "tests/unit/check_source_preservation_test.py"]),
        ("log-build", ["gcc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-DTDC_UNIT_TEST", "tests/unit/tdc_ut_log_test.c", "src/test/tdc_ut_log.c", "-o", str(OUT / "log.exe")]),
        ("log", [str(OUT / "log.exe")]),
        ("report-build", ["gcc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-DTDC_UNIT_TEST", "-Itests/unit/tdc_ut_report_host", "tests/unit/tdc_ut_report_test.c", "-o", str(OUT / "report.exe")]),
        ("report", [str(OUT / "report.exe")]),
        ("boot-report-build", ["gcc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-DTDC_UNIT_TEST", "-Itests/unit/tdc_ut_report_host", "-Itests/unit/tdc_ut_boot_mock", "-Isrc/0__bootloader/include", "tests/unit/tdc_ut_boot_report_test.c", "-o", str(OUT / "boot-report.exe")]),
        ("boot-report", [str(OUT / "boot-report.exe")]),
    ]
    commands += [(name.removesuffix(".py"), [sys.executable, "tests/unit/" + name]) for name in
                 ["check_rtt_wrappers.py", "check_cfx_analysis.py", "check_cfx_vectors.py", "check_cfx_numeric_reference.py", "tdc_ut_hear_reference_check.py", "tdc_ut_cfx_io_check.py", "tdc_ut_cm3_run.py", "tdc_ut_boot_check.py"]]
    commands += [("cm3-io", [sys.executable,"tests/unit/tdc_ut_io_cm3_build.py","--check-known-findings"]),
                 ("cm3-io-ram", [sys.executable,"tests/unit/tdc_ut_io_cm3_build.py","--ram","--check-known-findings"])]
    commands += [("cm3-state", [sys.executable,"tests/unit/check_cm3_state.py"]),
                 ("cm3-extra", [sys.executable,"tests/unit/tdc_ut_cm3_extra_run.py"]),
                 ("event-log", [sys.executable,"tests/unit/check_event_log.py"]),
                 ("event-log-checker", [sys.executable,"tests/unit/check_event_log_test.py"]),
                 ("remaining-analysis", [sys.executable,"tests/unit/check_remaining_analysis.py"]),
                 ("coverage", [sys.executable,"tests/unit/build_coverage.py"])]
    commands += [(name.removesuffix(".py"), [sys.executable, "tests/unit/" + name]) for name in
                 ["check_test_console.py", "check_collect_rtt.py", "check_cm3_batch.py",
                  "check_boot_batch.py", "check_cfx_batch.py", "check_console_adapters.py",
                  "check_prepare_batch_debug.py"]]
    if args.sdk:
        commands += [(f"{core}-{'normal' if normal else 'test'}", [sys.executable, "tests/unit/build_target_tests.py", core, *( ["--normal"] if normal else [])])
                     for core in ("CFX", "CM3", "BOOT") for normal in (False, True)]
        commands += [("target-artifacts",[sys.executable,"tests/unit/check_target_artifacts.py"])]
        commands += [("stack-budget",[sys.executable,"tests/unit/check_stack_budget.py"])]
    if guard:
        commands.append(("source-preservation", guard))
    build_dependencies = {"log": "log-build", "report": "report-build", "boot-report": "boot-report-build"}
    for name, command in commands:
        began = time.monotonic()
        dependency = build_dependencies.get(name)
        if dependency and any(r["name"] == dependency and r["returncode"] for r in results):
            # Never execute a previous binary after this run's compilation failed.
            run = subprocess.CompletedProcess(command, 1, "", "Skipped: current build failed: " + dependency)
        else:
            try:
                run = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=900)
            except subprocess.TimeoutExpired as exc:
                def decoded(value):
                    return value.decode("utf-8", errors="replace") if isinstance(value, bytes) else (value or "")
                run = subprocess.CompletedProcess(command, 124, decoded(exc.stdout), decoded(exc.stderr) + "\nHARNESS TIMEOUT after 900 seconds\n")
            except OSError as exc:
                run = subprocess.CompletedProcess(command, 127, "", "HARNESS START ERROR: " + str(exc))
        output = run.stdout + run.stderr
        (OUT / (name + ".log")).write_text(output, encoding="utf-8")
        results.append({"name": name, "command": command, "returncode": run.returncode, "seconds": round(time.monotonic()-began, 3), "log": str((OUT / (name + ".log")).relative_to(ROOT))})
        save_progress("RUNNING")
        print(f"{name}: {'HARNESS_OK' if run.returncode == 0 else 'HARNESS_ERROR'}", flush=True)
        if run.returncode:
            print(output[-6000:], flush=True)
    sources = list((ROOT / "tests/unit").rglob("*"))
    for folder in ("src/test", "src/0__bootloader/source/test", "src/1__cfx/test",
                   "src/2__cm3/source/tests"):
        sources += list((ROOT / folder).rglob("*"))
    result = {"device_execution": False, "meaning": "harness checks, not product acceptance",
              "status": "HARNESS_ERROR" if any(r["returncode"] for r in results) else "HARNESS_OK", "results": results,
              "sources": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in sources if p.is_file() and p.suffix in (".py", ".c", ".h", ".json", ".ps1")}}
    (OUT / "checks.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    if any(r["returncode"] for r in results):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
