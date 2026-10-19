"""Verify current SDK build evidence, default-off preservation and SYS-007 size.

--before names the four-file snapshot taken before this implementation. The
comparison ignores source-location macros only, not runtime logic. No flashing.
"""
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


def run(command: list[str], cwd: Path) -> str:
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True,
                            encoding="utf-8", errors="replace", timeout=60)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return result.stdout


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path, required=True)
    args = parser.parse_args()
    out = ROOT / "tmp/system-tests/sys7"
    out.mkdir(parents=True, exist_ok=True)
    project = ROOT / "src/2__cm3"
    sources, _ = build.configuration(project, "CM3")
    modes = {"off": "normal", "on": "normal/system7", "unit_and_system": "test/system7"}
    records = {}
    results = {"complete": False, "device_executed": False, "off_equivalent": {}, "builds": {}, "stack_frames": {}}
    result_path = out / "build-checks.json"
    result_path.write_text(json.dumps(results, indent=2), encoding="utf-8")
    for label, folder in modes.items():
        base = ROOT / "tmp/unit-tests/target/CM3" / folder
        record = json.loads((base / "checks.json").read_text(encoding="utf-8"))
        assert record["build_complete"]
        assert record["inputs"] == build.input_snapshot(project, sources, ROOT), "Stale build inputs"
        assert record["artifacts"] == build.artifact_snapshot("CM3", base, ROOT), "Stale build artifacts"
        records[label] = record
        nm = str(build.SDK / "arm_tools/bin/arm-none-eabi-nm.exe")
        symbols = run([nm, "-S", str(base / "test_image.elf")], out)
        assert ("tdc_sys_test_7_command" in symbols) == (label != "off")
        size = run([str(build.SDK / "arm_tools/bin/arm-none-eabi-size.exe"), str(base / "test_image.elf")], out)
        results["builds"][label] = {"size": size, "artifacts": record["artifacts"]}
    for name in ("main.c", "stimulationParaCal.c", "tdc_ui_command.c"):
        command = next(item["command"] for item in records["off"]["compile"] if Path(item["command"][-3]).name == name)
        # -g3 makes this SDK emit macro definitions even with -E; compare
        # expanded C declarations/statements, not debug macro inventories.
        flags = [arg for arg in command[:command.index("-c")] if arg != "-g3"]
        current = Path(command[-3])
        previous = args.before.resolve() / name
        normalized = [*flags, "-g0", "-E", "-P", "-D__LINE__=0", '-D__FILE__="SOURCE"']
        old_output = run([*normalized, str(previous)], out)
        now_output = run([*normalized, str(current)], out)
        if old_output != now_output:
            difference = "".join(difflib.unified_diff(old_output.splitlines(True), now_output.splitlines(True)))
            raise AssertionError(f"Default-off change outside source locations: {name}\n{difference[:12000]}")
        results["off_equivalent"][name] = {"before_sha256": hashlib.sha256(previous.read_bytes()).hexdigest(),
                                          "after_sha256": hashlib.sha256(current.read_bytes()).hexdigest(),
                                          "preprocessed_sha256": hashlib.sha256(now_output.encode()).hexdigest()}
    # The header keeps all former definitions, including the misspelled guard.
    header = project / "processorDirective.h"
    before_header = (args.before / header.name).read_text(encoding="utf-8")
    now_header = header.read_text(encoding="utf-8")
    inserted = now_header.index("/* SW-FW-SYS-Test-ID-7")
    end = now_header.index("////////////////////////////////////////////", inserted)
    assert (now_header[:inserted] + now_header[end:]).strip() == before_header.strip()
    results["header_original_definitions_preserved"] = True
    # Compile selected ON functions with stack usage output. SDK libc/ISR high
    # water remains a target measurement, not a host-stack assertion.
    for name in ("main.c", "stimulationParaCal.c", "tdc_sys_test_7.c", "tdc_ui_command.c"):
        command = next(item["command"] for item in records["on"]["compile"] if Path(item["command"][-3]).name == name)
        command = list(command)
        command[-1] = str(out / f"stack_{name}.o")
        command.insert(1, "-fstack-usage")
        run(command, out)
        usage = (out / f"stack_{name}.su").read_text(encoding="utf-8")
        results["stack_frames"][name] = usage
    for p in [project / "Cortex-M3-src/test/tdc_sys_test_7.c", project / "Cortex-M3-src/test/tdc_sys_test_7.h"]:
        text = p.read_text(encoding="utf-8")
        text.encode("cp949", errors="strict")
        assert not re.search("[\u2014\u2013\u2248\u00b5\u25ba]", text)
    results.update(complete=True, scope="SDK images/PC preservation; no board or physical LED test")
    result_path.write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding="utf-8")
    print("BUILD_CHECK_PASS: three linked images; normal logic unchanged; no device execution")
    print(result_path)


if __name__ == "__main__":
    main()
