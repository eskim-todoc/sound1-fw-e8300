from pathlib import Path
from lxml import etree as E
import json,zipfile
from integrate import sheetmap,vals,N,ref,sha
H=Path(__file__).resolve().parent
P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'))
M=json.loads((H/'boundary-cell-map.json').read_text(encoding='utf-8'))
out=[]
for k,b in P['books'].items():
 with zipfile.ZipFile(b['path']) as z:data={i.filename:z.read(i.filename) for i in z.infolist()}
 sn=next(s for s in sheetmap(data) if s.startswith(('기준서_','보고서_')))
 root=E.fromstring(data[sheetmap(data)[sn]]); v=vals(data,sheetmap(data)[sn]);styles=E.fromstring(data['xl/styles.xml']);xfs=styles.find('x:cellXfs',N)
 merges=[x.get('ref') for x in root.findall('x:mergeCells/x:mergeCell',N)];default=root.find('x:sheetFormatPr',N).get('defaultRowHeight','15');rows={int(r.get('r')):float(r.get('ht',default)) for r in root.findall('x:sheetData/x:row',N)}
 aa={x['cell'] for m in M if m['key']==k for x in m['matches'] if 'cell' in x}
 aa.update({'L1016','L1058'} if k=='SUVR' else [])
 rec=[]
 for a in sorted(aa,key=lambda a:(ref(a)[1],ref(a)[0])):
  c=root.find(f'.//x:c[@r="{a}"]',N)
  if c is None:continue
  m=next((m for m in merges if m.split(':')[0]==a),a); r1=ref(a)[1];r2=ref(m.split(':')[-1])[1]
  st=xfs[int(c.get('s','0'))];al=st.find('x:alignment',N)
  rec.append({'cell':a,'merge':m,'height_sum':sum(rows.get(r,15) for r in range(r1,r2+1)),'style':c.get('s'),'alignment':dict(al.attrib) if al is not None else {},'chars':len(v.get(a,'')),'lines':v.get(a,'').count('\n')+1})
 out.append({'key':k,'sha256':sha(b['path']),'sheet':sn,'default_height':default,'cells':rec})
(H/'boundary-alignment.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(out,ensure_ascii=True))
