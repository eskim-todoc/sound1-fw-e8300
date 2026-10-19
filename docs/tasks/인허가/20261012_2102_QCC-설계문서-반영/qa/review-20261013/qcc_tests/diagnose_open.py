from pathlib import Path
from lxml import etree as E
import json,zipfile,collections
from integrate import sheetmap,vals,N,sha
H=Path(__file__).resolve().parent;P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'));A=json.loads((H/'audit-current.json').read_text(encoding='utf-8'))
out=[]
for k in ['SUVC','SUVR']:
 with zipfile.ZipFile(P['books'][k]['path']) as z:d={i.filename:z.read(i.filename) for i in z.infolist()}
 sm=sheetmap(d);sn=P['books'][k]['sheet'];rt=E.fromstring(d[sm[sn]]);v=vals(d,sm[sn]);big=sorted([{'cell':a,'length':len(x),'before':len(A['books'][k]['all_cells'][sn].get(a,''))} for a,x in v.items()],key=lambda x:x['length'],reverse=True)[:8]
 structural={'sheet_order':[E.QName(x).localname for x in rt],'cells_types':dict(collections.Counter(x.get('t') for x in rt.findall('.//x:sheetData/x:row/x:c',N))),'rich_runs_max':max(len(x.findall('x:is/x:r',N)) for x in rt.findall('.//x:sheetData/x:row/x:c',N)),'file_members':len(d)}
 out.append({'key':k,'hash':sha(P['books'][k]['path']),'large':big,'structural':structural})
print(json.dumps(out,ensure_ascii=True));(H/'open-failure-diagnosis.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
