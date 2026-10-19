"""Check real targets in the three modified project Markdown files."""
from pathlib import Path
from urllib.parse import unquote
import hashlib
import json
import re
import sys

sys.stdout.reconfigure(encoding='utf-8')
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[5]
TASK = HERE.parents[1]
paths = [ROOT / 'docs/참고/시스템/Sound1 시스템 구조.md',
         ROOT / 'docs/참고/인허가/사운드1/QCC 자료 추가 위치 관리.md',
         TASK / '작업.md']
records, failures = [], []
for path in paths:
    content = path.read_text(encoding='utf-8-sig')
    fence = False
    for number, line in enumerate(content.splitlines(), 1):
        if re.match(r'^\s*(`{3,}|~{3,})', line):
            fence = not fence
            continue
        if fence:
            continue
        for match in re.finditer(r'!?\[[^\]]*\]\(([^)]+)\)', line):
            target = match[1].strip()
            if target.startswith('<'):
                target = target[1:target.index('>')]
            if target.startswith('#') or re.match(r'^(?:https?|mailto|codex|app):', target):
                continue
            target = unquote(target.split('#', 1)[0].split('?', 1)[0])
            real = Path(target) if re.match(r'^[A-Za-z]:[\\/]', target) else path.parent / target
            exists = real.exists()
            records.append({'file': str(path), 'line': number, 'target': str(real.resolve()), 'exists': exists})
            if not exists:
                failures.append(records[-1])
result = {'scope': 'Changed project Markdown only; all local and G-drive links',
          'files': [{'path': str(p), 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()} for p in paths],
          'links': records, 'failures': failures, 'pass': not failures}
(HERE / 'changed-markdown-links.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'files': len(paths), 'links': len(records), 'failures': failures}, ensure_ascii=False))
sys.exit(bool(failures))
