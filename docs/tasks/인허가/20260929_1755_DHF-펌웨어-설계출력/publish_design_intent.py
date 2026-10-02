"""Publish only the verified and visually reviewed current SDDD candidate."""
from datetime import datetime, timezone
from hashlib import sha256
from pathlib import Path
import json
import os
import tempfile

T=Path(__file__).resolve().parent
Q=T/'qa/design-readability/design-intent-and-placeholders'
LIVE=T.parents[2]/'참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어/SDDD-FW-TD24A 소프트웨어 상세 설계서.docx'
def digest(p):return sha256(p.read_bytes()).hexdigest()

def main():
    patch=json.loads((Q/'patch.json').read_text(encoding='utf8'))
    checks=json.loads((Q/'checks.json').read_text(encoding='utf8'))
    visual=json.loads((Q/'visual-check.json').read_text(encoding='utf8'))
    assert checks['units']==115 and checks['headings']==162 and checks['body_tables']==236 and checks['flows']==34
    assert checks['red_markers']==4 and not checks['short_pages'] and not checks['out_of_page_blocks']
    assert all(checks[k] for k in ['numbering_unchanged','source_rejection_unchanged','formulas_unchanged','unrelated_parts_unchanged'])
    assert visual['candidate_sha256']==checks['candidate_sha256']
    assert visual['pages_reviewed']==list(range(1,checks['pages']+1))
    assert not visual['unresolved_layout_defects']
    assert digest(Q/'candidate.docx')==checks['candidate_sha256']
    assert digest(LIVE)==patch['baseline'] and digest(Q/'before.docx')==patch['baseline'],'Later author edits detected'
    stage=None
    try:
        with tempfile.NamedTemporaryFile(prefix='.sddd-publish-',suffix='.tmp',dir=LIVE.parent,delete=False) as f:
            stage=Path(f.name).resolve();assert stage.parent==LIVE.parent.resolve()
            f.write((Q/'candidate.docx').read_bytes());f.flush();os.fsync(f.fileno())
        assert digest(stage)==checks['candidate_sha256'] and digest(LIVE)==patch['baseline']
        os.replace(stage,LIVE);stage=None
    finally:
        if stage is not None and stage.exists():
            assert stage.parent==LIVE.parent.resolve() and stage.name.startswith('.sddd-publish-')
            stage.unlink()
    assert digest(LIVE)==checks['candidate_sha256']
    result={'published_at':datetime.now(timezone.utc).isoformat(),'baseline_sha256':patch['baseline'],
            'published_sha256':digest(LIVE),'file':str(LIVE),'author_copy_written':False,
            'tracked_changes_preserved':True,'pages':checks['pages']}
    (Q/'publication.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(result,ensure_ascii=True))

if __name__=='__main__':main()
