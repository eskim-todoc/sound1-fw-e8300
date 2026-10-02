import json,hashlib
from update_method_definitions import Q,D,sha
checks=json.loads((Q/'checks.json').read_text(encoding='utf8'))
excluded=D/'03_설계 출력/10_성능검사기준서/EPTC-TD24A 필수성능시험 기준서.docx'
assert sha(excluded)==sha(Q/'performance/before.docx'),'Excluded original changed'
record={
 'date':'2026-10-01','hashes':{x['key']:x['candidate_sha'] for x in checks},
 'main_pages':{'input':4,'system':5,'unit':12,'integration':4},
 'review':'Native Excel PDF main pages, legend and comparison samples reviewed; actual changed characters verified for every comparison cell. No clipped cell text found.',
 'excluded':{'path':str(excluded),'sha':sha(excluded),'reason':'User explicitly excluded essential performance criteria before publication; original not changed.'},
 'protected_author_only_and_source':'Hash equality verified by update_method_definitions.py',
}
(Q/'visual-reviewed.json').write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf8')
print(record)
