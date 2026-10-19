"""Compile actual link/PMIC modules against host file and hardware boundaries.

This executes no device, uses no production file, and does not flash firmware.
Each executable starts fresh; a failed build never executes an older binary.
"""
from __future__ import annotations

import hashlib
import json
import re
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
OUT = ROOT / "tmp/link-pmic-host"


def function(text: str, name: str) -> str:
    match = re.search(r"^(?:static\s+)?(?:bool|int|void|EN_ISD_PowerState)\s+" + re.escape(name) + r"\([^;]*?\)\s*\{", text, re.M)
    if not match:
        raise ValueError("Missing actual PMIC function: " + name)
    tokens = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', re.S)
    depth = 1
    for token in tokens.finditer(text, match.end()):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if depth == 0:
                return text[match.start():token.end()]
    raise ValueError("Incomplete actual PMIC function: " + name)


def main() -> None:
    compiler = shutil.which("gcc")
    if not compiler:
        raise RuntimeError("Host gcc unavailable")
    OUT.mkdir(parents=True, exist_ok=True)
    # Even failed extraction/startup must invalidate any prior successful run.
    (OUT / "checks.json").write_text(json.dumps({"status": "RUNNING", "device_execution": False}), encoding="utf-8")
    fpga = (ROOT / "src/2__cm3/source/isd/tdc_isd_fpga.c").read_text(encoding="utf-8-sig")
    (OUT / "pmic_actual.inc").write_text("\n".join(function(fpga, name) for name in
        ("tdc_isd_fpga_write_change_tx_power_level", "tdc_isd_fpga_read_tx_power_level", "tdc_isd_fpga_read_isd_power_state")), encoding="utf-8")
    router = ROOT / "src/2__cm3/source/ble/sound1/tdc_ble_general_debug.c"
    (OUT / "general_debug_route.inc").write_text(function(router.read_text(encoding="utf-8-sig"), "tdc_ble_general_debug_handle"), encoding="utf-8")
    isd = ROOT / "src/2__cm3/source/isd/tdc_isd.c"
    (OUT / "integration_actual.inc").write_text("\n".join(function(isd.read_text(encoding="utf-8-sig"), name) for name in
        ("reset_live_link_cycle", "tdc_isd_update_link_by_backtel_live", "tdc_isd_update_link_by_backtel_mapping")), encoding="utf-8")
    results = []
    sources = [p for p in (ROOT / "src/2__cm3/source").rglob("tdc_link_*.*")]
    sources += [ROOT / "src/2__cm3/source/ble/sound1/tdc_ble_link_debug.c", ROOT / "src/2__cm3/source/ble/sound1/tdc_ble_link_debug.h", router,
                ROOT / "src/2__cm3/source/isd/tdc_isd_fpga.c", ROOT / "src/2__cm3/source/cfx_link/tdc_shm.h",
                ROOT / "src/2__cm3/source/drv/tdc_drv_isl9122.h", ROOT / "src/2__cm3/source/board/isd_ver1_1_2.h"]
    sources += [isd, ROOT / "src/1__cfx/environment/shared_memory.h"]
    sources += list((ROOT / "src/1__cfx").rglob("*sharedMemory*.h"))
    frozen = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
    folders = [ROOT / "src/2__cm3/source", *[p for p in (ROOT / "src/2__cm3/source").rglob("*") if p.is_dir()]]
    configurations = []
    for stem in ("config", "protocol", "runtime", "integration"):
        fixture = HERE / f"{stem}_host.c"
        if fixture.exists():
            configurations.append((stem, fixture))
    if not configurations:
        raise RuntimeError("No link/PMIC host fixtures exist")
    evidence = {"status": "RUNNING", "device_execution": False, "results": results}
    remote = ROOT / "src/2__cm3/source/ble/remote/tdc_ble_remote.c"
    branch = re.search(r"case EN__SND_BT_CMD_GENERAL_DEBUG:\s*\{(.*?)\}\s*break;", remote.read_text(encoding="utf-8-sig"), re.S)
    if not branch or branch[1].count("tdc_hal_spi_write_tx_buffer(") != 1 or branch[1].count("tdc_ble_remote_clear_command(") != 1:
        raise RuntimeError("General-debug caller no longer has one TX and one command completion")
    assert branch[1].index("tdc_ble_general_debug_handle(") < branch[1].index("tdc_hal_spi_write_tx_buffer(") < branch[1].index("tdc_ble_remote_clear_command(")
    sources.append(remote)
    frozen[str(remote.relative_to(ROOT))] = hashlib.sha256(remote.read_bytes()).hexdigest()
    evidence["single_tx_caller_check"] = "Source inspection: one TX and one clear after actual general-debug dispatch; router/protocol executed separately"
    destination = OUT / "checks.json"
    destination.write_text(json.dumps(evidence, indent=2), encoding="utf-8")
    for stem, fixture, optimization in [(stem, fixture, opt) for stem, fixture in configurations for opt in ("O0", "O2")]:
        name = f"{stem}-{optimization}"
        command = [compiler, "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-" + optimization,
                   "-ffunction-sections", "-fdata-sections", "-I", str(HERE / "host")]
        if stem == "integration":
            # Existing source debug enum/int comparison emits a host warning;
            # retain the warning and execute its unchanged function bodies.
            command += ["-Wno-error=sign-compare"]
        command += ["-I" + str(p) for p in folders]
        command += ["-I", str(OUT)]
        command += [str(fixture), "-Wl,--gc-sections", "-o", str(OUT / f"{name}.exe")]
        built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=60)
        output = built.stdout + built.stderr
        exit_code = built.returncode
        if not exit_code:
            scenarios = [str(i) for i in (0, *range(1, 13), 20, 21, 22)] if stem == "config" else [""]
            for scenario in scenarios:
                run = subprocess.run([str(OUT / f"{name}.exe"), *([scenario] if scenario else [])], cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=30)
                output += run.stdout + run.stderr
                if run.returncode:
                    exit_code = run.returncode
                    break
        (OUT / f"{name}.log").write_text(output, encoding="utf-8")
        results.append({"name": name, "command": command, "build_exit": built.returncode, "exit": exit_code,
                        "log": str((OUT / f"{name}.log").relative_to(ROOT))})
        destination.write_text(json.dumps(evidence, indent=2), encoding="utf-8")
        print(f"{name}: {'HOST_PASS' if not exit_code else 'FAIL'}")
        if exit_code:
            print(output[-7000:])
    unchanged = frozen == {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
    inputs = sources + [p for p in HERE.rglob("*") if p.suffix in (".c", ".h", ".py") and not p.name.startswith("cfx_")]
    evidence.update(status="PASS" if unchanged and all(not r["exit"] for r in results) else "FAIL", source_unchanged=unchanged,
                    scope="Actual config/control/protocol modules and extracted unchanged FPGA/route function bodies; host boundary models, not complete FPGA module or target execution",
                    inputs={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs})
    destination.write_text(json.dumps(evidence, indent=2), encoding="utf-8")
    if evidence["status"] != "PASS":
        raise SystemExit(1)


if __name__ == "__main__":
    main()
