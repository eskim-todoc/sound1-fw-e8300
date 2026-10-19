from pathlib import Path
import json,re
H=Path(__file__).resolve().parent
C=json.loads((H/'current-values.json').read_text(encoding='utf-8'))
out=[]
for k,b in C.items():
 for sn,cells in b.items():
  for a,t in cells.items():
   if re.search(r'26,\s*5,\s*(2|29),',t):out.append({'key':k,'sheet':sn,'cell':a,'value':t})
(H/'date-review.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(out,ensure_ascii=True))
