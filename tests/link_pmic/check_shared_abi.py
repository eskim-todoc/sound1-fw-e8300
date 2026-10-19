"""Compare real ARM-byte and ASIP-word shared layouts without executing a core."""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests/unit"))
from build_target_tests import SDK, configuration

OUT = ROOT / "tmp/link-pmic-abi"
TYPE = "ST__CFX_CM3_SharedMemory_ALL"
FIELDS = ("systemShare", "is_enabled_CFX_iteration", "is_i2s_source_cradle", "tdc_backtel_period_100ms", "tdc_power_stable_nop_count")


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    result = {"status": "RUNNING", "device_execution": False, "commands": []}
    destination = OUT / "checks.json"
    def run(command):
        record = subprocess.run(list(map(str, command)), cwd=OUT, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=120)
        result["commands"].append({"command": list(map(str, command)), "exit": record.returncode, "output": record.stdout + record.stderr})
        destination.write_text(json.dumps(result, indent=2), encoding="utf-8")
        if record.returncode:
            raise RuntimeError(record.stdout + record.stderr)
        return record.stdout
    destination.write_text(json.dumps(result), encoding="utf-8")
    cm3 = ROOT / "src/2__cm3"
    cfx = ROOT / "src/1__cfx"
    cm3_header = cm3 / "Cortex-M3-src/systemControl/cfx_cm3_sharedMemory.h"
    cfx_header = cfx / "environment/shared_memory.h"
    header_hashes = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in (cm3_header, cfx_header)}
    expressions = {"size": f"sizeof({TYPE})", "int_size": "sizeof(int)"}
    expressions.update({field: f"offsetof({TYPE}, {field})" for field in FIELDS})
    probe = OUT / "arm_probe.c"
    probe.write_text(f'#include <stddef.h>\n#include "{cm3_header.as_posix()}"\n' + "\n".join(
        f"const unsigned char tdc_probe_{name}[({expression})+1]={{0}};" for name, expression in expressions.items()), encoding="utf-8")
    _, includes = configuration(cm3, "CM3")
    arm = SDK / "arm_tools/bin/arm-none-eabi-gcc.exe"
    common = [arm, "-mcpu=cortex-m3", "-mthumb", "-std=gnu11", "-DSK5_CID=101", "-Dchess_storage(a)="]
    for p in (SDK / "include/cm3", SDK / "include/shared"):
        common += ["-isystem", p]
    common += ["-I" + str(p) for p in includes]
    run([*common, "-c", probe, "-o", OUT / "arm_probe.o"])
    symbols = run([SDK / "arm_tools/bin/arm-none-eabi-nm.exe", "-S", OUT / "arm_probe.o"])
    values = {name: int(size, 16)-1 for size, name in re.findall(r"[0-9a-fA-F]+\s+([0-9a-fA-F]+)\s+\w\s+tdc_probe_(\w+)", symbols)}
    if set(values) != set(expressions) or values["int_size"] != 4:
        raise RuntimeError("ARM probe did not emit all expected constants")
    baseline = subprocess.run(["git", "show", "f9fa45ff:src/2__cm3/Cortex-M3-src/systemControl/cfx_cm3_sharedMemory.h"], cwd=ROOT, capture_output=True, check=True).stdout
    old_header = OUT / "baseline_tdc_shm.h"
    old_header.write_bytes(baseline)
    old_expressions = {key: value for key, value in expressions.items() if key not in ("tdc_backtel_period_100ms", "tdc_power_stable_nop_count")}
    probe.write_text(f'#include <stddef.h>\n#include "{old_header.as_posix()}"\n' + "\n".join(
        f"const unsigned char tdc_probe_{name}[({expression})+1]={{0}};" for name, expression in old_expressions.items()), encoding="utf-8")
    run([*common, "-c", probe, "-o", OUT / "baseline_probe.o"])
    old_symbols = run([SDK / "arm_tools/bin/arm-none-eabi-nm.exe", "-S", OUT / "baseline_probe.o"])
    old_values = {name: int(size, 16)-1 for size, name in re.findall(r"[0-9a-fA-F]+\s+([0-9a-fA-F]+)\s+\w\s+tdc_probe_(\w+)", old_symbols)}
    assert set(old_values) == set(old_expressions)
    assert all(values[key] == old_values[key] for key in old_values if key != "size")
    assert values["tdc_backtel_period_100ms"] == old_values["size"] and values["size"] == old_values["size"] + 8
    probe = OUT / "cfx_probe.c"
    text = '#include <stddef.h>\n#include <shared_memory.h>\n'
    for name, expression in expressions.items():
        text += f"typedef char tdc_assert_{name}[(({expression})*4=={values[name]})?1:-1];\n"
    text += "int tdc_shared_abi_probe(void){return 1;}\n"
    probe.write_text(text, encoding="utf-8")
    compiler = SDK / "ASIP Programmer/U-2023.06-SP1/win64/bin/WINbin/chesscc.exe"
    model = SDK / "cf624c-a20_vU-2023.06/lib"
    _, includes = configuration(cfx, "CFX")
    includes += [SDK / "include/shared", SDK / "include/cfx", model / "runtime/include"]
    run([compiler, "-p", "cf624c", "-P", model, "-DSK5_CID=101", "-DHARDWARE=0", "-DISS=1", "-DTARGET=HARDWARE", "-DCFX_CORE=1",
         *["-I" + str(p) for p in includes], "-c", probe, "-o", OUT / "cfx_probe.o"])
    if header_hashes != {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in (cm3_header, cfx_header)}:
        raise RuntimeError("Shared headers changed during ABI probe")
    result.update(status="PASS", arm_bytes=values, baseline_arm_bytes=old_values, baseline_commit="f9fa45ff",
                  cfx_words={key: value//4 for key, value in values.items()}, headers=header_hashes)
    destination.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print("SHARED_ABI_TARGET_PASS", json.dumps(values), "CFX words x4 == ARM bytes; no execution")


if __name__ == "__main__":
    main()
