"""SYS-080 isolated RAM FatFs boundary tests; no device or user-data access.

Runs the current product reader body verbatim plus the actual ci_crc.c byte
feed. Only FatFs and CRC MMIO are modeled. AES is disabled and aborts if used.
Executes the real private-file arm/check/clear logic; no boot/default-install
model or ci_map init is called. Legacy restore uses independent backup fixtures.
Generated artifacts and persistent QA evidence stay under tmp/system-tests/sys80.
"""
from __future__ import annotations

import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
CM3 = ROOT / "src/2__cm3"
EVIDENCE = ROOT / "tmp/system-tests/sys80/checks.json"
DEFAULT_FIELDS = ("mapNum", "stimulVolume", "audioVolume", "indicatorLED_OnOff",
                  "indicatorStimul_OnOff", "teleCoil_OnOff", "Ble_Onff")
DEFAULT_VALUES = (1, 4, 1, 1, 1, 2, 1)


def verify_defaults(map_source: str, module_source: str) -> dict:
    """Fail on default drift instead of copying implementation output as an oracle."""
    source = re.sub(r"/\*.*?\*/|//[^\n]*", "", map_source, flags=re.S)
    match = re.search(r"if\s*\(\(validate_user_setting\s*<\s*0\)\s*\|\|\s*"
                      r"\(force_init\)\)\s*\{([^{}]*)\}", source, re.S)
    if match is None:
        raise AssertionError("ci_map user-setting recovery block changed; inspect defaults")
    assignments = re.findall(r"p_user_setting->(\w+)\s*=\s*(\d+)\s*;", match[1])
    expected = list(zip(DEFAULT_FIELDS, map(str, DEFAULT_VALUES)))
    if assignments != expected:
        raise AssertionError(f"ci_map recovery defaults drifted: {assignments!r}")
    array = re.search(r"s_tdc_default\s*\[7\]\s*=\s*\{([^}]+)\}", module_source)
    if array is None or tuple(int(n.strip().rstrip("uU")) for n in array[1].split(",")) != DEFAULT_VALUES:
        raise AssertionError("SYS80 defaults differ from the contract/ci_map recovery values")
    return dict(zip(DEFAULT_FIELDS, DEFAULT_VALUES))


def reader_body(source: str) -> str:
    """Keep the complete current function, including disabled branches/comments."""
    start = re.search(r"^int ci_filesystem_read_with_crc_and_aes128\s*\(", source, re.M)
    end = re.search(r"^int ci_filesystem_write_with_crc_and_aes128\s*\(", source, re.M)
    if start is None or end is None or end.start() <= start.start():
        raise RuntimeError("product reader boundaries changed; inspect before running")
    body = source[start.start():end.start()].rstrip()
    if not body.endswith("}"):
        raise RuntimeError("unexpected text between product reader and writer")
    return body + "\n"


def hashes(paths: list[Path]) -> dict[str, str]:
    return {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in paths}


def invoke(command: list[str], evidence: dict, stage: str, timeout: int = 45) -> str:
    entry = {"stage": stage, "command": command}
    evidence["commands"].append(entry)
    try:
        result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8",
                                errors="replace", timeout=timeout)
    except subprocess.TimeoutExpired as exc:
        entry["error"] = str(exc)
        entry["timed_out"] = True
        raise
    entry.update(returncode=result.returncode, stdout=result.stdout, stderr=result.stderr)
    if stage == "host_execute":
        evidence.update(stdout=result.stdout, stderr=result.stderr, returncode=result.returncode)
    if result.stdout:
        print(result.stdout, end="", flush=True)
    if result.stderr:
        print(result.stderr, end="", file=sys.stderr, flush=True)
    if result.returncode:
        raise RuntimeError(f"{stage} returned {result.returncode}")
    return result.stdout


def run() -> None:
    product = CM3 / "Gen1_5/FS/ci_filesystem.c"
    crc = CM3 / "Gen1_5/crc/ci_crc.c"
    module = CM3 / "Cortex-M3-src/test/tdc_sys_test_80.c"
    common_log = module.with_name("tdc_sys_test_storage.c")
    map_source = CM3 / "Gen1_5/FS/ci_map.c"
    sources = [product, product.with_suffix(".h"), crc, module, module.with_suffix(".h"), common_log,
               module.with_name("tdc_sys_test_storage.h"), Path(__file__),
               map_source, HERE / "sys80_host.c", *sorted((HERE / "sys80_host").glob("*.h"))]
    evidence = {
        "schema_version": 2, "complete": False, "device_executed": False,
        "boundary_checks_pass": False, "product_system_test_pass": False,
        "started_at_utc": datetime.now(timezone.utc).isoformat(),
        "run_command": [sys.executable, *sys.argv], "commands": [],
        "input_hashes": {}, "input_hashes_after": {}, "inputs_unchanged": False,
        "reader_body_sha256": None, "observer_source_sha256": None,
        "observer_header_sha256": None, "stdout": "", "stderr": "", "returncode": None,
        "scope": ["SYS80 private-file arm/check/clear in the same session on RAM FatFs",
                  "Current product reader body, gated observer and public observer declarations verbatim",
                  "Actual ci_crc.c byte feed with CCITT MMIO model",
                  "Actual common logger, 59/60-byte acceptance and over-limit rejection",
                  "ci_map recovery default drift detection against the seven-field contract",
                  "Injected failures, short IO, foreign/partial files and private-file isolation",
                  "Independent legacy CRC32 backup fixtures and exact 32/97/4096-byte restoration",
                  "Every reachable close failure rejects all later FS IO and false success"],
        "limitations": ["No physical device commands were executed",
                        "No reboot/default-install model or ci_map_init_map_data_all execution",
                        "CRC MMIO and FatFs are host models, not hardware/SDK filesystem validation",
                        "Flash power-loss durability, actual reboot and UI/application remain untested",
                        "Production return handling is unchanged; extra close rejection is test-gated",
                        "Main shared command guard integration and full call-chain stack are not covered"],
    }
    EVIDENCE.parent.mkdir(parents=True, exist_ok=True)
    failure = None
    try:
        evidence["input_hashes"] = hashes(sources)
        evidence["defaults_source_verified"] = verify_defaults(
            map_source.read_text(encoding="utf-8-sig"), module.read_text(encoding="utf-8-sig"))
        print("DEFAULTS SOURCE: ci_map recovery = 1,4,1,1,1,2,1", flush=True)
        compiler = shutil.which("gcc")
        nm = shutil.which("nm")
        if compiler is None or nm is None:
            raise RuntimeError("gcc and nm are required on PATH")
        fs_source = product.read_text(encoding="utf-8-sig")
        body = reader_body(fs_source)
        # Keep actual observer definitions/macros ahead of the actual reader.
        preamble = fs_source[:fs_source.index("CI_FILESYSTEM_FFT_PASS_BIN_T *")]
        if "tdc_fs_test_observe_begin" not in preamble:
            raise RuntimeError("shared FS observer has not been integrated")
        header_source = product.with_suffix(".h").read_text(encoding="utf-8-sig")
        start = re.search(r"^#if[^\n]*TDC_SYS_TEST_77_ENABLE[^\n]*\n", header_source, re.M)
        if start is None:
            raise RuntimeError("shared observer declaration gate changed; inspect before running")
        observer_header = header_source[start.start():header_source.index("\n#endif", start.end()) + 7]
        evidence["reader_body_sha256"] = hashlib.sha256(body.encode()).hexdigest()
        evidence["observer_source_sha256"] = hashlib.sha256(preamble.encode()).hexdigest()
        evidence["observer_header_sha256"] = hashlib.sha256(observer_header.encode()).hexdigest()
        print("READER body SHA256=" + evidence["reader_body_sha256"], flush=True)
        # This new temporary directory is an owned child of the explicit QA path.
        with tempfile.TemporaryDirectory(prefix="host-build-", dir=EVIDENCE.parent) as tmp:
            build = Path(tmp)
            extracted = build / "product_reader.c"
            extracted.write_text(preamble + body, encoding="utf-8")
            (build / "product_observer_types.inc").write_text(observer_header + "\n", encoding="utf-8")
            log_source = common_log.read_text(encoding="utf-8-sig")
            log_body = log_source[log_source.index("#define TDC_STORAGE_LINES "):
                                  log_source.index("void tdc_sys_storage_hex(")]
            evidence["log_body_sha256"] = hashlib.sha256(log_body.encode()).hexdigest()
            (build / "product_log.inc").write_text(log_body, encoding="utf-8")
            exe = build / "sys80_host.exe"
            flags = [compiler, "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                     "-Wno-unused-variable", "-Wno-unused-function",
                     "-I", str(HERE / "sys80_host"), "-I", str(build),
                     "-I", str(CM3 / "Cortex-M3-src")]
            invoke(flags + [str(HERE / "sys80_host.c"), str(extracted), str(crc),
                            "-o", str(exe)], evidence, "host_compile")
            output = invoke([str(exe)], evidence, "host_execute")
            result = re.search(r"SYS80 HOST PASS: scenarios=(\d+) assertions=(\d+) module_max_row=(\d+)B max_reply=(\d+)\+1 lines", output)
            if result is None:
                raise AssertionError("host exited without a complete result summary")
            evidence["metrics"] = dict(zip(("scenarios", "assertions", "module_max_row_bytes", "module_max_reply_lines"),
                                           map(int, result.groups())))
            evidence["metrics"]["common_extra_lines"] = 1
            closes = re.search(r"CLOSE GUARD: reader=(\d+) direct=(\d+) error_cleanup=(\d+)", output)
            if closes is None or any(int(n) == 0 for n in closes.groups()):
                raise AssertionError("reader/direct/error-cleanup close rejection coverage is incomplete")
            evidence["metrics"].update(zip(
                ("reader_close_failures_rejected", "direct_close_failures_rejected", "error_cleanup_close_failures_rejected"),
                map(int, closes.groups())))
            # Disabled macro must remove implementation and storage, including the public guard.
            off = build / "sys80_off.o"
            invoke(flags + ["-DTDC_SYS_TEST_80_ENABLE=0", "-c", str(module),
                            "-o", str(off)], evidence, "off_compile")
            symbols = invoke([nm, str(off)], evidence, "off_symbols", timeout=15)
            if any(name in symbols for name in ("tdc_sys_test_80_execute", "s_tdc_", "tdc_sys_test_80_requires_reboot")):
                raise AssertionError("disabled build leaked SYS80 code/storage")
            evidence["gate_off_symbols_absent"] = True
            print("GATE OFF: no SYS80 command/buffer/guard symbols")
        for path in [module, module.with_suffix(".h"), HERE / "sys80_host.c",
                     *sorted((HERE / "sys80_host").glob("*.h"))]:
            text = path.read_text(encoding="utf-8-sig")
            if text.encode("cp949").decode("cp949") != text:
                raise AssertionError(f"CP949 round trip changed {path.name}")
        evidence["cp949_round_trip_pass"] = True
        evidence["complete"] = True
    except Exception as exc:
        failure = exc
        evidence["error"] = f"{type(exc).__name__}: {exc}"
    finally:
        evidence["input_hashes_after"] = hashes(sources)
        evidence["inputs_unchanged"] = evidence["input_hashes"] == evidence["input_hashes_after"]
        if not evidence["inputs_unchanged"]:
            evidence["complete"] = False
            failure = RuntimeError("inputs changed during run; rerun on a stable baseline")
            evidence["error"] = str(failure)
        evidence["boundary_checks_pass"] = evidence["complete"] and failure is None
        evidence["finished_at_utc"] = datetime.now(timezone.utc).isoformat()
        EVIDENCE.write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"EVIDENCE {EVIDENCE}: complete={evidence['complete']} device_executed=false")
    if failure is not None:
        raise SystemExit(str(failure))
    print("SOURCE unchanged; CP949 round-trip OK; no device execution")


if __name__ == "__main__":
    run()
