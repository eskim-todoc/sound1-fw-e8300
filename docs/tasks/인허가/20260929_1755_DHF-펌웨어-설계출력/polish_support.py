from copy import deepcopy
import zipfile
from lxml import etree as E
from patch_sad_order import PATH,q,N,text
with zipfile.ZipFile(PATH) as z:parts={n:z.read(n) for n in z.namelist()}
r=E.fromstring(parts['word/document.xml']);b=r.find(q('body'))
h=next(p for p in b.findall(q('p')) if text(p)=='지원 소프트웨어')
old=next(p for p in b.findall(q('p')) if text(p)=='소프트웨어 아키텍쳐 검증')
run=h.find('.//w:ins/w:r',namespaces=N)
run.remove(run.find(q('rPr')))
original=old.find('.//w:r/w:rPr',namespaces=N)
if original is not None:run.insert(0,deepcopy(original))
sp=old.find(q('pPr')).find(q('spacing'));sp.set(q('before'),'160')
table=b[b.index(h)+2]
for row in table.findall(q('tr'))[1:]:
    run=row.find(q('tc')).find('.//w:ins/w:r',namespaces=N)
    t=run.find(q('t'));value=t.text
    if ' / ' in value:
        a,c=value.split(' / ',1);t.text=a
        E.SubElement(run,q('br'));E.SubElement(run,q('t')).text=c
parts['word/document.xml']=E.tostring(r,encoding='UTF-8',xml_declaration=True,standalone=True)
with zipfile.ZipFile(PATH,'w',zipfile.ZIP_DEFLATED) as z:
    for n,v in parts.items():z.writestr(n,v)
