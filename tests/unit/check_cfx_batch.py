"""Build/run the real CFX mailbox + CM3 relay against strict host fixtures.

No hardware execution and no DSP/HEAR calculation verdict. The target SDK
build is deliberately separate; CFX int/pointer widths are not host widths.
"""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "tmp/unit-tests/cfx-batch"
SOURCES = [
    "src/1__cfx/test/tdc_ut_batch_cfx.c",
    "src/2__cm3/source/tests/tdc_ut_batch_cfx_bridge.c",
    "src/2__cm3/source/tests/tdc_ut_report.c",
    "tests/unit/cfx_batch_fixture/protocol_test.c",
]


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    report = {"status": "RUNNING", "device_execution": False, "steps": []}
    report_path = OUT / "checks.json"
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    try:
        compiler = shutil.which("gcc")
        if not compiler:
            raise RuntimeError("gcc not found on PATH")
        binary = OUT / "protocol.exe"
        commands = [[compiler, "-std=c99", "-Wall", "-Wextra", "-Werror", "-O2",
                     "-DTDC_UNIT_TEST", "-DTDC_UT_BATCH_HOST_CFX",
                     "-Itests/unit/cfx_batch_fixture", *SOURCES, "-o", str(binary)],
                    [str(binary)]]
        for index, command in enumerate(commands):
            result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                                    encoding="utf-8", errors="replace", timeout=120)
            (OUT / f"step-{index}.log").write_text(result.stdout + result.stderr, encoding="utf-8")
            report["steps"].append({"command": command, "returncode": result.returncode})
            print(result.stdout + result.stderr, end="")
            if result.returncode:
                raise RuntimeError(f"step {index} failed ({result.returncode})")
        # Test-off files must introduce no symbols. No firmware link is claimed.
        for index, source in enumerate(SOURCES[:2]):
            obj = OUT / f"normal-{index}.o"
            command = [compiler, "-std=c99", "-c", source, "-o", str(obj)]
            subprocess.run(command, cwd=ROOT, check=True, capture_output=True, timeout=30)
            nm = shutil.which("nm")
            if not nm:
                raise RuntimeError("nm not found on PATH")
            symbols = subprocess.run([nm, "-g", str(obj)], check=True, capture_output=True,
                                     text=True, timeout=30).stdout
            if "tdc_ut" in symbols:
                raise RuntimeError(f"test symbol in normal object: {source}")
        report["status"] = "HOST_PASS"
    except (OSError, RuntimeError, subprocess.SubprocessError) as exc:
        report["status"] = "FAIL"
        report["error"] = str(exc)
        print(exc)
    finally:
        inputs = SOURCES + ["src/1__cfx/test/tdc_ut_batch_cfx.h",
                            "src/2__cm3/source/tests/tdc_ut_batch_cfx_bridge.h",
                            "src/test/tdc_ut_log.c", "src/test/tdc_ut_log.h",
                            "tests/unit/cfx_batch_fixture/hw.h",
                            "tests/unit/cfx_batch_fixture/SEGGER_RTT.h",
                            "tests/unit/check_cfx_batch.py"]
        report["inputs"] = {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in inputs}
        report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    return 0 if report["status"] == "HOST_PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
