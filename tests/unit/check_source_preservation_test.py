"""The preservation gate must detect edits/removal/addition outside test paths."""
import unittest
from check_source_preservation import compare


def rows(*paths):
    return {"files": [{"path": p, "sha256": value * 64} for p, value in paths]}


class PreservationTest(unittest.TestCase):
    def test_existing_user_edit_is_baseline(self):
        before = rows(("src/2__cm3/source/fs/tdc_fs_map.c", "a"))
        self.assertTrue(compare(before, before)["product_source_preserved"])

    def test_product_edit_remove_and_add_are_all_rejected(self):
        before = rows(("src/core/main.c", "a"), ("src/core/sections.ld", "a"))
        after = rows(("src/core/main.c", "b"), ("src/core/.cproject", "a"))
        report = compare(before, after)
        self.assertFalse(report["product_source_preserved"])
        self.assertEqual({r["change"] for r in report["product_changes"]},
                         {"added", "removed", "modified"})

    def test_only_explicit_test_directories_are_allowed(self):
        before = rows(("src/core/main.c", "a"))
        after = rows(("src/core/main.c", "a"),
                     ("src/1__cfx/test/tdc_ut_io_cfx.c", "b"),
                     ("src/test/tdc_ut_log.c", "b"),
                     ("src/core/test_like_name.c", "b"))
        report = compare(before, after)
        self.assertEqual(len(report["test_changes"]), 2)
        self.assertEqual(report["product_changes"][0]["path"], "src/core/test_like_name.c")

    def test_assembly_and_cfx_linker_are_product_inputs(self):
        for path in ("src/1__cfx/asm/asm_startup.S", "src/core/start.s",
                     "src/core/start.asm", "src/1__cfx/app_LCF.bcf"):
            with self.subTest(path=path):
                self.assertFalse(compare(rows((path, "a")), rows((path, "b")))["product_source_preserved"])

    def test_empty_duplicate_and_traversal_rejected(self):
        good = rows(("src/core/main.c", "a"))
        for bad in (rows(), rows(("src/a.c", "a"), ("src/a.c", "a")),
                    rows(("src/test/../core/a.c", "a"))):
            with self.assertRaises(ValueError):
                compare(bad, good)


if __name__ == "__main__":
    unittest.main()
