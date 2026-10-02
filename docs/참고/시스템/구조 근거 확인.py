"""Report source drift without silently accepting changed architecture evidence."""
from pathlib import Path
import hashlib
import json
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]

def check():
    baseline = json.loads((HERE / '분석 기준 파일.json').read_text(encoding='utf-8'))
    changed = []
    for item in baseline['files']:
        path = ROOT / item['path']
        actual = hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None
        if actual != item['sha256']:
            changed.append({'path': item['path'], 'state': 'changed' if actual else 'missing'})
    print(json.dumps({'checked_files': len(baseline['files']), 'changes': changed,
                     'meaning': 'Source identity check only, not correctness or runtime verification.'},
                    ensure_ascii=False, indent=2))
    return 1 if changed else 0

if __name__ == '__main__':
    sys.exit(check())
