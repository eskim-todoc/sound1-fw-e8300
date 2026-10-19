"""PC-only CM3 batch contract checks. No board download or full SDK build."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

import tdc_ut_io_cm3_build as io_build

ROOT = Path(__file__).resolve().parents[2]
TEST = ROOT / "tests/unit"
SOURCE = ROOT / "src/2__cm3/Cortex-M3-src/test"


def run(argv: list[str]) -> str:
    result = subprocess.run(argv, cwd=ROOT, capture_output=True, text=True,
                            encoding="utf-8", errors="replace", timeout=60)
    if result.returncode:
        raise RuntimeError(f"Command failed ({result.returncode}): {argv[0]}\n{result.stdout}\n{result.stderr[:6000]}")
    return result.stdout


def main() -> None:
    gcc, nm = shutil.which("gcc"), shutil.which("nm")
    if not gcc or not nm:
        raise RuntimeError("gcc and nm are required")
    inputs = [Path(__file__), TEST / "cm3_batch_host_test.c",
              SOURCE / "tdc_ut_batch_cm3.c", SOURCE / "tdc_ut_batch_cm3.h",
              SOURCE / "tdc_ut_report.c", SOURCE / "tdc_ut_report.h",
              ROOT / "src/test/tdc_ut_log.c", ROOT / "src/test/tdc_ut_log.h",
              *sorted((TEST / "cm3_batch_host").glob("*.h"))]
    hashes = lambda: {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    before = hashes()
    io_inputs = [io_build.CM3 / p for p in io_build.PRODUCTS] + [io_build.RUNNER, io_build.FIXTURE,
                 TEST / "cm3_batch_io_test.c", TEST / "cm3_batch_io_host/hw.h",
                 SOURCE / "tdc_ut_batch_cm3.c", SOURCE / "tdc_ut_batch_cm3.h",
                 SOURCE / "tdc_ut_report.c"]
    with tempfile.TemporaryDirectory(prefix="tdc-cm3-batch-") as temp:
        build = Path(temp)
        exe, normal = build / "batch.exe", build / "normal.o"
        run([gcc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-DTDC_UNIT_TEST",
             "-I", str(TEST / "cm3_batch_host"), str(TEST / "cm3_batch_host_test.c"), "-o", str(exe)])
        output = run([str(exe)])
        run([gcc, "-c", str(SOURCE / "tdc_ut_batch_cm3.c"), "-o", str(normal)])
        symbols = run([nm, "--defined-only", str(normal)])
        if "tdc_ut_batch" in symbols:
            raise RuntimeError("Normal object contains batch symbols")
        flags = [gcc, "-std=gnu11", "-DTDC_UNIT_TEST", "-DTDC_UT_IO_CM3_HOST",
                 "-DTDC_UT_IO_CM3_RAM_ONLY", "-DTDC_UT_IO_CM3_CHECK_KNOWN_FINDINGS",
                 "-DSK5_CID=101", "-Dchess_storage(a)=", "-ffunction-sections", "-fdata-sections",
                 "-fno-asynchronous-unwind-tables", "-fno-unwind-tables",
                 "-I", str(TEST / "cm3_batch_io_host"),
                 "-I", str(TEST / "tdc_ut_io_cm3_host")]
        for path in io_build.includes():
            flags += ["-I", str(path)]
        for path in ("include/cm3", "include/shared"):
            flags += ["-isystem", str(io_build.SDK / path)]
        sources = [io_build.CM3 / name for name in io_build.PRODUCTS] + [io_build.RUNNER]
        sources += [io_build.CM3 / name for name in (
            "Cortex-M3-src/systemControl/cfx_cm3_sharedMemory.c",
            "Cortex-M3-src/signalProcessing/commonDataProcessing.c",
            "Cortex-M3-src/internalDevice/isd_interface.c",
            "Cortex-M3-src/internalDevice/isd_interface_stimulationParaSetting.c",
            "Cortex-M3-src/systemControl/batteryNPowerControl.c",
            "Cortex-M3-src/systemControl/LedOutput.c", "Cortex-M3-src/error.c",
            "Cortex-M3-src/QCC/snd_qcc.c")]
        sources += [SOURCE / "tdc_ut_batch_cm3.c", SOURCE / "tdc_ut_report.c", TEST / "cm3_batch_io_test.c"]
        io_inputs += sources
        io_before = io_build.source_hashes(io_inputs)
        io_exe = build / "batch_io.exe"
        run([*flags, "-O0", "-Wl,--gc-sections", *map(str, sources), "-o", str(io_exe)])
        io_output = run([str(io_exe)])
    if before != hashes():
        raise RuntimeError("Input files changed during verification")
    if io_before != io_build.source_hashes(io_inputs):
        raise RuntimeError("IO source inputs changed during verification")
    report = dict(schema_version=1, batch_contract_pass=True, device_executed=False,
                  product_acceptance=False, source_sha256=before, stdout=output,
                  normal_symbol_isolation=True)
    report.update(real_io_fixture_pass=True, real_io_stdout=io_output, io_source_sha256=io_before,
                  io_known_mismatches=4, automatic_io_units=12, snapshot_units=24)
    destination = ROOT / "tmp/unit-tests/cm3-batch/checks.json"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(output, end="")
    print(io_output, end="")
    print(f"CM3_BATCH evidence: {destination}")


if __name__ == "__main__":
    main()
