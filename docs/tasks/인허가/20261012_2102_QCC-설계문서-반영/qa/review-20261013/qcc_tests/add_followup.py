from pathlib import Path
import json,difflib,zipfile
from integrate import sheetmap,vals
H=Path(__file__).resolve().parent
F=json.loads((H/'followup-plan.json').read_text(encoding='utf-8'));P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'))
V={}
for k in F:
 with zipfile.ZipFile(F[k]['path']) as z:
  d={i.filename:z.read(i.filename) for i in z.infolist()};V[k]=vals(d,sheetmap(d)[F[k]['sheet']])
def put(k,a,new,reason):
 sn=F[k]['sheet'];before=V[k].get(a,'');compold=P['books'][k]['comparison'][sn].get(a,{}).get('before',before)
 lines=[];old=compold.strip().splitlines();new=new.strip().splitlines()
 for op,lo,hi,la,ha in difflib.SequenceMatcher(a=old,b=new,autojunk=False).get_opcodes():
  if op=='equal':lines.extend({'status':'same','text':s} for s in new[la:ha])
  else:
   lines.extend({'status':'old','text':s} for s in old[lo:hi]);lines.extend({'status':'new','text':s} for s in new[la:ha])
   if la==ha:lines.append({'status':'new','text':''})
 display='\n'+'\n'.join(('[-] ' if x['status']=='old' else '[+] ' if x['status']=='new' else '')+x['text'] for x in lines)+'\n';current='\n'+'\n'.join(new)+'\n'
 F[k]['changes'][a]={'before':before,'current':current,'display':display,'lines':lines,'reason':reason}
 P['books'][k]['comparison'][sn][a]={'before':compold,'current':current,'display':display,'lines':lines}
zero='00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00'
for k in ['SUVC','SUVR']:
 put(k,'I1760','생산 비밀키 제외, 격리된 공개 시험키만 사용.\nshared_service_key(16byte,hex)='+zero+'\n[QCC 자료 입력] 리모콘 AES-CMAC의 nonce/입력 순서·길이, 전체16byte 출력의 독립 예상 벡터와 실측 미비. 키 역할/직렬화 미확정.\n[QCC 자료 입력] 전체 QCC RTT 형식/문자열과 실행 결과.','1/13/22: independent algorithm/key/input/output criteria, concrete public testkey, source prose removed')
for k in ['SSTC','SSTR']:
 current=P['books'][k]['comparison'][F[k]['sheet']]['I199']['current'].replace('페어링 후 2차 인증키 전송 전에 제어 명령을 보낸다.','페어링 후 CRA 완료 전에 일반 제어 명령을 보낸다.').replace('불일치 키와 일치 키를 각각 보내고, 인증 성공 후 제어 명령을 보낸다.','앱/QCC의 시험키를 불일치 및 일치 조건으로 각각 구성하여 CRA를 수행하고, 인증 성공 후 일반 제어 명령을 보낸다.')
 put(k,'I199',current,'1/11: CRA key configuration and authentication, no secret key transport assertion; old observation scope preserved')
 put(k,'J199','조건 [1] 페어링 완료/CRA 미완료 상태에서 일반 제어 명령 전송.\n통신 규칙에 따른 오류 응답이 와야 하며 일반 제어 명령이 실행되지 않아야 한다.','1/13: actual CRA state boundary')
 put(k,'J200','조건 [2] 페어링 후 리모콘/매핑 각각 QCC 공개 시험키00×16 유지, 클라이언트만01×16으로 구성하여 CRA 수행.\n인증 실패/오류 응답이며 일반 제어가 차단되어야 한다. 생산 비밀키는 사용/노출하지 않는다.','1/13: mismatched-key concrete input, historical observation remains scope')
 put(k,'J201','조건 [3] 페어링 후 일치하는 공개 시험키로 CRA 수행.\n리모콘 shared_service_key 및 매핑 clinician_master_key의 공개 시험값16byte(hex)='+zero+'\n리모콘 AES-CMAC 인증, 매핑 SHA-256 파생키 생성과 AES-CMAC 인증을 구분하여 정의된 순서로 성공하고 Status1과 정상 응답이 확인되어야 한다.\n[QCC 자료 입력] shared_service_key/clinician_master_key/clinic_service_key의 역할, serial/nonce/Challenge/Confirm 직렬화와 입력 길이, 전체16byte 독립 인증 예상 벡터 및 실제 전량 인증값. 미확정 범위는 판정 대기. 생산 비밀키는 제외한다.','1/13/22: actual algorithm/key/input/output and unresolved independent vectors; no source/process prose')
 put(k,'J202','조건 [4] CRA 성공 후 허용된 일반 내부기 제어 명령 전송.\n정상 응답과 허용된 제어 동작이 확인되어야 한다. 인증 성공을 AUTH 중계 패턴의 전송 성공으로 대신하지 않는다.','1/18: real CRA success versus transport pattern')
(H/'followup-plan.json').write_text(json.dumps(F,ensure_ascii=False,indent=2),encoding='utf-8')
(H/'author-payload.json').write_text(json.dumps(P,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:len(v['changes']) for k,v in F.items()},ensure_ascii=True))

