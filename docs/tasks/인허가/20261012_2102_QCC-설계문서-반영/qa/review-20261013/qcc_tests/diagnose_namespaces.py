from pathlib import Path
from lxml import etree as E
import json,zipfile,re
from integrate import N,sha
H=Path(__file__).resolve().parent;P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'))
issues=[];sample=[]
for k,p in P['books'].items():
 with zipfile.ZipFile(p['path']) as z:
  for name in z.namelist():
   if not name.endswith('.xml'):continue
   root=E.fromstring(z.read(name))
   for node in root.iter():
    for a,v in node.attrib.items():
     if E.QName(a).localname in ['Ignorable','MustUnderstand','Requires','ProcessContent','PreserveElements','PreserveAttributes']:
      tokens=v.split();missing=[]
      for t in tokens:
       pref=t.split(':')[0]
       if pref not in node.nsmap:missing.append(pref)
      sample.append({'key':k,'part':name,'element':E.QName(node).localname,'attribute':a,'value':v,'declarations':dict(node.nsmap),'missing':missing})
      if missing:issues.append(sample[-1])
print(json.dumps({'issues':issues,'checks':len(sample),'first':sample[:2]},ensure_ascii=True));(H/'namespace-qname-diagnosis.json').write_text(json.dumps({'issues':issues,'checks':sample},ensure_ascii=False,indent=2),encoding='utf-8')
