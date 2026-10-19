"""Run the real audit display and UI parser on PC, never a board or audit file."""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests/unit"))
import tdc_ut_cm3_run as existing


def run(command: list[str], directory: Path) -> str:
    result = subprocess.run(command, cwd=directory, capture_output=True, text=True,
                            encoding="utf-8", errors="replace", timeout=60)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return result.stdout


def main() -> None:
    compiler = shutil.which("gcc")
    if not compiler:
        raise SystemExit("Host GCC is required")
    sdk = Path(os.environ.get("TDC_E8300_SDK", str(existing.SDK_DEFAULT)))
    sources = [existing.CM3 / p for p in existing.SOURCES if "/test/" not in p]
    sources += [existing.CM3 / "Cortex-M3-src/test/tdc_sys_test_audit.c",
                Path(__file__).with_name("sys_audit_host.c")]
    inputs = sources + list(existing.CM3.rglob("*.h")) + [Path(__file__),
        ROOT / "tests/unit/tdc_ut_cm3_run.py", ROOT / "tests/unit/tdc_ut_cm3_host_prelude.h"]
    before = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    out = ROOT / "tmp/system-tests/audit"
    out.mkdir(parents=True, exist_ok=True)
    report = {"complete": False, "device_executed": False, "product_system_test_passed": False,
              "inputs": before, "variants": {}}
    record = out / "host-checks.json"
    record.write_text(json.dumps(report, indent=2), encoding="utf-8")
    for guards in (0, 1):
        flags = ["-std=gnu11", "-DSK5_CID=101", "-Dchess_storage(a)=", "-ffunction-sections",
                 "-fdata-sections", "-fmax-errors=5", "-DTDC_UNIT_TEST", "-O0", "-fno-asynchronous-unwind-tables",
                 "-fno-unwind-tables", "-DTDC_UT_CM3_HOST", "-DTDC_SYS_TEST_AUDIT_ENABLE=1",
                 f"-DTDC_SYS_TEST_7_ENABLE={guards}", f"-DTDC_SYS_TEST_23_ENABLE={guards}", "-include",
                 str(ROOT / "tests/unit/tdc_ut_cm3_host_prelude.h")]
        flags += ["-I" + str(existing.CM3 / p) for p in existing.INCLUDES]
        for part in ("include/cm3", "include/shared"):
            flags += ["-isystem", str(sdk / part)]
        objects = []
        for i, source in enumerate(sources):
            obj = out / f"guards_{guards}_{i:02}.o"
            run([compiler, *flags, "-c", str(source), "-o", str(obj)], out)
            objects.append(str(obj))
        binary = out / f"guards_{guards}.exe"
        run([compiler, *objects, "-Wl,--gc-sections,--exclude-all-symbols", "-o", str(binary)], out)
        stdout = run([str(binary)], out)
        (out / f"guards_{guards}.log").write_text(stdout, encoding="utf-8")
        if "HOST_PASS" not in stdout:
            raise AssertionError("Missing host completion evidence")
        report["variants"][str(guards)] = {"stdout": stdout,
            "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest()}
        print(stdout.splitlines()[-1])
    after = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    if before != after:
        raise AssertionError("Inputs changed during verification")
    report.update(complete=True, inputs_unchanged=True)
    record.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(record)


if __name__ == "__main__":
    main()
