"""BOOT 실제 제품 함수의 호스트 fixture 실행과 ARM 컴파일을 확인한다.

제품 보드에 연결하지 않는다. 실행 fixture는 tempfile 안에 생성하며,
실패를 포함한 명령/원시 출력/비교/입력 해시를 --output JSON에 보존한다.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import sys
import traceback
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[2]
BOOT = ROOT / "src/0__bootloader"
SDK_DEFAULT = Path("C:/Program Files (x86)/ON Semiconductor/Ezairo 8300 SDK")
RUNNABLE = (2, 4, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20)
ANALYSIS = (1, 3, 5, 6)
RAM_RECORDS = {8: 6, 9: 5, 10: 13, 11: 14, 12: 11, 13: 10, 14: 4, 19: 10, 20: 7}
KNOWN_FAILURES = {(7, 6): ([0], [-1]), (7, 17): ([0, 0, 0], [-9, 1, 0]),
                  (11, 6): ([0], [-1]), (11, 108): ([0], [-1]),
                  (11, 109): ([0], [-1]), (11, 110): ([1], [-1])}
RAM_FAILURES = {key: value for key, value in KNOWN_FAILURES.items() if key[1] >= 100}
EVIDENCE: dict = {"commands": [], "runs": []}


def execute(cmd: list[str], *, cwd: Path = ROOT) -> str:
    invocation = dict(command=cmd, cwd=str(cwd))
    EVIDENCE["commands"].append(invocation)
    try:
        result = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True,
                                encoding="utf-8", errors="replace", timeout=90)
    except subprocess.TimeoutExpired as exc:
        invocation.update(timeout_seconds=90, stdout=str(exc.stdout or ""), stderr=str(exc.stderr or ""))
        raise
    invocation.update(returncode=result.returncode, stdout=result.stdout, stderr=result.stderr)
    if result.returncode:
        errors = "\n".join(line for line in result.stderr.splitlines() if
                           "error:" in line or "undefined reference" in line)
        raise RuntimeError(f"command failed ({result.returncode}): {cmd}\n"
                           f"{result.stdout}{errors or result.stderr[-5000:]}")
    if result.stderr:
        print("\n".join(line for line in result.stderr.splitlines() if "warning:" in line))
    return result.stdout


def parse_output(output: str, unit: int) -> tuple[dict, list[dict]]:
    """행 유실/중복/잘못된 metadata를 알려진 제품 실패로 숨기지 않는다."""
    lines = output.splitlines()
    label, unit_no, ret, count, done, overflow = lines[0].split()
    assert label == "STATUS" and int(unit_no) == unit
    status = dict(unit=unit, ret=int(ret), count=int(count), done=int(done), overflow=int(overflow))
    assert len(lines) % 2 == 1, ("unpaired actual/expected", unit, lines)
    records = []
    seen = set()
    for a_line, e_line in zip(lines[1::2], lines[2::2]):
        a_label, *a_values = a_line.split()
        e_label, *e_values = e_line.split()
        assert (a_label, e_label) == ("ACTUAL", "EXPECTED")
        actual, expected = [int(v) for v in a_values], [int(v) for v in e_values]
        assert len(actual) >= 5 and actual[:4] == expected[:4]
        assert actual[0] == -999 and actual[1] == unit and actual[3] > 0
        assert len(actual) == len(expected) == actual[3] + 4
        assert actual[2] not in seen, ("duplicate case", unit, actual[2])
        seen.add(actual[2])
        records.append(dict(unit=unit, case=actual[2], actual=actual[4:],
                            expected=expected[4:], passed=actual == expected))
    assert sum(4 + len(r["actual"]) for r in records) == status["count"]
    return status, records


def verify_failures(records: list[dict], known: dict) -> list[dict]:
    failures = [r for r in records if not r["passed"]]
    found = {(r["unit"], r["case"]): (r["actual"], r["expected"]) for r in failures}
    assert found == known, ("unexpected failure or changed known defect", failures)
    return failures


def check_oracle_guards() -> None:
    """알려진 실패의 삭제/변질과 잘린 원시 로그가 검사에서 거부되는지 확인한다."""
    valid = "STATUS 11 0 5 1 0\nACTUAL -999 11 108 1 0\nEXPECTED -999 11 108 1 -1\n"
    known = {(11, 108): ([0], [-1])}
    _, records = parse_output(valid, 11)
    verify_failures(records, known)
    EVIDENCE["oracle_guards"] = []
    mutations = {
        "unpaired_record": valid.rsplit("EXPECTED", 1)[0],
        "wrong_length": valid.replace("EXPECTED -999 11 108 1 -1", "EXPECTED -999 11 108 1"),
        "duplicate_case": valid.replace("STATUS 11 0 5", "STATUS 11 0 10") + "\n".join(valid.splitlines()[1:]) + "\n",
        "known_failure_disappeared": valid.replace(" 1 -1", " 1 0"),
        "known_failure_changed": valid.replace(" 1 0\n", " 1 2\n"),
    }
    for name, output in mutations.items():
        try:
            _, mutated = parse_output(output, 11)
            verify_failures(mutated, known)
        except AssertionError:
            EVIDENCE["oracle_guards"].append(dict(name=name, rejected=True))
        else:
            raise AssertionError(f"oracle guard accepted {name}")


def run_units(exe: Path, build: Path, counts: dict[int, int], *, mode: str,
              optimization: str, raw: bool) -> dict:
    run: dict = dict(mode=mode, optimization=optimization, statuses=[], records=[])
    EVIDENCE["runs"].append(run)
    for unit in range(1, 21):
        fixture = build / f"{mode}-{optimization[1:]}-{unit}"
        fixture.mkdir()
        (fixture / "1").mkdir()
        (fixture / "2").mkdir()
        output = execute([str(exe), str(fixture), str(unit)])
        status, records = parse_output(output, unit)
        run["statuses"].append(status)
        run["records"].extend(records)
        if raw:
            print(output.strip())
        if not counts.get(unit, 0):
            assert status == dict(unit=unit, ret=-2, count=0, done=0, overflow=0), status
        else:
            assert status["ret"] == 0 and status["done"] == 1 and not status["overflow"], status
        assert len(records) == counts.get(unit, 0), (unit, len(records), counts.get(unit, 0))
        if mode == "RAM_ONLY":
            assert all(r["case"] >= 100 or unit in (9, 14) for r in records)
            assert not any(p.is_file() for p in fixture.rglob("*")), "RAM-only fixture wrote files"
    known = RAM_FAILURES if mode == "RAM_ONLY" else KNOWN_FAILURES
    run["failures"] = verify_failures(run["records"], known)
    run["completed"] = True
    print(f'{mode} {optimization}: {len(run["records"])} comparisons, '
          f'{len(run["failures"])} known failures; record/lifetime/restore checks completed')
    return run


def include_flags(sdk: Path) -> list[str]:
    includes = [BOOT / "include", BOOT / "tiny-AES-c", BOOT / "source/test"]
    includes += [BOOT / "include" / name for name in
                 ("bootloader", "bootmgr", "led", "segger_rtt", "service", "uart")]
    flags = [flag for path in includes for flag in ("-I", str(path))]
    for path in (sdk / "include/cm3", sdk / "include/shared"):
        flags += ["-isystem", str(path)]
    return flags


def check(sdk: Path, arm: bool, raw: bool = False) -> dict:
    if not __debug__:
        raise RuntimeError("Run without Python -O: checker assertions must remain enabled")
    check_oracle_guards()
    coverage = json.loads((ROOT / "tests/unit/boot_coverage.json").read_text(encoding="utf-8"))
    criteria = ROOT / coverage["criteria"]["path"]
    inputs = sorted({*BOOT.rglob("*.c"), *BOOT.rglob("*.h"), *BOOT.glob("*.ld"),
                     BOOT / ".cproject", ROOT / "tests/unit/tdc_ut_boot_host.c",
                     ROOT / "tests/unit/tdc_ut_boot_host.h", Path(__file__).resolve(),
                     ROOT / "tests/unit/boot_coverage.json", criteria,
                     *(ROOT / "tests/unit/tdc_ut_boot_mock").glob("*.h")})
    EVIDENCE["inputs"] = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in inputs}
    EVIDENCE["environment"] = dict(python=sys.version, sdk=str(sdk), target_executed=False)
    assert hashlib.sha256(criteria.read_bytes()).hexdigest() == coverage["criteria"]["sha256"], \
        "Criteria changed: re-read actual workbook before updating coverage"
    entries = coverage["coverage"]
    assert sorted(e["unit"] for e in entries) == list(range(1, 21))
    gcc = shutil.which("gcc")
    if not gcc:
        raise RuntimeError("gcc is not on PATH")
    sources = [BOOT / "source/bootloader.c", BOOT / "source/bootloader_crc.c",
               BOOT / "source/bootmgr/ci_boot.c", BOOT / "source/service/service.c",
               BOOT / "source/led/tdc_boot_led.c", BOOT / "source/tdc_util.c",
               BOOT / "source/test/tdc_ut_boot.c", BOOT / "tiny-AES-c/aes.c",
               ROOT / "tests/unit/tdc_ut_boot_host.c"]
    with tempfile.TemporaryDirectory(prefix="tdc-ut-boot-") as temp:
        build = Path(temp)
        base_cmd = [gcc, "-std=gnu11", "-Wall", "-Wextra", "-Wno-unused-parameter",
                    "-DTDC_UNIT_TEST", "-DTDC_UNIT_TEST_HOST", "-DTDC_BOOT_LED_ENABLE=1",
                    "-DSK5_CID=101", "-Dchess_storage(a)=", "-I",
                    str(ROOT / "tests/unit/tdc_ut_boot_mock"), "-I", str(ROOT / "tests/unit")]
        base_cmd += include_flags(sdk) + [str(p) for p in sources]
        counts = {entry["unit"]: entry["records"] for entry in entries}
        host_runs, ram_runs = [], []
        for mode, expected_counts, runs in (("HOST", counts, host_runs),
                                            ("RAM_ONLY", RAM_RECORDS, ram_runs)):
            for optimization in ("-O0", "-O2"):
                exe = build / f"{mode}-{optimization[1:]}.exe"
                cmd = base_cmd + [optimization]
                if mode == "RAM_ONLY":
                    cmd += ["-DTDC_UNIT_TEST_RAM_ONLY"]
                cmd += ["-o", str(exe)]
                execute(cmd)
                run = run_units(exe, build, expected_counts, mode=mode,
                                optimization=optimization, raw=raw)
                if runs:
                    assert run["records"] == runs[0]["records"], f"{mode} O0/O2 differ"
                runs.append(run)
        records, failures = host_runs[0]["records"], host_runs[0]["failures"]
        statuses = host_runs[0]["statuses"]
        assert len(records) == coverage["verification"]["host_comparison_records"]
        assert len(failures) == coverage["verification"]["failed_records"]
        assert len(ram_runs[0]["records"]) == coverage["verification"]["ram_only_host_comparison_records"]
        assert len(ram_runs[0]["failures"]) == coverage["verification"]["ram_only_host_failed_records"]
        arm_commands: list[list[str]] = []
        runner_bss = None
        stack_frames = []
        if arm:
            compiler = sdk / "arm_tools/bin/arm-none-eabi-gcc.exe"
            if not compiler.exists():
                raise RuntimeError(f"ARM compiler missing: {compiler}")
            for enabled in (False, True):
                for source in sources[:8]:
                    target = build / f"{source.parent.name}-{source.stem}-{'ut' if enabled else 'normal'}.o"
                    compile_cmd = [str(compiler), "-mcpu=cortex-m3", "-mthumb", "-std=gnu11",
                                   "-O0", "-Wall", "-Wextra", "-DSK5_CID=101", "-Dchess_storage(a)="]
                    if enabled:
                        compile_cmd += ["-DTDC_UNIT_TEST"]
                    if source == BOOT / "source/test/tdc_ut_boot.c":
                        compile_cmd += ["-Werror", "-fstack-usage"]
                    compile_cmd += include_flags(sdk) + ["-c", str(source), "-o", str(target)]
                    arm_commands.append(compile_cmd)
                    execute(compile_cmd)
                    if enabled and source == BOOT / "source/test/tdc_ut_boot.c":
                        size_cmd = [str(sdk / "arm_tools/bin/arm-none-eabi-size.exe"), str(target)]
                        size_line = execute(size_cmd).splitlines()[-1].split()
                        runner_bss = int(size_line[2])
                        assert runner_bss == coverage["implementation"]["runner_bss_bytes_measured"]
                        for line in target.with_suffix(".su").read_text().splitlines():
                            name, size, kind = line.rsplit("\t", 2)
                            stack_frames.append(dict(function=name.rsplit(":", 1)[-1],
                                                     bytes=int(size), kind=kind))
                        EVIDENCE["arm_runner_stack_frames"] = stack_frames
                        assert max(f["bytes"] for f in stack_frames) <= 1184, stack_frames
                        sections = execute([str(sdk / "arm_tools/bin/arm-none-eabi-objdump.exe"),
                                            "-h", str(target)])
                        workspace = [line.split() for line in sections.splitlines()
                                     if ".tdc_ut_boot_workspace" in line]
                        assert len(workspace) == 1 and int(workspace[0][2], 16) == 4136, workspace
                    if not enabled and source == BOOT / "source/test/tdc_ut_boot.c":
                        nm_cmd = [str(sdk / "arm_tools/bin/arm-none-eabi-nm.exe"), "-g", str(target)]
                        assert not execute(nm_cmd).strip(), "normal build unexpectedly has test symbols"
            print("ARM_COMPILE 16 translation units (8 normal + 8 TDC_UNIT_TEST); no target execution/link")
        after = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
        EVIDENCE["inputs_unchanged_during_check"] = after == EVIDENCE["inputs"]
        assert EVIDENCE["inputs_unchanged_during_check"], "BOOT inputs changed during check; rerun"
        return dict(host_records=len(records), passed=len(records)-len(failures),
                    failed=len(failures), known_product_failures=failures,
                    runnable_units=list(RUNNABLE), analysis_only_units=list(ANALYSIS),
                    target_safe_run_units=[8, 9, 10, 11, 12, 13, 14, 19, 20], target_executed=False,
                    ram_only_host_records=len(ram_runs[0]["records"]),
                    ram_only_host_known_failures=len(ram_runs[0]["failures"]),
                    host_optimizations=["-O0", "-O2"], ram_only_optimizations=["-O0", "-O2"],
                    max_record_words=max(s["count"] for s in statuses),
                    arm_runner_bss_bytes=runner_bss,
                    arm_translation_units=len(arm_commands),
                    arm_runner_max_frame_bytes=max((f["bytes"] for f in stack_frames), default=None),
                    host_command=base_cmd, arm_commands=arm_commands)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, default=SDK_DEFAULT)
    parser.add_argument("--arm", action="store_true", help="SDK ARM compiler로 정상/UT .o 검사")
    parser.add_argument("--raw", action="store_true", help="모든 실제값/기대값과 컴파일 인자를 표시")
    parser.add_argument("--output", type=Path, default=ROOT / "tmp/unit-tests/boot/checks.json",
                        help="실패한 명령/비교를 포함해 저장할 JSON 경로")
    args = parser.parse_args()
    EVIDENCE["started_utc"] = datetime.now(timezone.utc).isoformat()
    EVIDENCE["invocation"] = sys.argv
    exit_code = 1
    try:
        summary = check(args.sdk, args.arm, args.raw)
        if not args.raw:
            del summary["host_command"]
            del summary["arm_commands"]
        EVIDENCE["summary"] = summary
        exit_code = 0
        print("SUMMARY " + json.dumps(summary, ensure_ascii=False))
    except Exception:
        EVIDENCE["error"] = traceback.format_exc()
        print(EVIDENCE["error"], file=sys.stderr)
    finally:
        EVIDENCE["checker_exit_code"] = exit_code
        EVIDENCE["finished_utc"] = datetime.now(timezone.utc).isoformat()
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(EVIDENCE, ensure_ascii=False, indent=2), encoding="utf-8")
        print("EVIDENCE " + str(args.output))
    sys.exit(exit_code)
