from pathlib import Path
import json,zipfile,re
from lxml import etree as E
from integrate import sheetmap,vals,sha,N
import difflib
def comparison(old,new):
 lines=[]
 for op,lo,hi,la,ha in difflib.SequenceMatcher(a=old.strip().splitlines(),b=new.strip().splitlines(),autojunk=False).get_opcodes():
  before=old.strip().splitlines();after=new.strip().splitlines()
  if op=='equal':lines.extend({'status':'same','text':s} for s in after[la:ha])
  else:
   lines.extend({'status':'old','text':s} for s in before[lo:hi]);lines.extend({'status':'new','text':s} for s in after[la:ha])
   if la==ha:lines.append({'status':'new','text':''})
 return {'display':'\n'+'\n'.join(('[-] ' if x['status']=='old' else '[+] ' if x['status']=='new' else '')+x['text'] for x in lines)+'\n','lines':lines}
H=Path(__file__).resolve().parent
P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'))
M=json.loads((H/'method-audit.json').read_text(encoding='utf-8'))
plan={k:{'path':v['path'],'sheet':v['sheet'],'hash':sha(v['path']),'changes':{}} for k,v in P['books'].items() if k in ['SUVC','SUVR','SSTC','SSTR']}
for key,p in plan.items():
 with zipfile.ZipFile(p['path']) as z:
  data={i.filename:z.read(i.filename) for i in z.infolist()};p['values']=vals(data,sheetmap(data)[p['sheet']])
def put(k,a,v,reason):
 p=plan[k];before=p['values'].get(a,'');current='\n'+v+'\n';comp=comparison(before,current)
 p['changes'][a]={'before':before,'current':current,**comp,'reason':reason}
 P['books'][k]['comparison'][p['sheet']][a]={z:comp[z] for z in ['display','lines']}|{'before':before,'current':current}
for a in next(m['candidates'] for m in M if m['key']=='SUVC'):
 for k in ['SUVC','SUVR']:put(k,a['cell'],'T','9/10: full RTT numeric elements/counts or entire response byte/length comparison; original input/expected/observed preserved')
systemT={'D148','D155','D157','D159'}
systemMixed={'D50','D70','D74','D116','D121','D124','D130','D134','D138','D141','D146','D152','D230','D239','D247'}
for a in sorted(systemT|systemMixed):
 for k in ['SSTC','SSTR']:put(k,a,'T' if a in systemT else 'T/D','9/10: numeric response/array/count/time plus qualitative behavior; no new measurement or threshold introduced')
for a,v in plan['SSTR']['values'].items():
 if re.fullmatch(r'L\d+',a) and 34<=int(a[1:])<282 and '통과' in v and v.strip()!='통과':put('SSTR',a,'판정 대기','16/18: old observation preserved in K; current incomplete criterion remains pending, old status retained red comparison')
for p in plan.values():p.pop('values')
(H/'followup-plan.json').write_text(json.dumps(plan,ensure_ascii=False,indent=2),encoding='utf-8')
(H/'author-payload.json').write_text(json.dumps(P,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:{'cells':len(v['changes']),'addresses':list(v['changes'])} for k,v in plan.items()},ensure_ascii=True))

