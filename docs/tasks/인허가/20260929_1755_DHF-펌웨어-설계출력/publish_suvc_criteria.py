"""Publish only a verified candidate, protecting external edits and file locks."""
from pathlib import Path
import json,hashlib,shutil,os,zipfile
import openpyxl
T=Path(__file__).resolve().parent;ROOT=T.parents[3];Q=T/'qa/suvc-practical-criteria'
F=ROOT/'docs/참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어'
target=F/'SUVC-FW-TD24A 소프트웨어 유닛 검증 기준서.xlsx'
candidate=Q/'candidate.xlsx'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
baseline=json.loads((Q/'baseline.json').read_text('utf-8'));checks=json.loads((Q/'checks.json').read_text('utf-8'))
assert sha(target)==baseline['workbook_sha256'],'External workbook edits detected; do not overwrite'
assert sha(candidate)==checks['candidate_sha256'],'Candidate changed since verification'
assert sha(F/'SDDD-FW-TD24A 소프트웨어 상세 설계서.docx')==baseline['detail_sha256']
assert (Q/'visual-check.json').exists(),'Visual review not recorded'
assert json.loads((Q/'visual-check.json').read_text('utf-8'))['candidate_sha256']==sha(candidate)
with zipfile.ZipFile(Q/'before.xlsx') as a,zipfile.ZipFile(candidate) as b:
    media=lambda z:{n:z.read(n) for n in z.namelist() if n.startswith('xl/media/')}
    assert media(a)==media(b),'Images changed'
wb=openpyxl.load_workbook(candidate)
s=wb['기준서_유닛 검증'];assert s['D144'].value=='SW-FW-UnitTest-BOOT-ID-20'
assert 'QCC' in s['C19'].value and '여기에' in s['D19'].value
assert all(wb[n].freeze_panes is None for n in ['기준서_유닛 검증','기준서_유닛 검증_변경 비교'])
# Windows raises PermissionError if Word/Excel owns an incompatible file lock;
# never close a user-owned Office instance to bypass it.
tmp=F/'.suvc-verified-publication.tmp'
try:
    shutil.copyfile(candidate,tmp)
    assert sha(tmp)==checks['candidate_sha256']
    assert sha(target)==baseline['workbook_sha256'],'Source changed while preparing publication'
    os.replace(tmp,target)
except Exception:
    if tmp.exists():tmp.unlink()
    raise
assert sha(target)==checks['candidate_sha256']
(Q/'publication.json').write_text(json.dumps({'path':str(target),'sha256':sha(target),'source_protected':True,'same_file_name':True,'images_preserved':True},ensure_ascii=False,indent=2),encoding='utf-8')
print('Published:',target)
