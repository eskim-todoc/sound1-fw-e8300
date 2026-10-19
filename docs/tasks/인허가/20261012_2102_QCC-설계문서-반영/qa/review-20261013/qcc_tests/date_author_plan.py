from pathlib import Path
import json,copy
from integrate import sha
H=Path(__file__).resolve().parent
P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'))
C=json.loads((H/'current-values.json').read_text(encoding='utf-8'))
out={}
def add(k,a,lines,current=None,reason='same date input and expected-value correspondence clarified; existing observation unchanged'):
 b=P['books'][k];sn=b['sheet'];before=C[k][sn][a]
 display='\n'+''.join(('[-] ' if x['status']=='old' else '[+] ' if x['status']=='new' else '')+x['text']+'\n' for x in lines)
 active='\n'+'\n'.join(x['text'] for x in lines if x['status']!='old')+'\n'
 comp={'before':before,'display':display,'current':active,'lines':lines,'reason':reason}
 out.setdefault(k,{'path':b['path'],'sheet':sn,'hash':sha(b['path']),'changes':{}})['changes'][a]=comp
 P['books'][k]['comparison'].setdefault(sn,{})[a]=comp
def plain(t):return [{'status':'keep','text':x} for x in t.strip('\n').split('\n')]
for k in ['SUVC','SUVR']:
 sn=P['books'][k]['sheet'];a='K1212';lines=plain(C[k][sn][a])
 lines.append({'status':'new','text':'날짜는 시험 입력 매개변수다. 기준서 날짜 예시는 2026-05-02, 결과 시험의 확인된 고정 입력은 2026-05-29다. 각 시험에서 저장 시간은 해당 입력 날짜와 같아야 한다. 주소/이벤트/개수/위치/반환 및 999ms·1,000ms 초 전이 기준은 그대로 적용한다.'})
 add(k,a,lines)
for k in ['SSTC','SSTR']:
 sn=P['books'][k]['sheet'];a='J218';v=C[k][sn][a].strip().splitlines();lines=[{'status':'old','text':v[0]},{'status':'new','text':v[0].replace('고정 입력:','기준서 날짜의 고정 입력 예시(2026-05-02):')},{'status':'keep','text':v[1]},{'status':'new','text':'날짜는 시험 입력 매개변수다. 결과 시험은 확인된 고정 입력 [26,5,29,13,14,15]를 적용한다. 그 입력의 예상 기록은 시간=[26,5,29,13,14,15], 주소=[18,52,86,120,154,188], 이벤트=64다. 기준서 날짜를 결과 날짜로 관찰했다고 간주하지 않는다.'}]
 add(k,a,lines)
 add(k,'D217',[{'status':'old','text':'I'},{'status':'new','text':'I/T'}],reason='inspection of six timestamp fields plus comparison with six-byte numeric input; no tool-name classification')
sn=P['books']['SSTR']['sheet'];a='K218';lines=plain(C['SSTR'][sn][a]);lines.extend([
 {'status':'new','text':'이 결과 시험의 고정 입력은 시간=[26,5,29,13,14,15], 주소=[18,52,86,120,154,188], 이벤트=0x40이다. 해당 입력의 예상 저장 기록은 시간=[26,5,29,13,14,15], 주소=[18,52,86,120,154,188], 이벤트=64다.'},
 {'status':'new','text':'위 값은 기존 입력 보조기록이며 저장 후 독립적으로 읽은 관찰값과의 일치 근거는 미확인이다. 검증 [1]의 6개 시간 필드 표시 확인을 이 저장 일치 조건의 통과로 확대하지 않는다.'},
 {'status':'new','text':'[E8300 보완] 해당 입력으로 저장한 시간/주소/이벤트를 읽은 값과 원소별 비교 근거.'}])
add('SSTR',a,lines)
add('SSTR','L217',[{'status':'old','text':'통과'},{'status':'new','text':'판정 대기'}],reason='historical field-display pass retained in previous comparison; independent readback evidence for condition2 is pending')
(H/'date-plan.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
(H/'author-payload.json').write_text(json.dumps(P,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:list(v['changes']) for k,v in out.items()},ensure_ascii=True))
