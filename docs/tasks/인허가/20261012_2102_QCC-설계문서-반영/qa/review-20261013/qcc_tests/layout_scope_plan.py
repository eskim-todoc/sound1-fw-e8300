from pathlib import Path
import json
from integrate import sha
H=Path(__file__).resolve().parent;P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'));C=json.loads((H/'current-values.json').read_text(encoding='utf-8'))
out={}
for k in ['SUVC','SUVR','SSTC','SSTR']:
 b=P['books'][k];sn=b['sheet'];out[k]={'path':b['path'],'sheet':sn,'hash':sha(b['path']),'changes':{},'page_break_after_rows':[218] if k in ['SSTC','SSTR'] else []}
 if k in ['SUVC','SUVR']:
  a='D24';v=C[k][sn][a];lines=[{'status':'old','text':v.strip()},{'status':'new','text':'기존 E8300 유닛 시험의 시험자 확인 결과를 입력별 예상 출력과 대조하여 결과 칸에 기록했다. 이 확인을 새 QCC 시험 조건의 통과로 확대하지 않는다.'},{'status':'new','text':'QCC 시험 조건은 실행 결과가 없어 미실행 / 판정 대기다.'}]
  comp={'before':v,'lines':lines,'display':'\n'+''.join(('[-] ' if x['status']=='old' else '[+] ')+x['text']+'\n' for x in lines),'current':'\n'+'\n'.join(x['text'] for x in lines if x['status']=='new')+'\n','reason':'historic E8300 tester-confirmed result scope; QCC conditions remain unexecuted'}
  out[k]['changes'][a]=comp;P['books'][k]['comparison'].setdefault(sn,{})[a]=comp
(H/'layout-scope-plan.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
(H/'author-payload.json').write_text(json.dumps(P,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:{'cells':list(v['changes']),'breaks':v['page_break_after_rows']} for k,v in out.items()},ensure_ascii=True))
