"""Capture-parser checks, synthetic input only, no board or viewer access."""
from pathlib import Path
import tempfile
import unittest
from collect_rtt import parse, export

GOOD = "\r\n".join([
    "[UT-BATCH] CORE=CM3 BEGIN N=1",
    "[UT-CM3-027] C1 A[0:2]=40,1,24",
    "[UT-CM3-027] C1 PASS N=3 F=0",
    "[UT-BATCH] CORE=CM3 END N=1", ""])


class CaptureTests(unittest.TestCase):
    def test_good_and_repeated_sessions(self):
        result = parse(GOOD + GOOD)
        self.assertFalse(result["problems"])
        self.assertEqual(len(result["records"]), 2)

    def test_fail_preserved(self):
        text = GOOD.replace("40,1,24", "40,2,24").replace("PASS N=3 F=0", "I1 A=2 E=1\r\n[UT-CM3-027] C1 FAIL N=3 F=1")
        result = parse(text)
        self.assertFalse(result["problems"])
        self.assertEqual(result["records"][0]["status"], "FAIL")

    def test_missing_duplicate_and_malformed(self):
        for text in (GOOD.rstrip("\r\n"), GOOD[:-1],
                     GOOD.replace("END N=1", "END N=2"),
                     GOOD.replace("[UT-BATCH] CORE=CM3 END N=1", ""),
                     GOOD.replace("A[0:2]", "A[1:3]"),
                     GOOD.replace("N=3", "N=2"),
                     GOOD.replace("PASS N=3 F=0", "FAIL N=3 F=0"),
                     GOOD.replace("[UT-CM3-027] C1 A[0:2]=40,1,24", ""),
                     GOOD.replace("[UT-CM3-027] C1 PASS N=3 F=0", ""),
                     GOOD.replace("A[0:2]=40,1,24", "A[0:2]=40,1,24,5"), ""):
            with self.subTest(text=text): self.assertTrue(parse(text)["problems"])

    def test_immutable_raw_and_no_overwrite(self):
        with tempfile.TemporaryDirectory(prefix="tdc-capture-") as folder:
            source = Path(folder) / "raw.log"
            source.write_bytes(GOOD.encode("ascii"))
            out = Path(folder) / "split"
            export(source, out)
            self.assertEqual(source.read_bytes(), (out / "원본.log").read_bytes())
            self.assertTrue((out / "CM3-027.log").exists())
            with self.assertRaises(FileExistsError): export(source, out)

    def test_unknown_markers_unscoped_and_console_errors(self):
        for text in (
            GOOD + "[UT-CONSOLE] STOPPED_OR_ERROR\r\n",
            GOOD + "[UT-BATCH] CORE=CM3 BEGI\r\n",
            GOOD.replace("END N=1", "END N=x"),
            GOOD + "[UT-CM3-027] C2 A[0:0]=1\r\n[UT-CM3-027] C2 PASS N=1 F=0\r\n",
            GOOD + "[UT-BATCH] CORE=CFX RAW=NUMERIC U=23 C=1\r\n",
        ):
            with self.subTest(text=text): self.assertTrue(parse(text)["problems"])

    def test_raw_array_markers(self):
        record = "[UT-HEAR-002] C1 A[0:0]=0\r\n[UT-HEAR-002] C1 OBSERVED N=1 F=0\r\n"
        text = "[UT-BATCH] CORE=HEAR BEGIN N=1\r\n" + record
        for kind in ("FFT", "VMAG", "EXPONENT"):
            text += f"[UT-BATCH] CORE=HEAR RAW={kind} U=2 C=1\r\n" + record
        text += "[UT-BATCH] CORE=HEAR END N=1\r\n"
        parsed = parse(text)
        self.assertFalse(parsed["problems"])
        self.assertEqual([r["array_kind"] for r in parsed["records"]], ["RESULT", "FFT", "VMAG", "EXPONENT"])
        self.assertEqual(sum(" RAW=" in line for line in parsed["units"]["HEAR-002"]), 3)
        self.assertTrue(parse(text.replace(" RAW=FFT U=2 C=1", " RAW=FFT U=1 C=1"))["problems"])
        truncated = text.replace(" RAW=EXPONENT U=2 C=1\r\n" + record,
                                " RAW=EXPONENT U=2 C=1\r\n")
        self.assertTrue(parse(truncated)["problems"])

    def test_numeric_marker_covers_framed_case_stream(self):
        text = "[UT-BATCH] CORE=CFX BEGIN N=1\r\n[UT-BATCH] CORE=CFX RAW=NUMERIC U=23 C=0\r\n"
        for case in (11, 12):
            text += f"[UT-CFX-023] C{case} A[0:2]=1,0,2\r\n[UT-CFX-023] C{case} OBSERVED N=3 F=0\r\n"
        text += "[UT-BATCH] CORE=CFX END N=1\r\n"
        parsed = parse(text)
        self.assertFalse(parsed["problems"])
        self.assertEqual([r["array_kind"] for r in parsed["records"]], ["NUMERIC", "NUMERIC"])


if __name__ == "__main__": unittest.main()
