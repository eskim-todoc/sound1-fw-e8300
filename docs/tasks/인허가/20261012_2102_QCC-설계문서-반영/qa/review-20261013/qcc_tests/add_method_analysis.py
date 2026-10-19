from pathlib import Path
import json
H=Path(__file__).resolve().parent
F=json.loads((H/'followup-plan.json').read_text(encoding='utf-8'));P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'));C=json.loads((H/'current-values.json').read_text(encoding='utf-8'));M=json.loads((H/'method-analysis-audit.json').read_text(encoding='utf-8'))
for x in next(x['candidates'] for x in M if x['key']=='SUVC'):
 for k in ['SUVC','SUVR']:
  a=x['cell'];sn=F[k]['sheet'];before=C[k][sn][a];comp={'before':before,'current':'\nA/T\n','display':'\n[-] A\n[+] A/T\n','lines':[{'status':'old','text':'A'},{'status':'new','text':'A/T'}]}
  F[k]['changes'][a]=comp|{'reason':'9/10: existing source analysis plus full RTT numeric array/count comparison; observed scope unchanged'};P['books'][k]['comparison'][sn][a]=comp
(H/'followup-plan.json').write_text(json.dumps(F,ensure_ascii=False,indent=2),encoding='utf-8');(H/'author-payload.json').write_text(json.dumps(P,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:len(v['changes']) for k,v in F.items()}))
