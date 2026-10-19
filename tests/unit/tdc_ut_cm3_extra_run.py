"""PC-only CM3 state/orchestration fixtures; no firmware/device execution.

Every scenario starts a new process so function-local product state is reset by
the C runtime. Product sources are compiled intact with only peripheral/file
dependencies replaced at the linker boundary. Main suite integration is owned
by the parent task.
"""
from __future__ import annotations

import json
import pathlib
import re
import shutil
import subprocess
import tempfile

from tdc_ut_cm3_run import CM3, ROOT, SDK_DEFAULT, INCLUDES, run, save_records, source_hashes

SOURCES = (
    "source/board/electrodeMapping.c",
    "source/isd/tdc_isd.c",
    "source/isd/tdc_isd_init_fpga.c",
    "source/isd/tdc_isd_stim_para_setting.c",
    "source/isd/tdc_isd_stim_standalone.c",
    "source/stim/tdc_stim_indicator.c",
    "source/cfx_link/tdc_shm.c",
    "source/stim/tdc_stim_para_cal.c",
    "source/stim/tdc_stim_common.c",
    "source/sys/tdc_sys_error.c",
    "source/ui/tdc_ui_command.c",
    "source/util/tdc_printf.c",
)
SCENARIOS = ("selection", "power-ok", "power-reset-fail", "fpga-ok", "fpga-tdc_sys_error",
             "live-high", "live-low", "live-none", "live-many", "live-clear-fail",
             "mapping-high", "mapping-none", "mapping-many", "mapping-clear-fail",
             "mono", "bipolar", "mono-final-fail", "mono-read-fail", "mono-read-mismatch",
             "caller-ok", "caller-dac-fail")
EXPECTED_RECORD_COUNTS = {
    "selection": 141, "power-ok": 59, "power-reset-fail": 13,
    "fpga-ok": 21, "fpga-tdc_sys_error": 21,
    "live-high": 8, "live-low": 8, "live-none": 7, "live-many": 7, "live-clear-fail": 8,
    "mapping-high": 15, "mapping-none": 14, "mapping-many": 14, "mapping-clear-fail": 15,
    "mono": 32, "bipolar": 114, "mono-final-fail": 33,
    "mono-read-fail": 23, "mono-read-mismatch": 23, "caller-ok": 29, "caller-dac-fail": 9,
}


def expected_record_keys(scenario: str) -> list[tuple[int, int, str]]:
    """Fixture completeness contract, never learned from a subprocess result."""
    fields: list[tuple[int, str]] = []

    def add(unit: int, *names: str) -> None:
        fields.extend((unit, name) for name in names)

    if scenario == "selection":
        for _ in range(3):
            add(21, "selected", "id", "location", *(["synthetic_name"] * 25),
                *(["synthetic_passkey"] * 4), "gain_a", "gain_b", "input_boundaries")
            add(22, "count", *(["usable"] * 4), "stamp", *(["date"] * 4),
                "program", "list_unchanged")
    elif scenario.startswith("power-"):
        add(31, "power_request", *(["incomplete"] * 3), "power_request")
        if scenario == "power-ok":
            add(31, *(["incomplete"] * 51), "power_ready", "power_upper_bound", "ramp_executed")
        else:
            add(31, *(["incomplete"] * 7), "reset_failure_not_complete")
    elif scenario.startswith("fpga-"):
        add(31, *(["fpga_not_ready"] * 20), "fpga_result")
    elif scenario.startswith(("live-", "mapping-")):
        mapping = scenario.startswith("mapping-")
        unit = 33 if mapping else 32
        add(unit, *(["progress"] * 7) if mapping else ["no_completion_no_read"])
        add(unit, "received_once", "fifo_clear", "i2c_released", "recovery_state", "tdc_sys_error")
        if not mapping:
            add(unit, "completion_cleared")
        if not scenario.endswith(("none", "many")):
            add(unit, "power_adjustment")
        if mapping:
            add(unit, "no_stale_progress", "restart_progress")
    elif scenario.startswith("caller-"):
        add(29, "not_loaded", "not_loaded_no_calculation", "first_setting_incomplete",
            "copy_attempt", "uninitialized_read_skipped", "completion_publication",
            "tdc_sys_error", "recalculation_request_after")
        if scenario == "caller-dac-fail":
            add(29, "pcm_after_failed_calculation")
        else:
            add(41, *(["caller_completion"] * 15), "live_after_setting", "no_new_map_no_recalculation",
                "new_map_request", "new_map_consumed", "new_map_calculated", "new_map_incomplete")
    else:
        bipolar = scenario == "bipolar"
        read_failure = scenario in ("mono-read-fail", "mono-read-mismatch")
        last_tick = 9 if read_failure else 39 if bipolar else 14
        for tick in range(last_tick + 1):
            add(41, "step_error", "completion")
            if tick == (30 if bipolar else 5):
                add(41, "dac_write", "mode_write")
            if bipolar and tick in (8, 10):
                add(41, *(["reference_write"] * (24 if tick == 8 else 8)))
        if read_failure:
            add(41, "read_failure_recovery")
        if scenario == "mono-final-fail":
            add(41, "final_failure_recovery")
    if scenario not in EXPECTED_RECORD_COUNTS or len(fields) != EXPECTED_RECORD_COUNTS[scenario]:
        raise RuntimeError("Invalid fixture record manifest: " + scenario)
    return [(unit, case, field) for case, (unit, field) in enumerate(fields, 1)]


def validate_records(scenario: str, rows: list[dict]) -> list[str]:
    """Require every declared key, observation and exact known finding once."""
    errors = []
    expected_keys = expected_record_keys(scenario)
    actual_keys = [(row.get("unit"), row.get("case"), row.get("field")) for row in rows]
    if len(rows) != EXPECTED_RECORD_COUNTS[scenario]:
        errors.append(f"record count {len(rows)} != {EXPECTED_RECORD_COUNTS[scenario]}")
    if actual_keys != expected_keys:
        errors.append("ordered unit/case/field keys differ (missing, duplicate, reordered or unexpected record)")
    observed = {(29, 8, "recalculation_request_after")} if scenario.startswith("caller-") else set()
    known = {}
    if scenario == "caller-dac-fail":
        observed.add((29, 9, "pcm_after_failed_calculation"))
        known[(29, 6, "completion_publication")] = ([1], [0])
    for row, key in zip(rows, actual_keys):
        actual, expected = row.get("actual"), row.get("expected")
        if not isinstance(actual, list) or len(actual) != 1 or type(actual[0]) is not int:
            errors.append(f"{key}: actual must contain one integer")
        if key in observed:
            if expected is not None or "expected" not in row or row.get("known_failure", False) is not False:
                errors.append(f"{key}: observation must retain expected=null without known_failure")
            continue
        if not isinstance(expected, list) or len(expected) != 1 or type(expected[0]) is not int:
            errors.append(f"{key}: comparison must contain one expected integer")
        if row.get("known_failure") is not (key in known):
            errors.append(f"{key}: known_failure marker differs")
        if key in known:
            if (actual, expected) != known[key]:
                errors.append(f"{key}: exact known mismatch must remain actual=[1], expected=[0]")
        elif actual != expected:
            errors.append(f"{key}: unexpected mismatch actual={actual}, expected={expected}")
    return errors


def readback_body(source: str) -> str:
    """Extract one complete bool function without changing tokens or branches."""
    match = re.search(r"^bool tdc_isd_fpga_read_backtel_fifo\([^;]*?\)\s*\{", source, re.MULTILINE)
    if not match:
        raise RuntimeError("Actual readback function signature changed")
    tokens = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', re.DOTALL)
    depth = 1
    for token in tokens.finditer(source, match.end()):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if depth == 0:
                return source[match.start():token.end()]
    raise RuntimeError("Actual readback body is incomplete")


def main() -> None:
    compiler = shutil.which("gcc")
    if not compiler:
        raise RuntimeError("gcc unavailable")
    flags = ["-std=gnu11", "-DTDC_UNIT_TEST", "-DTDC_UT_CM3_HOST", "-DSK5_CID=101",
             "-Dchess_storage(a)=", "-O0", "-ffunction-sections", "-fdata-sections",
             "-fno-asynchronous-unwind-tables", "-fno-unwind-tables", "-fmax-errors=5",
             "-include", str(ROOT / "tests/unit/tdc_ut_cm3_host_prelude.h")]
    flags += ["-I" + str(CM3 / folder) for folder in INCLUDES]
    for folder in ("include/cm3", "include/shared"):
        flags += ["-isystem", str(SDK_DEFAULT / folder)]
    fixture = ROOT / "tests/unit/tdc_ut_cm3_extra_host.c"
    inputs = [CM3 / source for source in SOURCES] + list(CM3.rglob("*.h"))
    inputs += [fixture, pathlib.Path(__file__), ROOT / "tests/unit/tdc_ut_cm3_run.py",
               ROOT / "tests/unit/tdc_ut_cm3_host_prelude.h",
               CM3 / "source/isd/tdc_isd_fpga.c"]
    inputs.append(ROOT / json.loads((ROOT / "tests/unit/cm3_pure_control_coverage.json").read_text(encoding="utf-8"))["criterion"]["path"])
    before = source_hashes(inputs)
    records, executions = [], []
    with tempfile.TemporaryDirectory(prefix="tdc-cm3-extra-") as output:
        directory = pathlib.Path(output)
        objects = []
        for index, source in enumerate(SOURCES):
            obj = directory / f"extra_{index}.o"
            run([compiler, *flags, "-c", str(CM3 / source), "-o", str(obj)])
            objects.append(str(obj))
        exe = directory / "extra.exe"
        # The actual read wrapper changes ISD state on a failed transfer. Keep
        # its complete original body; a bool-only mock would omit that effect.
        readback = directory / "readback.c"
        prefix = "#include <tdc_isd_fpga.h>\n#include <tdc_isd.h>\n#include <tdc_sys_error.h>\n#include <tdc_hal_i2c_isd.h>\n#include <ci_printf.h>\n"
        source = (CM3 / "source/isd/tdc_isd_fpga.c").read_text(encoding="utf-8")
        readback.write_text(prefix + readback_body(source) + "\n", encoding="utf-8")
        run([compiler, *flags, str(fixture), str(readback), *objects,
             "-Wl,--gc-sections,--exclude-all-symbols", "-o", str(exe)])
        for trial in (1, 2):
            for scenario in SCENARIOS:
                result = subprocess.run([str(exe), scenario], cwd=ROOT, capture_output=True,
                                        text=True, encoding="utf-8", errors="replace", timeout=10)
                rows = [json.loads(line[7:]) for line in result.stdout.splitlines() if line.startswith("RECORD ")]
                validation_errors = validate_records(scenario, rows)
                for row in rows:
                    row.update(scenario=scenario, **{"pass": trial})
                records.extend(rows)
                executions.append(dict(scenario=scenario, trial=trial, returncode=result.returncode,
                                       stdout=result.stdout, stderr=result.stderr, record_count=len(rows),
                                       expected_record_count=EXPECTED_RECORD_COUNTS[scenario],
                                       record_contract_pass=not validation_errors,
                                       validation_errors=validation_errors))
                print(f"EXTRA {scenario} pass={trial} records={len(rows)}/{EXPECTED_RECORD_COUNTS[scenario]} "
                      f"contract={'FAIL' if validation_errors else 'PASS'} exit={result.returncode}")
                for tdc_sys_error in validation_errors:
                    print("RECORD_CONTRACT_FAIL", tdc_sys_error)
                for row in rows:
                    if row["expected"] is not None and row["actual"] != row["expected"]:
                        print("MISMATCH", json.dumps(row))
                if result.stderr:
                    print(result.stderr, end="")
    unchanged = before == source_hashes(inputs)
    ok = (unchanged and len(executions) == len(SCENARIOS) * 2
          and all(e["returncode"] == 0 and e["record_contract_pass"] for e in executions))
    save_records(ROOT / "tmp/unit-tests/cm3-extra/checks.json", records, before, ok,
                 sources_unchanged=unchanged, executions=executions, compiler=compiler, flags=flags,
                 exact_source_fragments=["tdc_isd_fpga.c:tdc_isd_fpga_read_backtel_fifo (unchanged body)"],
                 target_scope="PC only; actual peripheral/file boundaries and target ABI not executed")
    if not ok:
        raise RuntimeError("CM3 extra harness failed; inspect cm3-extra/checks.json")


if __name__ == "__main__":
    main()
