"""Publish only the verified binary artifacts while guarding external edits."""
from compact_unit_style import *
import os, tempfile

checks=json.loads((Q/'checks.json').read_text(encoding='utf8'))
template_checks=json.loads((RQ/'checks.json').read_text(encoding='utf8'))
assert checks['passed'] and checks['units']==115 and checks['headings']==162
assert checks['candidate_sha256']==sha(Q/'candidate.docx')
assert template_checks['native_template_creation'] and template_checks['styles']==59
assert template_checks['template_sha256']==sha(RQ/'candidate.dotx')
visual=json.loads((Q/'visual-check.json').read_text(encoding='utf8'))
assert visual['passed'] and visual['candidate_sha256']==sha(Q/'candidate.docx')
assert visual['pages']==checks['pages']
pairs=[(LIVE,Q/'candidate.docx',Q/'before.docx'),(TPL,RQ/'candidate.dotx',RQ/'before.dotx')]
for live,candidate,baseline in pairs:
    assert sha(live)==sha(baseline),('External edit; not replaced',str(live))
prepared=[]
try:
    for live,candidate,baseline in pairs:
        with tempfile.NamedTemporaryFile(dir=live.parent,prefix='.tdc-publish-',suffix=live.suffix,delete=False) as f:
            f.write(candidate.read_bytes());temp=Path(f.name)
        assert sha(temp)==sha(candidate)
        prepared.append((live,temp,candidate,baseline))
    for live,temp,candidate,baseline in prepared:
        assert sha(live)==sha(baseline),('External edit during preparation',str(live))
        os.replace(temp,live)
        assert sha(live)==sha(candidate)
finally:
    for _,temp,_,_ in prepared:
        if temp.exists():temp.unlink()
result={'published':True,'pages':checks['pages'],'files':[{'path':str(live),'baseline_sha256':sha(baseline),'final_sha256':sha(live)} for live,_,baseline in pairs]}
jsonout(Q/'publication.json',result)
jsonout(RQ/'publication.json',result)
print(json.dumps(result,ensure_ascii=False))
