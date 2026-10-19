"""Check SDK ON/OFF evidence, preserved OFF statements and source-only limits."""
from __future__ import annotations

import argparse
import difflib
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests/unit"))
import build_target_tests as build


def run(command: list[str], directory: Path) -> str:
    result = subprocess.run(command, cwd=directory, capture_output=True, text=True,
                            encoding="utf-8", errors="replace", timeout=60)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return result.stdout


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path, required=True)
    args = parser.parse_args()
    out = ROOT / "tmp/system-tests/sys23"
    out.mkdir(parents=True, exist_ok=True)
    project = ROOT / "src/2__cm3"
    sources, _ = build.configuration(project, "CM3")
    modes = {"off": "normal", "on": "normal/system23",
             "combined": "normal/system7/system23/dio35", "unit_combined": "test/system7/system23"}
    records = {}
    results = {"complete": False, "device_executed": False, "off_equivalent": {}, "builds": {}}
    record_path = out / "build-checks.json"
    record_path.write_text(json.dumps(results), encoding="utf-8")
    for label, folder in modes.items():
        base = ROOT / "tmp/unit-tests/target/CM3" / folder
        record = json.loads((base / "checks.json").read_text(encoding="utf-8"))
        assert record["build_complete"] and record["system_test_23"] == (label != "off")
        assert record["inputs"] == build.input_snapshot(project, sources, ROOT), "Stale build inputs"
        assert record["artifacts"] == build.artifact_snapshot("CM3", base, ROOT), "Stale build artifacts"
        records[label] = record
        symbols = run([str(build.SDK / "arm_tools/bin/arm-none-eabi-nm.exe"), "-S",
                       str(base / "test_image.elf")], out)
        assert ("tdc_sys_test_23_command" in symbols) == (label != "off")
        size = run([str(build.SDK / "arm_tools/bin/arm-none-eabi-size.exe"), str(base / "test_image.elf")], out)
        results["builds"][label] = {"size": size, "artifacts": record["artifacts"],
                                   "compile_files": len(record["compile"])}
    for name in ("main.c", "systemControl.c", "tdc_ui_command.c"):
        command = next(r["command"] for r in records["off"]["compile"] if Path(r["command"][-3]).name == name)
        flags = [arg for arg in command[:command.index("-c")] if arg != "-g3"]
        current = Path(command[-3])
        previous = args.before.resolve() / current.relative_to(ROOT)
        normalized = [*flags, "-g0", "-E", "-P", "-D__LINE__=0", '-D__FILE__="SOURCE"']
        # Verify both existing SYS7 configurations with SYS23 OFF.
        for sys7 in (0, 1):
            variant = [arg if not arg.startswith("-DTDC_SYS_TEST_7_ENABLE=") else
                       f"-DTDC_SYS_TEST_7_ENABLE={sys7}" for arg in normalized]
            old = run([*variant, str(previous)], out)
            now = run([*variant, str(current)], out)
            if old != now:
                diff = "".join(difflib.unified_diff(old.splitlines(True), now.splitlines(True)))
                raise AssertionError(f"OFF changed {name}, sys7={sys7}:\n{diff[:10000]}")
            results["off_equivalent"][f"{name}/sys7={sys7}"] = hashlib.sha256(now.encode()).hexdigest()
    header = project / "processorDirective.h"
    current = header.read_text(encoding="utf-8")
    previous = (args.before / header.relative_to(ROOT)).read_text(encoding="utf-8")
    begin = current.index("/* SW-FW-SYS-Test-ID-23")
    end = current.index("////////////////////////////////////////////", begin)
    assert (current[:begin] + current[end:]).strip() == previous.strip()
    results["existing_header_settings_preserved"] = True
    for relative in ("processorDirective.h", "Cortex-M3-src/main.c", "Cortex-M3-src/systemControl/systemControl.c",
                     "Cortex-M3-src/test/tdc_sys_test_23.c", "Cortex-M3-src/test/tdc_sys_test_23.h",
                     "Gen1_5/ui/tdc_ui_command.c"):
        text = (project / relative).read_text(encoding="utf-8")
        text.encode("cp949", errors="strict")
        assert not re.search("[\u2014\u2013\u2248\u00b5\u25ba]", text)
    # Check module stack frames under the real SDK; IRQ/libc high water remains unmeasured.
    cmd = next(r["command"] for r in records["on"]["compile"] if Path(r["command"][-3]).name == "tdc_sys_test_23.c")
    cmd = list(cmd); cmd.insert(1, "-fstack-usage"); cmd[-1] = str(out / "sys23_stack.o")
    run(cmd, out)
    results["stack_frames"] = (out / "sys23_stack.su").read_text(encoding="utf-8")
    bad = [arg if not arg.startswith("-DTDC_SYS_TEST_23_ENABLE=") else
           "-DTDC_SYS_TEST_23_ENABLE=2" for arg in cmd]
    failure = subprocess.run(bad, cwd=out, capture_output=True, text=True, timeout=60)
    assert failure.returncode != 0 and "must be 0 or 1" in failure.stderr
    results.update(complete=True, invalid_switch_rejected=True,
                   scope="SDK compile/link and expanded C preservation, not physical alarm or timing")
    record_path.write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding="utf-8")
    print("BUILD_CHECK_PASS: four SDK images; six OFF comparisons; no device execution")
    print(record_path)


if __name__ == "__main__":
    main()
