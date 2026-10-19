from pathlib import Path
from lxml import etree as E
import json,zipfile,copy
from integrate import sheetmap,N,NS,texts,sha
H=Path(__file__).resolve().parent;P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'));B=json.loads((H.parent/'review-baseline.json').read_text(encoding='utf-8'))
snapshot=next(x['snapshot'] for x in B if x.get('code')=='SUVC')
with zipfile.ZipFile(P['books']['SUVC']['path']) as z:infos=z.infolist();data={i.filename:z.read(i.filename) for i in infos}
with zipfile.ZipFile(snapshot) as z:before={i.filename:z.read(i.filename) for i in z.infolist()}
member=sheetmap(data)[P['books']['SUVC']['sheet']];order={t:i for i,t in enumerate('rFont charset family b i strike outline shadow condense extend color sz u vertAlign scheme'.split())}
result=[]
for mode in ['canonical-rich','plain-inline','restored-sheet']:
 d=data.copy();root=E.fromstring(d[member])
 if mode=='restored-sheet':d[member]=before[member]
 else:
  for isn in root.findall('.//x:sheetData/x:row/x:c/x:is',N):
   if mode=='plain-inline':
    value=texts(isn)
    for c in list(isn):isn.remove(c)
    t=E.SubElement(isn,'{'+NS+'}t');t.set('{http://www.w3.org/XML/1998/namespace}space','preserve');t.text=value
   else:
    for rp in isn.findall('x:r/x:rPr',N):
     elements=list(rp)
     for c in elements:rp.remove(c)
     for c in sorted(elements,key=lambda x:order.get(E.QName(x).localname,99)):rp.append(c)
  d[member]=E.tostring(root,xml_declaration=True,encoding='utf-8',standalone=True)
 p=H/('SUVC-probe-'+mode+'.xlsx')
 with zipfile.ZipFile(p,'w') as z:
  for i in infos:z.writestr(i,d[i.filename])
 result.append({'mode':mode,'path':str(p),'sha256':sha(p),'changed_part':member})
(H/'open-candidates.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(result,ensure_ascii=True))
