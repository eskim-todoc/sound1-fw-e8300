"""Compile real CM3 sources for host execution or ARM object validation.

This script creates only compiler outputs in a temporary directory. It does not
write the criterion workbook, firmware settings or NVM, and flashes no device.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import re
import shutil
import subprocess
import tempfile
from check_cfx_vectors import function

ROOT = pathlib.Path(__file__).resolve().parents[2]
CM3 = ROOT / "src/2__cm3"
SDK_DEFAULT = pathlib.Path(r"C:\Program Files (x86)\ON Semiconductor\Ezairo 8300 SDK")
INCLUDES = (
    "", "Gen1_5", "Gen1_5/fn_fromCFX", "Gen1_5/FS", "Gen1_5/common",
    "Gen1_5/SEGGER_RTT", "Gen1_5/tiny-AES-c", "Gen1_5/crc", "Gen1_5/dfu",
    "Gen1_5/ui", "99_includeBoard", "Cortex-M3-src", "Cortex-M3-src/test",
    "Cortex-M3-src/BleCommunication", "Cortex-M3-src/internalDevice",
    "Cortex-M3-src/QCC", "Cortex-M3-src/signalProcessing",
    "Cortex-M3-src/systemControl", "isdExecution", "signalProcessing", "systemControl",
)
SOURCES = (
    "Cortex-M3-src/test/tdc_ut_vectors_cm3.c",
    "Cortex-M3-src/test/tdc_ut_coordination_cm3.c",
    "Cortex-M3-src/test/tdc_ut_state_cm3.c",
    "Cortex-M3-src/systemControl/cfx_cm3_sharedMemory.c",
    "Cortex-M3-src/systemControl/tdc_touch_logic.c",
    "Cortex-M3-src/systemControl/batteryNPowerControl.c",
    "Cortex-M3-src/systemControl/LedOutput.c",
    "Cortex-M3-src/systemControl/systemControl.c",
    "Cortex-M3-src/signalProcessing/stimulationParaCal.c",
    "Cortex-M3-src/signalProcessing/commonDataProcessing.c",
    "Cortex-M3-src/internalDevice/indicatorByStimul.c",
    "Cortex-M3-src/error.c",
    "Gen1_5/ui/tdc_ui_command.c",
    "Gen1_5/common/ci_printf.c",
    "Cortex-M3-src/internalDevice/isd_interface.c",
    "Cortex-M3-src/internalDevice/isd_interface_stimulationStandAlone.c",
)


def run(command: list[str]) -> str:
    print("RUN", pathlib.Path(command[0]).name, pathlib.Path(command[-3]).name if "-c" in command else "link/execute", flush=True)
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                            encoding="utf-8", errors="replace", timeout=60)
    if result.stdout:
        print(result.stdout, end="")
    if result.stderr:
        print(result.stderr, end="")
    if result.returncode:
        raise RuntimeError(f"exit={result.returncode}: {subprocess.list2cmdline(command)}")
    return result.stdout


def source_hashes(paths: list[pathlib.Path]) -> dict[str, str]:
    return {str(p.relative_to(ROOT)).replace("\\", "/"): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(set(paths))}


def save_records(output: pathlib.Path, records: list[dict], sources: dict[str, str],
                 harness_ok: bool, **details) -> None:
    """Separate observed values, failed expectations and harness reproducibility."""
    compared = failures = interpreted = observed = 0
    for record in records:
        expected = record["expected"]
        if expected is None:
            record["verdict"] = "OBSERVED"
            record["failure_classification"] = "not_evaluated"
            observed += len(record["actual"])
            continue
        if len(record["actual"]) != len(expected):
            raise RuntimeError("Record actual/expected lengths differ")
        record["mismatches"] = [dict(index=i, actual=a, expected=e)
                                for i, (a, e) in enumerate(zip(record["actual"], expected)) if a != e]
        compared += len(expected)
        failures += len(record["mismatches"])
        record["verdict"] = "FAIL" if record["mismatches"] else "PASS"
        if record["mismatches"]:
            record["failure_classification"] = "interpretation_review" if record["unit"] == 30 else "contract_mismatch"
            if record["unit"] == 30:
                interpreted += len(record["mismatches"])
    report = dict(schema_version=1, device_executed=False, whole_unit_pass=False,
                  harness_reproduction="PASS" if harness_ok else "FAIL",
                  expectation_result="FAIL" if failures else "PASS" if compared else "NOT_RUN",
                  compared_values=compared, failed_values=failures,
                  contract_failed_values=failures-interpreted, interpretation_failed_values=interpreted,
                  observed_values=observed,
                  source_sha256=sources, records=records, **details)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")


def inspect_charge_configuration(compiler: str, flags: list[str]) -> None:
    """Report the actual preprocessed charge gate, without defining/enabling it.

    The host fixture reproduces known findings at the inactive-gate baseline.
    A future product policy change must update and review these vectors separately.
    """
    result = subprocess.run([compiler, *flags, "-dM", "-E",
                             str(CM3 / "Cortex-M3-src/signalProcessing/stimulationParaCal.c")],
                            cwd=ROOT, capture_output=True, text=True,
                            encoding="utf-8", errors="replace", timeout=60, check=True)
    macros = dict(re.findall(r"^#define[ \t]+(\w+)[ \t]*(.*)$", result.stdout, re.MULTILINE))
    active = "Df_MaxDeliveryChargeLimitation" in macros
    configured_name = "Df_MaxDeliveryChargeLimitationKKK" in macros
    limit = macros.get("df_MaxDeliveryCharge_pC")
    print(f"CHARGE source gate_active={int(active)} mismatched_name_defined={int(configured_name)} limit_pC={limit}", flush=True)
    if active or not configured_name or limit != "75000":
        raise RuntimeError("Charge baseline changed: review policy/expectations; do not auto-accept outputs")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", action="store_true", help="ARM objects only, no execution")
    parser.add_argument("--normal", action="store_true", help="ARM objects without TDC_UNIT_TEST")
    parser.add_argument("--sdk", type=pathlib.Path, default=SDK_DEFAULT)
    args = parser.parse_args()
    if args.normal and not args.target:
        parser.error("--normal requires --target; host fixtures are test-only")
    compiler = args.sdk / "arm_tools/bin/arm-none-eabi-gcc.exe" if args.target else shutil.which("gcc")
    if not compiler:
        raise RuntimeError("gcc is unavailable")
    flags = ["-std=gnu11", "-DSK5_CID=101", "-Dchess_storage(a)=",
             "-ffunction-sections", "-fdata-sections", "-fmax-errors=5"]
    if not args.normal:
        flags.append("-DTDC_UNIT_TEST")
    flags += ["-I" + str(CM3 / folder) for folder in INCLUDES]
    for folder in ("include/cm3", "include/shared"):
        flags += ["-isystem", str(args.sdk / folder)]
    if args.target:
        flags += ["-mcpu=cortex-m3", "-mthumb", "-O0", "-Wall", "-Wextra"]
    else:
        flags += ["-O0", "-fno-asynchronous-unwind-tables", "-fno-unwind-tables",
                  "-DTDC_UT_CM3_HOST", "-include",
                  str(ROOT / "tests/unit/tdc_ut_cm3_host_prelude.h")]
    inspect_charge_configuration(str(compiler), flags)
    # Include header inputs as well as translation units. Extra headers may be
    # conservative dependencies; hashes are source identity, never acceptance.
    inputs = [CM3 / source for source in SOURCES]
    inputs += list(CM3.rglob("*.h"))
    inputs += [CM3 / "Cortex-M3-src/main.c", pathlib.Path(__file__),
               ROOT / "tests/unit/tdc_ut_cm3_host.c", ROOT / "tests/unit/tdc_ut_cm3_host_prelude.h",
               ROOT / "tests/unit/check_cfx_vectors.py"]
    criterion = ROOT / json.loads((ROOT / "tests/unit/cm3_pure_control_coverage.json").read_text(encoding="utf-8"))["criterion"]["path"]
    inputs.append(criterion)
    before = source_hashes(inputs)
    with tempfile.TemporaryDirectory(prefix="tdc-ut-cm3-") as output:
        directory = pathlib.Path(output)
        objects = []
        for index, source in enumerate(SOURCES):
            obj = directory / f"cm3_{index}.o"
            run([str(compiler), *flags, "-c", str(CM3 / source), "-o", str(obj)])
            objects.append(str(obj))
        if args.target:
            print(f"ARM compile complete: {len(objects)} objects; mode={'normal' if args.normal else 'unit-test'}; execution/link not performed")
            if args.normal:
                nm = args.sdk / "arm_tools/bin/arm-none-eabi-nm.exe"
                symbols = subprocess.run([str(nm), "-g", "--defined-only", *objects],
                                         capture_output=True, text=True, timeout=60, check=True)
                forbidden = [line for line in symbols.stdout.splitlines() if "tdc_ut_cm3" in line]
                if forbidden:
                    raise RuntimeError("Test API leaked into normal objects: " + repr(forbidden))
                print("Normal symbol check: no tdc_ut_cm3 API/state symbols")
        else:
            # Retain the actual boot function body, not a rewritten test copy.
            # Its device boundaries abort unless the RAM-input ops are active.
            source = (CM3 / "Cortex-M3-src/main.c").read_text(encoding="utf-8")
            fragment = directory / "boot_body.c"
            prefix = '''#include <stdlib.h>
#include <cfx_cm3_sharedMemory.h>
#include <tdc_ut_coordination_cm3.h>
const tdc_ut_cm3_boot_ops_t *g_tdc_ut_cm3_boot_ops;
static void tdc_wait_for_cfx_start(void) {abort();}
void Initialize(void) {abort();}
static void tdc_print_default_isd_info(void) {abort();}
#define tdc_ui_command_init tdc_ut_forbidden_ui
static void tdc_ut_forbidden_ui(void) {abort();}
'''
            fragment.write_text(prefix + function(source,"tdc_normal_boot_sequence") + "\n" +
                                function(source,"tdc_ut_cm3_boot_sequence"),encoding="utf-8")
            executable = directory / "tdc_ut_cm3_host.exe"
            run([str(compiler), *flags, str(fragment), str(ROOT / "tests/unit/tdc_ut_cm3_host.c"),
                 *objects, "-Wl,--gc-sections,--exclude-all-symbols", "-o", str(executable)])
            result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True,
                                    encoding="utf-8", errors="replace", timeout=60)
            print(result.stdout, end="")
            print(result.stderr, end="")
            records = [json.loads(line[7:]) for line in result.stdout.splitlines() if line.startswith("RECORD ")]
            unchanged = before == source_hashes(inputs)
            save_records(ROOT / "tmp/unit-tests/cm3-vectors/checks.json", records, before,
                         result.returncode == 0 and unchanged and bool(records),
                         returncode=result.returncode, sources_unchanged=unchanged,
                         known_findings="CM3-006 contract mismatch; CM3-030 interpretation review; CM3-028/029 inactive charge gate",
                         stdout=result.stdout, stderr=result.stderr, compiler=str(compiler), flags=flags)
            if result.returncode or not unchanged or not records:
                raise RuntimeError("CM3 harness failed or source changed; see cm3-vectors/checks.json")


if __name__ == "__main__":
    main()
