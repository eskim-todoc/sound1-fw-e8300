"""Select actual changed rows, expanded across merges, for native Excel QA."""
from pathlib import Path
from zipfile import ZipFile
from lxml import etree
import json, re, sys
sys.stdout.reconfigure(encoding='utf-8')
from verify_outputs import sheet, col
HERE=Path(__file__).resolve().parent
base=json.loads((HERE/'evidence/source-baseline.json').read_text(encoding='utf-8-sig'))
records=[]
out=HERE/'qa/excel-native';out.mkdir(exist_ok=True)
for item in base:
    if '[E8300]' not in item['path'] or not item['path'].endswith('.xlsx'): continue
    path=Path(item['path'].replace('[E8300]','[E8300-QCC]'))
    if not path.is_file():continue
    code=re.search(r'\] (\w+)-',path.name)[1]
    if code not in ('SRS','SUVC','SUVR','SITC','SITR','SSTC','SSTR'):continue
    a,b=sheet(item['path']),sheet(path)
    changed=sorted({int(re.search(r'\d+',r)[0]) for r in set(a['values'])|set(b['values'])
                    if a['values'].get(r,'').strip()!=b['values'].get(r,'').strip()})
    changed=[r for r in changed if r>=18]
    if not changed:continue
    groups=[];start=last=changed[0]
    for row in changed[1:]:
        if row-last>4:groups.append([max(18,start-1),last+1]);start=row
        last=row
    groups.append([max(18,start-1),last+1])
    for g in groups:
        again=True
        while again:
            again=False
            for m in b['merges']:
                rs=[int(re.search(r'\d+',x)[0]) for x in m.split(':')]
                lo,hi=min(rs),max(rs)
                if lo<=g[1] and hi>=g[0]:
                    ng=[min(g[0],lo),max(g[1],hi)]
                    if ng!=g:g[:]=ng;again=True
    merged=[]
    for g in sorted(groups):
        if merged and g[0]<=merged[-1][1]+1:merged[-1][1]=max(merged[-1][1],g[1])
        else:merged.append(g)
    name=b['name']
    lastcol={'SRS':'I','SUVC':'K','SUVR':'M','SITC':'M','SITR':'O','SSTC':'J','SSTR':'L'}[code]
    parts=[{'range':f'B{x}:{lastcol}{y}','pdf':str(out/f'{code}-{x}-{y}.pdf')} for x,y in merged]
    records.append({'code':code,'path':str(path),'sheet':name,'changed_rows':changed,'parts':parts})
plan={'books':records,'recordPath':str(out/'native-export.json')}
(HERE/'qa/excel-plan.json').write_text(json.dumps(plan,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'books':len(records),'parts':sum(len(x['parts']) for x in records),'plan':str(HERE/'qa/excel-plan.json')},ensure_ascii=False))
