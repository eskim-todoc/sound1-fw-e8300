from pathlib import Path
import json
H=Path(__file__).resolve().parent;C=json.loads((H/'current-values.json').read_text(encoding='utf-8'));out=[]
for k,b in C.items():
 for sn,cs in b.items():
  for a,t in cs.items():
   if any(p in t for p in ['전 항목 통과','전항목 통과','모든 항목 통과','전체 시험 통과']):out.append({'key':k,'sheet':sn,'cell':a,'value':t})
print(json.dumps(out,ensure_ascii=True))
