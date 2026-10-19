from pathlib import Path
from copy import deepcopy
from datetime import datetime,timezone
import zipfile,hashlib,json
from lxml import etree as E
TASK=Path(__file__).parent
BASE=Path('G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF/03_설계 출력/SW/외부기')
PATH=next(BASE.glob('[[]E8300-QCC] SAD*.docx'))
ORIG=next(BASE.glob('[[]E8300] SAD*.docx'))
NS={'w':'http://schemas.openxmlformats.org/wordprocessingml/2006/main'};W='{'+NS['w']+'}'
def sha(b):return hashlib.sha256(b).hexdigest().upper()
raw=PATH.read_bytes();orig_hash=sha(ORIG.read_bytes())
with zipfile.ZipFile(PATH)as z:entries=[(deepcopy(i),z.read(i.filename))for i in z.infolist()]
before=E.fromstring(dict((i.filename,b)for i,b in entries)['word/document.xml']);root=deepcopy(before)
OLD=': 예, 사용되는 지원 소프트웨어의 종류, 버전 및 출처를 식별하여 관리한다.'
MAIN='기존 E8300의 지원 소프트웨어 식별·관리 범위는 유지한다. QCC는 제공설계 수준을 반영했으며 지원 소프트웨어의 정확한 제품·버전과 운영 검토 완료는 확인되지 않았다.'
GAP='[QCC 자료 입력] QCC 지원 소프트웨어의 정확한 제품·버전, 사용 구성·API 및 운영 검토 자료를 제출해야 한다.'
ps=[p for p in root.xpath('//w:body/w:p',namespaces=NS)if ''.join(p.xpath('.//w:t[not(ancestor::w:del)]/text()',namespaces=NS))==OLD];assert len(ps)==1
p=ps[0];runs=p.xpath('.//w:r[not(ancestor::w:del)][w:t]',namespaces=NS);assert len(runs)==1
run=runs[0];parent=run.getparent();assert parent.tag==W+'ins' and parent.getparent()is p
no=max(int(v)for v in root.xpath('//@w:id',namespaces=NS)if v.isdigit())+1
date=datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace('+00:00','Z')
def rev(kind,rid):return E.Element(W+kind,{W+'id':str(rid),W+'author':'김은수',W+'date':date})
mainrun=deepcopy(run);mainrun.find(W+'t').text=MAIN
gaprun=deepcopy(run);gaprun.find(W+'t').text=GAP
props=gaprun.find(W+'rPr')
if props is None:props=E.Element(W+'rPr');gaprun.insert(0,props)
base=props.find(W+'sz');base_half=int(base.get(W+'val'))if base is not None else 20
for tag,val in [('color','C00000'),('sz',str(base_half+4)),('szCs',str(base_half+4)),('u','single')]:
 e=props.find(W+tag)
 if e is None:e=E.SubElement(props,W+tag)
 e.set(W+'val',val)
gaprun.insert(1,E.Element(W+'br'))
delete=rev('del',no);parent.replace(run,delete)
for t in run.findall(W+'t'):t.tag=W+'delText'
delete.append(run)
insert=rev('ins',no+1);insert.extend([mainrun,gaprun]);p.insert(p.index(parent)+1,insert)
assert not insert.xpath('ancestor::w:ins|ancestor::w:del',namespaces=NS)
rejected=deepcopy(root);ins=rejected.xpath('//w:ins[@w:id="'+str(no+1)+'"]',namespaces=NS)[0];ins.getparent().remove(ins)
d=rejected.xpath('//w:del[@w:id="'+str(no)+'"]',namespaces=NS)[0];par=d.getparent();r=d[0];d.remove(r)
for t in r.iter(W+'delText'):t.tag=W+'t'
par.replace(d,r)
assert E.tostring(rejected,method='c14n')==E.tostring(before,method='c14n')
backup=TASK/'qa/qcc_design/SAD-before-SOUP-clarification.docx';assert not backup.exists();backup.write_bytes(raw)
candidate=TASK/'qa/qcc_design/SAD-SOUP-clarified.docx'
with zipfile.ZipFile(candidate,'w')as z:
 for info,b in entries:z.writestr(info,E.tostring(root,encoding='UTF-8',xml_declaration=True,standalone=True)if info.filename=='word/document.xml'else b)
assert PATH.read_bytes()==raw
PATH.write_bytes(candidate.read_bytes());assert sha(ORIG.read_bytes())==orig_hash
record={'date':date,'path':str(PATH),'sha256_before':sha(raw),'sha256_after':sha(PATH.read_bytes()),'locator':'6. 소프트웨어 아키텍쳐 검증 / SOUP 항목의 적절한 운영이 지원되는가? 답변','before':OLD,'after':MAIN+'\n'+GAP,'revision_ids':[str(no),str(no+1)],'author':'김은수','new_insert_direct_p_not_nested':True,'old_insert_retained_with_tracked_delete':True,'gap_format':{'base_pt':base_half/2,'size_pt':(base_half+4)/2,'color':'C00000','underline':'single','bold_inherited_unchanged':True},'reject_patch_c14n_equals_before':True,'other_package_bytes_preserved':True,'original_unchanged':True,'original_sha256':orig_hash,'other_current_verification_questions_already_scope_limited':True}
ev=TASK/'evidence/qcc_design_authored.json';j=json.loads(ev.read_text('utf-8'));j.setdefault('post_office_targeted_patches',[]).append(record);ev.write_text(json.dumps(j,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(record,ensure_ascii=True))
