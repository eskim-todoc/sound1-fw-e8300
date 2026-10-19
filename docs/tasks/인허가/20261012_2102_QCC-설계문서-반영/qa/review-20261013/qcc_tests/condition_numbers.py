from pathlib import Path
import json,re
H=Path(__file__).resolve().parent;T=H.parents[2]
C=json.loads((H/'current-values.json').read_text(encoding='utf-8'));P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'))
cfg={x['key']:x for x in json.loads((T/'builders/qcc_tests/payload.json').read_text(encoding='utf-8'))['books']}
out=[]
for ck,rk,idcol,expectedcol,resultcol in [('SUVC','SUVR','D','K','L'),('SITC','SITR','C','M','N'),('SSTC','SSTR','B','J','K')]:
 cs=C[ck][P['books'][ck]['sheet']];rs=C[rk][P['books'][rk]['sheet']]
 start=cfg[ck]['commonStart'];end=max(cfg[ck]['newRows'])
 starts=[r for r in range(start,end+1) if cs.get(idcol+str(r),'').strip().startswith('SW-FW-')]
 for i,r in enumerate(starts):
  stop=starts[i+1]-1 if i+1<len(starts) else end
  expected='\n'.join(cs.get(expectedcol+str(x),'') for x in range(r,stop+1));result='\n'.join(rs.get(resultcol+str(x),'') for x in range(r,stop+1))
  conditions={int(n) for n in re.findall(r'(?:조건|검증)\s*\[(\d+)\]',expected)}
  validations={int(n) for n in re.findall(r'검증\s*\[(\d+)\]',result)}
  # Full preserved RTT identifies its case as Cn even where the previous author
  # printed no separate Korean heading. It is not a missing product result.
  expected_rtt={int(n) for n in re.findall(r'^\[UT-[^\]]+\]\s+C(\d+)\b',expected,re.M)}
  observed_rtt={int(n) for n in re.findall(r'^\[UT-[^\]]+\]\s+C(\d+)\b',result,re.M)}
  expected_case_labels={int(n) for n in re.findall(r'\bC(\d+)\b',expected)} if ck=='SUVC' else set()
  observed_case_labels={int(n) for n in re.findall(r'\bC(\d+)\b',result)} if ck=='SUVC' else set()
  # C99 absence checks and C82 OBSERVED range-only checks have standalone
  # comparison headings, not emitted expected RTT. Preserve that distinction.
  conditions|=expected_rtt|expected_case_labels;validations|=observed_rtt|observed_case_labels
  out.append({'criterion':ck,'report':rk,'id':cs[idcol+str(r)].strip(),'rows':[r,stop],'conditions':sorted(conditions),'validations':sorted(validations),'expected_full_rtt_case_ids':sorted(expected_rtt),'observed_full_rtt_case_ids':sorted(observed_rtt),'expected_standalone_case_labels':sorted(expected_case_labels-expected_rtt),'observed_standalone_case_labels':sorted(observed_case_labels-observed_rtt),'missing_result_numbers':sorted(conditions-validations),'extra_result_numbers':sorted(validations-conditions),'condition_cells':[expectedcol+str(x) for x in range(r,stop+1) if cs.get(expectedcol+str(x),'').strip()]})
(H/'condition-numbers.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'groups':len(out),'differences':[x for x in out if x['missing_result_numbers'] or x['extra_result_numbers']]},ensure_ascii=True))
