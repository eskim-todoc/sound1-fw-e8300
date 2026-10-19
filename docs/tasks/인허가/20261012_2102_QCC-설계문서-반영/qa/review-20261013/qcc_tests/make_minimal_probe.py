from pathlib import Path
from lxml import etree as E
import json,zipfile
from integrate import N,sha
H=Path(__file__).resolve().parent
p=H/'SUVC-probe-canonical-rich.xlsx'
with zipfile.ZipFile(p) as z:infos=z.infolist();d={i.filename:z.read(i.filename) for i in infos}
member='xl/worksheets/sheet2.xml';root=E.fromstring(d[member]);allowed={'rFont','b','i','color','sz','u'}
for rp in root.findall('.//x:c/x:is/x:r/x:rPr',N):
 for c in list(rp):
  if E.QName(c).localname not in allowed:rp.remove(c)
d[member]=E.tostring(root,xml_declaration=True,encoding='utf-8',standalone=True)
p=H/'SUVC-probe-minimal-rich.xlsx'
with zipfile.ZipFile(p,'w') as z:
 for i in infos:z.writestr(i,d[i.filename])
print(json.dumps({'mode':'minimal rich properties: preserve explicit font/size/color/bold/italic/underline; remove family/charset/scheme metadata','path':str(p),'sha256':sha(p)},ensure_ascii=True))
