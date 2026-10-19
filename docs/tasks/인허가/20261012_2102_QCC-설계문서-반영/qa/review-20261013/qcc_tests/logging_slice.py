from pathlib import Path
import json
H=Path(__file__).resolve().parent;C=json.loads((H/'current-values.json').read_text(encoding='utf-8'))
print(json.dumps({k:{a:v for a,v in b[next(sn for sn in b if sn in ['기준서_시스템 시험','보고서_시스템 시험'])].items() if a in [f'{col}{row}' for col in 'BCDEFGHIJKL' for row in range(215,219)]} for k,b in C.items() if k in ['SSTC','SSTR']},ensure_ascii=True))
