"""Bounded host checks of actual CM3/BOOT console adapters with fake batches.

This compiles the current wrappers and their included generic console unchanged.
No SDK link, target access, product execution, or wrapper edit is performed.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
TEST = ROOT / "tests/unit"
WRAPPERS = {
    "CM3": ROOT / "src/2__cm3/source/tests/tdc_ut_console_cm3.c",
    "BOOT": ROOT / "src/0__bootloader/source/test/tdc_ut_console_boot.c",
}


def execute(argv: list[str]) -> dict:
    try:
        result = subprocess.run(argv, cwd=ROOT, capture_output=True, text=True,
                                encoding="utf-8", errors="replace", timeout=45)
    except subprocess.TimeoutExpired as exc:
        return {"command": argv, "returncode": -1, "timeout": True, "stdout": str(exc.stdout or ""),
                "stderr": str(exc.stderr or "")}
    return {"command": argv, "returncode": result.returncode,
            "stdout": result.stdout, "stderr": result.stderr}


def main() -> None:
    gcc = shutil.which("gcc")
    if not gcc:
        raise RuntimeError("Host gcc is required")
    bridge = ROOT / "src/2__cm3/source/tests/tdc_ut_batch_cfx_bridge.c"
    inputs = [Path(__file__), TEST / "console_adapter_host_test.c", *WRAPPERS.values(), bridge,
              TEST / "console_adapter_bridge_semantics.c",
              ROOT / "src/2__cm3/source/tests/tdc_ut_report.h",
              ROOT / "src/test/tdc_ut_console.c", ROOT / "src/test/tdc_ut_console.h",
              ROOT / "src/2__cm3/source/tests/tdc_ut_batch_cm3.h",
              ROOT / "src/2__cm3/source/tests/tdc_ut_batch_cfx_bridge.h",
              ROOT / "src/1__cfx/test/tdc_ut_batch_cfx.h",
              ROOT / "src/0__bootloader/source/test/tdc_ut_batch_boot.h",
              *sorted((TEST / "console_adapter_host").glob("*.h"))]
    def hashes() -> dict[str, str]:
        return {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}

    before = hashes()
    checks = []
    with tempfile.TemporaryDirectory(prefix="tdc-console-adapters-") as temp:
        for image, wrapper in WRAPPERS.items():
            executable = Path(temp) / f"{image.lower()}.exe"
            command = [gcc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-DTDC_UNIT_TEST",
                       "-I", str(TEST / "console_adapter_host")]
            if image == "BOOT":
                command.append("-DCONSOLE_ADAPTER_BOOT")
            built = execute([*command, str(wrapper), str(TEST / "console_adapter_host_test.c"), "-o", str(executable)])
            ran = execute([str(executable)]) if built["returncode"] == 0 else None
            ok = bool(ran and ran["returncode"] == 0 and f"CONSOLE_ADAPTER_{image} PASS:" in ran["stdout"])
            checks.append({"image": image, "pass": ok, "compile": built, "execution": ran})
            print(f"{image} adapter: {'PASS' if ok else 'FAIL'}")
            for result in (built, ran):
                if result and (result["returncode"] or result is ran):
                    print(result["stdout"] + result["stderr"])
            if built["returncode"] == 0:
                scenarios = ["transport-fault"]
                if image == "CM3":
                    scenarios += ["unconfigured", "late-open", "late-cm3", "late-observe", "late-quit", "late-external-exit"]
                for scenario in scenarios:
                    test = execute([str(executable), scenario])
                    passed = test["returncode"] == 0 and "CONSOLE_ADAPTER_GUARD PASS:" in test["stdout"]
                    checks.append({"image": image, "scenario": scenario, "pass": passed, "execution": test})
                    print(test["stdout"] + test["stderr"])
        executable = Path(temp) / "bridge_semantics.exe"
        built = execute([gcc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-DTDC_UNIT_TEST",
                         "-I", str(TEST / "console_adapter_host"), str(bridge),
                         str(TEST / "console_adapter_bridge_semantics.c"), "-o", str(executable)])
        ran = execute([str(executable)]) if built["returncode"] == 0 else None
        passed = bool(ran and ran["returncode"] == 0 and "CONSOLE_ADAPTER_BRIDGE PASS:" in ran["stdout"])
        checks.append({"image": "BRIDGE", "pass": passed, "compile": built, "execution": ran})
        for result in (built, ran):
            if result and (result["returncode"] or result is ran):
                print(result["stdout"] + result["stderr"])
    unchanged = before == hashes()
    ok = unchanged and all(check["pass"] for check in checks)
    report = {"schema_version": 1, "harness_pass": ok, "sources_unchanged": unchanged,
              "source_sha256": before, "device_executed": False, "product_acceptance": False,
              "scope": "Actual wrapper and generic console, fake batch lifecycle/RTT/hardware guards", "checks": checks}
    destination = ROOT / "tmp/unit-tests/console-adapters/checks.json"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(destination)
    if not ok:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
