from pathlib import Path
import json
H=Path(__file__).resolve().parent;P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'))
out=[]
for k,b in P['books'].items():
 if k in ['SUVR','SITR','SSTR']:continue
 for sn,cs in b['comparison'].items():
  for a,c in cs.items():
   for i,x in enumerate(c['lines']):
    if x['status']=='new' and '·' in x['text']:out.append({'key':k,'sheet':sn,'cell':a,'line':i,'text':x['text']})
(H/'new-punctuation-review.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(out,ensure_ascii=True))
