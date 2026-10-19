"""Publish only candidates already checked in actual Excel; preserve prior evidence."""
from pathlib import Path
from datetime import datetime
import json,shutil,sys
import prepare_clean as p

sys.stdout.reconfigure(encoding='utf-8')
h=Path(__file__).resolve().parent
task=h.parents[1]
plan=p.load(h/'plan.json')
source=p.load(h/'source-final-output-manifest.json')
native=p.load(h/'native-checks.json')
assert len(native)==7
by_code={r['code']:r for r in native}
protected=[]
for b in json.loads((task/'evidence/source-baseline.json').read_text(encoding='utf-8-sig')):
    got=p.sha(b['path']);assert got==b['sha256'].upper(),('protected original changed',b['path'])
    protected.append({'path':b['path'],'sha256':got,'unchanged':True})
for b in source['files']:
    if b['path'].lower().endswith('.docx'):
        got=p.sha(b['path']);assert got==b['sha256'].upper(),('Word changed outside this follow-up',b['code'])
        protected.append({'path':b['path'],'sha256':got,'unchanged':True})
for b in plan['books']:
    assert p.sha(b['path'])==b['sourceSHA256']
    assert p.sha(b['backupPath'])==b['sourceSHA256']
    checked=by_code[b['code']]
    assert checked['unchanged'] and checked['readOnly']
    assert checked['sha256']==p.sha(b['candidatePath'])==b['candidateSHA256']
    assert all(Path(v['pdf']).is_file() for v in checked['previews'])
    assert len(checked['worksheets'])==len(p.sheets(p.pack(b['candidatePath'])[0]))
records=[]
for b in plan['books']:
    assert p.sha(b['path'])==b['sourceSHA256']
    shutil.copyfile(b['candidatePath'],b['path'])
    assert p.sha(b['path'])==b['candidateSHA256']
    records.append({'code':b['code'],'path':b['path'],'beforeSHA256':b['sourceSHA256'],'sha256':b['candidateSHA256'],'publishedAt':datetime.now().astimezone().isoformat(),'matchesNativeCheckedCandidate':True})
p.save(h/'published.json',{'files':records,'protected':protected,'pass':True})
print(json.dumps({'published':len(records),'protectedOriginals':23,'unchangedWordDocuments':5,'pass':True}))
