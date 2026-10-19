"""Check current OFF/individual/combined SDK images without touching a device."""
from __future__ import annotations
import argparse
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
    result = subprocess.run(list(map(str, command)), cwd=cwd, capture_output=True, text=True,
                            encoding="utf-8", errors="replace", timeout=60)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return result.stdout


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path, required=True)
    args = parser.parse_args()
    before = args.before.resolve()
    out = ROOT / "tmp/system-tests/storage"
    out.mkdir(parents=True, exist_ok=True)
    project = ROOT / "src/2__cm3"
    report = {"complete": False, "device_executed": False, "builds": {}, "off_equivalent": {}, "stack": {}}
    record_path = out / "build-checks.json"
    record_path.write_text(json.dumps(report), encoding="utf-8")
    sources, _ = build.configuration(project, "CM3")
    variants = {"off": "normal", **{str(n): f"normal/system{n}" for n in (54, 73, 77, 80)},
                "combined": "normal/system7/system23/audit/dio35/system54/system73/system77/system80"}
    records = {}
    for label, folder in variants.items():
        base = ROOT / "tmp/unit-tests/target/CM3" / folder
        record = json.loads((base / "checks.json").read_text(encoding="utf-8"))
        assert record["build_complete"]
        assert record["inputs"] == build.input_snapshot(project, sources, ROOT), f"Stale inputs: {label}"
        assert record["artifacts"] == build.artifact_snapshot("CM3", base, ROOT)
        nm = run([build.SDK / "arm_tools/bin/arm-none-eabi-nm.exe", "-S", base / "test_image.elf"], out)
        for n in (54, 73, 77, 80):
            enabled = label in (str(n), "combined")
            assert record[f"system_test_{n}"] == enabled
            assert (f"tdc_sys_test_{n}_execute" in nm) == enabled
        size = run([build.SDK / "arm_tools/bin/arm-none-eabi-size.exe", base / "test_image.elf"], out)
        report["builds"][label] = {"files": len(record["compile"]), "size": size,
                                    "artifacts": record["artifacts"]}
        records[label] = record
    selected = ("Cortex-M3-src/main.c", "Gen1_5/ui/tdc_ui_command.c", "Gen1_5/FS/ci_event_log.c",
                "Gen1_5/FS/ci_filesystem.c")
    for relative in selected:
        current = project / relative
        command = next(r["command"] for r in records["off"]["compile"] if Path(r["command"][-3]) == current)
        flags = [s for s in command[:command.index("-c")] if s != "-g3"]
        for guards in (0, 1):
            config = [re.sub(r"(TDC_SYS_TEST_(?:7|23|AUDIT)_ENABLE)=0", rf"\g<1>={guards}", s) for s in flags]
            config += ["-g0", "-E", "-P", "-D__LINE__=0", '-D__FILE__="SOURCE"']
            previous = before / current.relative_to(ROOT)
            old = run([*config, previous], out)
            now = run([*config, current], out)
            assert old == now, f"OFF changed: {relative}, old-test switches={guards}"
            report["off_equivalent"][f"{relative}:{guards}"] = hashlib.sha256(now.encode()).hexdigest()
    header = (project / "processorDirective.h").read_text(encoding="utf-8")
    start = header.index("/* 저장/무결성 시스템 시험.")
    end = header.index("////////////////////////////////////////////", start)
    previous_header = (before / "src/2__cm3/processorDirective.h").read_text(encoding="utf-8")
    assert header[:start] + header[end:] == previous_header, "Original switches changed"
    for source in (project / "Cortex-M3-src/test").glob("tdc_sys_test_*.c"):
        if source.stem not in {"tdc_sys_test_storage", "tdc_sys_test_54", "tdc_sys_test_73", "tdc_sys_test_77", "tdc_sys_test_80"}:
            continue
        command = list(next(r["command"] for r in records["combined"]["compile"] if Path(r["command"][-3]) == source))
        command.insert(1, "-fstack-usage"); command[-1] = str(out / f"{source.stem}.o")
        run(command, out)
        report["stack"][source.name] = (out / f"{source.stem}.su").read_text(encoding="utf-8")
    checked = [project / p for p in ("processorDirective.h", *selected, "Gen1_5/FS/ci_event_log.h",
                                    "Gen1_5/FS/ci_filesystem.h")]
    checked += [p for p in (project / "Cortex-M3-src/test").glob("tdc_sys_test_*.*") if p.suffix in (".c", ".h")]
    for p in checked:
        text = p.read_text(encoding="utf-8")
        assert text.encode("cp949").decode("cp949") == text, p
        assert not re.search("[\u2014\u2013\u2248\u00b5\u25ba]", text), p
    flags = list(next(r["command"] for r in records["off"]["compile"] if Path(r["command"][-3]).name == "tdc_sys_test_storage.c"))
    flags[-1] = str(out / "invalid.o")
    for n in (54, 73, 77, 80):
        bad = [f"-DTDC_SYS_TEST_{n}_ENABLE=2" if s.startswith(f"-DTDC_SYS_TEST_{n}_ENABLE=") else s for s in flags]
        result = subprocess.run(bad, cwd=out, capture_output=True, text=True, timeout=30)
        assert result.returncode != 0 and "must be 0 or 1" in result.stderr
    report.update(complete=True, normal_switches_preserved=True, invalid_switches_rejected=True,
                  scope="SDK link, OFF preprocessing, local stack frames; not physical LED/storage endurance/boot acceptance")
    record_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print("BUILD_CHECK_PASS: 6 current images; 8 OFF comparisons; no hardware execution")


if __name__ == "__main__":
    main()
