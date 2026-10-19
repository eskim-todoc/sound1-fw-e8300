"""Apply narrowly owned Word layout repairs without accepting body revisions."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
from copy import deepcopy
from lxml import etree as E
import argparse
import hashlib
import json
import sys

sys.stdout.reconfigure(encoding="utf-8")
W = "http://schemas.openxmlformats.org/wordprocessingml/2006/main"
N = {"w": W}
def q(name):
    return "{" + W + "}" + name
def current_text(node):
    return "".join(t.text or "" for t in node.findall(".//w:t", N)
                   if not any(p.tag == q("del") for p in t.iterancestors()))
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("code", choices=("SGR", "SAD"))
    parser.add_argument("source", type=Path)
    parser.add_argument("candidate", type=Path)
    args = parser.parse_args()
    with ZipFile(args.source) as package:
        data = {part: package.read(part) for part in package.namelist()}
    doc = E.fromstring(data["word/document.xml"])
    records = []
    if args.code == "SGR":
        starters = ("onsemi E8300 SDK", "QCC5181은 제조사 제공 Bluetooth stack")
        for paragraph in doc.findall(".//w:body//w:p", N):
            text = current_text(paragraph)
            if not text.startswith(starters):
                continue
            props = paragraph.find("w:pPr", N)
            change = props.find("w:pPrChange", N) if props is not None else None
            # These are formatting revisions introduced during this same update.
            # Keep their original properties intact so rejecting the revision
            # still restores the pre-update paragraph formatting.
            if change is None or change.get(q("author")) != "김은수" or not change.get(q("date"), "").startswith("2026-10-12T23:56"):
                raise ValueError("Expected current-update formatting revision: " + text)
            previous = props.find("w:keepNext", N)
            if previous is None:
                previous = E.Element(q("keepNext"))
                style = props.find("w:pStyle", N)
                props.insert(1 if style is not None else 0, previous)
            previous.attrib.pop(q("val"), None)
            records.append({"text": text, "change_id": change.get(q("id")),
                            "repair": "Keep SOUP description and its evidence notice together; existing revision's old properties preserved"})
        if len(records) != 2:
            raise ValueError("Expected exactly two SOUP description paragraphs")
    else:
        paragraphs = doc.xpath('.//w:p[@w14:paraId="34E32455"]', namespaces={**N, "w14": "http://schemas.microsoft.com/office/word/2010/wordml"})
        if len(paragraphs) != 1:
            raise ValueError("TOC end paragraph locator changed")
        paragraph = paragraphs[0]
        props = paragraph.find("w:pPr", N)
        spacing = props.find("w:spacing", N)
        if spacing is None:
            spacing = E.SubElement(props, q("spacing"))
        before = dict(spacing.attrib)
        spacing.set(q("after"), "0")
        spacing.set(q("line"), "240")
        spacing.set(q("lineRule"), "auto")
        for run_props in [props.find("w:rPr", N)] + paragraph.findall("w:r/w:rPr", N):
            if run_props is None:
                continue
            for tag in ("sz", "szCs"):
                size = run_props.find("w:" + tag, N)
                if size is not None:
                    size.set(q("val"), "20")
            hidden = run_props.find("w:vanish", N)
            if hidden is not None:
                run_props.remove(hidden)
        records.append({"paragraph_id": "34E32455", "before_spacing": before,
                        "repair": "Normal 10pt/automatic TOC end paragraph; computed TOC only, existing pPrChange history preserved"})
    data["word/document.xml"] = E.tostring(doc, encoding="UTF-8", xml_declaration=True, standalone=True)
    args.candidate.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(args.candidate, "w", ZIP_DEFLATED) as package:
        for part, content in data.items():
            package.writestr(part, content)
    result = {"code": args.code, "source": str(args.source), "candidate": str(args.candidate),
              "source_sha256": hashlib.sha256(args.source.read_bytes()).hexdigest(),
              "candidate_sha256": hashlib.sha256(args.candidate.read_bytes()).hexdigest(),
              "repairs": records, "scope": "QA candidate only; no revisions accepted/rejected"}
    args.candidate.with_suffix(".layout.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False))
if __name__ == "__main__":
    main()
