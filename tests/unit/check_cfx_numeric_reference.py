"""Check independent numeric bounds, host discriminators and optional SDK compile.

--sdk compiles ONLY the owned CFX runner in an isolated directory; no parent
target build directory, Eclipse output, device, normal product source or
hardware configuration is changed. Host adapters are never reported as SDK
accuracy evidence. Rejected perturbations prove the prepared comparison can
detect errors, not that the target has passed.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import subprocess

import build_target_tests
import cfx_numeric_reference as reference
import check_cfx_vectors

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "tmp/unit-tests/cfx-numeric-reference"


def run(command, directory):
    result = subprocess.run(list(map(str, command)), cwd=directory, capture_output=True,
                            text=True, errors="replace", timeout=240)
    return dict(command=list(map(str, command)), returncode=result.returncode,
                stdout=result.stdout, stderr=result.stderr)


def sdk_compile():
    sdk = Path(os.environ.get("TDC_E8300_SDK", "C:/Program Files (x86)/ON Semiconductor/Ezairo 8300 SDK"))
    project = ROOT / "src/1__cfx"
    model = sdk / "cf624c-a20_vU-2023.06/lib"
    _, includes = build_target_tests.configuration(project, "CFX")
    includes += [p for p in project.iterdir() if p.is_dir() and p.name != "Debug"]
    includes += [sdk / "include/shared", sdk / "include/cfx", model / "runtime/include"]
    command = [sdk / "ASIP Programmer/U-2023.06-SP1/win64/bin/WINbin/chesscc.exe",
               "-p", "cf624c", "-P", model, "-g", "-DSK5_CID=101", "-DHARDWARE=0",
               "-DISS=1", "-DTARGET=HARDWARE", "-DCFX_CORE=1", "-DTDC_UNIT_TEST"]
    for folder in dict.fromkeys(includes):
        command += ["-I", folder]
    command += ["-c", project / "test/tdc_ut_vectors_cfx.c", "-o", OUT / "numeric_runner.o"]
    record = run(command, OUT)
    if record["returncode"]:
        raise RuntimeError(record["stdout"] + record["stderr"])
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", action="store_true")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    data = reference.references()
    assert reference.HEADER.read_text(encoding="utf-8") == reference.render_header(data)
    # An independently analytic power must fit the SDK/quantization interval.
    # Use the exact configured p/normalization, not an output from the runner.
    for x in (1, 8, 16, 64, 128, 512, 4096, 16384, 262143):
        normalized = x * 32 / 2**47
        value = normalized ** (reference.P_Q23 / 2**23) * reference.NORMALIZE_Q16
        lo, hi = reference.power_bounds(x)
        assert lo <= math.floor(value) <= hi, (x, value, lo, hi)
        assert hi - lo < 20, "an overly broad power tolerance would hide errors"
    for row in data["agc"]:
        assert all(lo <= hi for lo, hi in row["bounds"])
    check_cfx_vectors.main()
    gcc = shutil.which("gcc")
    if not gcc:
        raise RuntimeError("gcc unavailable")
    mutations = []
    for name, define in (("log_plus_1dB", "HOST_NUMERIC_LOG_BIAS=65536"),
                         ("exp_times_0p8", "HOST_NUMERIC_EXP_PERCENT=80")):
        exe = OUT / (name + ".exe")
        command = [gcc, "-std=c99", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function",
                   "-D" + define, check_cfx_vectors.OUT / "vectors.c", "-o", exe]
        build = run(command, OUT)
        assert build["returncode"] == 0, build
        rejected = run([exe], OUT)
        assert rejected["returncode"] != 0 and "g_tdc_ut_cfx_vector_failed==0" in rejected["stderr"], rejected
        mutations.append(dict(name=name, build=build, execution=rejected, expected="rejected numeric mismatch"))
        print(f"CFX NUMERIC MUTATION_REJECTED {name}")
    sdk = sdk_compile() if args.sdk else None
    paths = ["src/1__cfx/test/tdc_ut_vectors_cfx.c", "src/1__cfx/test/tdc_ut_vectors_cfx.h",
             "src/1__cfx/test/tdc_ut_numeric_reference_cfx.h", "src/1__cfx/signalProcessing/agc.c",
             "src/1__cfx/signalProcessing/nonlinearMapping.c", "tests/unit/check_cfx_vectors.py",
             "tests/unit/cfx_numeric_reference.py", "tests/unit/check_cfx_numeric_reference.py"]
    report = dict(scope="independent references + host control/function execution + optional compile, no SDK execution",
                  target_executed=False, units=[23, 31, 32], numeric_checks={"23": 36, "31": 64, "32": 128},
                  inputs=data, mutations=mutations, sdk_compile=sdk,
                  numeric_layout={"record": "-999/unit/case/length, then triples actual/lower/upper",
                                  "23": "C10..C21: goal Q8.16, smoothed Q8.16, gain Q12.12",
                                  "31": "C10: channel0 A Q16.8/B Q16.8, through channel31",
                                  "32": "C10 mute1/C11 mute2: channel0 PCM/shared code, through channel31",
                                  "pass_records": "same cases in vector arrays: one in-range predicate per numeric triple"},
                  sources={p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in paths})
    for p in paths[:3]:
        (ROOT / p).read_text(encoding="utf-8").encode("cp949")
    (OUT / "checks.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print("CFX NUMERIC_HARNESS_OK; SDK compile=" + str(sdk is not None) + "; target execution=0")


if __name__ == "__main__":
    main()
