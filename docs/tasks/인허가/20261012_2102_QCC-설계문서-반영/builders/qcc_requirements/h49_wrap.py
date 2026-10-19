import copy,hashlib,json,pathlib,zipfile
from lxml import etree as E
ROOT=pathlib.Path(r'E:\workspace\projects\sound1-fw-e8300-backup\docs\tasks\인허가\20261012_2102_QCC-설계문서-반영');evfile=ROOT/'evidence/qcc_requirements_authored.json';ev=json.loads(evfile.read_text(encoding='utf8'));out=pathlib.Path(ev['outputs'][0]['file']);expected='3E2DA4B6238FCBB9D6C8FA5E71EFC90AEDD84D08B04F8914E232EAEFB44CFEFF';X='{http://schemas.openxmlformats.org/spreadsheetml/2006/main}'
def parts(f):
 with zipfile.ZipFile(f) as z:return {n:z.read(n) for n in z.namelist()}
def key(el):return (el.tag,tuple(sorted(el.attrib.items())),el.text,tuple(key(c) for c in el))
assert hashlib.sha256(out.read_bytes()).hexdigest().upper()==expected
p=parts(out);before=copy.deepcopy(p);s=E.fromstring(p['xl/worksheets/sheet2.xml']);c=s.find('.//'+X+'c[@r="H49"]');original=copy.deepcopy(c);styles=E.fromstring(p['xl/styles.xml']);xfs=styles.find(X+'cellXfs');oldstyle=int(c.get('s'));desired=copy.deepcopy(xfs[oldstyle]);desired.find(X+'alignment').set('wrapText','1');desired.set('applyAlignment','1')
candidate=parts(ROOT/'qa/qcc_requirements/h49-wrap-artifact.xlsx');candidate_sheet=E.fromstring(candidate['xl/worksheets/sheet2.xml']);candidate_c=candidate_sheet.find('.//'+X+'c[@r="H49"]');candidate_xfs=E.fromstring(candidate['xl/styles.xml']).find(X+'cellXfs');assert candidate_xfs[int(candidate_c.get('s'))].find(X+'alignment').get('wrapText')=='1'
matches=[i for i,xf in enumerate(xfs) if key(xf)==key(desired)];assert matches,'existing identical wrap style expected';newstyle=matches[0];c.set('s',str(newstyle))
check=copy.deepcopy(c);check.set('s',str(oldstyle));assert key(check)==key(original)
height=s.find('.//'+X+'row[@r="49"]').get('ht');assert float(height)>=193
p['xl/worksheets/sheet2.xml']=E.tostring(s,encoding='UTF-8',xml_declaration=True,standalone=True)
assert [n for n in before if before[n]!=p[n]]==['xl/worksheets/sheet2.xml']
assert hashlib.sha256(out.read_bytes()).hexdigest().upper()==expected
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
 for name,data in p.items():z.writestr(name,data)
after=parts(out);assert [n for n in before if before[n]!=after[n]]==['xl/worksheets/sheet2.xml'];saved=E.fromstring(after['xl/worksheets/sheet2.xml']);saved_c=saved.find('.//'+X+'c[@r="H49"]');assert int(saved_c.get('s'))==newstyle and saved.find('.//'+X+'row[@r="49"]').get('ht')==height
newhash=hashlib.sha256(out.read_bytes()).hexdigest().upper();ev['outputs'][0]['sha256']=newhash;ev['outputs'][0]['H49_native_QA_wrap_fix']={'old_sha256':expected,'new_sha256':newhash,'cell':'H49','value_id_unchanged':True,'old_style':oldstyle,'reused_wrap_style':newstyle,'row49_height_unchanged':height,'only_package_part_changed':'xl/worksheets/sheet2.xml','artifact_candidate':str(ROOT/'qa/qcc_requirements/h49-wrap-artifact.xlsx')};evfile.write_text(json.dumps(ev,ensure_ascii=False,indent=2),encoding='utf8');print(json.dumps(ev['outputs'][0]['H49_native_QA_wrap_fix'],ensure_ascii=False))
