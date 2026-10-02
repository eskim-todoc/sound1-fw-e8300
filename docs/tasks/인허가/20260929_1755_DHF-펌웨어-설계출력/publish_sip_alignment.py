"""Publish only the checked SIP candidate without overwriting external edits."""
from hashlib import sha256
import json
import os
from patch_sip_alignment import QA, LIVE, BASE

checks=json.loads((QA/'verify.json').read_text(encoding='utf-8'))
assert checks['passed']
visual=json.loads((QA/'visual-check.json').read_text(encoding='utf-8'))
assert visual['reviewed_pages']==list(range(1,13)) and visual['new_layout_defects']==[]
data=(QA/'candidate.docx').read_bytes()
digest=sha256(data).hexdigest()
assert digest==checks['candidate_sha256']
assert sha256(LIVE.read_bytes()).hexdigest()==BASE,'기존 문서가 외부에서 변경됨'
staged=QA/'publication-staged.docx'
staged.write_bytes(data)
os.replace(staged,LIVE)
assert sha256(LIVE.read_bytes()).hexdigest()==digest
result={'published':True,'path':str(LIVE),'sha256':digest,
        'baseline_sha256':BASE,'tracked_changes_preserved':True}
(QA/'publication.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(result,ensure_ascii=False))
