"""Verify current SDK images and OFF equivalence, without loading a device."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

from verify_sys23_builds import ROOT, build, run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path, required=True)
    args = parser.parse_args()
    project = ROOT / "src/2__cm3"
    out = ROOT / "tmp/system-tests/audit"
    out.mkdir(parents=True, exist_ok=True)
    result = {"complete": False, "device_executed": False, "builds": {}, "off_equivalent": {}}
    record_path = out / "build-checks.json"
    record_path.write_text(json.dumps(result), encoding="utf-8")
    sources, _ = build.configuration(project, "CM3")
    records = {}
    for label, folder in {"off": "normal", "on": "normal/audit",
                          "combined": "normal/system7/system23/audit/dio35"}.items():
        base = ROOT / "tmp/unit-tests/target/CM3" / folder
        record = json.loads((base / "checks.json").read_text(encoding="utf-8"))
        assert record["build_complete"] and record["system_test_audit"] == (label != "off")
        assert record["inputs"] == build.input_snapshot(project, sources, ROOT), "Stale build inputs"
        assert record["artifacts"] == build.artifact_snapshot("CM3", base, ROOT), "Stale artifacts"
        symbols = run([str(build.SDK / "arm_tools/bin/arm-none-eabi-nm.exe"), "-S",
                       str(base / "test_image.elf")], out)
        assert ("tdc_sys_test_audit_command" in symbols) == (label != "off")
        result["builds"][label] = {"compile_files": len(record["compile"]),
            "artifacts": record["artifacts"], "size": run([str(build.SDK / "arm_tools/bin/arm-none-eabi-size.exe"),
                                                         str(base / "test_image.elf")], out)}
        records[label] = record
    command = next(r["command"] for r in records["off"]["compile"] if Path(r["command"][-3]).name == "tdc_ui_command.c")
    flags = [a for a in command[:command.index("-c")] if a != "-g3"]
    current = Path(command[-3])
    previous = args.before.resolve() / current.relative_to(ROOT)
    for sys7, sys23 in ((0, 0), (1, 0), (0, 1), (1, 1)):
        variant = [f"-DTDC_SYS_TEST_7_ENABLE={sys7}" if a.startswith("-DTDC_SYS_TEST_7_ENABLE=") else
                   f"-DTDC_SYS_TEST_23_ENABLE={sys23}" if a.startswith("-DTDC_SYS_TEST_23_ENABLE=") else a for a in flags]
        variant += ["-g0", "-E", "-P", "-D__LINE__=0", '-D__FILE__="SOURCE"']
        old = run([*variant, str(previous)], out)
        now = run([*variant, str(current)], out)
        assert old == now, f"OFF UI changed: SYS7={sys7}, SYS23={sys23}"
        result["off_equivalent"][f"sys7={sys7},sys23={sys23}"] = hashlib.sha256(now.encode()).hexdigest()
    header = project / "processorDirective.h"
    current_header = header.read_text(encoding="utf-8")
    original_header = (args.before / header.relative_to(ROOT)).read_text(encoding="utf-8")
    begin = current_header.index("/* SYS-050/051")
    end = current_header.index("////////////////////////////////////////////", begin)
    assert (current_header[:begin] + current_header[end:]) == original_header
    for relative in ("processorDirective.h", "Gen1_5/ui/tdc_ui_command.c",
                     "Cortex-M3-src/test/tdc_sys_test_audit.c", "Cortex-M3-src/test/tdc_sys_test_audit.h"):
        text = (project / relative).read_text(encoding="utf-8")
        assert text.encode("cp949").decode("cp949") == text
        assert not re.search("[\u2014\u2013\u2248\u00b5\u25ba]", text)
    cmd = list(next(r["command"] for r in records["on"]["compile"] if Path(r["command"][-3]).name == "tdc_sys_test_audit.c"))
    cmd.insert(1, "-fstack-usage"); cmd[-1] = str(out / "audit_stack.o")
    run(cmd, out)
    result["stack_frames"] = (out / "audit_stack.su").read_text(encoding="utf-8")
    undefined = run([str(build.SDK / "arm_tools/bin/arm-none-eabi-nm.exe"), "-u", str(out / "audit_stack.o")], out)
    assert not re.search(r"ci_event_log_|f_open|f_write|ci_timer_|snd_batt|Pcm", undefined)
    result["module_external_calls"] = undefined
    bad = ["-DTDC_SYS_TEST_AUDIT_ENABLE=2" if a.startswith("-DTDC_SYS_TEST_AUDIT_ENABLE=") else a for a in cmd]
    failure = subprocess.run(bad, cwd=out, capture_output=True, text=True, timeout=60)
    assert failure.returncode != 0 and "must be 0 or 1" in failure.stderr
    result.update(complete=True, prior_header_settings_preserved=True, invalid_switch_rejected=True,
                  scope="SDK build/OFF equivalence, not actual event capture or system-test acceptance")
    record_path.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print("BUILD_CHECK_PASS: 3 current SDK images, 4 OFF comparisons; no device execution")


if __name__ == "__main__":
    main()
