from pathlib import Path
from lxml import etree as E
from copy import deepcopy
import json,zipfile,hashlib
from integrate import sheetmap,N,texts,sha
H=Path(__file__).resolve().parent
P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'))
ps={k:Path(P['books'][k]['path']) for k in ['SSTC','SSTR']}
packages={}
for k,p in ps.items():
 with zipfile.ZipFile(p) as z:packages[k]=(z.infolist(),{i.filename:z.read(i.filename) for i in z.infolist()})
sm={k:sheetmap(packages[k][1]) for k in ps}
sn={k:next(n for n in sm[k] if '시스템' in n) for k in ps}
roots={k:E.fromstring(packages[k][1][sm[k][sn[k]]]) for k in ps}
cells={k:{c.get('r'):c for c in roots[k].findall('.//x:sheetData/x:row/x:c',N)} for k in ps}
changed=[]
for a in [c for c in cells['SSTC'] if 34 <= int(''.join(x for x in c if x.isdigit())) <= 285 and ''.join(x for x in c if x.isalpha()) in 'B C D E F G H I J'.split() and cells['SSTC'][c].find('x:is',N) is not None]:
 src=cells['SSTC'][a].find('x:is',N);dst=cells['SSTR'][a].find('x:is',N)
 assert src is not None and dst is not None and texts(src)==texts(dst),(a,'value mismatch')
 if E.tostring(src)!=E.tostring(dst):
  c=cells['SSTR'][a];c.replace(dst,deepcopy(src));changed.append(a)
p=ps['SSTR'];before=sha(p)
infos,data=packages['SSTR'];member=sm['SSTR'][sn['SSTR']]
data[member]=E.tostring(roots['SSTR'],xml_declaration=True,encoding='utf-8',standalone=True)
tmp=H/'SSTR-common-rich-aligned.xlsx'
with zipfile.ZipFile(tmp,'w') as z:
 for i in infos:z.writestr(i,data[i.filename])
with zipfile.ZipFile(tmp) as z:
 assert z.testzip() is None
with p.open('wb') as f:f.write(tmp.read_bytes())
result={'reason':'SITC common rich text scheme=minor vs SITR missing; exact criteria rich text applied to report changed common cells','key':'SSTR','before':before,'after':sha(p),'sheet':sn['SSTR'],'cells':changed,'members':[member],'values_merges_rows':'unchanged'}
(H/'rich-alignment-3.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(result,ensure_ascii=True))


