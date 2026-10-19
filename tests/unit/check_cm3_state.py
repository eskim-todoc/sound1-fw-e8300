"""CM3-30 actual setter host test / isolated ARM object compile; never flashes."""
from __future__ import annotations

import argparse
import json
import pathlib
import shutil
import subprocess
import tempfile

from tdc_ut_cm3_run import CM3, INCLUDES, ROOT, SDK_DEFAULT

SOURCES = (
    "Cortex-M3-src/internalDevice/isd_interface.c",
    "Cortex-M3-src/systemControl/cfx_cm3_sharedMemory.c",
    "Cortex-M3-src/test/tdc_ut_state_cm3.c",
)


def run(command: list[str]) -> str:
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                            encoding="utf-8", errors="replace", timeout=60)
    if result.stdout:
        print(result.stdout, end="")
    if result.returncode:
        print(result.stderr)
        raise RuntimeError(f"exit={result.returncode}: {subprocess.list2cmdline(command)}")
    return result.stdout


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", action="store_true")
    parser.add_argument("--normal", action="store_true")
    parser.add_argument("--sdk", type=pathlib.Path, default=SDK_DEFAULT)
    args = parser.parse_args()
    if args.normal and not args.target:
        parser.error("--normal requires --target")
    compiler = args.sdk / "arm_tools/bin/arm-none-eabi-gcc.exe" if args.target else shutil.which("gcc")
    if not compiler:
        raise RuntimeError("gcc unavailable")
    flags = ["-std=gnu11", "-DSK5_CID=101", "-Dchess_storage(a)=", "-O0",
             "-ffunction-sections", "-fdata-sections", "-fmax-errors=5"]
    if not args.normal:
        flags += ["-DTDC_UNIT_TEST"]
    flags += ["-I" + str(CM3 / folder) for folder in INCLUDES]
    for folder in ("include/cm3", "include/shared"):
        flags += ["-isystem", str(args.sdk / folder)]
    if args.target:
        flags += ["-mcpu=cortex-m3", "-mthumb"]
    else:
        flags += ["-fno-asynchronous-unwind-tables", "-fno-unwind-tables",
                  "-DTDC_UT_CM3_HOST", "-include", str(ROOT / "tests/unit/tdc_ut_cm3_host_prelude.h")]
    for relative in SOURCES:
        source = (CM3 / relative).read_text(encoding="utf-8")
        source.encode("cp949")
        assert not any(char in source for char in "\u2014\u2013\u2248\u00b5\u25ba")
    with tempfile.TemporaryDirectory(prefix="tdc-cm3-state-") as output:
        directory = pathlib.Path(output)
        objects = []
        for index, relative in enumerate(SOURCES):
            obj = directory / f"state_{index}.o"
            run([str(compiler), *flags, "-c", str(CM3 / relative), "-o", str(obj)])
            objects.append(str(obj))
        if args.target:
            if args.normal:
                symbols = run([str(args.sdk / "arm_tools/bin/arm-none-eabi-nm.exe"),
                               "-g", "--defined-only", *objects])
                assert "tdc_ut_cm3_isd_state" not in symbols and "tdc_ut_cm3_state_run" not in symbols
            print(f"ARM_OBJECTS_OK: {len(objects)} mode={'normal' if args.normal else 'test'}; no link/device execution")
        else:
            exe = directory / "state.exe"
            run([str(compiler), *flags, str(ROOT / "tests/unit/tdc_ut_state_cm3_test.c"),
                 *objects, "-Wl,--gc-sections,--exclude-all-symbols", "-o", str(exe)])
            run([str(exe)])
    coverage = json.loads((ROOT / "tests/unit/cm3_state_coverage.json").read_text(encoding="utf-8"))
    assert coverage["unit"] == 30 and coverage["whole_unit_pass"] is False


if __name__ == "__main__":
    main()
