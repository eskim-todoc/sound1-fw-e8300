from pathlib import Path
import json,difflib
HERE=Path(__file__).resolve().parent
P=json.loads((HERE/'changes.json').read_text(encoding='utf-8'))
A=json.loads((HERE/'audit-current.json').read_text(encoding='utf-8'))
for k,b in P['books'].items():
 b['comparison']={}
 for sn,cells in b['sheets'].items():
  comp={}
  for addr,new in cells.items():
   old=A['books'][k]['all_cells'][sn].get(addr,'')
   before=old.strip().splitlines();after=new.strip().splitlines();lines=[]
   for op,lo,hi,la,ha in difflib.SequenceMatcher(a=before,b=after,autojunk=False).get_opcodes():
    if op=='equal':lines.extend({'status':'same','text':s} for s in after[la:ha])
    else:
     lines.extend({'status':'old','text':s} for s in before[lo:hi])
     lines.extend({'status':'new','text':s} for s in after[la:ha])
     if la==ha:lines.append({'status':'new','text':''})
   display='\n'+'\n'.join(('[-] ' if t['status']=='old' else '[+] ' if t['status']=='new' else '')+t['text'] for t in lines)+'\n'
   active='\n'+'\n'.join(t['text'] for t in lines if t['status']!='old' and t['text'])+'\n'
   # Ignore only empty paragraph spacing; no character/value loss is allowed.
   assert active.strip().splitlines()==[x for x in new.strip().splitlines() if x],(k,sn,addr)
   comp[addr]={'before':old,'current':new,'display':display,'lines':lines}
  b['comparison'][sn]=comp
(HERE/'author-payload.json').write_text(json.dumps(P,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:sum(map(len,b['comparison'].values())) for k,b in P['books'].items()}))
