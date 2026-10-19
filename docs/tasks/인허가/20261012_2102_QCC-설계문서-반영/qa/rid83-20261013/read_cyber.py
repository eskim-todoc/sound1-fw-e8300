from pathlib import Path
import sys,json
H=Path(__file__).resolve().parent;sys.path.insert(0,str(H.parent/'sv-clean-20261013'))
from prepare_clean import *
sys.stdout.reconfigure(encoding='utf-8')
for code in ['SSTC','SSTR']:
 d,_=pack(H/'backups'/f'{code}.xlsx');s=next(s for s in sheets(d) if s['name']=='사이버보안 시험 연계');r,c,ss=cellmap(d,s['part'])
 print(code)
 for a in ['J38','K38']:print(a,value(c[a],ss) if a in c else '')
 print('row',dict(r.find("x:sheetData/x:row[@r='38']",N).attrib))
