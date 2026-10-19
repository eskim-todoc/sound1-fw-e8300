from pathlib import Path
import json,hashlib,shutil
from zipfile import ZipFile
QA=Path(__file__).resolve().parent/'qa/sad-qcc-cleanup-20261016'
meta=json.loads((QA/'changes.json').read_text(encoding='utf8'))
check=json.loads((QA/'word-render-check.json').read_text(encoding='utf8'))
target=Path(meta['target']);candidate=QA/'candidate.docx'
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert check['reject_new_matches_before_after_word_accept']
allowed={meta['sha256_before']}
prior=QA/'publication.json'
if prior.exists():allowed.add(json.loads(prior.read_text(encoding='utf8'))['sha256_candidate'])
assert digest(target) in allowed,'Concurrent edit; do not replace'
assert digest(candidate)==meta['sha256_candidate']
with ZipFile(candidate)as z:assert z.testzip()is None
shutil.copyfile(candidate,target)
assert digest(target)==meta['sha256_candidate']
meta['published']=True
meta['verification']={
 'native_word_open_and_all_pages_rendered':True,'pages':check['pages'],
 'visual_review':'54 pages reviewed; page 4 blank and shortened header of later section are also present in before.pdf and were preserved.',
 'native_word_reject_new_matches_before':True,
 'table3_caption_and_table_absent_in_accepted_preview':True,
 'final_readback_hash':digest(target),
 'repaired_before_publication':'Deleted SEQ instruction converted to delInstrText; native Word open and rejection repeated successfully.'}
(QA/'publication.json').write_text(json.dumps(meta,ensure_ascii=False,indent=2),encoding='utf8')
print('PUBLISHED',target,'SHA256',digest(target))
