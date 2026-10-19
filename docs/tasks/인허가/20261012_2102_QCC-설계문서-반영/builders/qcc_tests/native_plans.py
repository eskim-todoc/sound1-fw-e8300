import json
from pathlib import Path
HERE=Path(__file__).resolve().parent;TASK=HERE.parent.parent
p=json.loads((HERE/'payload.json').read_text(encoding='utf-8'));b={x['key']:x for x in p['books']};out=TASK/'qa/excel-native';out.mkdir(parents=True,exist_ok=True)
plans=[]
for key,rng in [('SUVC','B1733:K1735'),('SUVR','B1733:M1735'),('SITC','B131:M145'),('SITR','B131:O145'),('SSTR','B282:L286')]:
 pdf=out/(key+'-final-v2.pdf');plan=HERE/(key+'-final-v2-plan.json')
 data={'recordPath':str(out/(key+'-final-v2-export.json')),'books':[{'code':key+'-final-v2','path':b[key]['target'],'sheet':b[key]['sheet'],'parts':[{'range':rng,'pdf':str(pdf)}]}]}
 plan.write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8');plans.append({'plan':str(plan),'pdfs':[str(pdf)]})
ranges=['A2:D2','A78:D83','A184:D185','A190:D190','A199:D202','A206:D214','A222:D224','A227:D229']
parts=[{'range':r,'pdf':str(out/('SSTR-aux-'+r.replace(':','-')+'-final-v2.pdf'))}for r in ranges]
plan=HERE/'SSTR-aux-final-v2-plan.json';data={'recordPath':str(out/'SSTR-aux-final-v2-export.json'),'books':[{'code':'SSTR-aux-final-v2','path':b['SSTR']['target'],'sheet':'시험 보조자료','parts':parts}]}
plan.write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8');plans.append({'plan':str(plan),'pdfs':[part['pdf']for part in parts]})
(HERE/'native-final-v2-plans.json').write_text(json.dumps(plans,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'plans':len(plans),'pdfs':sum(len(x['pdfs'])for x in plans)},ensure_ascii=True))
