"""Accept only the five already-deleted E8300 rows; preserve all other ZIP parts."""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import xml.parsers.expat
import zipfile

from lxml import etree

ROOT = Path(__file__).resolve().parents[4]
TASK = Path(__file__).resolve().parent
QA = TASK / "qa" / "sad-accept-five-deleted-rows-20261012"
TARGET = ROOT / "docs/참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어/[김은수 작성] [Rev.0] SAD-FW-TD24A 소프트웨어 아키텍쳐 기술서.docx"
N = {"w": "http://schemas.openxmlformats.org/wordprocessingml/2006/main"}
IDS = {f"SW-FW-RID-{i}" for i in (15, 44, 45, 69, 70)}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def row_text(row):
    return "".join(row.xpath(".//w:t/text() | .//w:delText/text()", namespaces=N))


def row_spans(data):
    parser = xml.parsers.expat.ParserCreate()
    spans, stack = [], []

    def start(name, attrs):
        if name == "w:tr":
            idx = len(spans)
            spans.append([parser.CurrentByteIndex, None])
            stack.append(idx)

    def end(name):
        if name == "w:tr":
            idx = stack.pop()
            spans[idx][1] = data.index(b">", parser.CurrentByteIndex) + 1

    parser.StartElementHandler = start
    parser.EndElementHandler = end
    parser.Parse(data, True)
    return spans


def visible_text(root):
    clone = copy.deepcopy(root)
    for row in list(clone.xpath(".//w:tr[w:trPr/w:del]", namespaces=N)):
        row.getparent().remove(row)
    for element in list(clone.xpath(".//w:del | .//w:moveFrom", namespaces=N)):
        if element.getparent() is not None:
            element.getparent().remove(element)
    return clone.xpath(".//w:t/text()", namespaces=N)


def revisions(root):
    names = ("ins", "del", "moveFrom", "moveTo", "pPrChange", "rPrChange", "trPrChange", "tcPrChange", "tblPrChange", "tblGridChange", "sectPrChange")
    return {name: len(root.findall(f".//w:{name}", N)) for name in names}


def prepare():
    QA.mkdir(parents=True, exist_ok=True)
    before = QA / "before.docx"
    candidate = QA / "candidate.docx"
    if before.exists() or candidate.exists():
        raise RuntimeError("QA baseline already exists; inspect it before rerunning.")
    shutil.copy2(TARGET, before)
    with zipfile.ZipFile(before) as zin:
        data = zin.read("word/document.xml")
        root = etree.fromstring(data)
        rows = root.findall(".//w:tr", N)
        spans = row_spans(data)
        assert len(rows) == len(spans)
        chosen = []
        for index, row in enumerate(rows):
            if "[E8300" not in row_text(row):
                continue
            assert row.find("w:trPr/w:del", N) is not None, "A current row must not be removed."
            first = row.find("w:tc", N)
            rid = row_text(first).strip()
            assert rid in IDS, rid
            chosen.append((index, rid))
        assert len(chosen) == 5 and {rid for _, rid in chosen} == IDS
        cuts = sorted((spans[index] for index, _ in chosen), reverse=True)
        edited = data
        for a, b in cuts:
            edited = edited[:a] + edited[b:]
        after_root = etree.fromstring(edited)
        expected_root = copy.deepcopy(root)
        expected_rows = expected_root.findall(".//w:tr", N)
        for index, _ in chosen:
            row = expected_rows[index]
            row.getparent().remove(row)
        assert etree.tostring(expected_root, method="c14n") == etree.tostring(after_root, method="c14n")
        assert visible_text(root) == visible_text(after_root)
        assert "[E8300" not in "".join(after_root.itertext())
        with zipfile.ZipFile(candidate, "w") as zout:
            zout.comment = zin.comment
            for entry in zin.infolist():
                zout.writestr(entry, edited if entry.filename == "word/document.xml" else zin.read(entry.filename))
    with zipfile.ZipFile(before) as a, zipfile.ZipFile(candidate) as b:
        changed = [name for name in a.namelist() if a.read(name) != b.read(name)]
        assert a.namelist() == b.namelist()
        assert changed == ["word/document.xml"]
        assert b.testzip() is None
        search_matches = []
        for name in b.namelist():
            if name.endswith(".xml"):
                doc = etree.fromstring(b.read(name))
                if "[E8300" in "".join(doc.itertext()):
                    search_matches.append(name)
        assert not search_matches
        assert a.read("word/settings.xml") == b.read("word/settings.xml")
    result = {
        "target": str(TARGET), "before_sha256": sha(before), "candidate_sha256": sha(candidate),
        "accepted_deleted_rows": [rid for _, rid in chosen], "changed_parts": changed,
        "all_other_bytes_preserved": True, "current_text_identical": True,
        "search_matches": search_matches, "before_revisions": revisions(root),
        "after_revisions": revisions(after_root), "other_revisions_exactly_preserved": True,
        "qcc_current_markers": "".join(visible_text(after_root)).count("[QCC 자료 입력]"),
    }
    (QA / "checks.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, indent=2))


def publish():
    check = json.loads((QA / "checks.json").read_text(encoding="utf-8"))
    assert sha(TARGET) == check["before_sha256"], "Source changed during review."
    assert sha(QA / "candidate.docx") == check["candidate_sha256"]
    staging = TARGET.with_name(TARGET.name + ".tmp")
    if staging.exists():
        raise RuntimeError("An existing staging file must be preserved.")
    shutil.copy2(QA / "candidate.docx", staging)
    os.replace(staging, TARGET)
    assert sha(TARGET) == check["candidate_sha256"]
    print("Published: " + str(TARGET))


if __name__ == "__main__":
    args = argparse.ArgumentParser()
    args.add_argument("--publish", action="store_true")
    config = args.parse_args()
    publish() if config.publish else prepare()
