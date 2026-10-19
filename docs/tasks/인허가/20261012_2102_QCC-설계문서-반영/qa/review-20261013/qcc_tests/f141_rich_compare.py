from pathlib import Path
from lxml import etree as E
import json,zipfile
from integrate import sheetmap,N,sha
H=Path(__file__).resolve().parent;P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'));out=[]
for k in ['SITC','SITR']:
 b=P['books'][k]
 with zipfile.ZipFile(b['path']) as z:d={i.filename:z.read(i.filename) for i in z.infolist()}
 sn=next(s for s in sheetmap(d) if s.startswith(('기준서_','보고서_')));root=E.fromstring(d[sheetmap(d)[sn]]);cell=root.find('.//x:c[@r="F141"]',N)
 out.append({'key':k,'sha256':sha(b['path']),'runs':[{'text':r.findtext('x:t',namespaces=N),'rPr':E.tostring(r.find('x:rPr',N)).decode() if r.find('x:rPr',N) is not None else ''} for r in cell.findall('x:is/x:r',N)]})
(H/'f141-rich-comparison.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps([{'key':o['key'],'rPr':o['runs'][1]['rPr']} for o in out],ensure_ascii=True))
