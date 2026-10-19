"""Exercise real SYS54/SYS77/UI with in-memory FatFs and software CRC boundary."""
from __future__ import annotations
import argparse
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
                            encoding="utf-8", errors="replace", timeout=90)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return result.stdout


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--optimization", choices=("O0", "O2"), default="O0")
    args = parser.parse_args()
    compiler = shutil.which("gcc")
    if not compiler:
        raise SystemExit("Host GCC is required")
    sdk = Path(os.environ.get("TDC_E8300_SDK", str(existing.SDK_DEFAULT)))
    sources = [existing.CM3 / p for p in existing.SOURCES if "/test/" not in p]
    sources += [existing.CM3 / p for p in (
        "Cortex-M3-src/test/tdc_sys_test_storage.c", "Cortex-M3-src/test/tdc_sys_test_54.c",
        "Cortex-M3-src/test/tdc_sys_test_77.c", "Cortex-M3-src/test/tdc_sys_test_73.c",
        "Gen1_5/FS/ci_event_log.c", "Gen1_5/FS/ci_filesystem.c", "Gen1_5/tiny-AES-c/aes.c")]
    sources += [Path(__file__).with_name("sys_storage_host.c")]
    inputs = sources + list(existing.CM3.rglob("*.h")) + [Path(__file__),
        Path(__file__).with_name("sys_storage_host_prelude.h"), ROOT / "tests/unit/tdc_ut_cm3_host_prelude.h"]
    before = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    out = ROOT / "tmp/system-tests" / ("storage" if args.optimization == "O0" else "storage-O2")
    out.mkdir(parents=True, exist_ok=True)
    report = {"complete": False, "device_executed": False, "system_test_passed": False,
              "optimization": args.optimization,
              "scope": "Real UI, SYS54 parser and SYS77 production CRC/AES file wrapper; RAM FatFs and CRC hardware substitute",
              "inputs": before, "cases": {}}
    record = out / "host-checks.json"
    record.write_text(json.dumps(report, indent=2), encoding="utf-8")
    flags = ["-std=gnu11", "-DSK5_CID=101", "-Dchess_storage(a)=", "-ffunction-sections", "-fdata-sections",
             "-fmax-errors=4", "-DTDC_UNIT_TEST", "-DTDC_UT_CM3_HOST", "-" + args.optimization, "-fno-asynchronous-unwind-tables",
             "-fno-unwind-tables", "-DTDC_SYS_TEST_7_ENABLE=0", "-DTDC_SYS_TEST_23_ENABLE=0",
             "-DTDC_SYS_TEST_AUDIT_ENABLE=0", "-DTDC_SYS_TEST_54_ENABLE=1", "-DTDC_SYS_TEST_73_ENABLE=1",
             "-DTDC_SYS_TEST_77_ENABLE=1", "-DTDC_SYS_TEST_80_ENABLE=0", "-include",
             str(Path(__file__).with_name("sys_storage_host_prelude.h"))]
    flags += ["-I" + str(existing.CM3 / p) for p in existing.INCLUDES]
    for part in ("include/cm3", "include/shared"):
        flags += ["-isystem", str(sdk / part)]
    objects = []
    for i, source in enumerate(sources):
        obj = out / f"{i:02}.o"
        run([compiler, *flags, "-c", str(source), "-o", str(obj)], out)
        objects.append(str(obj))
    binary = out / "storage.exe"
    run([compiler, *objects, "-Wl,--gc-sections,--exclude-all-symbols", "-o", str(binary)], out)
    cases = ("normal", "existing", "write_error", "read_short", "image", "header", "data", "truncated", "missing_section",
             "encrypted", "volatile", "guards", "backpressure", "invalid", "audit_open", "audit_init", "audit_led",
             "create_sync", "create_close", "write_sync", "write_close", "raw_open", "raw_close", "read_close",
             "read_error", "remove_error", "diagnostics", "queue_limit")
    for case in cases:
        stdout = run([str(binary), case], out)
        if "HOST_PASS" not in stdout:
            raise AssertionError("Missing host completion evidence")
        (out / f"{case}.log").write_text(stdout, encoding="utf-8")
        report["cases"][case] = stdout
        print(stdout.splitlines()[-1])
    after = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    if before != after:
        raise AssertionError("Inputs changed during verification; rerun after edits finish")
    report.update(complete=True, inputs_unchanged=True, binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                  crc_known_vector={"input": "123456789", "ccitt_false": "29B1"},
                  wrapper_product_return_preserved=True, max_rtt_line_bytes=60, queue_capacity=64,
                  fs_failure_policy="retain scratch; reject further FS until reboot; status/stop allowed",
                  direct_diagnostics="observed outside HOLD; must not occur inside HOLD")
    record.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
