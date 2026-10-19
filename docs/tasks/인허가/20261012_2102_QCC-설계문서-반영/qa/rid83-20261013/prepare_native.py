from pathlib import Path
import json
H=Path(__file__).resolve().parent
p=json.loads((H/'plan.json').read_text(encoding='utf-8'));checks=json.loads((H/'package-checks.json').read_text(encoding='utf-8'));cm={x['code']:x for x in checks}
(H/'native').mkdir(exist_ok=True);books=[]
for b in p['books']:
 code=b['code'];previews=[]
 for i,g in enumerate(b['groups']):
  for which in ['main','compare']:
   suffix=f'{code}-{i}-{which}'
   rng=('B3:I107' if which=='main' else 'B93:I94') if code=='SRS' else (('B2:L303' if code=='SSTR' else 'B2:J303') if which=='main' else ('B285:L285' if code=='SSTR' else 'B285:J285')) if i==0 else ('A38:L38' if code=='SSTR' else 'A38:J38')
   previews.append({'sheet':g[which]['name'],'range':rng,'pdf':str(H/'native'/f'{suffix}.pdf'),'scope':'Full main print area' if which=='main' and i==0 else 'Changed view and retained comparison history'})
   if code=='SSTR' and i==0 and which=='compare':previews[-1]['range']='B285:L286'
 books.append({'code':code,'path':cm[code]['path'],'sha256':cm[code]['sha256'],'formulas':[{'sheet':b['groups'][0]['main']['name'],'cell':'G93','expected':'SW-FW-RID-83'}] if code=='SRS' else [],'previews':previews})
(H/'native-plan.json').write_text(json.dumps({'books':books,'recordPath':str(H/'native-checks.json')},ensure_ascii=False,indent=2),encoding='utf-8')
