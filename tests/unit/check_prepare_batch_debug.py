"""Synthetic build/prepare regression tests. Never run an SDK or a device.

Every build command is intercepted, and every input/output lives in a temporary
directory. Timestamps are deliberately irrelevant: approval binds SHA256 bytes
and the current local input inventory to one successful build.
"""
from contextlib import redirect_stdout
import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

import build_target_tests as build
import prepare_batch_debug as prepare


MAP = "0x100..0x101 ::g_tdc_ut_cfx_batch_mailbox (Data, Global, .bss.XMEM.1)\n"


class SyntheticTree(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="tdc-prepare-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.shared = self.root / "src/test/shared.h"
        self.put(self.shared, "/* common test contract */\n")
        for core, name in build.PROJECT_FOLDERS.items():
            project = self.root / "src" / name
            self.put(project / ".cproject",
                     '<cproject><sourceEntries><entry name="" excluding="Debug|Release"/>'
                     '</sourceEntries></cproject>')
            self.put(project / "unit.c", "int test_input;\n")
            self.put(project / "unit.h", "extern int test_input;\n")
            self.put(project / "sections.ld", "/* synthetic ARM layout */\n")
            self.put(project / "app_LCF.bcf", "/* synthetic CFX layout */\n")
            if core == "CFX":
                self.put(project / "test/tdc_ut_batch_cfx.h",
                         "enum tdc_ut_batch_word { UTB_STATE, UTB_RESULT, UTB_WORD_COUNT };\n")

    @staticmethod
    def put(path, text):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def folder(self, core="CFX", normal=False):
        return self.root / "tmp/unit-tests/target" / core / ("normal" if normal else "test")

    def project(self, core="CFX"):
        return self.root / "src" / build.PROJECT_FOLDERS[core]

    def record(self, core="CFX", normal=False):
        return json.loads((self.folder(core, normal) / "checks.json").read_text(encoding="utf-8"))

    def approve(self, core="CFX"):
        """Create a consistent prior-success record, not a real SDK build."""
        folder = self.folder(core)
        image, map_file = build.artifact_paths(core, folder)
        self.put(image, "linked " + core)
        self.put(map_file, MAP if core == "CFX" else "ARM map\n")
        sources, _ = build.configuration(self.project(core), core)
        inputs = build.input_snapshot(self.project(core), sources, self.root)
        checks = {"core": core, "test_enabled": True, "build_complete": True,
                  "compile": [{"returncode": 0} for _ in sources], "link": {"returncode": 0},
                  "inputs": inputs,
                  "sources": {str(p.relative_to(self.root)): inputs[str(p.relative_to(self.root))]
                              for p in sources},
                  "artifacts": build.artifact_snapshot(core, folder, self.root)}
        build.write_checks(folder, checks)
        return checks

    def fake_build(self, core="CFX", *, normal=False, compile_only=False,
                   compile_rc=0, link_rc=0, omit=None, empty=None, mutate=None,
                   interrupt=False):
        folder = self.folder(core, normal)
        calls = []

        def fake_run(command, directory):
            calls.append(command)
            self.assertEqual(directory, folder)
            # A reader must never see the last build's approval during this one.
            in_progress = self.record(core, normal)
            self.assertIs(in_progress["build_complete"], False)
            self.assertEqual(in_progress["artifacts"], {})
            if interrupt:
                raise RuntimeError("synthetic compiler interruption")
            compiling = "-c" in command
            rc = compile_rc if compiling else link_rc
            if not compiling:
                for kind, path in zip(("image", "map"), build.artifact_paths(core, folder)):
                    # Even a successful tool return must not inherit old outputs.
                    self.assertFalse(path.exists())
                    if kind != omit:
                        self.put(path, "" if kind == empty else
                                 (MAP if kind == "map" and core == "CFX" else
                                  f"new {core} {normal} {kind}\n"))
                if mutate:
                    mutate()
            return {"command": list(map(str, command)), "returncode": rc, "output": "synthetic"}

        argv = ["build_target_tests.py", core]
        if normal:
            argv.append("--normal")
        if compile_only:
            argv.append("--compile-only")
        with mock.patch.object(build, "ROOT", self.root), mock.patch.object(sys, "argv", argv), \
                mock.patch.object(build, "run", side_effect=fake_run), redirect_stdout(io.StringIO()):
            build.main()
        return calls

    def run_prepare(self):
        out = self.root / "instructions"
        with mock.patch.object(prepare, "ROOT", self.root), mock.patch.object(prepare, "OUT", out), \
                mock.patch.object(sys, "argv", ["prepare_batch_debug.py"]), redirect_stdout(io.StringIO()):
            prepare.main()
        return (out / "현재-빌드-시작명령.txt").read_text(encoding="utf-8")


class BuildSnapshotTests(SyntheticTree):
    def test_six_successful_modes_bind_both_artifacts(self):
        for core in build.PROJECT_FOLDERS:
            for normal in (False, True):
                with self.subTest(core=core, normal=normal):
                    calls = self.fake_build(core, normal=normal)
                    self.assertEqual(len(calls), 2)  # one source, then link
                    checks = self.record(core, normal)
                    self.assertIs(checks["build_complete"], True)
                    self.assertEqual(checks["test_enabled"], not normal)
                    self.assertEqual(len(checks["artifacts"]), 2)
                    self.assertEqual(checks["artifacts"], build.artifact_snapshot(
                        core, self.folder(core, normal), self.root))
                    self.assertFalse((self.folder(core, normal) / "checks.json.tmp").exists())
                    if not normal:
                        self.assertEqual(prepare.current_build(core, self.root), self.folder(core))

    def test_missing_or_empty_link_output_cannot_inherit_old_approval(self):
        for core in build.PROJECT_FOLDERS:
            for condition in ("omit", "empty"):
                for kind in ("image", "map"):
                    with self.subTest(core=core, condition=condition, kind=kind):
                        self.approve(core)
                        with self.assertRaises(SystemExit) as result:
                            self.fake_build(core, **{condition: kind})
                        self.assertEqual(result.exception.code, 1)
                        checks = self.record(core)
                        self.assertIs(checks["build_complete"], False)
                        self.assertEqual(checks["artifacts"], {})
                        with self.assertRaises(ValueError):
                            prepare.current_build(core, self.root)

    def test_compile_and_link_failure_invalidate_previous_success(self):
        for failure in ("compile_rc", "link_rc"):
            with self.subTest(failure=failure):
                self.approve()
                with self.assertRaises(SystemExit):
                    self.fake_build(**{failure: 1})
                self.assertIs(self.record()["build_complete"], False)
                with self.assertRaises(ValueError):
                    prepare.current_build("CFX", self.root)

    def test_interruption_leaves_in_progress_record_not_old_success(self):
        self.approve()
        with self.assertRaisesRegex(RuntimeError, "interruption"):
            self.fake_build(interrupt=True)
        self.assertIs(self.record()["build_complete"], False)
        self.assertEqual(self.record()["artifacts"], {})
        with self.assertRaises(ValueError):
            prepare.current_build("CFX", self.root)

    def test_compile_only_is_not_a_linked_build(self):
        self.approve()
        self.assertEqual(len(self.fake_build(compile_only=True)), 1)
        self.assertIs(self.record()["build_complete"], False)
        with self.assertRaises(ValueError):
            prepare.current_build("CFX", self.root)

    def test_inputs_changed_during_link_are_not_approved(self):
        for name in ("unit.c", "new.c", "new.h", "sections.ld", ".cproject"):
            with self.subTest(name=name):
                self.approve()
                target = self.project() / name
                original = target.read_bytes() if target.exists() else None
                try:
                    with self.assertRaises(SystemExit):
                        self.fake_build(mutate=lambda: self.put(target, "changed during link"))
                    self.assertIs(self.record()["build_complete"], False)
                    self.assertIn("validation_error", self.record())
                finally:
                    if original is None:
                        target.unlink()
                    else:
                        target.write_bytes(original)

    def test_empty_source_inventory_never_approves(self):
        self.approve()
        (self.project() / "unit.c").unlink()
        with self.assertRaises(SystemExit):
            self.fake_build()
        self.assertIs(self.record()["build_complete"], False)


class PrepareValidationTests(SyntheticTree):
    def test_missing_legacy_incomplete_or_malformed_record_is_rejected(self):
        cases = [None, "{", "[]", "null"]
        good = self.approve()
        for key, value in (("build_complete", False), ("build_complete", None),
                           ("core", "BOOT"), ("test_enabled", False),
                           ("link", None), ("link", {"returncode": 1}),
                           ("compile", []), ("compile", [{"returncode": 1}]),
                           ("compile", [{"returncode": 0}] * 2),
                           ("compile", [{}]), ("artifacts", {}), ("artifacts", None),
                           ("inputs", {}), ("sources", {})):
            changed = dict(good)
            changed[key] = value
            cases.append(json.dumps(changed))
        legacy = dict(good)
        del legacy["build_complete"]
        del legacy["artifacts"]
        cases.append(json.dumps(legacy))
        record = self.folder() / "checks.json"
        for text in cases:
            with self.subTest(record=text):
                if text is None:
                    record.unlink(missing_ok=True)
                else:
                    self.put(record, text)
                with self.assertRaises(ValueError):
                    prepare.current_build("CFX", self.root)

    def test_missing_empty_changed_stale_images_and_maps_rejected(self):
        for core in build.PROJECT_FOLDERS:
            for kind in (0, 1):
                for mutation in ("missing", "empty", "changed", "old-linked-output"):
                    with self.subTest(core=core, kind=kind, mutation=mutation):
                        self.approve(core)
                        target = build.artifact_paths(core, self.folder(core))[kind]
                        if mutation == "missing":
                            target.unlink()
                        else:
                            self.put(target, "" if mutation == "empty" else mutation)
                        with self.assertRaises(ValueError):
                            prepare.current_build(core, self.root)

    def test_added_changed_deleted_inputs_rejected(self):
        for name in ("unit.c", "unit.h", "test/tdc_ut_batch_cfx.h", "sections.ld",
                     "app_LCF.bcf", ".cproject", "../test/shared.h", "new.c", "new.h"):
            for mutation in ("changed", "deleted"):
                with self.subTest(name=name, mutation=mutation):
                    self.approve()
                    target = self.project() / name
                    original = target.read_bytes() if target.exists() else None
                    if mutation == "deleted" and original is None:
                        continue
                    try:
                        if mutation == "deleted":
                            target.unlink()
                        else:
                            # Retain valid XML to test hashing, not parser rejection.
                            self.put(target, (target.read_text(encoding="utf-8") if target.exists() else "") + "\n")
                        with self.assertRaises(ValueError):
                            prepare.current_build("CFX", self.root)
                    finally:
                        if original is None:
                            target.unlink(missing_ok=True)
                        else:
                            target.write_bytes(original)

    def test_unbuilt_other_mode_cannot_be_used_as_test_image(self):
        self.fake_build("CFX", normal=True)
        with self.assertRaises(ValueError):
            prepare.current_build("CFX", self.root)

    def test_instructions_bind_four_artifacts_and_require_restore_before_stop(self):
        self.approve("CFX")
        self.approve("CM3")
        text = self.run_prepare()
        for core in ("CFX", "CM3"):
            for path in build.artifact_paths(core, self.folder(core)):
                sha = hashlib.sha256(path.read_bytes()).hexdigest()
                self.assertIn(f"{path.relative_to(self.root)}: {sha}", text)
        restore = text.index("call tdc_ut_cfx_bridge_restored()")
        confirm = text.index("반드시 반환값 1")
        stop = text.index("g_tdc_ut_cfx_batch_service_stop = 1")
        self.assertLess(restore, confirm)
        self.assertLess(confirm, stop)
        self.assertIn("CFX 서비스를 계속 실행", text)
        self.assertIn("통제된 재기동", text)
        self.assertIn("서비스 중단/정상 코드 복귀/다음 시험을 하지 않는다", text)

    def test_failed_prepare_does_not_emit_instructions(self):
        for core in ("CFX", "CM3"):
            with self.subTest(core=core):
                self.approve("CFX")
                self.approve("CM3")
                self.put(build.artifact_paths(core, self.folder(core))[1], "stale map")
                with self.assertRaises(ValueError):
                    self.run_prepare()
                self.assertFalse((self.root / "instructions").exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
