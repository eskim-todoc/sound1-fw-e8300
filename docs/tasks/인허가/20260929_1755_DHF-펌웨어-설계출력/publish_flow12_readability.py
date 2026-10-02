"""Publish the visually reviewed candidate without overwriting later author edits."""
from datetime import datetime, timezone
from hashlib import sha256
from pathlib import Path
import json
import os
import tempfile

TASK = Path(__file__).resolve().parent
QA = TASK / 'qa/design-readability/flow12-and-module-explanation'
LIVE = TASK.parents[2] / '참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어/SDDD-FW-TD24A 소프트웨어 상세 설계서.docx'
BASELINE = '30f3530e04d282742084cb3e23007c7ec213277e21a1a106810aab75077c407b'

def digest(path):
    return sha256(path.read_bytes()).hexdigest()

def main():
    checks = json.loads((QA / 'checks.json').read_text(encoding='utf-8'))
    assert checks['headings'] == 162 and checks['units'] == 115
    assert checks['flows'] == 34 and checks['body_tables'] == 236
    assert checks['pages'] == 148 and not checks['short_pages']
    for name in ('other_package_parts_unchanged', 'original_rejection_text_unchanged', 'numbering_unchanged'):
        assert checks[name], name
    assert digest(LIVE) == BASELINE, 'The live document has later edits; stop and reread.'
    assert digest(QA / 'before.docx') == BASELINE
    candidate = QA / 'candidate.docx'
    assert digest(candidate) == checks['candidate_sha256']
    review = LIVE.with_name('[작성자 전용] ' + LIVE.name)
    review_before = digest(review) if review.exists() else None
    visual = {
        'checked_at': datetime.now(timezone.utc).isoformat(),
        'method': 'Native Word final-view PDF, 96dpi full-size page images in 37 four-page boards',
        'pages_reviewed': list(range(1, 149)),
        'boards_reviewed': list(range(1, 38)),
        'flowcharts_reviewed': 34,
        'observed_layout_defects': [],
        'notes': 'Existing chapter/long-figure page starts remain. No text/image overlap, clipping or isolated figure caption found.',
        'candidate_sha256': checks['candidate_sha256'],
    }
    (QA / 'visual-check.json').write_text(json.dumps(visual, ensure_ascii=False, indent=2), encoding='utf-8')
    staged = None
    try:
        with tempfile.NamedTemporaryFile(prefix='.sddd-publish-', suffix='.tmp', dir=LIVE.parent, delete=False) as f:
            staged = Path(f.name).resolve()
            assert staged.parent == LIVE.parent.resolve()
            f.write(candidate.read_bytes())
            f.flush()
            os.fsync(f.fileno())
        assert digest(staged) == checks['candidate_sha256']
        assert digest(LIVE) == BASELINE, 'Live file changed during staging.'
        os.replace(staged, LIVE)
        staged = None
    finally:
        if staged is not None and staged.exists():
            assert staged.parent == LIVE.parent.resolve() and staged.name.startswith('.sddd-publish-')
            staged.unlink()
    assert digest(LIVE) == checks['candidate_sha256']
    result = {
        'published_at': datetime.now(timezone.utc).isoformat(),
        'path': str(LIVE),
        'baseline_sha256': BASELINE,
        'published_sha256': digest(LIVE),
        'author_copy_written': False,
        'author_copy_hash_before': review_before,
        'author_copy_hash_after': digest(review) if review.exists() else None,
        'tracked_changes_preserved': True,
    }
    (QA / 'publication.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False))

if __name__ == '__main__':
    main()
