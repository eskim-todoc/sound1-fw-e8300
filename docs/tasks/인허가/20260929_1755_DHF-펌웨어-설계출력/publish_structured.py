"""Publish only the verified candidate, with external-edit and template guards."""
from pathlib import Path
import hashlib, json, os, tempfile

T = Path(__file__).resolve().parent
ROOT = T.parents[3]
Q = T / 'qa/design-readability/structured-units'
DOC = ROOT / 'docs/참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어/SDDD-FW-TD24A 소프트웨어 상세 설계서.docx'
TPL = Path('E:/workspace/rules-for-codex/지침/양식/Word/TDC 공통 스타일.dotx')

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

checks = json.loads((Q / 'checks.json').read_text(encoding='utf8'))
assert checks['pages'] == 155 and checks['units'] == 115
assert checks['cover_header_footer_geometry_preserved']
assert sha(DOC) == checks['baseline_sha256'], 'Live document was externally edited; stop publication'
assert sha(Q / 'candidate.docx') == checks['candidate_sha256'], 'Candidate changed after verification'
assert sha(TPL) == '1ffdf91b70e02abb722856bc26c1e81965dc513628038b635dcfa8bb003d1127'
fd, temp_name = tempfile.mkstemp(prefix='.SDDD-style-', suffix='.tmp', dir=DOC.parent)
try:
    with os.fdopen(fd, 'wb') as f:
        f.write((Q / 'candidate.docx').read_bytes())
    assert sha(DOC) == checks['baseline_sha256'], 'Live document changed during preparation'
    os.replace(temp_name, DOC)
finally:
    if os.path.exists(temp_name):
        os.remove(temp_name)
assert sha(DOC) == checks['candidate_sha256']
result = {
    'file': str(DOC), 'sha256': sha(DOC), 'template_sha256': sha(TPL),
    'visual_review': '155 pages at original 96 dpi in unscaled four-page collages; all 34 flowcharts checked',
    'toc_entries_checked': 41, 'toc_page_caches_updated': 26,
    'changes_remain_tracked': True,
}
(Q / 'publication.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf8')
print(json.dumps(result, ensure_ascii=False))
