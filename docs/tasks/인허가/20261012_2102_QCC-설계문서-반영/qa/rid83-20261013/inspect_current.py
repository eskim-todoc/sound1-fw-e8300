from pathlib import Path
import sys,json,shutil,re
sys.stdout.reconfigure(encoding='utf-8')
H=Path(__file__).resolve().parent
T=H.parent.parent
sys.path.insert(0,str(H.parent/'sv-clean-20261013'))
from prepare_clean import pack,sheets,cellmap,value,sha,load,save,N
mf=load(T/'evidence/final-output-manifest.json')
save(H/'source-manifest.json',mf)
out=[]
for f in mf['files']:
    if f['code'] not in ['SRS','SSTC','SSTR']:continue
    assert sha(f['path'])==f['sha256'],f['code']
    dest=H/'backups'/f"{f['code']}.xlsx";dest.parent.mkdir(exist_ok=True)
    if not dest.exists():shutil.copy2(f['path'],dest)
    assert sha(dest)==f['sha256']
    d,_=pack(dest);book={'code':f['code'],'source':f['path'],'sha256':f['sha256'],'sheets':[]}
    for s in sheets(d):
        if s['name']=='표지' or '사이버보안' in s['name']:continue
        r,cm,ss=cellmap(d,s['part']);lo,hi=(90,95) if f['code']=='SRS' else (282,293)
        cells={a:value(c,ss) for a,c in cm.items() if lo<=int(re.search(r'\d+',a)[0])<=hi and value(c,ss)}
        merges=[m.get('ref') for m in r.findall('x:mergeCells/x:mergeCell',N) if any(lo<=int(x)<=hi for x in re.findall(r'\d+',m.get('ref')))]
        rows={row.get('r'):dict(row.attrib) for row in r.findall('x:sheetData/x:row',N) if lo<=int(row.get('r'))<=hi}
        book['sheets'].append(dict(s,cells=cells,merges=merges,rows=rows))
    out.append(book)
save(H/'before.json',out)
for book in out:
    print(book['code'])
    for s in book['sheets']:
        print(s['name'],s['part'])
        print(json.dumps(s['cells'],ensure_ascii=False,indent=2))
        print('MERGES',s['merges'],'ROWS',s['rows'])
