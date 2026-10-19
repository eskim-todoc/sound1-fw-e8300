"""Check BOOT safe-RAM batches, ownership and RTT drain without a board.

Compiles the real target RAM vectors/product functions with existing host
boundaries, compares batch logs with independent single-unit records, and
injects control/transport failures. This does not run a full SDK link/build.
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
import tempfile
import traceback

import tdc_ut_boot_check as single

ROOT = Path(__file__).resolve().parents[2]
BOOT = ROOT / "src/0__bootloader"
TEST = BOOT / "source/test"
GROUPS = {1: [8, 9, 10, 11, 12, 13, 14, 19, 20],
          2: [8, 9, 10, 11, 12, 19, 20], 3: [13, 14]}
EVIDENCE: dict = {"commands": [], "target_executed": False, "full_sdk_build": False}


def execute(cmd: list[str]) -> str:
    entry: dict = {"command": cmd}
    EVIDENCE["commands"].append(entry)
    result = subprocess.run(cmd, cwd=ROOT, capture_output=True, timeout=90)
    stdout = result.stdout.decode("utf-8", errors="replace")
    stderr = result.stderr.decode("utf-8", errors="replace")
    entry.update(returncode=result.returncode, stdout=stdout, stderr=stderr)
    if result.returncode:
        raise RuntimeError(f"{cmd}\n{stdout}\n{stderr[-8000:]}")
    return stdout


def verify_capture(output: str, group: int, reference: dict, cancelled: bool = False) -> None:
    raw, status = output.split("HOST_STATUS ")
    assert status.strip() == (f"G={group} RC={-4 if cancelled else 1} "
                              f"COMPLETED={3 if cancelled else len(GROUPS[group])}")
    lines = raw.splitlines(keepends=True)
    assert lines and all(s.endswith("\r\n") and len(s.encode("ascii")) <= 60 for s in lines)
    assert lines[0] == f"[UT-BATCH] CORE=BOOT BEGIN N={len(GROUPS[group])}\r\n"
    if cancelled:
        assert lines[-1] == "[UT-BATCH] CORE=BOOT CANCEL N=3\r\n"
        assert not any(" CORE=BOOT END " in s for s in lines)
        return
    assert lines[-1] == f"[UT-BATCH] CORE=BOOT END N={len(GROUPS[group])}\r\n"
    values: dict = {}
    summaries: dict = {}
    differences: dict = {}
    for line in lines[1:-1]:
        value = re.fullmatch(r"\[UT-BOOT-(\d{3})\] C(\d+) A\[(\d+):(\d+)\]=(-?\d+(?:,-?\d+)*)\r\n", line)
        difference = re.fullmatch(r"\[UT-BOOT-(\d{3})\] C(\d+) I(\d+) A=(-?\d+) E=(-?\d+)\r\n", line)
        summary = re.fullmatch(r"\[UT-BOOT-(\d{3})\] C(\d+) (PASS|FAIL) N=(\d+) F=(\d+)\r\n", line)
        if value:
            unit, case, first, last = map(int, value.groups()[:4])
            key = (unit, case)
            vector = values.setdefault(key, [])
            chunk = list(map(int, value[5].split(",")))
            assert first == len(vector) and last-first+1 == len(chunk)
            vector.extend(chunk)
        elif difference:
            unit, case, index, actual, expected = map(int, difference.groups())
            key = (unit, case)
            diffs = differences.setdefault(key, {})
            assert index not in diffs
            diffs[index] = (actual, expected)
        elif summary:
            unit, case, passed, count, failed = summary.groups()
            key = (int(unit), int(case))
            assert key not in summaries
            summaries[key] = (passed, int(count), int(failed))
        else:
            raise AssertionError(f"malformed/extra log: {line!r}")
    expected_records = {key: r for key, r in reference.items() if key[0] in GROUPS[group]}
    assert set(values) == set(summaries) == set(expected_records)
    assert list(values) == list(expected_records)
    assert set(differences) <= set(expected_records)
    assert list(dict.fromkeys(key[0] for key in values)) == GROUPS[group]
    for key, record in expected_records.items():
        actual, expected = record["actual"], record["expected"]
        diffs = {i: (a, e) for i, (a, e) in enumerate(zip(actual, expected)) if a != e}
        assert values[key] == actual
        assert differences.get(key, {}) == diffs
        assert summaries[key] == ("FAIL" if diffs else "PASS", len(actual), len(diffs))
    assert not any("[UT-BATCH]" in s and "PASS" in s for s in lines)


def captures(output: str) -> list[str]:
    chunks = re.findall(r".*?HOST_STATUS[^\r\n]*\n", output, flags=re.S)
    assert "".join(chunks) == output
    return chunks


def check(sdk: Path) -> None:
    if not __debug__:
        raise RuntimeError("Run without Python -O: checker assertions must be enabled")
    gcc = shutil.which("gcc")
    if not gcc:
        raise RuntimeError("gcc is not on PATH")
    sources = [BOOT / "source/bootloader.c", BOOT / "source/bootloader_crc.c",
               BOOT / "source/bootmgr/ci_boot.c", BOOT / "source/service/service.c",
               BOOT / "source/led/tdc_boot_led.c", BOOT / "source/tdc_util.c",
               TEST / "tdc_ut_boot.c", BOOT / "tiny-AES-c/aes.c",
               TEST / "tdc_ut_boot_report.c", TEST / "tdc_ut_batch_boot.c",
               ROOT / "tests/unit/boot_batch_ram_fixture.c"]
    inputs = sorted({*BOOT.rglob("*.c"), *BOOT.rglob("*.h"),
                     *BOOT.glob("*.ld"), ROOT / "src/test/tdc_ut_log.c",
                     ROOT / "src/test/tdc_ut_log.h", Path(__file__).resolve(),
                     ROOT / "tests/unit/boot_batch_state_fixture.c",
                     ROOT / "tests/unit/boot_batch_ram_fixture.c",
                     ROOT / "tests/unit/tdc_ut_boot_host.c",
                     ROOT / "tests/unit/tdc_ut_boot_host.h",
                     ROOT / "tests/unit/tdc_ut_boot_check.py",
                     *(ROOT / "tests/unit/tdc_ut_boot_mock").glob("*.h"),
                     ROOT / "tests/unit/tdc_ut_report_host/SEGGER_RTT.h"})
    EVIDENCE["inputs"] = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in inputs}
    single.check_oracle_guards()
    with tempfile.TemporaryDirectory(prefix="tdc-boot-batch-") as temp:
        build = Path(temp)
        for optimization in ("-O0", "-O2"):
            state = build / f"state{optimization}.exe"
            execute([gcc, "-std=c99", optimization, "-Wall", "-Wextra", "-Werror",
                     "-DTDC_UNIT_TEST", "-Itests/unit/tdc_ut_report_host",
                     "-Itests/unit/tdc_ut_boot_mock", "-Isrc/0__bootloader/include",
                     f"-I{TEST}", "tests/unit/boot_batch_state_fixture.c",
                     str(TEST / "tdc_ut_batch_boot.c"), str(TEST / "tdc_ut_boot_report.c"),
                     "-o", str(state)])
            assert "HOST_PASS" in execute([str(state)])
            ram = build / f"ram{optimization}.exe"
            flags = [gcc, "-std=gnu11", optimization, "-Wall", "-Wextra", "-Wno-unused-parameter",
                     "-DTDC_UNIT_TEST", "-DTDC_UNIT_TEST_HOST", "-DTDC_UNIT_TEST_RAM_ONLY",
                     "-DTDC_BOOT_LED_ENABLE=1", "-DSK5_CID=101", "-Dchess_storage(a)=",
                     "-Itests/unit/tdc_ut_boot_mock", "-Itests/unit"]
            flags += single.include_flags(sdk)
            execute(flags + [str(p) for p in sources] + ["-o", str(ram)])
            reference = {}
            for unit in GROUPS[1]:
                status, records = single.parse_output(execute([str(ram), "direct", str(unit)]), unit)
                assert status["ret"] == 0 and status["done"] == 1 and not status["overflow"]
                assert len(records) == single.RAM_RECORDS[unit]
                reference.update({(r["unit"], r["case"]): r for r in records})
            assert len(reference) == 80
            failures = single.verify_failures(list(reference.values()), single.RAM_FAILURES)
            for mode, groups in (("repeat", [1, 1]), ("groups", [2, 3]), ("cancel", [1, 1])):
                output = execute([str(ram), mode])
                blocks = captures(output)
                assert len(blocks) == 2
                for index, (block, group) in enumerate(zip(blocks, groups)):
                    verify_capture(block, group, reference, cancelled=mode=="cancel" and index==0)
                if mode == "repeat":
                    valid = blocks[0]
                    lost = next(s for s in valid.splitlines(keepends=True) if " C108 I0 " in s)
                    mutations = {
                        "lost_mismatch": valid.replace(lost, ""),
                        "false_end_count": valid.replace("END N=9", "END N=8"),
                        "masked_failure": valid.replace("C108 FAIL N=1 F=1", "C108 PASS N=1 F=0"),
                        "missing_end": valid.replace("[UT-BATCH] CORE=BOOT END N=9\r\n", ""),
                        "extra_mismatch": valid.replace("[UT-BATCH] CORE=BOOT END N=9",
                            "[UT-BOOT-008] C999 I0 A=0 E=1\r\n[UT-BATCH] CORE=BOOT END N=9"),
                    }
                    for name, mutated in mutations.items():
                        try:
                            verify_capture(mutated, 1, reference)
                        except AssertionError:
                            EVIDENCE.setdefault("capture_guards", []).append(
                                {"optimization": optimization, "name": name, "rejected": True})
                        else:
                            raise AssertionError(f"accepted damaged capture: {name}")
            EVIDENCE.setdefault("runs", []).append({"optimization": optimization,
                "records": len(reference), "known_failures": failures,
                "groups": GROUPS, "real_ram_restore_sentinels": True,
                "repeat_cancel_resume": True})
        # Existing standalone reporter must still compile and retain behavior.
        reporter = build / "reporter.exe"
        execute([gcc, "-std=c99", "-Wall", "-Wextra", "-Werror", "-DTDC_UNIT_TEST",
                 "-Itests/unit/tdc_ut_report_host", "-Itests/unit/tdc_ut_boot_mock",
                 "-Isrc/0__bootloader/include", "tests/unit/tdc_ut_boot_report_test.c",
                 "-o", str(reporter)])
        assert "HOST_PASS" in execute([str(reporter)])
        # Disabled macro must add no test API symbols; enabled object small/no heap.
        for enabled in (False, True):
            obj = build / f"batch-{enabled}.o"
            flags = [gcc, "-std=c99", "-O0", "-Wall", "-Wextra", "-Werror", "-fstack-usage",
                     "-Itests/unit/tdc_ut_report_host", "-Itests/unit/tdc_ut_boot_mock",
                     "-Isrc/0__bootloader/include"]
            if enabled:
                flags += ["-DTDC_UNIT_TEST"]
            execute(flags + ["-c", str(TEST / "tdc_ut_batch_boot.c"), "-o", str(obj)])
            symbols = execute([shutil.which("nm") or "nm", "-g", str(obj)])
            if not enabled:
                assert not symbols.strip()
            else:
                assert "malloc" not in symbols and "calloc" not in symbols and "free" not in symbols
                EVIDENCE["host_object_size"] = execute([shutil.which("size") or "size", str(obj)])
                EVIDENCE["host_stack_frames"] = obj.with_suffix(".su").read_text()
                assert max(int(s.split("\t")[1]) for s in EVIDENCE["host_stack_frames"].splitlines()) <= 128
                sections = execute([shutil.which("objdump") or "objdump", "-h", str(obj)])
                assert ".tdc_ut_entry" in sections
    EVIDENCE["inputs_unchanged_during_check"] = all(
        hashlib.sha256((ROOT / name).read_bytes()).hexdigest()==digest
        for name, digest in EVIDENCE["inputs"].items())
    assert EVIDENCE["inputs_unchanged_during_check"]
    EVIDENCE["passed"] = True


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, default=single.SDK_DEFAULT)
    parser.add_argument("--output", type=Path, default=ROOT / "tmp/unit-tests/boot-batch/checks.json")
    args = parser.parse_args()
    result = 1
    try:
        check(args.sdk)
        result = 0
        print("BOOT_BATCH HOST_PASS: O0/O2, 80 RAM records, 3 known FAIL records preserved")
    except Exception:
        EVIDENCE["error"] = traceback.format_exc()
        print(EVIDENCE["error"], file=sys.stderr)
    finally:
        EVIDENCE["exit_code"] = result
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(EVIDENCE, ensure_ascii=False, indent=2), encoding="utf-8")
        print(f"EVIDENCE {args.output}")
    sys.exit(result)
