"""Archive batch-test delivery checks. No board, download or report mutation."""
from pathlib import Path
import hashlib
import json
import re
import shutil
import subprocess
import sys
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[5]
TASK = Path(__file__).resolve().parents[1]
OUT = TASK / "qa/batch-tests"
DHF = ROOT / "docs/참고/인허가/사운드1/DHF"
MANUAL = DHF / "04_설계 검증/SW/펌웨어/유닛 시험 일괄 실행 매뉴얼.md"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):
    return json.loads(path.read_text(encoding="utf-8"))


def array(path, name):
    text = path.read_text(encoding="utf-8")
    body = re.search(r"\b" + name + r"\[\]\s*=\s*\{([^}]+)\}", text).group(1)
    body = body.replace("TDC_BATCH_FAILURE_OBSERVATION", "29")
    return [int(n) for n in re.findall(r"\b\d+\b", body)]


def main():
    suite = read(ROOT / "tmp/unit-tests/suite/checks.json")
    assert suite["status"] == "HARNESS_OK" and not suite["device_execution"]
    assert all(r["returncode"] == 0 for r in suite["results"])
    target = read(ROOT / "tmp/unit-tests/target/final-checks.json")
    assert len(target["records"]) == 6 and not target["device_execution"]
    preserved = subprocess.run([sys.executable, "tests/unit/check_source_preservation.py",
                               "tmp/unit-tests/batch/before-product.json"], cwd=ROOT,
                              capture_output=True, text=True, encoding="utf-8", check=True)
    text = MANUAL.read_text(encoding="utf-8")
    assert "·" not in text and "실기 통과 확인을 수행하지 않았다" in text
    for command in ("ut auto 1", "ut cm3 1", "ut hear 1", "ut boot 1", "ut observe 3 103",
                    "collect_rtt.py", "RESTORED=0", "WAITING_RESTORE", "5,000줄"):
        assert command in text, command
    cm3 = ROOT / "src/2__cm3/Cortex-M3-src/test/tdc_ut_batch_cm3.c"
    calc, io = array(cm3, "s_tdc_calc"), array(cm3, "s_tdc_io")
    assert len(calc) == 22 and len(io) == 12 and len(set(calc + io)) == 33
    cfx_header = (ROOT / "src/1__cfx/test/tdc_ut_batch_cfx.h").read_text(encoding="utf-8")
    cfx = []
    for name in ("TDC_UT_BATCH_VECTOR_IDS", "TDC_UT_BATCH_IO_IDS"):
        line = re.search(r"#define\s+" + name + r"\s+([^\n]+)", cfx_header).group(1)
        cfx.extend(int(n) for n in re.findall(r"\b\d+\b", line))
    boot = array(ROOT / "src/0__bootloader/source/test/tdc_ut_batch_boot.c", "s_all")
    assert len(set(cfx)) == 29 and len(set(boot)) == 9
    assert len(set(calc + io)) + len(set(cfx)) + len(set(boot)) + 2 == 73
    documents = [MANUAL, ROOT / "tests/unit/README.md",
                 ROOT / "docs/참고/시스템/Sound1 시스템 구조.md",
                 ROOT / "docs/참고/인허가/사운드1/QCC 자료 추가 위치 관리.md"]
    links = 0
    for doc in documents:
        for ref in re.findall(r"\]\(([^)\n]+)\)", doc.read_text(encoding="utf-8")):
            if ref.startswith(("http:", "https:", "#")):
                continue
            assert (doc.parent / unquote(ref.split("#", 1)[0])).resolve().exists(), (doc, ref)
            links += 1
    expected = {
        "SUVR-FW-TD24A 소프트웨어 유닛 검증 보고서.xlsx": "e54f87bdb18ab7be074d083ace764b3fbe6dbc0e172e96c2ee9296259b2380e3",
        "SITR-FW-TD24A 소프트웨어 통합시험 보고서.xlsx": "ef3a9868d03a16e764bc2ee24969ba6458804392a68c4f258d21edd1c6c051d7",
        "SSTR-FW-TD24A 소프트웨어 시스템 시험 보고서.xlsx": "edfe3357de6dcee1fd213bccfc9851c1cbcee0ae320c655d7296c609f5e4b16f",
        "SUVC-FW-TD24A 소프트웨어 유닛 검증 기준서.xlsx": "89967fa692c4f51e3be916c73270d75859b4f692c96928c2d73da0cc6c5096fd",
    }
    for name, digest in expected.items():
        found = list(DHF.rglob(name))
        assert len(found) == 1 and sha(found[0]) == digest, name
    from expected_io_cm3_io import build_log_export, validate, validate_log_export
    original = read(TASK / "qa/expected-io/cm3-io.json")
    expected_check = validate(original)
    current = build_log_export(original)
    log_check = validate_log_export(original, current)
    assert not current["counts"]["target_blocked_units"]
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "current-cm3-io-logs.json").write_text(json.dumps(current, ensure_ascii=False, indent=2), encoding="utf-8")
    evidence = ["suite/checks.json", "suite/source-preservation.json", "target/final-checks.json",
                "stack-budget/checks.json", "cm3-batch/checks.json", "boot-batch/checks.json",
                "cfx-batch/checks.json", "console-adapters/checks.json", "batch/before-product.json"]
    evidence += [f"target/{core}/{mode}/checks.json" for core in ("CFX", "CM3", "BOOT")
                 for mode in ("test", "normal")]
    evidence += [str(p.relative_to(ROOT / "tmp/unit-tests"))
                 for p in sorted((ROOT / "tmp/unit-tests/suite").glob("*.log"))]
    evidence += ["batch/현재-빌드-시작명령.txt"]
    saved = {}
    for relative in evidence:
        source = ROOT / "tmp/unit-tests" / relative
        dest = OUT / relative
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, dest)
        saved[relative] = sha(dest)
    report = {"passed": True, "device_execution": False, "product_acceptance": False,
              "suite_checks": len(suite["results"]), "sdk_links": 6, "automatic_unit_ids": 73,
              "links_checked": links, "protected_workbooks": expected,
              "source_preservation": preserved.stdout, "expected_values": expected_check,
              "expected_log_export": log_check, "evidence_sha256": saved,
              "manual_sha256": sha(MANUAL)}
    (OUT / "checks.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({k: report[k] for k in ("passed", "device_execution", "suite_checks", "sdk_links",
                                              "automatic_unit_ids", "links_checked")}, ensure_ascii=False))


if __name__ == "__main__":
    main()
