from pathlib import Path
import json,re,collections
H=Path(__file__).resolve().parent;C=json.loads((H/'current-values.json').read_text(encoding='utf-8'))
groups=json.loads((H/'condition-numbers.json').read_text(encoding='utf-8'));out=[]
for g in groups:
 if g['criterion']!='SUVC':continue
 cs=C['SUVC']['기준서_유닛 검증'];rs=C['SUVR']['보고서_유닛 시험'];a,b=g['rows']
 exp='\n'.join(cs.get('K'+str(r),'') for r in range(a,b+1));obs='\n'.join(rs.get('L'+str(r),'') for r in range(a,b+1))
 el=[l.strip() for l in exp.splitlines() if l.strip().startswith('[UT-')];ol=[l.strip() for l in obs.splitlines() if l.strip().startswith('[UT-')]
 if not el and not ol:continue
 # Match exact emitted whole lines. OBSERVED cases and intentionally absent
 # C99 have distinct comparison criteria, and are retained as exceptions.
 e=collections.Counter(el);o=collections.Counter(ol)
 out.append({'id':g['id'],'rows':g['rows'],'expected_lines':len(el),'observed_lines':len(ol),'missing_expected_lines':list((e-o).elements()),'extra_observed_lines':list((o-e).elements())})
(H/'rtt-correspondence.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'groups':len(out),'exact_groups':sum(not x['missing_expected_lines'] and not x['extra_observed_lines'] for x in out),'differences':[{'id':x['id'],'rows':x['rows'],'missing':len(x['missing_expected_lines']),'extra':len(x['extra_observed_lines']),'sample_missing':x['missing_expected_lines'][:2],'sample_extra':x['extra_observed_lines'][:2]} for x in out if x['missing_expected_lines'] or x['extra_observed_lines']]},ensure_ascii=True))
