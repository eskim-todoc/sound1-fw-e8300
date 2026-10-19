from pathlib import Path
import json,re
H=Path(__file__).resolve().parent;C=json.loads((H/'current-values.json').read_text(encoding='utf-8'))
out=[]
for k,sn,col in [('SUVC','기준서_유닛 검증','K'),('SUVR','보고서_유닛 시험','L')]:
 for a,t in C[k][sn].items():
  if a.startswith(col) and 644<=int(a[1:])<=788 and re.search(r'(?:C99\b|검증\s*\[82\]|C82\b|측정.*82)',t):out.append({'key':k,'cell':a,'value':t})
print(json.dumps(out,ensure_ascii=True))
