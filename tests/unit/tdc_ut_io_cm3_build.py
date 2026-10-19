"""Compile the owned seams for ARM, or run product functions on isolated host MMIO.

No device, project filesystem image, Git refs or build metadata are changed.
SDK files are used from the local licensed installation, never copied.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
CM3 = ROOT / "src/2__cm3"
SDK = Path(os.environ.get("TDC_E8300_SDK", "C:/Program Files (x86)/ON Semiconductor/Ezairo 8300 SDK"))
PRODUCTS = [
    "Cortex-M3-src/systemControl/driver_i2c.c",
    "Cortex-M3-src/systemControl/driver_SPI.c",
    "Cortex-M3-src/systemControl/tdc_touch_iqs323.c",
    "Cortex-M3-src/systemControl/dirver_i2c_for_ISD.c",
    "Cortex-M3-src/internalDevice/isd_interface_FPGA.c",
    "Cortex-M3-src/internalDevice/driver_REN_ISL9122.c",
    "Cortex-M3-src/BleCommunication/tdc_remote_gain_control.c",
    "Cortex-M3-src/BleCommunication/ci_ble_control_ota.c",
    "Cortex-M3-src/BleCommunication/ci_ble_control_boot.c",
    "Cortex-M3-src/BleCommunication/ble_communication.c",
    "Cortex-M3-src/BleCommunication/mappingControl.c",
    "Cortex-M3-src/BleCommunication/remoteControl.c",
    "Gen1_5/FS/tdc_gain_storage.c",
]
RUNNER = CM3 / "Cortex-M3-src/test/tdc_ut_io_cm3.c"
FIXTURE = ROOT / "tests/unit/tdc_ut_io_cm3_host.c"
HOST_EXTENDED = [
    CM3 / "Cortex-M3-src/systemControl/tdc_touch_logic.c",
    CM3 / "Gen1_5/FS/ci_map.c",
    *[CM3 / f"Gen1_5/fn_fromCFX/fn_from_cfx_eeprom_{name}.c" for name in ("read", "write", "erase", "recover")],
    CM3 / "Cortex-M3-src/internalDevice/isd_interface_mapping_readWrtieMapData.c",
    *[ROOT / f"tests/unit/tdc_ut_io_cm3_{name}.c" for name in ("legacy_12", "legacy_13", "touch_init", "extended")],
]


def includes() -> list[Path]:
    tree = ET.parse(CM3 / ".cproject")
    option = next(e for e in tree.iter("option") if e.get("superClass") == "ilg.gnuarmeclipse.managedbuild.cross.option.c.compiler.include.paths")
    paths = [CM3 / "Cortex-M3-src/test", ROOT / "tests/unit"]
    for entry in option:
        value = entry.get("value", "").strip('"')
        prefix = "${workspace_loc:/${ProjName}"
        if value.startswith(prefix):
            relative = value[len(prefix):].removesuffix("}").lstrip("/")
            path = CM3 / relative
            if path.is_dir():
                paths.append(path)
    paths.append(ROOT / "src/0__bootloader/include")
    return list(dict.fromkeys(paths))


def invoke(command: list[str], *, timeout: int = 60, allow_failure: bool = False) -> subprocess.CompletedProcess[str]:
    print(subprocess.list2cmdline(command), flush=True)
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, errors="replace", timeout=timeout)
    if result.stdout:
        print(result.stdout, end="")
    if result.stderr:
        # Avoid cascading header errors hiding the first real diagnostic.
        print(result.stderr[:16000], end="")
    if result.returncode and not allow_failure:
        raise SystemExit(result.returncode)
    return result


def source_hashes(sources: list[Path]) -> dict[str, str]:
    # Wrapper includes and transitive local headers are recorded too. A superset
    # is intentional: compiler include dependencies are not inferred from logs.
    paths = set(sources) | set(CM3.rglob("*.h"))
    paths |= set((CM3 / "Cortex-M3-src/internalDevice").glob("driver_REN_ISL*.c"))
    paths.add(CM3 / "Cortex-M3-src/systemControl/tdc_touch.c")
    paths |= set((ROOT / "tests/unit").glob("tdc_ut_io_cm3*.*"))
    paths |= set((ROOT / "tests/unit/tdc_ut_io_cm3_host").glob("*.h"))
    paths.add(CM3 / ".cproject")
    return {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(paths)}


def save_result(args: argparse.Namespace, result: subprocess.CompletedProcess[str],
                sources: list[Path], before: dict[str, str], command: list[str]) -> None:
    output = result.stdout + result.stderr
    records = []
    for match in re.finditer(r"CM3 unit=(\d+) cases=(\d+) words=(\d+) mismatches=(\d+)", output):
        unit, cases, words, failed = map(int, match.groups())
        records.append({"unit": unit, "cases": cases, "compared_words": words,
                        "payload_comparisons": words - 4 * cases, "failed_comparisons": failed,
                        "runner": "ram" if args.ram else "host"})
    for match in re.finditer(r"CM3 EXT unit=(\d+) cases=(\d+) comparisons=(\d+) mismatches=(\d+) known=(\d+)", output):
        unit, cases, comparisons, failed, known = map(int, match.groups())
        records.append({"unit": unit, "cases": cases, "compared_words": comparisons,
                        "payload_comparisons": comparisons, "failed_comparisons": failed,
                        "known_findings": known, "runner": "host_extension"})
    failures = []
    pattern = r"CM3 (?P<extended>EXT )?unit=(?P<unit>\d+) case=(?P<case>\d+) (?P<field>.+?) actual=(?P<actual>-?\d+) expected=(?P<expected>-?\d+)"
    for match in re.finditer(pattern, output):
        row = {key: int(match[key]) for key in ("unit", "case", "actual", "expected")}
        row["field"] = match["field"]
        row["runner"] = "host_extension" if match["extended"] else ("ram" if args.ram else "host")
        failures.append(row)
    after = source_hashes(sources)
    complete = "KNOWN FINDINGS CHECK OK" in output if args.check_known_findings else "CM3 IO executed=" in output and (args.ram or "CM3 EXT failed_comparisons=" in output)
    verified = bool(args.check_known_findings and complete and result.returncode == 0)
    observations = [line for line in output.splitlines() if line.startswith("CM3 OBS ")]
    report = {
        "schema_version": 1, "mode": "ram" if args.ram else "host",
        "command": [sys.executable, *sys.argv], "compile_command": command,
        "returncode": result.returncode, "known_findings_mode": args.check_known_findings,
        "complete": complete, "not_device_run": True,
        "product_contract_pass": complete and not failures and result.returncode == 0,
        "known_findings_verified": verified, "known_findings": failures if verified else [],
        "units": records, "cases": sum(r["cases"] for r in records),
        "observation_only_cases": len(observations), "observations": observations,
        "compared_words_including_framing": sum(r["compared_words"] for r in records),
        "payload_comparisons": sum(r["payload_comparisons"] for r in records),
        "failed_comparisons": failures,
        "source_hashes_before": before, "source_hashes_after": after,
        "source_unchanged_during_run": before == after,
        "stdout": result.stdout, "stderr": result.stderr,
    }
    directory = ROOT / "tmp/unit-tests/cm3-io"
    directory.mkdir(parents=True, exist_ok=True)
    path = directory / ("ram-checks.json" if args.ram else "checks.json")
    path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"RESULT {path}: payload_comparisons={report['payload_comparisons']} failures={len(failures)} product_contract_pass={report['product_contract_pass']}")
    if before != after:
        raise SystemExit("source changed during execution; result cannot be reused")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", action="store_true", help="ARM compile only; never loads a board")
    parser.add_argument("--no-unit-test", action="store_true", help="compile the normal path for ARM")
    parser.add_argument("--ram", action="store_true", help="run exactly the target RAM vector branches on host MMIO")
    parser.add_argument("--check-known-findings", action="store_true", help="verify exact known defects, without calling affected units PASS")
    args = parser.parse_args()
    target = args.target or args.no_unit_test
    compiler = str(SDK / "arm_tools/bin/arm-none-eabi-gcc.exe") if target else "gcc"
    sources = [CM3 / name for name in PRODUCTS] + [RUNNER]
    with tempfile.TemporaryDirectory(prefix="tdc-ut-io-cm3-") as directory:
        build = Path(directory)
        common = [compiler, "-std=gnu11", "-DSK5_CID=101", "-Dchess_storage(a)=", "-ffunction-sections", "-fdata-sections"]
        if not args.no_unit_test:
            common += ["-DTDC_UNIT_TEST"]
        if args.ram:
            common += ["-DTDC_UT_IO_CM3_RAM_ONLY"]
        if args.check_known_findings:
            common += ["-DTDC_UT_IO_CM3_CHECK_KNOWN_FINDINGS"]
        if target:
            common += ["-mcpu=cortex-m3", "-mthumb"]
        else:
            # Keep the licensed FatFs C99 ABI; its Windows branch imports MMIO-hostile
            # Windows macros and uses a different integer ABI.
            common += ["-DTDC_UT_IO_CM3_HOST", "-fno-asynchronous-unwind-tables", "-fno-unwind-tables", "-I", str(ROOT / "tests/unit/tdc_ut_io_cm3_host")]
        for path in includes():
            common += ["-I", str(path)]
        for path in [SDK / "include/cm3", SDK / "include/shared"]:
            common += ["-isystem", str(path)]
        if target:
            for index, source in enumerate(sources):
                strict = ["-Wall", "-Wextra", "-Werror", "-Wno-unused-function"] if source == RUNNER else []
                invoke(common + strict + ["-c", str(source), "-o", str(build / f"{index}.o")])
            invoke([str(SDK / "arm_tools/bin/arm-none-eabi-size.exe"), str(build / f"{len(sources)-1}.o")])
            if args.no_unit_test:
                nm = subprocess.run([str(SDK / "arm_tools/bin/arm-none-eabi-nm.exe"), *map(str, build.glob("*.o"))], capture_output=True, text=True, check=True)
                assert "tdc_ut_io_cm3" not in nm.stdout, "normal path leaked unit-test symbols"
                print("NORMAL SYMBOL ISOLATION OK: no tdc_ut_io_cm3 symbols")
            print(f"ARM COMPILE OK: {len(sources)} translation units; TDC_UNIT_TEST={not args.no_unit_test}")
        else:
            # Actual shared-state accessors and bit extraction; no output-value mocks.
            sources += [CM3 / "Cortex-M3-src/systemControl/cfx_cm3_sharedMemory.c", CM3 / "Cortex-M3-src/signalProcessing/commonDataProcessing.c", CM3 / "Cortex-M3-src/internalDevice/isd_interface.c", CM3 / "Cortex-M3-src/internalDevice/isd_interface_stimulationParaSetting.c", CM3 / "Cortex-M3-src/systemControl/batteryNPowerControl.c", CM3 / "Cortex-M3-src/systemControl/LedOutput.c", CM3 / "Cortex-M3-src/error.c", CM3 / "Cortex-M3-src/QCC/snd_qcc.c", FIXTURE]
            executable = build / "tdc_ut_io_cm3.exe"
            if not args.ram:
                sources += HOST_EXTENDED
            command = common + ["-O0", "-g", "-Wl,--gc-sections", *map(str, sources), "-o", str(executable)]
            before = source_hashes(sources)
            built = invoke(command, allow_failure=True)
            try:
                result = invoke([str(executable)], timeout=30, allow_failure=True) if built.returncode == 0 else built
            except subprocess.TimeoutExpired as exc:
                def decoded(value: str | bytes | None) -> str:
                    return value.decode(errors="replace") if isinstance(value, bytes) else (value or "")
                result = subprocess.CompletedProcess([str(executable)], -1, decoded(exc.stdout), decoded(exc.stderr) + "\nHOST TIMEOUT after 30 seconds\n")
            save_result(args, result, sources, before, command)
            if result.returncode:
                raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
