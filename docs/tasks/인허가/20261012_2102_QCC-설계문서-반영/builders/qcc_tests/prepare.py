import json,re,math
from pathlib import Path

HERE=Path(__file__).resolve().parent
TASK=HERE.parent.parent
BASE=json.loads(Path(r'C:\Users\EUNSU\AppData\Local\Temp\qcc_test_books_readonly.json').read_text(encoding='utf-8'))
EV=json.loads((TASK/'evidence/qcc_tests.json').read_text(encoding='utf-8'))
def pad(s): return '\n'+str(s).strip()+'\n' if s is not None else ''
def rownum(c): return int(re.search(r'\d+',c)[0])
def col(c): return re.match('[A-Z]+',c)[0]
def missing(s,owner='QCC'): return '['+owner+' 자료 입력] '+s if owner!='E8300' else '[E8300 보완] '+s
def packet(h): return ' '.join(h)
ACK='34 01 '+'00 '*18+'02'
REQ80='34 50 01 01 '+'00 '*16
REQ40='34 28 00 02 '+'00 '*16
assert len(ACK.split())==21 and len(REQ80.split())==20
allbooks=[]
unitstarts=[]
common={}
for kind,baseix,commonEnd,lastCol,start,insertAt in [('unit',0,'K','K',30,1727),('integration',1,'M','M',25,131),('system',2,'J','J',34,282)]:
 bc=BASE[baseix]; br=BASE[baseix+3]
 cells={k:v for k,v in bc['sheets'][1]['cells'].items() if start<=rownum(k)<insertAt and 'B'<=col(k)<=commonEnd}
 common[kind]={'base':cells,'edits':{},'newRows':[], 'merges':[], 'heights':{},'at':insertAt, 'groups':[]}

# 15 QCC source groups / 38 original conditions. Each condition remains a separate row.
u=common['unit']; rr=u['at']
module={1:'SI-ID-QCC-002\n하드웨어 인터페이스',2:'SI-ID-QCC-002\n하드웨어 인터페이스',3:'SI-ID-QCC-002\n하드웨어 인터페이스',4:'SI-ID-QCC-001\n시스템 관리',5:'SI-ID-QCC-001\n시스템 관리',6:'SI-ID-QCC-001\n시스템 관리',7:'SI-ID-QCC-001\n시스템 관리',8:'SI-ID-QCC-003\nBluetooth',9:'SI-ID-QCC-003\nBluetooth',10:'SI-ID-QCC-003\nBluetooth',11:'SI-ID-QCC-003\nBluetooth',12:'SI-ID-QCC-003\nBluetooth',13:'SI-ID-QCC-004\n보안',14:'SI-ID-QCC-004\n보안',15:'SI-ID-QCC-004\n보안'}
general='QCC 시험 이미지/버전·소스·회로·도구 버전을 기록한다. 초기 상태를 조건별 복원한다. 제공 자료는 기준서이며 실제 결과/전체 RTT가 없다.'
for n,g in enumerate(EV['groups'],1):
 first=rr; cnt=len(g['cases']); notices=g['unresolved'][:]
 for i,case in enumerate(g['cases']):
  r=rr;rr+=1;u['newRows'].append(r)
  inp=case['input']; expected=case['expected']; proc=case['procedure']
  if n==2 and i==0: expected+='; 설정 기준 전압/잔량은 조건[3]의 6개 쌍과 동일하다.'
  if n==2 and i==2:
   expected='공급 전압[V]→잔량[%]: 4.2 이상→100(Full), 3.95→80, 3.7→60, 3.475→40, 3.35→20, 3.0→0(Empty). 각 전압에서 실제 공급/ADC/잔량값을 옆 결과에 각각 기록. Empty에서 동작 정지/저전압 진입. 원문 10% 허용오차의 상대오차/%p 의미가 확정되기 전 수치 합격 판정 대기.'
  if n==4 and i==1:
   expected='입력 A: 잔량80%, charger_connected=1, lid=1. QCC 송신20byte(hex)='+REQ80.strip()+'\n입력 B: 잔량40%, charger_connected=0, lid=2. 송신20byte(hex)='+REQ40.strip()+'\n각 입력의 E8300 응답 저장배열21byte(hex)='+ACK+'; 마지막 index20=응답길이2, 유효 ACK=34 01.\nE8300 전체 예상 RTT A: [BT] CMD 0x34, CHARGER STATE: 1, BATT LEVEL 80 PERCENT, LID STATE 1\n전체 예상 RTT B: [BT] CMD 0x34, CHARGER STATE: 0, BATT LEVEL 40 PERCENT, LID STATE 2\n위 RTT는 E8300 구현 출력의 예상 문자열이며 QCC 실측 로그가 아니다. 각 상태80→40, charger1→0, lid1→2. shutdown_ind 전송 시점은 미확정.'
  if n==11 and i==1:
   expected='Challenge central→QCC(12byte): 00 01 02 03 04 05 06 07 08 09 0A 0B\nResponse QCC→central(12byte): 10 11 12 13 14 15 16 17 18 19 1A 1B\nConfirm central→QCC(12byte): 20 21 22 23 24 25 26 27 28 29 2A 2B\nStatus QCC→central(1byte): 30\n각 길이/순서/전바이트가 위 패턴과 일치. AUTH 중계 범위이며 실제 CRA 계산/인증 성공을 입증하지 않는다.'
  if n==12 and i==0:
   inp='편이 설정; Pair=19 04 02 01 80 80, State=19 04 02 01 81 00 01. 시험 A Left/37%, B Right/72%.'
   expected='Pair 응답6byte(hex)=19 04 04 01 80 01.\nState A 응답10byte=19 04 03 01 84 01 00 25 00 00; State B=19 04 03 01 84 01 01 48 00 00. aa는 원문 편이 정의0=Left/1=Right, bb는37(25h)/72(48h). 위 값은 재현용 예상값이며 측정값이 아니다. 실제 프레이밍/길이필드는 확정 자료와 대조.'
  if n==12 and i==1:
   inp='양이 설정; Pair=19 04 02 01 03 80 80, State=19 04 02 01 03 81 00 01. 양쪽 nn/aa/잔량37·72를 구별해 준비.'
   expected='Pair 응답 template=19 04 04 01 nn 80 01. State 응답 template=19 04 03 01 nn 84 01 aa bb 00 00. bb=37(25h) 또는72(48h). nn 외부기 위치와 aa는 원문 1=Left/0=Right 및 반대 문장이 충돌한다. 확정된 위치별 기대 전바이트가 마련되기 전 판정 대기. BLE 메시지를20byte SPI로 자동 변환하지 않는다.'
  if n==13 and i==3: inp='정상 시험키16byte=00×16를 QCC에 유지하고 클라이언트 한쪽만01×16로 변경; 앱별 auth_type은1리모콘/2매핑 유지. 원문의 변경 대상이 미확정이므로 실제 설정 확인 후 실행.'
  if n==14 and i==1:
   expected='상세설계 근거: clinician_master_key를 NVM에서 읽고 정의된 입력의 SHA-256 결과 앞16byte로 clinic_service_key 생성; 인증값 계산은 AES-CMAC. QCC SRS의 매핑 SHA-256 표기와 역할 정합성을 확인한다. 입력 직렬화/시험 벡터가 제공되지 않아 digest/CMAC 수치는 미확정이며 결과 생성 금지.'
  if n==15 and i==0: expected+=' 생산 저장 키(shared_service_key/clinician_master_key)와 생성된 clinic_service_key를 구분한다. 펌웨어 읽기 전용을 PSTool/디버거 접근 차단으로 확대하지 않는다.'
  selected=notices[i::cnt]
  limits='실제 QCC 로그 원문을 수집하며 생성 예상 로그로 대체하지 않는다. 생산 비밀키를 기록하지 않고 공개 시험키를 사용한다.'
  if selected: limits+='\n'+missing(' / '.join(selected))
  u['edits'].update({f'F{r}':pad('T' if any(x in case['method'] for x in ['T']) else case['method']),f'H{r}':pad(general+'\n입력: '+inp),f'I{r}':pad(limits),f'J{r}':pad(proc+'\n각 입력과 전량 실제값을 동일 조건의 결과 칸에 기입한다. 부분 로그만 제공된 경우 전체 형식/자료 부족을 기록한다.'),f'K{r}':pad('조건 ['+str(i+1)+']\n입력: '+inp+'\n예상/판정: '+expected)})
  # Conservative per-row heights, bounded by Excel409.5pt. Group metadata is vertically merged.
  maxlines=max(math.ceil(len(limits)/22),math.ceil((len(inp)+len(expected))/44),math.ceil((len(general)+len(inp))/27))
  u['heights'][r]=min(409.5,max(270,28+maxlines*16))
 last=rr-1
 for c,value in {'B':'QCC','C':module[n],'D':g['proposed_test_id'],'E':g['title']+'\n원문 '+g['source_test_id']+'\n요구: '+(', '.join(g['requirement_refs']['e8300_rids']) or '直接要求ID未提供 / 요구 추적 자료 필요'),'G':'\n'.join(g['design_unit_refs'])}.items():
  u['edits'][f'{c}{first}']=pad(value.replace('直接要求ID未提供','직접 요구 ID 미제공'))
  if last>first:u['merges'].append(f'{c}{first}:{c}{last}')
 u['groups'].append({'id':g['proposed_test_id'],'source':g['source_test_id'],'rows':[first,last],'units':g['design_unit_refs'],'cases':cnt})
assert len(u['newRows'])==38

# QCC integration conditions are additions to the17 E8300 tests; old results keep their old scope.
itg=common['integration']; r=131
def add_integ(n,title,item,units,methods,cases,limits):
 global r
 first=r
 for i,(inp,exp,proc) in enumerate(cases,1):
  itg['newRows'].append(r);itg['heights'][r]=min(409.5,max(270,28+max(math.ceil(len(exp)/43),math.ceil(len(limits)/23))*16))
  itg['edits'].update({f'I{r}':pad(methods),f'J{r}':pad('QCC/E8300 시험 이미지·버전·연결 상대·회로/통신 규칙·수집 도구를 고정한다. 초기 상태를 조건별 복원한다.\n입력: '+inp),f'K{r}':pad(limits),f'L{r}':pad(proc+'\nQCC 입력/전송, E8300 수신·상태/응답 및 실제 장치 동작을 따로 저장한다.'),f'M{r}':pad('조건 ['+str(i)+']\n입력: '+inp+'\n예상/판정: '+exp)})
  r+=1
 last=r-1
 values={'B':'QCC/E8300','C':f'SW-FW-IntegrationTest-ID-{n:03d}','D':item,'E':title,'F':units,'G':'QCC 외부기/E8300 및 격리된 시험 지그','H':'QCC 모듈과 E8300 실제 입력 경계 연계'}
 for c,v in values.items():
  itg['edits'][f'{c}{first}']=pad(v)
  if last>first:itg['merges'].append(f'{c}{first}:{c}{last}')
 itg['groups'].append({'id':f'SW-FW-IntegrationTest-ID-{n:03d}','rows':[first,last],'cases':len(cases)})

add_integ(18,'QCC 전원 상태→SPI→E8300 상태와 ACK','SI-ID-QCC-001\nSI-ID-QCC-002','SU-ID-QCC-002/006\nSW-FW-RID-21/22/29/63','T',[
 ('잔량80%, binary충전1, lid1', '송신20byte(hex)='+REQ80.strip()+'\n응답 저장21byte='+ACK+'; 유효 ACK=34 01, index20=2. E8300 잔량80/충전1/lid1. 전체 예상 RTT: [BT] CMD 0x34, CHARGER STATE: 1, BATT LEVEL 80 PERCENT, LID STATE 1', 'QCC 실제 SPI 송신을 캡처하고 E8300 상태 및 실제 RTT/ACK 전량을 비교한다.'),
 ('잔량40%, binary충전0, lid2', '송신20byte(hex)='+REQ40.strip()+'\n응답 저장21byte='+ACK+'; E8300 잔량40/충전0/lid2. 전체 예상 RTT: [BT] CMD 0x34, CHARGER STATE: 0, BATT LEVEL 40 PERCENT, LID STATE 2', '독립 초기화 후 두번째 입력의 전바이트/상태를 비교한다.'),
 ('QCC 충전 상태4종을 각각 발생', 'QCC4상태가 확정 변환표에 따라0x34의0/1로 변환되어야 한다. E8300은0/1만 의미가 확정. 4상태 이름/코드→binary 표 미제공이므로 기대바이트/판정 대기.', '4상태별 QCC 내부 enum/송신nn/E8300 인식을 대응 기록한다.')],missing('충전4상태 변환표, shutdown_ind=0 상태 송신과 절전 시점, 주기/재송신 자료.','QCC/E8300'))
add_integ(19,'부착·종료 신호와 Bluetooth 상태 연계','SI-ID-QCC-001\nSI-ID-QCC-003','SU-ID-QCC-004/005/006/008/009\nSW-FW-RID-46/67/68/69/70','D/T',[
 ('내부기 미부착→부착→탈착→재부착', 'inter_dev_conn_ind 원문0=Bluetooth비활성,1=활성. 실제 부착 시 BLE/Classic 검색 가능, 탈착 시 미검색 및 기존 연결 해제. E8300 ISD 상태/PIO와 QCC 상태를 함께 확인.', '각 신호값·스캔 결과·실제 프로파일 연결을 같은 사건의 시각으로 수집한다. 연결1분은 탈착 전 준비 시간이다.'),
 ('E8300 정상→종료 요청', 'E8300 QCC_CTRL High=정상, Low=종료요청. QCC 원문 shutdown_ind=0 절전/1wake 및0에서SPI송신 기술의 물리/논리 대응을 확인한다. 확정 전 극성 추정 및 종료 통과 대기.', '계측한 핀 전압/논리값과 QCC 이벤트·절전·SPI 마지막 거래를 기록한다.')],missing('shutdown_ind 핀/논리 매핑, 절전 시점/탈착 인지 지연/연결 해제 시각 자료.','QCC/E8300'))
add_integ(20,'AUTH service의 시험 패턴 전량 중계','SI-ID-QCC-003','SU-ID-QCC-010\nSW-FW-RID-48/52/71/76','T',[
 ('Challenge/Response/Confirm/Status 시험 패턴', 'central→QCC Challenge12byte=00 01 02 03 04 05 06 07 08 09 0A 0B; QCC→central Response12byte=10 11 12 13 14 15 16 17 18 19 1A 1B; central→QCC Confirm12byte=20 21 22 23 24 25 26 27 28 29 2A 2B; QCC→central Status1byte=30. 네 방향·길이·전체값 일치.', '정의된 시험 전송 경계에서 네 교환을 수집하고 예상 전바이트와 비교한다.')],missing('UUID/속성/길이/접근권한 및 실제 수집값. 패턴 중계는 실제 CRA 인증/암호화 성공의 증거가 아니다.'))
add_integ(21,'실제 CRA·데이터 게이트와 서로 다른 시간 계약','SI-ID-QCC-003\nSI-ID-QCC-004','SU-ID-QCC-010/011/012/013/014\nSW-FW-RID-47/52/71/72/76','T/D',[
 ('리모콘/매핑 각각 인증 전, 불일치키, 성공후 동일 일반 제어 요청', '인증 전/실패 시 일반 제어 차단, 성공 후 허용. 앱별 키16byte=128bit, 타입1리모콘/2매핑. 리모콘 AES-CMAC; 매핑 키 SHA-256 생성과 AES-CMAC 인증 계산의 역할을 독립 벡터로 확인한다.', '공개 시험키와 정의된 Challenge를 사용해 전체 계산/Status 및 제어 허용을 비교한다.'),
 ('인증 미완료 연결 및30초 전 완료한 독립 정상 연결', '미완료는30초에서 타임아웃/해제, 완료 연결은인증 타임아웃으로 해제되지 않음. 실패/성공/진행중/타임아웃4종을 구별하며 비밀을 응답하지 않는다.', '연결 성립/타이머 시작/인증 완료/해제 시각을 수집한다. 허용오차 확정 전 판정대기.'),
 ('인증 완료 BLE연결 후 데이터 송수신 모두 중단180초; 별도 자동 상태 조회만 유지', '무송수신3분(180초)에 연결 해제. 무제어명령과 무송수신 감시/reset 차이 조건은 확정 계약에 맞아야 한다.', '마지막 TX/RX와 해제 시각을 기록하고 송신만/수신만 및 제어 외 상태 조회의 reset을 비교한다.'),
 ('같은 대상 CRA5회 연속 실패,6회째·제한 중 시도·60초 경과 후 정상시도', '5회 연속 실패 시1분(60초) 연결 제한. 실패1..5 횟수, 시작시각,6회 및60초 경계 후 허용 여부를 비교. 페어링 실패로 치환하지 않는다.', '대상/키/타입을 고정하고 실제 실패 횟수 및 각 시도·제한 해제 시각을 수집한다.')],missing('CRA 입력/상태코드/키 역할, 타이머 start/reset/허용오차, 실패횟수 reset/재접속 및 경계. 실제 CRA/게이트/시간 결과.','QCC/E8300'))
add_integ(22,'Classic 수신→QCC I2S→E8300 처리','SI-ID-QCC-003\nSI-ID-CFX-001','SU-ID-QCC-008\nSW-FW-RID-15/16/69','T',[
 ('시험음원 Classic연결, I2S출력 구성, I2S OFF→ON→OFF', 'QCC 출력 유효 오디오24bit/16kHz, E8300 I2S_FLAG DIO29 Low=유효. 실제 BCLK/LRCLK/데이터 배열 및 CFX수신을 확인. 기존 SIT013 믹싱 계산/자극은 별도 단계로 비교.', 'Classic 유효 연결→계측한 I2S 프레임/샘플→E8300 버퍼→지그의 자극을 경계별 저장한다.')],missing('QCC 프로파일/코덱, I2S 채널·슬롯·정렬·클록/위상·실측 및 입력음원과 자극 대응. 활성 로그/시험스피커 무음만으로 통과시키지 않는다.','QCC/E8300'))
add_integ(23,'BLE 편이/양이 GAIA 메시지 처리','SI-ID-QCC-003','SU-ID-QCC-011\nSW-FW-RID-29/35/38/44/45/70','T',[
 ('편이 Pair19 04 02 01 80 80; State19 04 02 01 81 00 01,Left37%/Right72%', 'Pair=19 04 04 01 80 01; StateLeft37=19 04 03 01 84 01 00 25 00 00; Right72=19 04 03 01 84 01 01 48 00 00. 전바이트·위치·잔량이 일치.', 'central의 전체 요청과 응답, QCC 저장 위치/잔량을 비교한다.'),
 ('양이 Pair19 04 02 01 03 80 80; State19 04 02 01 03 81 00 01', 'Pair=19 04 04 01 nn 80 01; State=19 04 03 01 nn 84 01 aa bb 00 00. 원문 aa 좌우값의 상충과 nn위치 정의를 확정한 뒤 기대 전바이트를 정한다. 미확정 판정 대기.', '양쪽 nn/수술위치/잔량을 다르게 설정하고 routing과 응답을 각각 기록한다.')],missing('좌우 aa 값 충돌, nn/03주소/길이필드·GAIA 프레이밍 계약. BLE 예시를 SPI로 자동 확장하지 않는다.'))
add_integ(24,'생산 NVM→키 생성·복호화→CRA 계산','SI-ID-QCC-004','SU-ID-QCC-012/013/014\nSW-FW-RID-52/76/78','A/T',[
 ('공개 시험 RAND/BD_ADDR와 암호화된16byte시험키00', 'PS_USR40 RAND10byte/43 clinician_master_key16byte/44 shared_service_key16byte. 정의된 도출/복호화 후 시험평문00×16. 매핑은 SHA-256결과 앞16byte clinic_service_key 생성 및 AES-CMAC 인증. 저장 암호문·중간값·결과 전체를 독립 예상벡터와 비교.', '생산 비밀키 대신 격리된 공개 시험키/암호문을 사용해 NVM읽기→각 계산 경계를 기록한다.')],missing('입력 직렬화·KDF/AES모드/시험벡터, 저장 대상 키 역할/암호화·접근권한 실제 적용. NVM읽기 전용과 디버거 접근 차단을 구분한다.'))

# Existing system tests: same-input concrete criteria / pending reassessment where old results do not prove new criteria.
s=common['system']
def sy(c,text): s['edits'][c]=pad(text)
def append(c,text): sy(c,s['base'].get(c,'').strip()+'\n'+text)
append('H78',missing('QCC I2S24bit/16kHz의 실제 슬롯·클록·정렬/계측 자료.','QCC/E8300'))
append('I78','조건[3]에서 QCC I2S 신호의24bit 유효 데이터 및16kHz 샘플 주기를 계측하고 E8300 수신과 비교한다.')
append('J80','QCC I2S 오디오 출력은24bit/16kHz이어야 한다. I2S_FLAG DIO29 Low=유효. 활성 로그와 버퍼 변화만으로 프레임/클록 실측을 대체하지 않는다.')
sy('D78','T/D')
append('H82','16kHz 샘플레이트의 Nyquist 경계8kHz는 신호 위상/필터/측정 경로 영향을 받는다. '+missing('오디오0~8kHz 판정의 주파수 응답/허용오차/8kHz 경계 시험법.','QCC/E8300'))
sy('J83','조건[2] 8000Hz 순음 입력. 오디오 대역0~8kHz 요구와16kHz 샘플레이트를 구분한다. 8kHz 경계의 실제 입력/위상/샘플 및 출력 응답을 기록하고 확정된 주파수 응답 기준과 비교한다. 단순 청취만으로8kHz 끝점 통과를 판정하지 않는다.')
append('H184',missing('Classic 연결의 프로파일/코덱 및QCC I2S24bit16kHz 프레임/클록 실제 수집 자료.','QCC/E8300'))
append('I184','Classic 수신→QCC I2S24bit16kHz→E8300 수신→자극 지그의 각 실제 경계를 별도로 기록한다.')
append('J184','실제 QCC I2S24bit/16kHz와 E8300 수신을 확인한다. QCC 시험스피커 출력/무음·I2S활성 로그는 최종 전극 자극 출력의 증거로 대체하지 않는다.')
sy('D184','T/D')
sy('C190','제어 명령 미수신 및 QCC 무송수신3분 연결 종료')
sy('H190',missing('무제어명령과 무송수신의 timer reset 정합성, 감시 start/허용오차.','QCC/E8300'))
sy('I190','페어링/CRA 완료 후 마지막 BLE TX/RX 시각을 저장한다. 제어 외 자동 상태 조회도 중단하고180초 무송수신 및 연결 해제 시각/주체를 계측한다. 독립 조건으로 상태조회만 유지하여 무제어명령 감시와 데이터 TX/RX reset의 차이를 확인한다.')
sy('J190','조건[1] 마지막 BLE 데이터 송수신 이후180초(3분) 동안 TX/RX없음→연결 해제. 기존 무제어명령 시 연결종료 의도도 보존한다. 상태조회만 송수신하며 제어명령은 없는 조건의 reset 계약을 대조한다. 두 감시의 정합성/허용오차가 미확정인 부분은 판정 대기. 30분 관찰 기록은180초의 실측으로 간주하지 않는다.')
sy('H199',missing('리모콘/매핑 키16byte(128bit), 실제 CRA 입력/상태코드/키역할·독립 계산벡터. 매핑 SHA-256키생성/AES-CMAC인증과 SRS 알고리즘 표기 정합성.'))
append('I199','리모콘 및 매핑 앱을 각각 시험한다. 공개16byte 시험키를 쓰고 불일치 조건은 한쪽만 변경한다. 실제 Challenge/Response/Confirm/Status 전량을 독립 계산과 비교한다. AUTH시험패턴 중계 성공으로 CRA 성공을 대체하지 않는다.')
append('J200','리모콘/매핑 각각 한쪽만 다른16byte키를 적용하며 실제 CRA 실패/응답/차단을 비교한다.')
append('J201','리모콘 AES-CMAC 및 매핑 SHA-256키생성/인증 AES-CMAC의 역할을 확정하고16byte 키의 독립 계산 예상값과 전체 인증값을 대조한다. 원문 표기 정합성/벡터 미제공 부분은 판정 대기.')
sy('D199','T/D')
append('H206',missing('QCC 칩셋 이미지 무결성 검사 방식·대상 이미지/오류 주입 및 실제 기동 차단 자료. E8300 CRC 결과와 분리한다.'))
append('J206','여기 기록한 E8300 이미지 CRC 확인을 QCC 자체 이미지 무결성 통과로 확대하지 않는다. QCC 정상/손상 이미지 조건은 QCC 자료 확정 후 같은 기준/보고 행으로 추가한다.')
sy('D210','T/D')
sy('H210',missing('CRA 상태4종 코드/허용필드,30초 타이머 start/reset/허용오차 및 실제 계산/연결 시각.'))
append('I210','인증을 완료하지 않은 BLE연결에서 타이머 시작과30초 종료/해제 시각을 계측한다. 30초 전 인증완료 조건의 연결유지도 독립 확인한다.')
sy('J211','조건[2] CRA 상태 실패/성공/진행중/타임아웃을 각각 확인한다. 인증이30초 동안 완료되지 않으면 타임아웃과 연결 해제. 시작/reset/허용오차는 확정 계약과 대조한다. 30초 전 완료 연결은 인증타임아웃 때문에 해제되지 않아야 한다. 상태 코드의 실제 전체 응답에는 비밀·추론 가능한 인증 정보가 없어야 한다. 3분 무송수신 종료/60초 실패 제한과 구분한다.')
sy('H212',missing('CRA 실패 카운터 대상/reset·재접속·제한 start/60초 경계 및 허용오차.'))
sy('I212','같은 대상 CRA 인증을1~5회 연속 실패시킨다. 각 실제 시각/횟수를 기록한다. 6회째 및60초 미만 제한 중 시도 후,60초 경과의 정상 인증 허용 여부를 확인한다. 페어링 키 실패를 CRA 실패로 치환하지 않는다.')
sy('J212','조건[1] 제한은 CRA 인증5회 연속 실패에 적용하며1분(60초) 연결 제한이다. 실패 카운터 대상/start/reset·재접속 계약과 actual시각을 비교한다.')
sy('J213','조건[2] 같은 대상의 CRA 인증을1~5회 연속 실패시킨다. 각 실패/횟수/시각 기록으로5번째 실패 후60초 제한의 시작을 확인한다. 일반 페어링 실패 기록만으로 통과하지 않는다.')
sy('J214','조건[3] 6회째 및60초 제한 구간의 시도는 연결 제한. 제한이 해제되는60초 경계와 그 이후 정상 CRA 인증/연결 허용을 확인한다. 경계/허용오차 미확정 부분은 판정 대기.')
append('H222',missing('생산 설치 shared_service_key/clinician_master_key와 생성 clinic_service_key의 역할·저장 대상/암호화 방식/접근 제한 실제 적용.'))
append('I222','생산 NVM PS_USR40 RAND10byte,43 clinician_master_key16byte,44 shared_service_key16byte의 실제 위치/길이/보호를 확인한다. 비밀키 원문은 기록하지 않는다.')
sy('J224','조건[3] 생산 NVM에는 별도 리모콘 shared_service_key 및 매핑 clinician_master_key를 설치하고 암호화 저장해야 한다. 생성 clinic_service_key와 저장 대상/역할을 구분한다. PS_USR40 RAND10byte/43·44 각16byte, 실제 암호화/접근제한 자료를 대조한다. 암호문이 평문과 다름 또는 펌웨어 Write인터페이스 부재만으로 암호화/제조보호 완료를 판정하지 않는다.')
append('H227',missing('QCC AES-CMAC/매핑 SHA-256키생성·AES-128복호화의 모드/입력/검증벡터·규격 권고의 적용 자료.'))
append('J229','QCC 인증 AES-CMAC 및 매핑키 생성 SHA-256, NVM 복호화 AES-128은 BOOT AES-ECB/CRC 확인과 분리하여 실제 알고리즘/입력/보호 목적 및 적용 권고를 분석한다.')

# New system82/83 conditions occupy previously blank rows282..287; footer stays at288.
for row,vals in [
 (282,{'B':'SW-FW-SYS-Test-ID-82','C':'외부기 마이크 오디오의 Bluetooth Classic 송신','D':'D','E':'SW-FW-RID-82','F':'SYS-RID-SP-HW-11','G':'외부기 마이크 입력, QCC 시험 이미지와 Bluetooth Classic 수신 상대/녹음·계측 도구를 준비한다. 시험음원1000Hz 및무음 조건을 고정한다. 음원·경로/샘플값/프로파일/코덱을 기록한다.','H':missing('마이크의QCC 입력경로·프로파일/코덱/상대장치·동시 송수신 허용 및주파수/품질/지연 판정 기준.','QCC/E8300'),'I':'외부기 마이크에1000Hz 시험음원을 입력하고 Classic 상대에서 실제 수신 오디오/데이터를 수집한다. 독립 무음 조건과 비교하고 경로·녹음/샘플·동시수신 설정을 기록한다.','J':'조건[1] 외부기 마이크1000Hz입력→다른 Bluetooth 장치에Classic송신. 상대의 실제 수신 오디오가 입력에 대응해야 한다. 경로/프로파일/코덱·허용오차를 확정한 뒤 전체 수신값을 비교한다. Classic음악수신 또는 I2S출력 시험만으로 이 송신 요구를 통과시키지 않는다.'}),
 (283,{'G':'위와 동일한 송신 연결/수집 설정, 외부 시험음원만 제거해 독립 초기화한다.','H':missing('마이크 TX mute/무음·동시Classic수신 영향 및실제 수집값.'),'I':'외부음원 없음 조건에서 Classic 수신 상대의 오디오/패킷과 연결 상태를 수집한다. 필요하면 별도로 QCC 음악수신을 병행하여 동시 동작 허용 계약과 비교한다.','J':'조건[2] 무음 및정의된 동시 송수신 조건. 입력 상태에 대응한 상대 수신값/송신 활성 여부가 확정 계약에 맞아야 한다. 경로·동시동작 근거 미제공 상태는 판정 대기.'}),
 (285,{'B':'SW-FW-SYS-Test-ID-83','C':'사용자 설명서의 안전·사용·문제 해결 정보','D':'I','E':'SW-FW-RID-83','F':missing('정식 상위 요구 추적 식별자 미제공.','E8300'),'G':'사용자에게 배포될 제품 설명서의 파일명/버전/언어/대상제품과검토 기준을 확인한다. 현재 제품 문서가 미제공이면대기한다.','H':missing('제품 사용설명서 현재본 및정식 상위 요구 추적.','E8300'),'I':'배포 설명서의 안전 예방조치·사용 방법·문제 해결 방법을 실제 본문에서 찾아 위치/내용/제품 적합성을 검토한다.','J':'조건[1] 설명서에 안전 예방조치, 사용 방법, 문제 해결 방법이 모두 포함되고 외부기 제품/사용 흐름과 맞아야 한다. 실제 문서 각 위치와 내용을 옆 결과에 기록한다. QCC SRS의 요구 문구만으로 설명서 내용 확인을 통과시키지 않는다.'})]:
 s['newRows'].append(row);s['heights'][row]=360 if row==282 else 300
 for c,v in vals.items():sy(f'{c}{row}',v)
for c in 'BCDEF':s['merges'].append(f'{c}282:{c}283')
s['groups']=[{'id':'SW-FW-SYS-Test-ID-82','rows':[282,283],'cases':2},{'id':'SW-FW-SYS-Test-ID-83','rows':[285,285],'cases':1}]

# Main-sheet data canonicalization: reports use exact criterion common values/rows/merges.
for kind,ix,end,rescol,vercol,font,widths,centers in [
 ('unit',0,'K','L','M','Calibri',{'B':5.375,'C':19.75,'D':26.625,'E':54.625,'F':5.25,'G':19.625,'H':36.625,'I':31.625,'J':56.625,'K':64.625,'L':64.625,'M':13.875},['B','D','F','G','M']),
 ('integration',1,'M','N','O','맑은 고딕',{'B':12,'C':27,'D':23,'E':34,'F':23,'G':25,'H':27,'I':6,'J':42,'K':32,'L':48,'M':64.625,'N':64.625,'O':13.875},['B','C','D','F','I','O']),
 ('system',2,'J','K','L','Calibri',{'B':27,'C':54.625,'D':6,'E':24,'F':24,'G':42,'H':32,'I':48,'J':64.625,'K':64.625,'L':13.875},['B','D','E','F','L'])]:
 plan=common[kind]
 ninsert=len(plan['newRows']) if kind=='unit' else 0
 finalEnd=(1734+ninsert if kind=='unit' else max(plan['newRows']) if kind=='integration' else 303)
 criterion=plan['base'].copy();criterion.update(plan['edits'])
 if kind in ['unit','integration']:
  criterion['C19']=pad('QCC 시험 도구/자료')
  criterion['D19']=pad('QCC 담당자 기준서의 시험 조건을 반영했다. QCC SDK/PSTool/nRF Connect/CRA 앱, 실제 QCC 로그와 전바이트 수집 도구를 확인한다. 실행 결과는 제공되지 않았다. '+missing('실제 SDK·소스/도구·회로/프로토콜 버전 및수집자료.'))
 for isreport in [False,True]:
  b=BASE[ix+3 if isreport else ix];key=['SUVC','SITC','SSTC'][ix] if not isreport else['SUVR','SITR','SSTR'][ix]
  edits=criterion.copy()
  if isreport:
   for rrow in plan['newRows']:
    inp=plan['edits'].get(f'{end}{rrow}','').strip().split('예상/판정:')[0].strip()
    edits[f'{rescol}{rrow}']=pad('미실행 / 실제 결과 미제공\n'+inp+'\n실제 입력/수신 전바이트·상태/시각: 미수집. 생성 예상값을 관찰값으로 기록하지 않았다.\n'+missing('이 조건의 실제 실행·수집 원문/계측 자료.','E8300' if kind=='system'and rrow==285 else'QCC/E8300' if kind=='integration' else'QCC'))
    edits[f'{vercol}{rrow}']=pad('미실행\n판정 대기')
   # Changed criteria preserve historical statements, and cannot inherit old passes.
   if kind=='system':
    reportcells=b['sheets'][1]['cells']
    changed=[80,83,184,185,190,199,200,201,202,206,207,208,209,210,211,212,213,214,222,223,224,227,228,229]
    for rr0 in changed:
     old=reportcells.get(f'K{rr0}','').strip()
     edits[f'K{rr0}']=pad('기존 E8300 관찰/작성자 확인 기록(과거 범위):\n'+old+'\n현재 추가/수정 기준의 실제 QCC 결과: 미실행/미수집.\n'+missing('옆 기준의 동일 입력·전량 값/시각·계산/보호 자료.','QCC/E8300' if rr0 in[80,83,184,185,190]else'QCC'))
    # Verdict cells are group merged anchors.
    for anchor in [78,82,184,190,199,206,210,212,222,227]:edits[f'L{anchor}']=pad('기존 관찰 보존\n현재 판정 대기')
  cover={'L3':pad('Rev.0-QCC 초안'),'F25':pad('QCC 요구/설계 입력 반영. 신규 QCC 결과는 미실행/판정 대기. 기존 E8300 관찰 범위 보존.'),'B25':pad('QCC 초안'),'D25':pad('2026-10-12')}
  # No historical signer/date is promoted into a new approval.
  for c in ['J12','M12','H14','J14','M14','H16','J16','M16']:cover[c]=''
  target=b['path'].replace('[E8300]','[E8300-QCC]')
  aux={};history={}
  if kind=='system':
   aux=BASE[2]['sheets'][-1]['cells'].copy()
   aux['A3']=pad('1.5세대 기록은 참고 이력이다. 기존 Sound1 QCC 인증/암호화의 작성자 확인은 과거 관찰 범위이며 이번 QCC 입력 추가 조건의 실행을 증명하지 않는다. 본문 최신 조건의 QCC 결과는 미실행/판정 대기이다. '+missing('실제 인증/암호화/시간·키 보호 자료.'))
   for ar in range(5,40):
    refs=aux.get(f'G{ar}','')
    updates=[]
    if 'SW-FW-RID-47\n'in refs:updates.append('무송수신180초 종료 및 무제어명령 감시/reset 정합성(본문SYS47).')
    if 'SW-FW-RID-52\n'in refs:updates.append('리모콘/매핑16byte키, AES-CMAC 인증과 SHA-256매핑키 생성 역할·벡터(본문SYS52).')
    if 'SW-FW-RID-71\n'in refs:updates.append('CRA 상태4종 및 인증 미완료30초 타임아웃(본문SYS71).')
    if 'SW-FW-RID-72\n'in refs:updates.append('CRA5회 연속 실패 후60초 연결 제한·카운터/reset/경계(본문SYS72).')
    if 'SW-FW-RID-76\n'in refs:updates.append('인증 전후 데이터 게이트, 생산키 설치/NVM암호화·생성키 역할(본문SYS76).')
    if updates:
     aux[f'I{ar}']=pad(aux.get(f'I{ar}','').strip()+'\n최신 QCC 입력 조건: '+' '.join(updates)+' 실제 같은 입력/전량 결과를 본문에서 비교한다.')
     aux[f'J{ar}']=pad(aux.get(f'J{ar}','').strip()+'\n최신 조건/수치는 본문 최신 기준과 같다. '+missing('해당 조건의 실제 데이터·시각·계산/보호 자료.'))
     if isreport:
      old=b['sheets'][-1]['cells'].get(f'K{ar}','').strip()
      aux[f'K{ar}']=pad('기존 관찰/작성자 확인 범위:\n'+old+'\n최신 QCC 추가 조건: 미실행/실제 자료 미수집. 본문 결과의 현재 판정 대기와 같다.')
      aux[f'L{ar}']=pad('기존 관찰 보존\n현재 판정 대기')
   aux['I15']=pad('CRA 완료 연결에서 마지막 BLE TX/RX시각 후180초 무송수신 종료를 계측한다. 제어 외 상태조회가 계속되는 독립 무명령 조건의 reset도 확인한다. 재접속 시 인증을 다시 확인한다.')
   aux['J15']=pad('무송수신3분(180초) 연결 해제. 기존 무제어명령 의도 및 reset정합성을 보존한다. 재접속에는 필요한 인증이 적용되어야 한다. '+missing('timer start/reset/허용오차·실제시각.','QCC/E8300'))
   aux['J10']=pad(aux['J10'].strip()+'\n실패/성공/진행중/타임아웃4종, 미완료30초 타임아웃; 실제 전체응답/시각과 허용오차를 비교한다.')
   aux['J11']=pad(aux['J11'].strip()+'\nCRA5회 연속 실패 후1분(60초) 연결 제한. 페어링 실패로 대체하지 않는다.')
   aux['G38']=pad(aux['G38'].strip()+'\nSW-FW-SYS-Test-ID-83\nSW-FW-RID-83\n정식 상위 추적 미제공')
   aux['J38']=pad(aux['J38'].strip()+'\n설명서의 안전 예방조치·사용 방법·문제 해결 방법은 본문SYS83에서 실제 파일 내용으로 확인한다. '+missing('제품 설명서 현재본 및상위 추적.','E8300'))
   if isreport:
    history['A2']=pad('시험 보조자료 / 기존 E8300 관찰 이력 및 본문 참조\n이번 QCC 추가/변경 조건의 현재 기준·결과·판정은 보고서 본문에서 같은 입력의 예상/실제값으로 비교한다. 변경된 조건의 아래 B/C/D는 최신 본문과 일치시켰다. 과거 관찰은 현재 QCC 실행 근거로 확대하지 않는다.')
    for hr in [80,83,184,185,190,199,200,201,202,206,207,208,209,210,211,212,213,214,222,223,224,227,228,229]:
     history[f'B{hr}']=edits.get(f'J{hr}',BASE[2]['sheets'][1]['cells'].get(f'J{hr}',''))
     history[f'C{hr}']=edits.get(f'K{hr}','')
     history[f'D{hr}']=pad('기존 관찰 보존\n현재 판정 대기')
  allbooks.append({'key':key,'kind':kind,'isReport':isreport,'target':target,'original':b['path'],'originalHash':b['sha256'],'sheet':b['sheets'][1]['name'],'commonStart':30 if kind=='unit'else 25 if kind=='integration'else 34,'commonEndRow':1726 if kind=='unit'else 130 if kind=='integration'else 281,'commonLast':end,'lastCol':vercol if isreport else end,'cells':edits,'cover':cover,'aux':aux,'history':history,'newRows':plan['newRows'],'newMerges':plan['merges'],'heights':plan['heights'],'insertAt':plan['at'],'insertCount':ninsert,'finalEnd':finalEnd,'font':font,'widths':{c:w for c,w in widths.items() if c<=(vercol if isreport else end)},'centerCols':[c for c in centers if c<=(vercol if isreport else end)],'previewStart':plan['newRows'][0],'previewEnd':min(plan['newRows'][0]+1,plan['newRows'][-1]),'groups':plan['groups'],'resultCol':rescol if isreport else None,'verdictCol':vercol if isreport else None})
(HERE/'payload.json').write_text(json.dumps({'books':allbooks},ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'books':[{'key':b['key'],'newRows':len(b['newRows']),'groups':len(b['groups'])}for b in allbooks]},ensure_ascii=True))
