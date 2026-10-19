from pathlib import Path
import json,re
import prepare_clean as p

h=Path(__file__).resolve().parent
plan=p.load(h/'plan.json')
prior=p.load(h/'source-final-output-manifest.json')
old={b['code']:b for b in prior['files']}
(h/'native-final').mkdir(exist_ok=True)
books=[]
sample_rows={'SRS':(50,57),'SUVC':(1734,1740),'SUVR':(1734,1740),'SITC':(126,131),'SITR':(126,131),'SSTC':(218,219),'SSTR':(218,219)}
for b in plan['books']:
    book={'code':b['code'],'path':b['candidatePath'],'sha256':p.sha(b['candidatePath']),'previews':[],'formulas':[]}
    data,_=p.pack(b['candidatePath'])
    tabs={s['name']:s for s in p.sheets(data)}
    for s in b['sheets']:
        before=next(x for x in old[b['code']]['native_previews'] if x['sheet']==s['name'])
        suffix='cyber' if '사이버' in s['name'] else 'main'
        book['previews'].append({'sheet':s['name'],'range':before['range'],'pdf':str(h/'native-final'/f"{b['code']}-{suffix}.pdf"),'scope':'Current main sheet, complete existing print area; read-only Excel preview'})
        cols=re.findall('[A-Z]+',before['range'])
        first,last=(37,39) if suffix=='cyber' else sample_rows[b['code']]
        book['previews'].append({'sheet':s['compareName'],'range':f'{cols[0]}{first}:{cols[1]}{last}','pdf':str(h/'native-final'/f"{b['code']}-{suffix}-compare.pdf"),'scope':'New comparison sheet representative range; full original comparison cells/layout verified against backup'})
        root,cells,ss=p.cellmap(data,tabs[s['name']]['part'])
        for address,c in cells.items():
            if c.find('x:f',p.N) is not None:
                for name in [s['name'],s['compareName']]:
                    book['formulas'].append({'sheet':name,'cell':address,'expected':p.value(c,ss)})
    books.append(book)
p.save(h/'native-plan.json',{'books':books,'recordPath':str(h/'native-checks.json')})
print(json.dumps({'books':len(books),'previews':sum(len(b['previews']) for b in books),'formulas':sum(len(b['formulas']) for b in books)}))
