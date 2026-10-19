"""Recheck the three published documents without modifying them."""
from pathlib import Path
import hashlib
import io
import json
import zipfile
from lxml import etree

task = Path(__file__).resolve().parent
manifest = json.loads((task / 'evidence/final-output-manifest.json').read_text(encoding='utf-8'))
for item in manifest['files']:
    if item['code'] not in {'SAD', 'SDDD', 'SIP'}:
        continue
    data = Path(item['path']).read_bytes()
    actual = hashlib.sha256(data).hexdigest()
    assert actual.lower() == item['sha256'].lower(), item['code']
    with zipfile.ZipFile(io.BytesIO(data)) as package:
        assert package.testzip() is None
        for name in package.namelist():
            if name.endswith(('.xml', '.rels')):
                etree.fromstring(package.read(name))
    qa = item['latest_verification']
    assert actual == qa['published_sha256']
    assert Path(qa['qa_record']).is_file()
    print(json.dumps({'code': item['code'], 'sha256': actual, 'zip_and_xml': 'pass', 'pages_reviewed': qa['pages']}, ensure_ascii=False))
