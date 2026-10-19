"""Exercise actual SYS-073 and audit/error source files, using RAM IO only.

Every case runs in a fresh PC process. Recognized failures, known product gaps,
and the missing direct write/sync boundary remain separate. This harness never
loads a board, writes the audit file, or certifies product LED/stop behavior.
Normal-path OFF preprocessing and complete SDK integration belong to main.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "tests/unit"))
import build_target_tests as target

CM3 = ROOT / "src/2__cm3"
OUT = ROOT / "tmp/system-tests/sys73"
FAULTS = ("open", "read", "short-read", "write", "short-write", "sync", "close")
# Independent expected observations of the current implementation, not lowered
# SYS-073 acceptance criteria. Requirement is always ret=-1 and error=10/detail.
OBSERVED = {
    "init": ((-1, 0, 1), (0, 0, 1), (0, 0, 1), (-1, 0, 1), (-1, 0, 1), (0, 0, 1), (0, 0, 1)),
    "write": ((-1, 1, 1), (-1, 3, 1), (-1, 3, 1), (-1, 2, 1), (-1, 2, 1), (0, 0, 0), (0, 0, 1)),
}
DETAILS = (1, 3, 3, 2, 2, 2, 2)


def run(command: list[str], transcript: Path) -> str:
    result = subprocess.run(command, cwd=ROOT, capture_output=True, timeout=45)
    transcript.write_bytes(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(transcript.name + ": " + result.stderr.decode("utf-8", errors="replace"))
    # Preserve CRLF before validating the emitted RTT lines.
    return result.stdout.decode("utf-8")


def main() -> None:
    compiler = shutil.which("gcc")
    if compiler is None:
        raise RuntimeError("Host GCC is required on PATH")
    OUT.mkdir(parents=True, exist_ok=True)
    record = OUT / "checks.json"
    report = {"status": "RUNNING", "device_executed": False, "product_system_test_passed": False}
    record.write_text(json.dumps(report, indent=2), encoding="utf-8")
    sources = [CM3 / "Gen1_5/FS/ci_event_log.c", CM3 / "Cortex-M3-src/test/tdc_sys_test_73.c",
               CM3 / "Cortex-M3-src/error.c", HERE / "sys73_host.c"]
    inputs = sources + [p.with_suffix(".h") for p in sources[:3]] + [
        CM3 / "processorDirective.h", CM3 / "Gen1_5/FS/ci_filesystem.h",
        CM3 / "Gen1_5/common/ci_timer.h", CM3 / ".cproject", Path(__file__),
        HERE / "sys73_host_prelude.h", ROOT / "tests/unit/build_target_tests.py",
        ROOT / "tests/unit/tdc_ut_cm3_host_prelude.h", target.SDK / "include/cm3/ff.h",
        target.SDK / "include/cm3/ffconf.h"]
    before = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs}
    _, includes = target.configuration(CM3, "CM3")
    flags = ["-std=gnu11", "-DSK5_CID=101", "-Dchess_storage(a)=", "-ffunction-sections",
             "-fdata-sections", "-fno-asynchronous-unwind-tables", "-fno-unwind-tables",
             "-DTDC_UNIT_TEST", "-DTDC_UT_CM3_HOST", "-DTDC_SYS_TEST_73_ENABLE=1",
             "-I" + str(ROOT / "tests/unit"), "-I" + str(CM3 / "Cortex-M3-src/test"),
             *["-I" + str(path) for path in includes],
             "-isystem", str(target.SDK / "include/cm3"),
             "-isystem", str(target.SDK / "include/shared"),
             "-include", str(HERE / "sys73_host_prelude.h")]
    runs = []
    for optimization in ("O0", "O2"):
        objects = []
        for index, source in enumerate(sources):
            obj = OUT / f"{optimization}_{index}.o"
            run([compiler, *flags, "-" + optimization, "-c", str(source), "-o", str(obj)],
                OUT / f"{optimization}_{index}_build.log")
            objects.append(str(obj))
        executable = OUT / f"sys73_{optimization}.exe"
        run([compiler, *objects, "-Wl,--gc-sections,--exclude-all-symbols", "-o", str(executable)],
            OUT / f"{optimization}_link.log")
        cases = []
        for operation, expectations in OBSERVED.items():
            for index, (fault, observed) in enumerate(zip(FAULTS, expectations, strict=True)):
                output = run([str(executable), operation, fault], OUT / f"{optimization}_{operation}_{fault}.log")
                actual_return, error, reached = observed
                verdict = "NOT_REACHED" if not reached else "MATCH" if error else "GAP"
                io_result = 0 if not reached or fault.startswith("short-") else 1
                expected_lines = [
                    f"SRC=RAM_IO OP={operation} FAULT={fault}",
                    f"RET={actual_return} EXPECT=-1",
                    f"ERROR={10 if error else 0}/{error} EXPECT=10/{DETAILS[index]}",
                    f"FAULT_REACHED={reached} IO_RES={io_result}", f"CHECK={verdict}",
                    "OBS=PRODUCT_ERROR" if error else "OBS=NO_PRODUCT_ERROR",
                    "PRODUCT_DIAG=SUPPRESSED", "NVM=UNTOUCHED TEST_HOLD=1",
                    "PRODUCT_STOP=NOT_PROVEN", "RESULT=NOT_SYSTEM_PASS"]
                lines = [line for line in output.splitlines(keepends=True) if line.startswith("[SYS-073]")]
                assert lines == [f"[SYS-073] {body}\r\n" for body in expected_lines], (operation, fault, output)
                assert all(len(line.encode("ascii")) <= 60 for line in lines)
                assert "HOST_OK lines=10 " in output and f"ACTUAL_DETAIL={error}\n" in output
                cases.append({"operation": operation, "fault": fault, "return": actual_return,
                              "error_detail": error, "fault_reached": reached, "check": verdict,
                              "required_return": -1, "required_error_detail": DETAILS[index],
                              "rtt_lines": expected_lines})
        for action, count in (("usage", 0), ("boundary", 1), ("led", 5)):
            output = run([str(executable), action], OUT / f"{optimization}_{action}.log")
            assert f"HOST_OK lines={count} " in output
            lines = [line for line in output.splitlines(keepends=True) if line.startswith("[SYS-073]")]
            assert len(lines) == count and all(len(line.encode("ascii")) <= 60 for line in lines)
            if action == "led":
                assert "OBS=LED_DEMO_ONLY\r\n" in output and "CHECK=MATCH" not in output
        if runs:
            assert cases == runs[0]["cases"], "Optimization-dependent result"
        runs.append({"optimization": optimization, "cases": cases})
    after = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs}
    assert after == before, "Inputs changed while checking; rerun on a stable tree"
    for path in sources[:2] + [path.with_suffix(".h") for path in sources[:2]] + [sources[-1], HERE / "sys73_host_prelude.h"]:
        text = path.read_text(encoding="utf-8-sig")
        assert text.encode("cp949").decode("cp949") == text, path.name
    report.update(status="HARNESS_OK_WITH_KNOWN_GAPS", fault_cases=14, matched=5,
                  known_gaps=8, not_reached=1, auxiliary_cases=3, max_rtt_bytes=60,
                  inputs=before, runs=runs,
                  unverified=["device LED/stop", "common preflight/HOLD/UI", "FatFs/NVM durability",
                              "normal-path OFF preprocess", "complete SDK integration"])
    record.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("SYS73 HOST OK: 14 faults + 3 auxiliary; O0/O2 agree")
    print("5 MATCH, 8 known GAP, 1 NOT_REACHED; RTT <=60 bytes")
    print("Actual audit/error sources; no device/system PASS")
    print("REPORT tmp/system-tests/sys73/checks.json")


if __name__ == "__main__":
    main()
