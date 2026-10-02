from copy import deepcopy
from pathlib import Path
import hashlib,json,zipfile,shutil,re
from lxml import etree as E
from update_method_definitions import Q,D,LEGEND
P=D/'03_설계 출력/10_성능검사기준서/EPTC-TD24A 필수성능시험 기준서.docx'
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main';ns={'w':W}
def t(n):return '{'+W+'}'+n
def text(p):return ''.join(p.xpath('.//w:t[not(ancestor::w:del)]/text()',namespaces=ns))
qa=Q/'performance';qa.mkdir(exist_ok=True)
if not (qa/'before.docx').exists():shutil.copy2(P,qa/'before.docx')
assert P.read_bytes()==(qa/'before.docx').read_bytes()
with zipfile.ZipFile(P) as z:parts={n:z.read(n) for n in z.namelist()}
r=E.fromstring(parts['word/document.xml']);body=r.find(t('body'))
anchor=next(p for p in body.findall(t('p')) if text(p).strip()=='시험 목록')
normal=next(p for p in r.xpath('//w:p',namespaces=ns) if text(p).strip().startswith('외부와 소음'))
newid=max([int(x.get(t('id'))) for x in r.xpath('//w:ins|//w:del',namespaces=ns)]+[0])+1
def para(s,bold=False):
 global newid
 p=E.Element(t('p'));pr=normal.find(t('pPr'))
 if pr is not None:p.append(deepcopy(pr))
 ins=E.SubElement(p,t('ins'));ins.set(t('id'),str(newid));newid+=1;ins.set(t('author'),'김은수');ins.set(t('date'),'2026-10-01T00:00:00Z')
 run=E.SubElement(ins,t('r'));rp=E.SubElement(run,t('rPr'))
 ff=E.SubElement(rp,t('rFonts'));ff.set(t('ascii'),'맑은 고딕');ff.set(t('eastAsia'),'맑은 고딕');ff.set(t('hAnsi'),'맑은 고딕')
 E.SubElement(rp,t('sz')).set(t('val'),'20')
 if bold:E.SubElement(rp,t('b'))
 tt=E.SubElement(run,t('t'));tt.text=s
 return p
position=body.index(anchor)
for s in ['시험 방법 구분']+LEGEND.splitlines():
 body.insert(position,para(s,s=='시험 방법 구분'));position+=1
labels=[]
for p in list(body.findall(t('p'))):
 s=text(p).strip()
 if re.fullmatch(r'(?:시험 )?ID : SYS-EP-TEST-ID-\d+',s):
  number=int(s.split('-')[-1]);labels.append(number)
  body.insert(body.index(p)+1,para('시험 방법 : T (Test)'))
assert sorted(labels)==list(range(1,10)),labels
parts['word/document.xml']=E.tostring(r,encoding='UTF-8',xml_declaration=True,standalone=True)
settings=E.fromstring(parts['word/settings.xml'])
if settings.find(t('trackRevisions')) is None:E.SubElement(settings,t('trackRevisions'))
parts['word/settings.xml']=E.tostring(settings,encoding='UTF-8',xml_declaration=True,standalone=True)
candidate=qa/'candidate.docx'
with zipfile.ZipFile(candidate,'w',zipfile.ZIP_DEFLATED) as z:
 for n,data in parts.items():z.writestr(n,data)
# Removing only the inserted paragraphs reconstructs the unchanged source XML tree.
original=E.fromstring(zipfile.ZipFile(qa/'before.docx').read('word/document.xml'))
copy=deepcopy(r)
for p in list(copy.find(t('body'))):
 if p.tag==t('p') and p.find(t('ins')) is not None and int(p.find(t('ins')).get(t('id')))>max([int(x.get(t('id'))) for x in original.xpath('//w:ins|//w:del',namespaces=ns)]+[0]):p.getparent().remove(p)
assert E.tostring(copy)==E.tostring(original)
(qa/'checks.json').write_text(json.dumps({'source_sha':hashlib.sha256(P.read_bytes()).hexdigest(),'candidate_sha':hashlib.sha256(candidate.read_bytes()).hexdigest(),'inserted_paragraphs':15,'test_methods':labels,'other_content_preserved':True,'changed_parts':['word/document.xml','word/settings.xml']},indent=2),encoding='utf8')
print('EPTC definitions and 9 test methods inserted with tracked changes')
