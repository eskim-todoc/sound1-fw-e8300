from pathlib import Path
from lxml import etree as E
import json,zipfile
from integrate import sheetmap,vals,N,sha
H=Path(__file__).resolve().parent;P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'));V=json.loads((H/'current-values.json').read_text(encoding='utf-8'))
starts={'SU':[67,72,101,149,285,309,333,578,650,789,809,915,1003,1010,1016,1040,1058,1148,1159,1175,1308,1644,1686],'SS':[78,86,114,124,134,148,159,188,217,225,278],'SI':[32,67]}
out={}
for k,b in P['books'].items():
 with zipfile.ZipFile(b['path']) as z:data={i.filename:z.read(i.filename) for i in z.infolist()}
 sn=next(s for s in sheetmap(data) if s.startswith(('기준서_','보고서_')));member=sheetmap(data)[sn];v=vals(data,member);root=E.fromstring(data[member]);changes={}
 if k.startswith('SI'):
  before=v['F141'];new='\nSU-ID-QCC-008\nSU-ID-CFX-005, I2S 인터페이스 초기화: I2S 초기화와 인터럽트 수신 준비\nSU-ID-CFX-008, I2S 입력 상태 판정: 수신 상태 판정/복사/스트리밍 상태 갱신\nSU-ID-CFX-015, I2S 수신 버퍼 관리: 16표본 FIFO에서 수신 버퍼로 복사하고 완료 플래그 갱신\nSW-FW-RID-15/16/69\n수신 경계: QCC I2S 송신부터 E8300 버퍼/완료 상태까지 확인한다. 믹싱/자극 경로는 SW-FW-IntegrationTest-ID-013에서 별도로 확인한다.\n'
  lines=[{'status':'old','text':line} for line in V[k][sn]['F141'].splitlines() if line]+[{'status':'new','text':line} for line in new.splitlines() if line]
  display='\n'+''.join(('[-] ' if line['status']=='old' else '[+] ')+line['text']+'\n' for line in lines)
  changes['F141']={'before':before,'current':new,'display':display,'lines':lines}
  # Punctuation only in the newly authored notice, with its original run style.
  assert v['M131'].count('위치·SPI')==1
  changes['M131']={'before':v['M131'],'current':V[k][sn]['M131'].replace('위치·SPI','위치/SPI'),'display':v['M131'].replace('위치·SPI','위치/SPI'),'punctuation_only':True,'old':'위치·SPI','new':'위치/SPI'}
 heights={'333':180,'334':180} if k.startswith('SU') else {}
 if k.startswith('SI'):
  # Respect existing row height; compare the final native display before any
  # additional height change. F141's trace uses the same 14-condition structure.
  heights={}
 rows=starts[k[:2]];idcol='D' if k.startswith('SU') else 'C' if k.startswith('SI') else 'B';ids=[]
 for r in rows:
  found=[(int(a[len(idcol):]),t.strip()) for a,t in V[k][sn].items() if a.startswith(idcol) and a[len(idcol):].isdigit() and int(a[len(idcol):])<=r and t.strip().startswith('SW-FW-')]
  ids.append({'start_row':r,'id':max(found)[1] if found else None,'is_condition_start':r in [333,1016,1058]})
 out[k]={'path':b['path'],'hash':sha(b['path']),'sheet':sn,'member':member,'changes':changes,'heights':heights,'top_alignment_cells':['H78'] if k.startswith('SS') else [],'page_break_before_rows':rows,'break_ids_and_tests':ids,'candidate_path':str(H/'final-layout-candidate'/Path(b['path']).name)}
(H/'final-layout-plan.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'files':len(out),'break_starts':starts,'heights':{'333':180,'334':180},'alignment':'SSTC/SSTR H78 left/top only','content':'SITC/SITR F141 trace; M131 punctuation only','excluded_normal_boundary':'SUVC35/CFX31 preparation/procedure fully visible, no group322 break; array-only line continuations preserved'},ensure_ascii=True))
