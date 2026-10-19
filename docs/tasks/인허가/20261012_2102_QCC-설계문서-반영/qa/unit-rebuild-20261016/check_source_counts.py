from rebuild import Book,H,OUT,QCC,digest
from pathlib import Path
import json,re
b=Book(OUT);q=Book(QCC);n='보고서_유닛 시험'
rows=[]
for r in range(1724,1765):
 vals={c:b.value(n,c+str(r)).strip() for c in 'BCDEFGHIJKLM'}
 rows.append({'row':r,**vals})
alltext='\n'.join(v for row in rows if row['row']>=1727 for k,v in row.items() if k!='row')
units=sorted(set(re.findall(r'SU-ID-QCC-\d+',alltext)))
tests=sorted(set(re.findall(r'SW-FW-UnitTest-QCC-ID-\d+',alltext)),key=lambda s:int(s.rsplit('-',1)[1]))
comparison=[{'address':c+str(r),'same':b.value(n,c+str(r))==q.value(n,c+str(r))} for r in range(1727,1765) for c in 'BCDEFGHIJKLM']
data={'outputSHA':digest(OUT),'donorSHA':digest(QCC),'sheets':list(b.sheets),'headings':{c:b.value(n,c+'28') for c in 'BCDEFGHIJKLM'},'rows':rows,'units':units,'tests':tests,'donorEqual':all(x['same'] for x in comparison),'differences':[x for x in comparison if not x['same']]}
(H/'source-check-workbook.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in data.items() if k not in ['rows']},ensure_ascii=False))
for row in rows:
 if row['D']: print(json.dumps({k:v for k,v in row.items() if k not in ['H','I','J','K','L','M']},ensure_ascii=False))
