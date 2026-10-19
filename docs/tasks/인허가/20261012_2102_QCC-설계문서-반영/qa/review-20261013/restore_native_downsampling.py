"""Restore only diagnosed, byte-identified native downsampling differences."""
from pathlib import Path
import json
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8')
record = Path(sys.argv[1])
data = json.loads(record.read_text(encoding='utf-8-sig'))
assert data['checks']['body_revision_fingerprints']
assert data['checks']['current_notice_text']
mapping = {}
for view in ('accept', 'reject'):
    a, b = data['before_semantics'][view], data['after_semantics'][view]
    assert a['body_text_sha256'] == b['body_text_sha256']
    assert a['body_text_length'] == b['body_text_length']
    assert len(a['picture_hashes_in_order']) == len(b['picture_hashes_in_order'])
    for source, changed in zip(a['picture_hashes_in_order'], b['picture_hashes_in_order']):
        if source != changed:
            assert source not in mapping or mapping[source] == changed
            mapping[source] = changed
assert mapping, 'No exclusively identified media difference'
for original, compressed in mapping.items():
    subprocess.run([sys.executable, str(Path(__file__).with_name('restore_word_media.py')),
                    data['before'], data['after'], original, compressed], check=True)
