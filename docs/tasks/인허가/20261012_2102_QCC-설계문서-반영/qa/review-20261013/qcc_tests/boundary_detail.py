from pathlib import Path
from lxml import etree as E
import json,zipfile
from integrate import sheetmap,vals,N
H=Path(__file__).resolve().parent;P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'));out=[]
for k,rr in [('SUVC',range(330,337)),('SUVR',range(330,337))]:
 b=P['books'][k]
 with zipfile.ZipFile(b['path']) as z:data={i.filename:z.read(i.filename) for i in z.infolist()}
 sn=next(s for s in sheetmap(data) if s.startswith(('기준서_','보고서_')));member=sheetmap(data)[sn];root=E.fromstring(data[member]);v=vals(data,member)
 for r in rr:
  row=root.find(f'.//x:row[@r="{r}"]',N)
  out.append({'key':k,'row':r,'row_attrs':dict(row.attrib),'K':v.get('K'+str(r)),'L':v.get('L'+str(r))})
print(json.dumps(out,ensure_ascii=True))
