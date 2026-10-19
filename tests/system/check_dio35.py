"""Check DIO35 markers without loading hardware or asserting a 1 ms result.

Real firmware bodies run with host event sinks; real CM3/CFX SDK builds are
checked separately. OFF comparison uses the pre-change working-tree snapshot,
not HEAD, so other pending test changes remain part of the baseline.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests/unit"))
import build_target_tests as build

FLAG = "TDC_SYSTEM_TEST_DIO35_ENABLE"
FUNCTIONS = {
    "ci_dio_configure_normal": "src/2__cm3/Gen1_5/common/ci_dio.c",
    "FIFO_4_ISR": "src/1__cfx/systemControl/interrupt_service_routine.c",
    "pcmDataOut": "src/1__cfx/internalDevice/driver_PCM.c",
}
HEADERS = ("src/2__cm3/processorDirective.h", "src/1__cfx/test/tdc_sys_test_config.h")


def run(command, cwd):
    result = subprocess.run(list(map(str, command)), cwd=cwd, capture_output=True,
                            text=True, encoding="utf-8", errors="replace", timeout=60)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return result.stdout


def clean(text):
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)


def function(text, name):
    # Selected functions have ordinary braces; remove comments before counting.
    text = clean(text)
    match = re.search(r"\b" + name + r"\s*\(\s*(?:void)?\s*\)\s*(?:property\(isr\))?\s*\{", text)
    if not match:
        raise AssertionError(f"Missing function: {name}")
    start = text.index("{", match.start())
    depth = 1
    end = start + 1
    while depth and end < len(text):
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    assert depth == 0
    return text[start:end]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path, required=True)
    args = parser.parse_args()
    gcc = shutil.which("gcc")
    if not gcc:
        raise SystemExit("Host GCC is required")
    out = ROOT / "tmp/system-tests/dio35"
    out.mkdir(parents=True, exist_ok=True)
    paths = [ROOT / p for p in (*FUNCTIONS.values(), *HEADERS)]
    paths += [Path(__file__), Path(__file__).with_name("dio35_host.c"), ROOT / "tests/unit/build_target_tests.py"]
    before = {str(p.relative_to(ROOT)): digest(p) for p in paths}
    results = {"complete": False, "device_executed": False, "timing_measured": False,
               "inputs": before, "off_equivalent": {}, "host": {}, "builds": {}}
    record_path = out / "checks.json"
    record_path.write_text(json.dumps(results, indent=2), encoding="utf-8")
    bodies = {name: function((ROOT / path).read_text(encoding="utf-8"), name)
              for name, path in FUNCTIONS.items()}
    generated = []
    for name, body in bodies.items():
        ctype = "int" if name == "ci_dio_configure_normal" else "void"
        generated.append(f"{ctype} {name}(void) {body}\n")
    (out / "firmware_functions.inc").write_text("\n".join(generated), encoding="utf-8")
    for enabled in (0, 1):
        binary = out / f"host_{enabled}.exe"
        run([gcc, "-std=c11", "-Wall", "-Wextra", "-Werror", f"-D{FLAG}={enabled}",
             "-I", out, Path(__file__).with_name("dio35_host.c"), "-o", binary], out)
        results["host"][str(enabled)] = run([binary], out).strip()
    for name, relative in FUNCTIONS.items():
        previous = args.before.resolve() / relative
        old_body = function(previous.read_text(encoding="utf-8"), name)
        normalized = []
        for label, body in (("before", old_body), ("after", bodies[name])):
            source = out / f"{label}_{name}.c"
            source.write_text(f"{body}\n", encoding="utf-8")
            expanded = run([gcc, "-E", "-P", f"-D{FLAG}=0", source], out)
            normalized.append(" ".join(expanded.split()))
        assert normalized[0] == normalized[1], f"OFF changed existing body: {name}"
        results["off_equivalent"][name] = {"before_sha256": digest(previous),
                                          "after_sha256": digest(ROOT / relative)}
    for relative in HEADERS:
        header = ROOT / relative
        # Both source defaults are 0; command-line override accepts only 0/1.
        assert re.search(r"#define\s+" + FLAG + r"\s+0\b", header.read_text(encoding="utf-8"))
        for value in (0, 1, 2, -1):
            result = subprocess.run([gcc, "-E", "-P", "-x", "c", f"-D{FLAG}={value}", str(header)],
                                    cwd=out, capture_output=True, text=True, timeout=30)
            assert (result.returncode == 0) == (value in (0, 1)), (relative, value)
    # Removing just the newly added block recovers the user's original header.
    now = (ROOT / HEADERS[0]).read_text(encoding="utf-8")
    start = now.index("/* 시스템 시험용 DIO35")
    end = now.index("/* SW-FW-SYS-Test-ID-7", start)
    assert now[:start] + now[end:] == (args.before / HEADERS[0]).read_text(encoding="utf-8")
    for core in ("CM3", "CFX"):
        project = ROOT / "src" / build.PROJECT_FOLDERS[core]
        sources, _ = build.configuration(project, core)
        modes = [(False, "normal", False), (True, "normal/dio35", False)]
        if core == "CM3":
            modes.append((True, "normal/system7/dio35", True))
        for enabled, folder, sys7 in modes:
            base = ROOT / "tmp/unit-tests/target" / core / folder
            record = json.loads((base / "checks.json").read_text(encoding="utf-8"))
            assert record["build_complete"] and record["system_test_dio35"] == enabled
            assert record["system_test_7"] == sys7
            assert record["inputs"] == build.input_snapshot(project, sources, ROOT), "Stale build inputs"
            assert record["artifacts"] == build.artifact_snapshot(core, base, ROOT), "Stale artifacts"
            label = f"{core}_{int(enabled)}" + ("_SYS7" if sys7 else "")
            results["builds"][label] = {
                "sources": len(record["sources"]), "artifacts": record["artifacts"],
                "warnings": record["link"]["output"]}
    # Inspect real linked CFX instructions, not only the host's GPIO event sink.
    disassembly = out / "cfx_on.dis"
    run([build.SDK / "ASIP Programmer/U-2023.06-SP1/win64/bin/WINbin/chesscc.exe",
         "+d", "-p", "cf624c", "-P", build.SDK / "cf624c-a20_vU-2023.06/lib",
         ROOT / "tmp/unit-tests/target/CFX/normal/dio35/test_image", "-o", disassembly], out)
    assembly = disassembly.read_text(encoding="utf-8")
    gpio_assembly = {}
    for symbol, address in (("FIFO_4_ISR", "1269"), ("void_pcmDataOut", "126b")):
        block = assembly.split(".label " + symbol + "\n", 1)[1].split(".label ", 1)[0]
        assert len(re.findall(r"store p\[0x" + address + r"\]", block)) == 1
        assert ",0x800" in block
        gpio_assembly[symbol] = block
    obj = next((ROOT / "tmp/unit-tests/target/CM3/normal/dio35").glob("*_ci_dio.o"))
    cm3_assembly = run([build.SDK / "arm_tools/bin/arm-none-eabi-objdump.exe", "-dr",
                        "--disassemble=ci_dio_configure_normal", obj], out)
    tail = re.split(r"R_ARM_THM_CALL\s+snd_qcc_init", cm3_assembly, maxsplit=1)[1]
    assert tail.index("Sys_GPIO_Set_Low") < tail.index("Sys_DIO_Config")
    assert len(re.findall(r"#35\b", tail)) == 2 and re.search(r"#3\b", tail)
    results["target_instructions"] = {"CFX": gpio_assembly, "CM3": tail}
    assert results["builds"]["CFX_0"]["warnings"] == results["builds"]["CFX_1"]["warnings"]
    for path in paths:
        if path.suffix in (".c", ".h"):
            text = path.read_text(encoding="utf-8")
            text.encode("cp949", errors="strict")
            assert not re.search("[\u2014\u2013\u2248\u00b5\u25ba]", text)
    assert before == {str(p.relative_to(ROOT)): digest(p) for p in paths}, "Inputs changed during check"
    results.update(complete=True, scope="Host call ordering and SDK link only; no GPIO waveform/1ms acceptance")
    record_path.write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding="utf-8")
    print("DIO35_CHECK_PASS: OFF unchanged, ON order checked, 5 SDK images, no device execution")
    print(record_path)


if __name__ == "__main__":
    main()
