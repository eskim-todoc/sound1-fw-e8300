"""Restore a known source PNG after native field-save image compression."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
from io import BytesIO
from PIL import Image
import argparse
import hashlib
import json
import sys

sys.stdout.reconfigure(encoding="utf-8")
p = argparse.ArgumentParser()
p.add_argument("before", type=Path)
p.add_argument("candidate", type=Path)
p.add_argument("original_hash")
p.add_argument("compressed_hash")
a = p.parse_args()
def sha(value):
    return hashlib.sha256(value).hexdigest()
with ZipFile(a.before) as package:
    sources = [(name, package.read(name)) for name in package.namelist()
               if name.startswith("word/media/") and sha(package.read(name)) == a.original_hash]
with ZipFile(a.candidate) as package:
    data = {name: package.read(name) for name in package.namelist()}
targets = [name for name, content in data.items()
           if name.startswith("word/media/") and sha(content) == a.compressed_hash]
if len(sources) != 1 or len(targets) != 1:
    raise ValueError("Expected exactly one identified original and compressed media part")
source_name, content = sources[0]
target = targets[0]
original_size = Image.open(BytesIO(content)).size
compressed_size = Image.open(BytesIO(data[target])).size
if original_size[0] <= compressed_size[0] or original_size[1] <= compressed_size[1]:
    raise ValueError("Not the diagnosed native downsampling case")
before_hash = sha(a.candidate.read_bytes())
data[target] = content
temporary = a.candidate.with_suffix(".media-restored.docx")
with ZipFile(temporary, "w", ZIP_DEFLATED) as package:
    for name, content in data.items():
        package.writestr(name, content)
temporary.replace(a.candidate)
record = {"source": str(a.before), "candidate": str(a.candidate),
          "original_part": source_name, "candidate_part": target,
          "original_sha256": a.original_hash, "compressed_sha256": a.compressed_hash,
          "original_size": original_size, "compressed_size": compressed_size,
          "candidate_before_sha256": before_hash, "candidate_after_sha256": sha(a.candidate.read_bytes()),
          "scope": "One known media part restored byte-for-byte; document, relationships, fields, styles and revisions untouched"}
a.candidate.with_suffix(".media-restoration.json").write_text(json.dumps(record, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps(record, ensure_ascii=False))
