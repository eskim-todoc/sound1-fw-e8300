"""Fail-closed result handling; mock processes, never build or run devices."""
import contextlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import run_suite


class SuiteResultTests(unittest.TestCase):
    def exercise(self, failure):
        with tempfile.TemporaryDirectory(prefix="tdc-suite-regression-") as folder:
            root = Path(folder)
            out = root / "out"
            out.mkdir()
            report = out / "checks.json"
            report.write_text('{"status":"OLD_SUCCESS"}', encoding="utf-8")
            commands = []

            def execute(command, **kwargs):
                commands.append(command)
                # Old success must already be invalidated at the first process.
                self.assertEqual(json.loads(report.read_text(encoding="utf-8"))["status"], "RUNNING")
                if command[0] == "gcc" and "tdc_ut_log_test.c" in " ".join(command):
                    if isinstance(failure, Exception):
                        raise failure
                    return subprocess.CompletedProcess(command, failure, "", "forced compile failure")
                return subprocess.CompletedProcess(command, 0, "test fixture only\n", "")

            with patch.object(run_suite, "ROOT", root), patch.object(run_suite, "OUT", out), \
                    patch.object(sys, "argv", ["run_suite.py"]), \
                    patch.object(run_suite.subprocess, "run", side_effect=execute), \
                    contextlib.redirect_stdout(io.StringIO()):
                with self.assertRaises(SystemExit) as stopped:
                    run_suite.main()
            self.assertEqual(stopped.exception.code, 1)
            result = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(result["status"], "HARNESS_ERROR")
            self.assertFalse(result["device_execution"])
            rows = {r["name"]: r for r in result["results"]}
            self.assertNotEqual(rows["log-build"]["returncode"], 0)
            self.assertNotEqual(rows["log"]["returncode"], 0)
            self.assertFalse(any(c[0] == str(out / "log.exe") for c in commands))
            return rows, (out / "log-build.log").read_text(encoding="utf-8")

    def test_failed_build_never_runs_old_binary(self):
        rows, _ = self.exercise(3)
        self.assertEqual(rows["log-build"]["returncode"], 3)

    def test_timeout_is_persisted(self):
        rows, log = self.exercise(subprocess.TimeoutExpired(["gcc"], 900, output=b"partial"))
        self.assertEqual(rows["log-build"]["returncode"], 124)
        self.assertIn("partial", log)
        self.assertIn("TIMEOUT", log)

    def test_missing_tool_is_persisted(self):
        rows, log = self.exercise(FileNotFoundError("fixture tool absent"))
        self.assertEqual(rows["log-build"]["returncode"], 127)
        self.assertIn("START ERROR", log)


if __name__ == "__main__":
    unittest.main()
