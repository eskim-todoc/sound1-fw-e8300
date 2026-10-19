from pathlib import Path
import json
from integrate import sha
H=Path(__file__).resolve().parent;P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'));L=json.loads((H/'final-layout-plan.json').read_text(encoding='utf-8'));R=json.loads((H/'final-layout-published.json').read_text(encoding='utf-8'))
for rec in R['files']:
 k=rec['key'];assert sha(rec['path'])==rec['sha256_candidate'],(k,'published hash changed');sn=L[k]['sheet']
 P['books'][k]['expected_hash']=rec['sha256_candidate']
 for a,c in L[k]['changes'].items():
  if c.get('punctuation_only'):
   cc=P['books'][k]['comparison'][sn][a];cc['display']=cc['display'].replace(c['old'],c['new']);cc['current']=cc['current'].replace(c['old'],c['new'])
   for line in cc.get('lines',[]):
    if line['status']!='old':line['text']=line['text'].replace(c['old'],c['new'])
  else:P['books'][k]['comparison'].setdefault(sn,{})[a]=c
(H/'author-payload.json').write_text(json.dumps(P,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'synchronized':6,'G_written':False,'source':'root final-layout-published.json','hashes':{r['key']:r['sha256_candidate'] for r in R['files']}},ensure_ascii=True))
