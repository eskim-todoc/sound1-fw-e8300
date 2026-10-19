import json,re
from pathlib import Path
HERE=Path(__file__).resolve().parent
a=json.loads((HERE/'audit-current.json').read_text(encoding='utf-8'))
out={'unit':[],'integration':[], 'result_policy':'새 조건은 미실행/판정 대기. 실제 시험 결과와 전체 QCC RTT 없음. AUTH 중계 패턴은 CRA 성공과 구분.', 'hardware':'DMIC2는 E8300/QCC 양쪽에 연결. 통화 시 QCC 수음→Bluetooth 통화 상대 송신, QCC→E8300 I2S 하향 경로와 구분.'}
purposes=['PIO 방향/주변장치·초기값 및 입력변화 메시지','전압→잔량·충전·저전압 상태','외부기 일련번호 PS 매핑/읽기/쓰기','기동 설정/초기화와 전원 상태 SPI/ACK','여섯 시스템 상태 저장/조회','생산 시 Standalone/TWS 구성과 편이/양이 오디오·데이터 처리','연결표시 입력에 따른 Bluetooth 활성 및 장치명','부착/탈착 시 BLE/Classic 검색·연결 활성','광고/스캔 응답과 현재/peer 잔량·연결 상태','Classic 페어링·연결, 시험 스피커/양이 및 I2S 하향','AUTH 서비스 정의/등록 및 네 characteristic 패턴 중계','GAIA 편이/양이 메시지 처리와 위치·잔량 응답','1:1, CRA 성공/실패·30초 timeout','리모콘 키, SHA-256 파생 매핑키, AES-CMAC 인증 및 NVM 복호화','보안 PS 영역/길이·읽기 인터페이스 및 암호화 저장값/복호화']
for key,dest in [('SUVC','unit'),('SITC','integration')]:
 b=a['books'][key];name=b['sheets'][1]['name'];c=b['all_cells'][name]
 for i,g in enumerate(b['groups']):
  row=g.get('start',g.get('rows',[0])[0]);span=g.get('rows');rows=list(range(span[0],span[-1]+1)) if span else list(range(row,g.get('end',row)+1))
  # Builder groups use start/end; saved ID is authoritative.
  idcol='D' if key=='SUVC' else 'C';descol='E';unitcol='G' if key=='SUVC' else 'F';expected='K' if key=='SUVC' else 'M';proc='J' if key=='SUVC' else 'L'
  out[dest].append({'id':c.get(idcol+str(row)), 'rows':rows,'title':c.get(descol+str(row),'').strip().split('\n')[0], 'purpose':purposes[i] if key=='SUVC' else c.get(descol+str(row),'').strip(), 'unit_requirement_trace':c.get(unitcol+str(row),''), 'conditions':[{'row':r,'criterion':c.get(expected+str(r),''),'procedure':c.get(proc+str(r),'')} for r in rows], 'proposed_change': '장치 상태는 Standalone/TWS 설계 조건으로 보완. 원문 ID와 병합 설명은 기능 본문에서 제거. 준비조건은 사전 상태, 입력 실행은 절차로 분리.' if key=='SUVC' else '사전조건/수행절차 분리, QCC 직접 기능과 E8300 완주 검증 범위 유지.'})
(HERE/'test-contract.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'unit':[(x['id'],x['rows']) for x in out['unit']], 'integration':[(x['id'],x['rows']) for x in out['integration']]},ensure_ascii=True))
