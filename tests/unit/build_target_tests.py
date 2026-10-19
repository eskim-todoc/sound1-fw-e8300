"""Build isolated SDK images; never load a board or touch Eclipse outputs.

Source entries/includes follow the existing project metadata. SDK libraries
remain in the licensed installation. A normal build can be checked separately.
"""
from __future__ import annotations

import argparse
import fnmatch
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
SDK = Path(os.environ.get("TDC_E8300_SDK", "C:/Program Files (x86)/ON Semiconductor/Ezairo 8300 SDK"))
PROJECT_FOLDERS = {"CFX": "1__cfx", "CM3": "2__cm3", "BOOT": "0__bootloader"}


def input_snapshot(project, sources, root):
    """Include the current file inventory, not only files in an older record."""
    paths = set(sources)
    paths.update(p for p in project.rglob("*.h")
                 if not any(v in ("Debug", "Release", ".metadata") for v in p.parts))
    paths.update(p for p in (root / "src/test").glob("*") if p.suffix in (".c", ".h"))
    paths.update(p for p in (project / ".cproject", project / "sections.ld", project / "app_LCF.bcf")
                 if p.is_file())
    return {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(paths)}


def artifact_paths(core, folder):
    """Both the executable and its map belong to the same successful link."""
    return (folder / ("test_image" if core == "CFX" else "test_image.elf"),
            folder / "test_image.map")


def artifact_snapshot(core, folder, root):
    result = {}
    for path in artifact_paths(core, folder):
        data = path.read_bytes()  # Missing output must invalidate success.
        if not data:
            raise ValueError(f"Empty build artifact: {path}")
        result[str(path.relative_to(root))] = hashlib.sha256(data).hexdigest()
    return result


def write_checks(folder, checks):
    """Readers see a complete record; an interrupted build stays unapproved."""
    pending = folder / "checks.json.tmp"
    pending.write_text(json.dumps(checks, ensure_ascii=False, indent=2), encoding="utf-8")
    pending.replace(folder / "checks.json")


def run(command, directory):
    result = subprocess.run(list(map(str, command)), cwd=directory, capture_output=True, text=True, errors="replace", timeout=240)
    return {"command": list(map(str, command)), "returncode": result.returncode, "output": result.stdout + result.stderr}


def configuration(project, core):
    tree = ET.parse(project / ".cproject")
    entries = list(tree.iter("sourceEntries"))[0]
    selected = set()
    for entry in entries:
        base = project / entry.get("name", "")
        excluded = entry.get("excluding", "").split("|")
        for p in base.rglob("*"):
            if p.suffix.lower() not in ([".c", ".asm", ".s"] if core == "CFX" else [".c", ".s"]):
                continue
            relative = p.relative_to(base).as_posix()
            if any(pattern and (relative == pattern or relative.startswith(pattern.rstrip("/") + "/") or fnmatch.fnmatch(relative, pattern)) for pattern in excluded):
                continue
            selected.add(p)
    sources = sorted(selected)
    includes = []
    for option in tree.iter("option"):
        if option.get("valueType") not in ("includePath", "includePaths"):
            continue
        for child in option:
            value = child.get("value", "").strip('"')
            prefix = "${workspace_loc:/${ProjName}"
            if value.startswith(prefix):
                value = value[len(prefix):].removesuffix("}").lstrip("/")
                includes.append(project / value)
    # CFX uses workspace loc syntax with a backslash in older Eclipse files.
    includes += [p for p in project.iterdir() if p.is_dir() and p.name != "Debug"] if core == "CFX" else []
    return sources, list(dict.fromkeys(includes))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("core", choices=["CFX", "CM3", "BOOT"])
    parser.add_argument("--normal", action="store_true")
    parser.add_argument("--compile-only", action="store_true")
    parser.add_argument("--system-test-7", action="store_true",
                        help="Enable the CM3 RTT system-test command without changing product protection")
    parser.add_argument("--system-test-23", action="store_true",
                        help="Enable the CM3 low-battery alarm test input; retain real power protection")
    parser.add_argument("--system-test-audit", action="store_true",
                        help="Enable SYS-050/051 RAM audit display; never write real audit storage")
    parser.add_argument("--system-test-dio35", action="store_true",
                        help="Enable system-test DIO35 init (CM3) or timing markers (CFX); never flash")
    for test_id in (54, 73, 77, 80):
        parser.add_argument(f"--system-test-{test_id}", action="store_true",
                            help=f"Enable SYS-{test_id:03d} storage test; never load a device")
    args = parser.parse_args()
    storage_switches = {test_id: getattr(args, f"system_test_{test_id}") for test_id in (54, 73, 77, 80)}
    if any(storage_switches.values()) and args.core != "CM3":
        parser.error("Storage system tests are only available for CM3")
    if args.system_test_7 and args.core != "CM3":
        parser.error("--system-test-7 is only available for CM3")
    if args.system_test_23 and args.core != "CM3":
        parser.error("--system-test-23 is only available for CM3")
    if args.system_test_audit and args.core != "CM3":
        parser.error("--system-test-audit is only available for CM3")
    if args.system_test_dio35 and args.core == "BOOT":
        parser.error("--system-test-dio35 is only available for CM3 or CFX")
    folder = PROJECT_FOLDERS[args.core]
    project = ROOT / "src" / folder
    out = ROOT / "tmp/unit-tests/target" / args.core / ("normal" if args.normal else "test")
    if args.system_test_7:
        out = out / "system7"
    if args.system_test_23:
        out = out / "system23"
    if args.system_test_audit:
        out = out / "audit"
    if args.system_test_dio35:
        out = out / "dio35"
    for test_id, enabled in storage_switches.items():
        if enabled:
            out = out / f"system{test_id}"
    out.mkdir(parents=True, exist_ok=True)
    sources, includes = configuration(project, args.core)
    # Freeze local compilation inputs, including test headers and linker rules.
    # A successful link from an earlier header/layout must not verify new bytes.
    inputs = input_snapshot(project, sources, ROOT)
    checks = {"core": args.core, "test_enabled": not args.normal, "system_test_7": args.system_test_7,
              "system_test_23": args.system_test_23,
              "system_test_audit": args.system_test_audit,
              "system_test_dio35": args.system_test_dio35,
              "compile": [], "link": None,
              "sources": {str(p.relative_to(ROOT)): inputs[str(p.relative_to(ROOT))] for p in sources},
              "inputs": inputs, "artifacts": {}, "build_complete": False}
    checks.update({f"system_test_{key}": value for key, value in storage_switches.items()})
    # Invalidate a previous successful record before touching its output files.
    # A compile/link exception must not leave old approval beside new artifacts.
    write_checks(out, checks)
    mode = [] if args.normal else ["-DTDC_UNIT_TEST"]
    if args.core in ("CM3", "CFX"):
        # Deliberately test OFF/ON without changing either source header switch.
        mode += [f"-DTDC_SYSTEM_TEST_DIO35_ENABLE={int(args.system_test_dio35)}"]
    if args.core == "CM3":
        # Test ON/OFF explicitly; preserve the user's current header switch.
        mode += [f"-DTDC_SYS_TEST_7_ENABLE={int(args.system_test_7)}"]
        mode += [f"-DTDC_SYS_TEST_23_ENABLE={int(args.system_test_23)}"]
        mode += [f"-DTDC_SYS_TEST_AUDIT_ENABLE={int(args.system_test_audit)}"]
        mode += [f"-DTDC_SYS_TEST_{key}_ENABLE={int(value)}" for key, value in storage_switches.items()]
    if args.core == "CFX":
        compiler = SDK / "ASIP Programmer/U-2023.06-SP1/win64/bin/WINbin/chesscc.exe"
        model = SDK / "cf624c-a20_vU-2023.06/lib"
        common = [compiler, "-p", "cf624c", "-P", model, "-g", "-DSK5_CID=101", "-DHARDWARE=0", "-DISS=1", "-DTARGET=HARDWARE", "-DCFX_CORE=1", *mode]
        includes += [SDK / "include/shared", SDK / "include/cfx", model / "runtime/include"]
    else:
        compiler = SDK / "arm_tools/bin/arm-none-eabi-gcc.exe"
        common = [compiler, "-mcpu=cortex-m3", "-mthumb", "-std=gnu11", "-O0", "-g3", "-ffunction-sections", "-fdata-sections", "-fsigned-char", "-DSK5_CID=101", "-Dchess_storage(a)=", *mode]
        for p in [SDK / "include/cm3", SDK / "include/shared"]:
            common += ["-isystem", p]
    for p in includes:
        common += ["-I", p]
    objects = [out / f"{i:03d}_{p.stem}.o" for i, p in enumerate(sources)]
    def compile_one(pair):
        source, obj = pair
        result = run([*common, "-c", source, "-o", obj], out)
        print(f"{args.core} {'OK' if not result['returncode'] else 'ERROR'} {source.relative_to(project)}", flush=True)
        return result
    with ThreadPoolExecutor(max_workers=2) as pool:
        records = list(pool.map(compile_one, zip(sources, objects)))
    failed = [r for r in records if r["returncode"]]
    link = None
    if not failed and not args.compile_only:
        # These two generated outputs must be produced by THIS link. Otherwise
        # a tool that returns success without emitting a map could approve the
        # previous map. The prior approval was already invalidated above.
        for artifact in artifact_paths(args.core, out):
            artifact.unlink(missing_ok=True)
        if args.core == "CFX":
            # ASIP otherwise discards public debugger runners as unreachable.
            # +s retains supplied object files, not every runtime archive.
            retain = [] if args.normal else ["+Wbridge,+s"]
            command = [*common, *retain, "+l", project / "app_LCF.bcf", "-L", model, "-L", model / "runtime/lib", "-L", SDK / "lib/cfx",
                       "-lcf624c", "-lc", "-lsyslib", "-lHAL", "-lHALWrapper", *objects, "-o", out / "test_image"]
        else:
            command = [compiler, "-mcpu=cortex-m3", "-mthumb", "-T", project / "sections.ld", "-nostartfiles", "--specs=nano.specs",
                       "-Wl,--gc-sections", f"-Wl,-Map,{out / 'test_image.map'}", "-L", SDK / "lib/cm3", *objects,
                       "-lcmsis", "-lfatfs", "-lnvmlib", "-lHAL", "-o", out / "test_image.elf"]
        link = run(command, out)
    checks.update(compile=records, link=link)
    validation_error = None
    try:
        current_sources, _ = configuration(project, args.core)
        if not sources or input_snapshot(project, current_sources, ROOT) != inputs:
            raise ValueError("Compilation inputs/inventory changed during build or source list is empty")
        if not failed and link and link["returncode"] == 0:
            checks["artifacts"] = artifact_snapshot(args.core, out, ROOT)
            checks["build_complete"] = True
    except (OSError, ValueError, ET.ParseError) as exc:
        validation_error = str(exc)
        checks.update(artifacts={}, build_complete=False, validation_error=validation_error)
    write_checks(out, checks)
    for item in failed:
        print(item["output"])
    if link:
        print(link["output"])
    if validation_error:
        print(validation_error)
    if failed or (link and link["returncode"]) or validation_error:
        raise SystemExit(1)
    print(f"{args.core} BUILD_PASS: {len(sources)} files; link={link is not None}; no device execution")


if __name__ == "__main__":
    main()
