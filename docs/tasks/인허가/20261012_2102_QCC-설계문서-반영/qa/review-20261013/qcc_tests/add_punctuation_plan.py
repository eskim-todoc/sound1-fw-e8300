from pathlib import Path
import json,copy
from integrate import sha
H=Path(__file__).resolve().parent
P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'));F=json.loads((H/'date-plan.json').read_text(encoding='utf-8'))
changed=[]
for k,b in P['books'].items():
 sn=b['sheet']
 for a,comp in b['comparison'].get(sn,{}).items():
  indexes=[i for i,x in enumerate(comp['lines']) if x['status']=='new' and '·' in x['text']]
  if not indexes:continue
  before=comp['display'];lines=copy.deepcopy(comp['lines'])
  for i in indexes:lines[i]['text']=lines[i]['text'].replace('·',' / ')
  updated={**comp,'lines':lines,'display':'\n'+''.join(('[-] ' if x['status']=='old' else '[+] ' if x['status']=='new' else '')+x['text']+'\n' for x in lines),'current':'\n'+'\n'.join(x['text'] for x in lines if x['status']!='old')+'\n'}
  if a in F.get(k,{}).get('changes',{}):updated['before']=F[k]['changes'][a]['before']
  else:updated['before']=before
  updated['reason']=comp.get('reason','')+'; punctuation in newly authored prose only, original RTT/official names/observations unchanged'
  F.setdefault(k,{'path':b['path'],'sheet':sn,'hash':sha(b['path']),'changes':{}})['changes'][a]=updated
  P['books'][k]['comparison'][sn][a]=updated
  changed.append({'key':k,'sheet':sn,'cell':a,'changed_new_paragraphs':indexes,'scope':'only status=new prose; old/keep strings intact'})
(H/'date-plan.json').write_text(json.dumps(F,ensure_ascii=False,indent=2),encoding='utf-8')
(H/'author-payload.json').write_text(json.dumps(P,ensure_ascii=False,indent=2),encoding='utf-8')
(H/'punctuation-applied-plan.json').write_text(json.dumps(changed,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'cells':len(changed),'by_book':{k:sum(x['key']==k for x in changed) for k in P['books']}},ensure_ascii=True))
