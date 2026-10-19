"""Publish only the five reviewed RID83 corrections, with current-file guards."""
from pathlib import Path
from zipfile import ZipFile
from lxml import etree
import hashlib, json, shutil, datetime, sys

sys.stdout.reconfigure(encoding='utf-8')
QA = Path(__file__).resolve().parent
TASK = QA.parent.parent
def read(path): return json.loads(Path(path).read_text(encoding='utf-8-sig'))
def digest(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()
def save(path, data): Path(path).write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf-8')

baseline = read(QA/'source-manifest.json')
protected = read(TASK/'evidence/source-baseline.json')
excel = read(QA/'package-checks.json')
word = read(QA/'word/authored.json')
native_excel = {x['code']:x for x in read(QA/'native-checks.json')}
native_word = {x['code']:x for x in read(QA/'word/native-checks.json')}
native_revisions = read(QA/'word/native-projection-checks.json')
assert all(x['nativeAcceptMatchesCurrent'] and x['nativeRejectMatchesBefore'] for x in native_revisions)
assert {x['code'] for x in native_revisions} == {'SR','SAD'}
test_review = read(QA/'tests-final-review.json')
assert test_review['status'] == 'PASS_SCOPED' and not test_review['failures']
selected = {x['code']:(Path(x['path']),x['sha256']) for x in excel}
selected.update({x['code']:(Path(x['candidate']),x['candidate_sha256']) for x in word})
assert set(selected) == {'SRS','SSTC','SSTR','SR','SAD'}

# Refuse to overwrite any intervening user edits, including the other seven files.
for old in baseline['files']:
    assert digest(old['path']) == old['sha256'].upper(), f"Current G file changed: {old['code']}"
for old in protected:
    assert digest(old['path']) == old['sha256'].upper(), f"Protected original changed: {old['path']}"
for code,(candidate,expected) in selected.items():
    assert digest(candidate) == expected.upper(), f"Candidate changed: {code}"
    check = native_excel.get(code) or native_word[code]
    assert check['sha256'].upper() == expected.upper(), f"Native evidence stale: {code}"
    if code in {'SR','SAD'}:
        projection_check=next(x for x in native_revisions if x['code']==code)
        assert projection_check['candidateSHA256'].upper()==expected.upper(), f"Revision QA stale: {code}"
    with ZipFile(candidate) as package:
        assert package.testzip() is None
        for name in package.namelist():
            if name.endswith(('.xml','.rels')): etree.fromstring(package.read(name))

record={'checked_at':datetime.datetime.now().astimezone().isoformat(), 'files':[], 'protected_originals':[],
        'scope':'Five RID83 corrections; seven other current combined documents unchanged; no product execution or approval.'}
save(QA/'publish-preflight.json',{'all_12_current_hashes_match':True,'all_23_originals_unchanged':True,
                                'validated_candidates':{c:h.upper() for c,(_,h) in selected.items()}})
for old in baseline['files']:
    code=old['code']; destination=Path(old['path'])
    if code in selected:
        candidate,expected=selected[code]
        # Keep the immediately preceding complete package before any replacement.
        recovery=QA/'publish-backups'/destination.name
        recovery.parent.mkdir(exist_ok=True)
        if recovery.exists(): assert digest(recovery)==old['sha256'].upper()
        else: shutil.copy2(destination,recovery)
        assert digest(destination)==old['sha256'].upper()
        shutil.copyfile(candidate,destination)
        assert digest(destination)==expected.upper()
    else: expected=old['sha256']
    record['files'].append({'code':code,'path':str(destination),'sha256':digest(destination),
                            'previous_sha256':old['sha256'],'changed':code in selected,
                            'candidate_matches_published':digest(destination)==expected.upper()})
    save(QA/'published.json',record)
for old in protected:
    unchanged=digest(old['path'])==old['sha256'].upper()
    assert unchanged
    record['protected_originals'].append({'path':old['path'],'sha256':old['sha256'],'unchanged':unchanged})
save(QA/'published.json',record)
print(json.dumps({'changed':len(selected),'other_current_unchanged':7,'protected_unchanged':len(protected)},ensure_ascii=False))
