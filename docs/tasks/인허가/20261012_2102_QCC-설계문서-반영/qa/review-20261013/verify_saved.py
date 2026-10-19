"""Read the authorized saved packages; distinguish current text from comparison history."""
from pathlib import Path
from zipfile import ZipFile
import hashlib
import json
import posixpath
import re
import sys

from lxml import etree as ET

sys.stdout.reconfigure(encoding="utf-8")
HERE = Path(__file__).resolve().parent
W = "http://schemas.openxmlformats.org/wordprocessingml/2006/main"
S = "http://schemas.openxmlformats.org/spreadsheetml/2006/main"
R = "http://schemas.openxmlformats.org/officeDocument/2006/relationships"
NS = {"w": W, "s": S, "r": R}
PREFIXES = ("[QCC 자료 입력]", "[QCC/E8300 자료 입력]", "[E8300 보완]")


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def active_comparison(value: str) -> str:
    # A cell may compare only one changed paragraph, with unchanged text around it.
    keep = True
    fragments = []
    for part in re.split(r"(\[-\]|\[\+\])", value):
        if part == "[-]":
            keep = False
        elif part == "[+]":
            keep = True
        elif keep:
            fragments.append(part)
    return "".join(fragments).strip()


def current_word_text(node) -> str:
    fragments = []
    for text in node.findall(".//w:t", NS):
        deleted = any(
            parent.tag == f"{{{W}}}del"
            or (parent.tag == f"{{{W}}}tr" and parent.find("w:trPr/w:del", NS) is not None)
            for parent in text.iterancestors()
        )
        if not deleted:
            fragments.append(text.text or "")
    return "".join(fragments)


def read_word(package: ZipFile) -> dict:
    doc = ET.fromstring(package.read("word/document.xml"))
    body = doc.find("w:body", NS)
    blocks = []
    for block in body:
        if block.tag == f"{{{W}}}sdt" and block.find(".//w:docPartGallery", NS) is not None:
            continue
        blocks.append(current_word_text(block))
    text = "\n".join(blocks)
    small = []
    deleted_empty_marks = []
    for paragraph in body.findall(".//w:p", NS):
        if any(parent.tag in (f"{{{W}}}del", f"{{{W}}}pPrChange") for parent in paragraph.iterancestors()):
            continue
        props = paragraph.find("w:pPr", NS)
        if props is None:
            continue
        if (props.find("w:rPr/w:del", NS) is not None
                and not current_word_text(paragraph)
                and paragraph.find(".//w:drawing", NS) is None):
            deleted_empty_marks.append(paragraph.get("{http://schemas.microsoft.com/office/word/2010/wordml}paraId"))
            continue
        size = props.find("w:rPr/w:sz", NS)
        line = props.find("w:spacing", NS)
        if size is not None and int(size.get(f"{{{W}}}val")) < 12:
            small.append({"text": current_word_text(paragraph)[:100], "size_half_pt": size.get(f"{{{W}}}val"), "paragraph_id": paragraph.get("{http://schemas.microsoft.com/office/word/2010/wordml}paraId")})
        if line is not None and line.get(f"{{{W}}}lineRule") == "exact" and int(line.get(f"{{{W}}}line", "0")) < 120:
            small.append({"text": current_word_text(paragraph)[:100], "line_twips": line.get(f"{{{W}}}line"), "paragraph_id": paragraph.get("{http://schemas.microsoft.com/office/word/2010/wordml}paraId")})
    return {"text": text, "small_body_paragraphs": small,
            "historical_deleted_empty_paragraph_marks": deleted_empty_marks,
            "markers": {p: text.count(p) for p in PREFIXES}}


def read_book(package: ZipFile) -> dict:
    shared = []
    if "xl/sharedStrings.xml" in package.namelist():
        shared = ["".join(t.text or "" for t in item.findall(".//s:t", NS)) for item in ET.fromstring(package.read("xl/sharedStrings.xml")).findall("s:si", NS)]
    workbook = ET.fromstring(package.read("xl/workbook.xml"))
    relations = {item.get("Id"): item.get("Target") for item in ET.fromstring(package.read("xl/_rels/workbook.xml.rels"))}
    sheets = []
    for tab in workbook.findall("s:sheets/s:sheet", NS):
        target = relations[tab.get(f"{{{R}}}id")]
        part = target.lstrip("/") if target.startswith("/") else posixpath.normpath("xl/" + target)
        root = ET.fromstring(package.read(part))
        values = {}
        heights = {}
        for row in root.findall("s:sheetData/s:row", NS):
            if row.get("ht") and float(row.get("ht")) > 409.5:
                heights[row.get("r")] = row.get("ht")
            for cell in row.findall("s:c", NS):
                value = cell.find("s:v", NS)
                text = value.text or "" if value is not None else ""
                if cell.get("t") == "s":
                    text = shared[int(text)]
                elif cell.get("t") == "inlineStr":
                    text = "".join(t.text or "" for t in cell.findall("s:is//s:t", NS))
                values[cell.get("r")] = text
        sheets.append({"name": tab.get("name"), "values": values, "merges": [m.get("ref") for m in root.findall("s:mergeCells/s:mergeCell", NS)], "heights_over_limit": heights})
    main = next(s for s in sheets if s["name"] != "표지")
    text = "\n".join(active_comparison(v) for v in main["values"].values())
    return {"sheets": sheets, "main": main, "text": text, "markers": {p: text.count(p) for p in PREFIXES}}


def column(reference: str) -> int:
    value = 0
    for letter in re.match(r"[A-Z]+", reference).group():
        value = 26 * value + ord(letter) - 64
    return value


def main() -> int:
    baseline = json.loads((HERE / "review-baseline.json").read_text(encoding="utf-8-sig"))
    before = "--before" in sys.argv[1:]
    report = {"protected_sources": [], "files": [], "pairs": [], "failures": []}
    lookup = {}
    for item in baseline:
        path = Path(item["snapshot"] if before and item["role"] == "editable-output" else item["path"])
        if item["role"] == "protected-source":
            unchanged = sha(path) == item["sha256"]
            report["protected_sources"].append({"path": str(path), "unchanged": unchanged})
            if not unchanged:
                report["failures"].append("Protected source changed: " + str(path))
            continue
        record = {"code": item["code"], "path": str(path), "sha256": sha(path), "changed_from_current_baseline": sha(path) != item["sha256"]}
        with ZipFile(path) as package:
            for part in package.namelist():
                if part.endswith((".xml", ".rels")):
                    ET.fromstring(package.read(part))
            record["all_xml_parse"] = True
            data = read_word(package) if path.suffix == ".docx" else read_book(package)
        record["current_requirement_ids"] = sorted(set(re.findall(r"SW-FW-RID-\d+", data["text"])), key=lambda value: int(value.rsplit("-", 1)[1]))
        record["markers"] = data["markers"]
        if path.suffix == ".docx":
            record["small_body_paragraphs"] = data["small_body_paragraphs"]
            record["historical_deleted_empty_paragraph_marks"] = data["historical_deleted_empty_paragraph_marks"]
            if record["small_body_paragraphs"]:
                report["failures"].append(item["code"] + " has small body paragraph formatting")
        else:
            record["row_height_over_limit"] = {s["name"]: s["heights_over_limit"] for s in data["sheets"] if s["heights_over_limit"]}
            if record["row_height_over_limit"]:
                report["failures"].append(item["code"] + " has rows above 409.5pt")
        if "SW-FW-RID-82" in record["current_requirement_ids"]:
            report["failures"].append(item["code"] + " still has current duplicate RID82")
        report["files"].append(record)
        lookup[item["code"]] = data
    for criteria, result, end_column in (("SUVC", "SUVR", 11), ("SITC", "SITR", 13), ("SSTC", "SSTR", 10)):
        a, b = lookup[criteria]["main"], lookup[result]["main"]
        diffs = []
        for cell in sorted(set(a["values"]) | set(b["values"])):
            if not 2 <= column(cell) <= end_column or int(re.search(r"\d+", cell).group()) < 24:
                continue
            if a["values"].get(cell, "") != b["values"].get(cell, ""):
                diffs.append({"cell": cell, "criteria": a["values"].get(cell, ""), "report": b["values"].get(cell, "")})
        def relevant_merge(sheet):
            return sorted(m for m in sheet["merges"] if column(m.split(":")[-1]) <= end_column and int(re.search(r"\d+", m).group()) >= 24)
        equal = relevant_merge(a) == relevant_merge(b)
        report["pairs"].append({"criteria": criteria, "report": result, "common_cells_differences": diffs, "common_merges_equal": equal})
        if diffs or not equal:
            report["failures"].append(criteria + "/" + result + " common cells/merges differ")
    target = HERE / ("initial-package-checks.json" if before else "saved-package-checks.json")
    target.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"files": len(report["files"]), "protected": len(report["protected_sources"]), "failures": report["failures"], "record": str(target)}, ensure_ascii=False))
    return 1 if report["failures"] else 0


if __name__ == "__main__":
    sys.exit(main())
