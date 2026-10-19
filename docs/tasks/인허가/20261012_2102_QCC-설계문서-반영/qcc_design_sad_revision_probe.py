from pathlib import Path
import json, zipfile, hashlib
from lxml import etree as E
T=Path(__file__).parent
B=Path('G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF/03_설계 출력/SW/외부기')
N={'w':'http://schemas.openxmlformats.org/wordprocessingml/2006/main'}
W='{'+N['w']+'}'
p=next(B.glob('[[]E8300-QCC] SAD*.docx'))
with zipfile.ZipFile(p) as z: parts={i.filename:(i,z.read(i.filename)) for i in z.infolist()}
r=E.fromstring(parts['word/document.xml'][1])
changes=[]
for c in r.xpath('//w:pPrChange',namespaces=N):
    changes.append({'id':c.get(W+'id'),'author':c.get(W+'author'),'date':c.get(W+'date'),'path':r.getroottree().getpath(c),'nested_pPrChange':len(c.xpath('.//w:pPrChange',namespaces=N)),'siblings_pPrChange':len(c.getparent().findall(W+'pPrChange')),'xml':E.tostring(c.getparent(),encoding='unicode')})
target=r.xpath('//w:pPrChange[@w:id="3424"]',namespaces=N)
assert len(target)==1
target[0].getparent().remove(target[0])
local=T/'qa/qcc_design/SAD-diagnostic-no-tail-format-track.docx'
with zipfile.ZipFile(local,'w') as z:
    for name,(i,b) in parts.items():z.writestr(i,E.tostring(r,encoding='UTF-8',xml_declaration=True,standalone=True) if name=='word/document.xml' else b)
(T/'qa/qcc_design/SAD-format-track-xml-probe.json').write_text(json.dumps({'g_path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest().upper(),'format_changes':changes,'diagnostic_copy_only':str(local),'removed_id_only_in_diagnostic':'3424'},ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'format_change_count':len(changes),'target':next(c for c in changes if c['id']=='3424'),'local':str(local)},ensure_ascii=True))
