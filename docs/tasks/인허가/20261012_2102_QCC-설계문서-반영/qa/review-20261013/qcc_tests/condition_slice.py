from pathlib import Path
import json,re
H=Path(__file__).resolve().parent;C=json.loads((H/'current-values.json').read_text(encoding='utf-8'))
out=[]
for start,end in [(72,100),(860,875),(1124,1141),(1159,1164)]:
 for k,sn,col in [('SUVC','기준서_유닛 검증','K'),('SUVR','보고서_유닛 시험','L')]:
  cells=C[k][sn]
  for r in range(start,end+1):
   value=cells.get(col+str(r),'')
   if value:out.append({'key':k,'cell':col+str(r),'lines':[x for x in value.splitlines() if re.search(r'(조건|검증)\s*\[',x)],'content':value[:500]})
print(json.dumps(out,ensure_ascii=True))
