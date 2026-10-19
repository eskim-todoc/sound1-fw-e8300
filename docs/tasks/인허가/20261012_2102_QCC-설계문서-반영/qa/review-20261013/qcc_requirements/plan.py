import json,pathlib,re
ROOT=pathlib.Path(__file__).parent
D=json.loads((ROOT/'inspection.json').read_text(encoding='utf-8'))
def rows(label):
 return {int(r['cells']['G'+str(r['row'])]['value'].rsplit('-',1)[1]):{'row':r['row'],**{c:r['cells'].get(c+str(r['row']),{}).get('value') or '' for c in 'FGHI'}} for r in D['files'][label]['sheets'][1]['rows'] if str(r['cells'].get('G'+str(r['row']),{}).get('value')).startswith('SW-FW-RID-')}
old=rows('[E8300] SRS');cur=rows('[E8300-QCC] SRS')
def norm(s):return re.sub(r'-0+(\d+)$',r'-\1',s)
matrix={norm(r['cells']['G'+str(r['row'])]['value']):{'row':r['row'],'id':r['cells']['G'+str(r['row'])]['value'],'text':r['cells'].get('F'+str(r['row']),{}).get('value') or ''} for s in D['matrix']['sheets'] if s['name']=='설계 입출력 검증 매트릭스' for r in s['rows'] if r['cells'].get('G'+str(r['row']),{}).get('value')}
specific={
15:('E8300은 DMIC1/DMIC2의 입력과 QCC가 전달하는 I2S 오디오를 처리한다. DMIC2는 QCC에도 입력된다. QCC의 Bluetooth 수신 오디오는 I2S의 24비트 데이터, 16kHz 샘플링 주파수로 E8300에 전달한다.','[QCC 자료 입력] I2S의 클록/프레임과 유효 신호의 출력 조건, 동시 입력 조건 및 측정 결과를 확인한다.'),
16:('오디오 입력 대역은 0~8kHz이며, 16kHz 샘플링 주파수와 구분한다.','[QCC/E8300 자료 입력] 8kHz 대역 끝점의 입력/출력 조건, 허용 범위와 측정 결과를 확인한다.'),
44:('리모콘 앱과 BLE로 충전/배터리 상태, Bluetooth 제어 정보 및 사운드 처리기의 명령/응답을 송수신한다. QCC는 E8300의 명령/응답을 SPI로 중계한다.','[QCC 자료 입력] Bluetooth 제어 명령/상태와 충전 상태 데이터의 전달 조건 및 검증 결과를 확인한다.'),
45:('매핑 프로그램과 BLE로 매핑 시작/종료 및 사운드 처리기의 명령/응답을 송수신한다. QCC는 E8300의 명령/응답을 SPI로 중계한다.','[QCC 자료 입력] 매핑 시작/종료의 상태 전이, 실패 처리, 동시 연결 조건 및 검증 결과를 확인한다.'),
69:('QCC는 수신 오디오를 I2S로 E8300에 전달한다. DMIC2의 입력은 E8300과 QCC에 연결되며 전화 통화 시 QCC가 DMIC2의 음성을 수음하여 통화 상대에게 송신한다. DMIC2 마이크 입력과 QCC에서 E8300으로 전달하는 I2S 오디오는 서로 다른 경로다.','[QCC 자료 입력] DMIC2의 QCC 입력 연결은 확인되었다. 마이크 핀/클록, 연결 상대, 프로파일/코덱별 송수신, 동시 동작 조건과 실제 시험 결과를 확인한다.'),
70:('QCC는 BLE 연결과 데이터 접근을 처리하고 E8300의 명령/응답을 SPI로 중계한다.','[QCC 자료 입력] GATT 서비스/특성 값, 명령/응답의 허용 상태 및 검증 결과를 확인한다.'),
47:('BLE 데이터 송수신이 3분(180초) 동안 없으면 연결을 해제한다. 데이터 송수신의 감시 범위가 제어 명령 미수신 조건을 충족하는지 확인해야 한다.','[QCC/E8300 자료 입력] 감시 대상, 타이머 시작/갱신/초기화 조건, 180초의 허용오차와 경계 동작을 확인한다. 제어 명령이 아닌 송수신에 의해 연결이 유지되는 경우의 처리 기준도 필요하다.'),
48:('인증 서비스는 Challenge, Response, Confirm, Status 특성을 사용한다. 보호 대상 기기 제어/매핑 데이터 접근은 인증 이후에 허용한다.','[QCC 자료 입력] 페어링 전 허용 서비스, GATT 권한과 인증 전후 접근 차단/허용 조건 및 검증 결과를 확인한다.'),
49:('', '[QCC 자료 입력] BLE 링크 암호화 활성화와 페어링 전 차단을 확인한다. CRA 인증값 계산과 키 파생의 확인은 링크 암호화의 검증과 구분한다.'),
52:('기기 제어용 키와 의료진 매핑용 키를 구분한다. AES-CMAC 인증값 계산과 SHA-256에 의한 매핑 키 파생은 역할이 다르다. 의료진 키와 제조번호로 생성한 SHA-256 결과의 앞 16 bytes를 매핑용 키로 사용한다. 키 길이만으로 보안 검증 완료를 판단하지 않는다.','[QCC 자료 입력] 앱별 CRA 인증값 계산 알고리즘, 키 역할, 입력 직렬화와 인증값 길이를 확인한다. AES-CMAC 인증과 SHA-256 키 파생의 관계, HMAC/ENC 표기 및 검증 벡터를 대조한다.'),
54:('E8300의 부팅 과정에서는 실행파일의 CRC와 파일/이미지 무결성을 확인한다. QCC는 전원 인가 후 부팅 과정에서 칩셋이 제공하는 방식으로 펌웨어 이미지의 무결성을 검사한다.','[QCC 자료 입력] QCC의 부트 검사 방식/활성화 설정, 손상 또는 변조 이미지의 거부/복구 동작과 검증 결과를 확인한다.'),
71:('30초 동안 인증이 완료되지 않으면 타임아웃으로 처리한다.','[QCC 자료 입력] 30초 타이머의 시작/갱신/초기화 조건, 허용오차와 상태별 응답 및 인증정보 미노출의 검증 결과를 확인한다.'),
72:('5회 연속 인증 실패 시 1분(60초) 동안 BLE 연결을 제한한다.','[QCC 자료 입력] 실패 횟수의 누적/초기화, 전원 재기동, 제한 종료 후 재접속과 시간 경계/허용오차 및 검증 결과를 확인한다.'),
76:('생산 시 설치하는 shared_service_key/clinician_master_key와 생성되는 매핑용 clinic_service_key를 구분한다.','[QCC 자료 입력] 실제 설치/저장 대상, 키 역할, 저장 암호화/접근 제한과 복호화 시작 조건을 확인한다. 인증 전후 송수신/OTA 보호와 조건별 검증 결과도 확인한다.'),
78:('CRA 인증값 계산, 매핑 키 파생, 비휘발성 저장 암호화와 BLE 링크 보호의 사용 목적을 구분한다.','[QCC/E8300 자료 입력] AES-CMAC/SHA-256/AES-128의 사용 목적/키 관리, E8300 AES 저장/부트 보호와의 연결을 확인한다. HMAC/ENC 표기, AES-128 모드 및 BLE 링크 보호의 실제 적용과 검증 결과를 대조한다.'),
60:('', '[QCC 자료 입력] 무선 통신 CRC의 적용 계층, 검사 실패 시 처리와 조건별 검증 결과를 확인한다.')
}
result=[]
for rid,r in cur.items():
 if rid==82:continue
 m=[]
 if rid==83:
  m=[matrix['SYS-RID-ACDOC-'+str(n)] for n in [17,18,20]]
  base='\n'.join(x['text'] for x in m)
  detail='안전 예방조치, 사용 방법과 문제 해결 방법을 제품 사용 설명서에 포함한다.'
  notices=['[E8300 보완] 제품 사용 설명서의 해당 절과 내용, 배포 대상 및 검토 결과를 확인한다.']
  upper='\n'+', '.join(x['id'] for x in m)+'\n'
 else:
  upper=old[rid]['H'];ids=re.findall(r'SYS-RID-[A-Z0-9-]+',upper)
  m=[matrix[norm(i)] for i in ids if norm(i) in matrix]
  base='\n'.join(x['text'].strip('\n') for x in m) if m else old[rid]['F'].strip('\n').split('\n\n')[0]
  if rid in specific:detail,notice=specific[rid];notices=[notice] if notice else []
  else:
   prior=cur[rid]['F'].strip('\n');notices=[line for line in prior.splitlines() if line.startswith(('[QCC 자료 입력]','[QCC/E8300 자료 입력]','[E8300 보완]'))]
   prior='\n'.join(line for line in prior.splitlines() if not line.startswith(('[QCC 자료 입력]','[QCC/E8300 자료 입력]','[E8300 보완]'))).strip()
   detail=prior if prior.strip()!=base.strip() else ''
  # Preserve user-removed notices by taking current, never reintroduce from original.
  if rid not in specific and not cur[rid]['F'].count('[QCC') and rid!=81:notices=[]
 value='\n'+base
 if detail:value+='\n상세: '+detail
 if notices:value+='\n\n'+'\n'.join(notices)
 value+='\n'
 result.append({'id':'SW-FW-RID-'+str(rid),'rid':rid,'old_row':r['row'],'base_text':base,'detail':detail,'notices':notices,'F':value,'H':upper,'I':old[rid]['I'] if rid in old else '', 'matrix':m,'matrix_match':bool(m),'existing_upper_preserved':rid not in old or upper==old[rid]['H'],'old_F':r['F'],'old_H':r['H'],'old_I':r['I']})
out={'date':'2026-10-13','matrix_url':'https://docs.google.com/spreadsheets/d/1tT8US7ry-1s9Dqdg_eqpJ6ixSB6gbsjUezcIrn23Ogc/edit?gid=774914198','matrix_sheet':'설계 입출력 검증 매트릭스','matrix_sheet_id':'774914198','matrix_snapshot':str(ROOT/'matrix-readonly.xlsx'),'matrix_sha256':D['matrix']['sha256'],'source_current_sha':D['files']['[E8300-QCC] SRS']['sha256'],'active_requirement_count':len(result),'removed_duplicate_rid':82,'merged_into_rid':69,'requirements':result,'raw_qcc_id_mapping_work_evidence_only':{'RI09':[15,16],'RI10':[69],'RI11':[69],'RI12 BLE':[70],'RI13 remocon':[44],'RI14 mapping':[45],'RI13 pairing':[48,49],'RI14 auth-access':[48,76],'RI15':[48,52,71],'RI16 crypto':[52,76,78],'RI17':[47,72],'RI18':[54],'RI16 manual':[83]}}
(ROOT/'requirements-contract.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'saved':str(ROOT/'requirements-contract.json'),'active_requirements':len(result),'matrix_exact_base_count':sum(x['matrix_match'] for x in result),'qraw_I_to_remove':[x['old_row'] for x in result if x['old_I']!=x['I']]},ensure_ascii=False))
