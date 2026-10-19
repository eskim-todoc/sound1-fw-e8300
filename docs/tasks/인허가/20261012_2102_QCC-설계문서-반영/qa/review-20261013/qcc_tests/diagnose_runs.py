from pathlib import Path
from lxml import etree as E
import json,zipfile
from integrate import sheetmap,N
H=Path(__file__).resolve().parent
P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'))
for k in ['SSTC','SSTR']:
 with zipfile.ZipFile(P['books'][k]['path']) as z:
  d={i.filename:z.read(i.filename) for i in z.infolist()}; sm=sheetmap(d)
  sn=[x for x in sm if '시스템' in x][0]; rt=E.fromstring(d[sm[sn]])
  for a in ['B34','G116']:
   c=next(x for x in rt.findall('.//x:c',N) if x.get('r')==a)
   isn=c.find('x:is',N)
   print(json.dumps({'key':k,'cell':a,'namespaces':dict(rt.nsmap),'runs':[{'text':x.findtext('x:t',namespaces=N),'props':[(E.QName(y).localname,dict(y.attrib)) for y in x.find('x:rPr',N)]} for x in ([] if isn is None else isn.findall('x:r',N))]},ensure_ascii=True))


