import json,re,hashlib
from pathlib import Path
HERE=Path(__file__).resolve().parent
TASK=HERE.parents[2]
AUD=json.loads((HERE/'audit-current.json').read_text(encoding='utf-8'))
CFG={b['key']:b for b in json.loads((TASK/'builders/qcc_tests/payload.json').read_text(encoding='utf-8'))['books']}
PLAN={k:{'path':b['target'],'sheet':b['sheet'],'expected_hash':AUD['books'][k]['hash'],'sheets':{},'reasons':[]} for k,b in CFG.items()}
def pad(s):return '\n'+s.strip()+'\n'
def old(k,a,sheet=None):return AUD['books'][k]['all_cells'][sheet or CFG[k]['sheet']].get(a,'')
def put(k,a,v,reason,sheet=None):
 s=sheet or CFG[k]['sheet'];v=pad(v)
 if old(k,a,s)!=v:PLAN[k]['sheets'].setdefault(s,{})[a]=v;PLAN[k]['reasons'].append({'sheet':s,'cell':a,'reason':reason})
def pair(ck,a,v,reason,sheet=None):
 for k in [ck,{'SUVC':'SUVR','SITC':'SITR','SSTC':'SSTR'}[ck]]:put(k,a,v,reason,sheet)
def notice(k,a):
 return '\n'.join(line for line in old(k,a).strip().splitlines() if line.startswith('['))

# All old preparation fields are inspected. Preserve their exact inputs/actions in
# procedure; replace the field with actual preconditions and no execution claim.
for ck,prep,proc,unit,title,start,end in [('SUVC','H','J','G','E',30,1726),('SITC','J','L','F','E',25,130),('SSTC','G','I',None,'C',34,281)]:
 cells=AUD['books'][ck]['all_cells'][CFG[ck]['sheet']]
 for addr,text in cells.items():
  if not re.fullmatch(prep+r'\d+',addr) or not start<=int(addr[1:])<=end:continue
  r=int(addr[1:]);proca=proc+str(r)
  if not old(ck,proca).strip():continue # merged continuations are not extra tests
  label=old(ck,(unit or title)+str(r)).strip()
  if ck=='SUVC':
   method=old(ck,'F'+str(r)).strip()
   pre=f'{label}의 분석 대상 소스/빌드 설정과 대응 설계. 조건별 입력/출력 경계가 확인된 상태.' if method=='A' else f'{label}를 실행할 시험 이미지와 해당 입력/출력 경계, RTT 또는 메모리 관찰 수단. 다른 실행이 대상 입력/결과 버퍼를 변경하지 않는 독립 시험 구성.'
  elif ck=='SITC':pre=f'{old(ck,title+str(r)).strip()}에 필요한 시험 이미지와 연결 유닛, 입력/출력 관찰 경로. 조건별 초기 상태를 독립 복원할 수 있는 시험 구성.'
  else:
   # Required devices remain named in the preparation column; the exact physical
   # connections/settings move to the procedure unchanged.
   resources=[]
   for word in ['외부기','내부기','출력 측정용 지그','PBS 용액','오실로스코프','32채널 핀 헤더 내부기 테스트 지그','전원공급기','순음 발생기','음원 재생 장치','OD1 출력 수신기','리모콘','매핑 앱','QCC','RTT','디버거','충전기','시험 저장소','오류 재현 지그','사이버보안 규격']:
    if word in text:resources.append(word)
   pre='시험 구성: '+', '.join(resources)+'. 조건별 연결/설정/초기 상태를 독립 적용할 수 있는 구성.'
   if 'PDM' in text:pre+=' OD1 수신기는 차동 PDM 지원 또는 아날로그 변환 회로를 갖춘 구성.'
  pair(ck,addr,pre,'11: 사전 구성과 수행 행동 분리, 원래 입력/경계 보존')
  pair(ck,proca,'1. 사전 구성과 입력 설정\n'+text.strip()+'\n\n2. 수행/확인\n'+old(ck,proca).strip(),'11: 원래 준비의 실제 행동을 절차 앞에 이동')

unitnames={1:'초기화/설정',2:'시스템 상태/SPI',3:'장치 상태(Standalone/TWS)',4:'Bluetooth 상태/활성/장치명',5:'PIO',6:'Power',7:'NVM',8:'Bluetooth Classic',9:'BLE',10:'AUTH service',11:'BLE Data(GAIA)',12:'CRA 인증',13:'보안 toolbox',14:'보안 NVM'}
purposes=json.loads((HERE/'test-contract.json').read_text(encoding='utf-8'))['unit']
for i,g in enumerate(CFG['SUVC']['groups']):
 r=g['rows'][0]; e=old('SUVC','E'+str(r)).strip()
 e='\n'.join(line for line in e.splitlines() if not line.startswith('원문 '))
 e=e.replace('기존 E8300 시험 결과 범위를 유지한다.','').replace('기존E8300 결과의범위를유지한다.','')
 e=e.replace('SYS46/67/68','SW-FW-SYS-Test-ID-46(등록 내부기 연결)/67(기동 자극 억제)/68(종료 자극 억제)').replace('SYS46 경로','SW-FW-SYS-Test-ID-46(등록 내부기 연결) 경로').replace('SYS48/49','SW-FW-SYS-Test-ID-48(페어링 전 제어 차단)/49(보호 데이터 전송)')
 lines=e.splitlines();lines.insert(1,'목적: '+purposes[i]['purpose']+'.')
 if r==1733:lines[-1]='[QCC 자료 입력] 외부기 일련번호 NVM 동작의 정식 직접 요구 추적.'
 pair('SUVC','E'+str(r),'\n'.join(lines),'1/3/6/7: 원문 ID 제거, 기능 목적과 정상 추적 유지')
 ids=re.findall(r'SU-ID-QCC-(\d+)',old('SUVC','G'+str(r)))
 pair('SUVC','G'+str(r),'\n'.join(f'SU-ID-QCC-{int(n):03d}\n({unitnames[int(n)]})' for n in ids),'3/7: E8300 동종 필드처럼 실제 유닛 ID/이름 연결')
pair('SUVC','C1740','SI-ID-QCC-001\n시스템 관리\nSI-ID-QCC-003\nBluetooth','3: 활성화 주담당 유닛004와 보조008/009 모듈 대응')

# Concrete per-condition inputs/expectations and executable procedures. Unknown
# wire formats/crypto contracts stay explicit and cannot produce product PASS.
conditions={
1727:('초기 PIO 설정과 입출력/주변장치 매핑','1. 회로의 PIO 입력/출력/주변기능 및 출력 초기값을 설정 코드와 대조한다.\n2. 기동 호출 순서를 분석하고 Application 초기화 전 tdPio_preInit() 호출 및 PIO 초기화 로그를 확인한다.','tdPio_preInit()이 Application 초기화보다 먼저 실행되고 PIO 방향/출력 초기값이 설정과 같아야 한다. 확인 문자열: TD: TD: tdPio_init() / TD: PIO init end. 두 문자열 외 전체 QCC RTT 형식은 미확정.','PIO 방향/출력 초기값, preInit/Application 호출 순서와 전체 로그'),
1728:('shutdown_ind/inter_dev_conn_ind 각각 초기값0/1 및 0→1,1→0','1. 두 입력의 초기0/1 조건을 독립 적용하여 초기 알림을 확인한다.\n2. 다른 입력은 고정한 채 대상 입력을0→1→0으로 바꾸고 PIO 신호, TD_MSG_PIO_CHANGED 및 상태 로그의 대응을 확인한다.','초기 상태와 각 변화에 TD_MSG_PIO_CHANGED가 전달되고 장치 로그에 같은 이벤트가 나타나야 한다. 입력 하나의 변화로 다른 입력 상태가 바뀌지 않아야 한다.','두 PIO 신호, 초기/변화 이벤트와 상태 로그'),
1729:('배터리 특성 설정과 전압4.2/3.95/3.7/3.475/3.35/3.0 V','1. 배터리 특성의 최소/최대 전압과 충전 설정을 분석한다.\n2. 지정 전압을 독립 적용하고 ADC 주기 읽기, 변환값 및 저장 잔량을 비교한다.','전압[V]→잔량[%]: 4.2 이상→100,3.95→80,3.7→60,3.475→40,3.35→20,3.0→0. 주기 읽기와 변환/저장 경로가 연결되어야 한다. 주기 수치 및10% 허용오차의 상대오차/%p 의미는 미확정.','공급/ADC 전압, 읽기 시각, 변환/저장 잔량'),
1730:('충전 케이블 연결→분리, 동일 배터리 상태','1. 배터리 초기 상태와 충전 여부를 확인한다.\n2. 케이블을 연결하여 충전 상태/잔량 변화를 관찰한다.\n3. 분리하여 충전 연결 해제를 확인한다.','연결과 분리를 각각 인식하고 충전 중 잔량 증가를 보여야 한다. 연결됨/미연결/충전 중/비충전 상태는 서로 구분한다.','연결/분리 시각, 충전 상태 및 잔량 변화'),
1731:('외부 충전 Enable, 공급 전압4.2 V 이상/3.95/3.7/3.475/3.35/3.0 V','1. PPK2로 각 전압을 독립 공급하고 실제 공급/ADC 전압을 기록한다.\n2. 같은 입력의 잔량/배터리 상태와 동작 정지/저전압 진입을 비교한다.','전압[V]→잔량[%]/상태: 4.2 이상→100/Full,3.95→80,3.7→60,3.475→40,3.35→20,3.0→0/Empty. Empty에서는 모든 동작 정지/저전압 진입. 충전 중 deep sleep 금지 조건과 Empty 우선순위가 미확정이면 그 범위의 판정은 대기한다. 10%의 상대오차/%p 기준이 미확정인 수치 판정도 대기한다.','각 공급/ADC 전압과 잔량/상태, Empty 동작 및 충전 상태'),
1732:('shutdown_ind=0/1, 충전 중/비충전 상태 구분','1. 비충전 상태에서 shutdown_ind를0/1로 각각 적용하여 절전/깨움 상태와 SPI 가능 시점을 관찰한다.\n2. 충전 중 조건을 별도로 적용하여 충전 중 절전 금지와의 관계를 확인한다.','QCC 논리 조건0=저전압 진입,1=깨움. E8300 QCC_CTRL은 High=정상/Low=종료 요청이다. 논리 입력과 물리 핀의 반전/매핑 및 충전 중 동작이 확정되지 않은 부분은 판정 대기.','논리 입력/물리 핀, 충전 상태, 절전/깨움 및 마지막 SPI 시각'),
1733:('Application PS 키 TD_PS_KEY_EX_DEV_SERIAL_NUNM의 격리된 시험 일련번호','1. 일련번호 PS 위치/길이와 읽기/쓰기 인터페이스를 분석한다.\n2. 확인된 데이터 형식/길이의 공개 시험 번호를 적용하고 쓰기 전 값, 쓴 전량 값과 다시 읽은 전량 값을 비교한다.','키 매핑과 읽기/쓰기 인터페이스가 존재하고 읽기 전량 값이 같은 조건에서 쓴 시험 번호와 정확히 같아야 한다. 일련번호 형식/길이가 미확정이면 구체 시험 번호와 기대 전바이트를 확정할 때까지 판정 대기.','PS 위치/길이, 시험 번호의 쓰기/읽기 전량'),
1734:('초기 설정 완료 후 시스템 기동','1. SPI/I2S/PIO/NVM/보안 설정을 확인한다.\n2. Built-in library→scheduler→Power→Bluetooth→HW→PIO의 초기화와 외부기 Application 호출 순서를 분석한다.','초기 설정이 초기화 전에 끝나고 Application은 선행 시스템 부분의 초기화 후 수행되어야 한다. Application 이전 PIO preInit과 HW 초기화 중 PIO 설정을 구분한다. Battery/Charging/Power/Classic/BLE/충전케이스 통신 초기화가 동작 경로에 포함되어야 한다.','설정 완료 시점, 각 초기화/Application 호출 순서'),
1736:('다른 상태는 고정, shutdown_ind/inter_dev_conn_ind=0→1→0, 잔량40→80, 충전 분리→연결, Classic/BLE 연결→해제','1. 여섯 상태의 저장/조회 경로를 분석한다.\n2. 각 상태를 독립 변경하고 입력값/저장값/조회값과 상태 로그를 비교한다.','두 PIO의 조회값0→1→0, 잔량40→80, 충전 연결/분리 및 Classic/BLE 연결/해제가 실제 입력과 같고 다른 상태의 저장값에 혼선이 없어야 한다.','여섯 입력/저장/조회값 및 전체 상태 로그'),
1737:('생산 구성: A=Standalone1대, B=TWS2대1세트, C=TWS 구성의1대 단독 사용','1. NVM_DEV_TYPE과 생산 설정의 읽기/모드 적용 경로를 분석한다.\n2. 동작 중 모드가 변경되지 않는지 확인한다.\n3. A는L/R 오디오 모두/구분 없는 데이터, B는장치별L/R 및 위치별 데이터, C는한 장치 단독 사용을 확인한다.','A:1대에서L/R 오디오 모두 처리하고 데이터는 별도 위치 구분 없이 처리. B:2대가 각각L/R 오디오와 위치별 데이터를 처리. C:TWS 구성도1대 단독 사용 가능. 모드는 생산 시 결정되며 동작 중 바뀌지 않아야 한다. 정확한 모드 코드/좌우 배정/peer 전달·복구 계약은 미확정.','생산 모드/동작 중 유지, A/B/C별 오디오/데이터 및 peer 상태'),
1738:('inter_dev_conn_ind=0→1→0','1. 다른 상태를 고정한다.\n2. 입력을0→1→0으로 바꾸고 QCC 활성 상태, BLE/Classic 검색과 연결 상태를 확인한다.','0=Bluetooth 비활성,1=활성. 다시0이면 비활성으로 돌아가야 한다. 이 입력은 E8300 내부기 연결 표시이며 등록 신원/RF 검증 결과와 구분한다.','연결 표시, 활성 상태, 검색/연결 상태'),
1739:('연결 표시1, 수술위치L/R, 시험 사용자TEST01, 외부기 시험 식별번호','1. E8300 내부기 정보 요청과 응답을 확인한다.\n2. 같은 사용자/번호에서L/R만 바꿔 생성 장치명, 로그 및 실제 스캔값을 전량 비교한다.','장치명은TD_<L 또는 R>__SOUND_<UserName>_<ExternalDeviceInfo> 구성으로 위치/사용자/외부기 식별을 반영하고 생성값/로그/검색값이 같아야 한다. 구분자/공백 및 식별번호 직렬화가 미확정이면 기대 문자열 확정 전 판정 대기.','정보 요청/응답, L/R별 생성 이름·로그·스캔 전량'),
1740:('내부기 미부착→부착→탈착→재부착, nRFConnect 검색','1. 각 부착 상태에서 실제 연결 표시와 BLE 검색을 확인한다.\n2. 부착/재부착 때 생성 장치명과 검색 이름을 비교한다.','미부착/탈착은 미검색, 부착/재부착은 검색 가능. 검색 이름과 생성/로그 이름이 같아야 한다.','각 부착 상태의 PIO/광고/검색 이름'),
1741:('내부기 부착, BLE 연결 후1분 유지→탈착','1. nRFConnect와 BLE 연결한다.\n2. 연결을1분 유지한 후 탈착한다.\n3. 연결 표시, QCC 연결 상태와 해제 주체/시각을 확인한다.','탈착하면 BLE 연결이 해제되어야 한다. 1분은 탈착 전 유지 시간이며 해제 지연의 허용상한이 아니다.','유지 시작/탈착/해제 시각, 연결 상태 및 해제 주체'),
1742:('Classic 페어링 준비, 미부착→부착→탈착→재부착','1. Classic 페어링 가능 상태로 설정한다.\n2. 각 부착 상태에서 스마트폰 검색과 장치명/로그를 비교한다.','미부착/탈착은 미검색, 부착/재부착은 검색 가능. 스마트폰 이름과 생성/로그 이름이 같아야 한다.','각 부착 상태의 Classic 검색/장치명'),
1743:('내부기 부착, Classic 연결 후1분 유지→탈착','1. 스마트폰에서 검색/Classic 연결한다.\n2. 연결을1분 유지한 후 탈착한다.\n3. 프로파일 연결과 해제 주체/시각을 확인한다.','탈착하면 Classic 연결이 해제되어야 한다. 1분은 탈착 전 유지 시간이다.','유지 시작/탈착/해제 시각, 프로파일 상태 및 해제 주체'),
1744:('부착/양이peer 구성, 현재잔량37→72%, peer72→37%','1. 각 잔량 쌍을 독립 적용한다.\n2. 생성 Advertising/ScanResponse와 nRFConnect 수신 전량 바이트/장치 로그를 비교한다.','Advertising은장치명/시리얼번호/Flags, ScanResponse는Appearance/Flags 및현재/peer잔량·충전상태를 포함해야 한다. 현재/peer 잔량쌍(37,72)→(72,37)이 갱신되어야 한다. UUID/AD형식/필드 위치가 미확정이면 기대 전바이트를 확정하기 전 판정 대기.','잔량쌍, Advertising/ScanResponse 생성/수신 전바이트 및 전체 로그'),
1745:('내부기 부착, nRFConnect BLE 연결→해제','1. BLE 연결을 성립시킨다.\n2. central에서 해제한다.\n3. 각 변경의 QCC 저장/조회 상태와 전체 로그를 확인한다.','연결/해제마다 해당 상태를 저장/조회하고 장치 로그가 실제 상태에 대응해야 한다.','연결/해제 시각, 저장/조회 상태 및 전체 로그'),
1746:('내부기 부착/BLE 연결, Pair 요청19 04 02 01 80 80','1. 페어링 요청 전 스마트폰 Classic 검색 상태를 확인한다.\n2. nRFConnect에서 요청6byte를 전송한다.\n3. 응답 전량과 Pairing 처리/스마트폰 검색을 확인한다.','응답6byte(hex)=19 04 04 01 80 01. 요청을Pairing 명령으로 처리하고 요청 후 스마트폰에서 검색 가능해야 한다.','요청/응답6byte, Pairing 처리와 전후 검색 상태'),
1747:('스마트폰 Classic 연결→해제','1. 스마트폰에서 외부기를 검색하여 연결한다.\n2. 연결을 해제하고 QCC 저장/조회 상태와 전체 로그를 확인한다.','Classic 연결/해제마다 저장/조회 상태와 로그가 실제 상태를 반영해야 한다.','프로파일 연결/해제와 저장/조회 상태·전체 로그'),
1748:('Classic 시험 스피커 출력 구성, 스마트폰 음악 재생','1. 시험 스피커 경로로 Classic 연결한다.\n2. 식별 가능한 시험 음악을 재생하고 오디오 연결 상태/로그 및 청취 동작을 확인한다.','오디오 연결을 인식하고 시험 스피커에서 입력 음악에 대응하는 소리를 들어야 한다. 이 시험 스피커 경로는 제품 전극 자극 출력의 판정과 구분한다.','오디오 연결 상태/로그와 시험 스피커 청취'),
1749:('시험 스피커 구성2대, TWS 양이 설정, 같은 스마트폰 음악','1. 양이2대의 위치/peer 연결을 확인한다.\n2. 음악을 재생하여 양쪽 스피커 청취와 개별 오디오 상태를 확인한다.','두 외부기의 시험 스피커에서 입력 음악에 대응하는 소리를 들어야 한다. 양이 위치/peer 라우팅은 설정과 같아야 한다.','두 장치의 peer/오디오 상태 및 개별 청취'),
1750:('Classic 수신/I2S 하향 구성, 스마트폰 시험 음악, I2S OFF→ON→OFF','1. Classic 오디오 연결한다.\n2. I2S OFF/ON/OFF를 구분하여24bit/16kHz 프레임과 유효 데이터를 계측한다.\n3. E8300 수신 버퍼와 I2S_FLAG를 비교한다.','I2S 활성 로그와 시험 스피커 무음 외에 QCC I2S24bit/16kHz 유효 데이터와 E8300 수신이 확인되어야 한다. DIO29 Low=유효. 전극 자극/혼합은 SW-FW-SYS-Test-ID-69(Classic 수신 자극) 및 SW-FW-IntegrationTest-ID-013(I2S/마이크 혼합)의 별도 범위. DMIC2→QCC 통화 상향 송신과 구분한다.','Classic 연결, BCLK/LRCLK/샘플 전량, DIO29 및 E8300 수신 버퍼'),
1751:('내부기 부착/BLE 연결 후 AUTH 서비스 탐색','1. AUTH 정의/등록 경로와 tdGattAuthServer_init() 호출을 분석한다.\n2. nRFConnect에서 서비스/characteristic을 탐색해 정의와 비교한다.','AUTH 서비스 등록과초기화가 실행되어야 한다. Challenge/Confirm=write, Response/Status=read,notify를 정의와 탐색에서 확인. pairing 완료 후 AUTH 접근을 구분한다. UUID와 세부 권한 미확정 부분은 판정 대기.','AUTH 등록/초기화, 서비스/characteristic 속성·권한'),
1752:('Challenge00..0B,Response10..1B,Confirm20..2B 각12byte 및Status30','1. AUTH 중계 시험 경계를 적용한다.\n2. Challenge/Response/Confirm/Status를 지정 방향으로 각각 독립 교환한다.\n3. 실제 수신 길이/순서/전바이트를 비교한다.','Challenge central→QCC12byte=00 01 02 03 04 05 06 07 08 09 0A 0B\nResponse QCC→central12byte=10 11 12 13 14 15 16 17 18 19 1A 1B\nConfirm central→QCC12byte=20 21 22 23 24 25 26 27 28 29 2A 2B\nStatus QCC→central1byte=30\n각 방향/길이/전량 값 일치. 중계 시험의12byte/Status30은 실제CRA의16byte nonce/상태코드0..3과 별개 시험 패턴이다.','네 교환의 실제 방향/길이/전량 바이트'),
1755:('내부기 부착/CRA앱 BLE 연결, 별도nRFConnect 검색; 앱 해제와내부기 탈착','1. CRA앱과 연결한 상태에서 nRFConnect 검색한다.\n2. 앱 연결 해제 후 내부기를 탈착하고 다시 검색한다.','앱 연결 중 미검색이고 해제 후 탈착 상태에서도 미검색이어야 한다. 1:1/검색 조건이며 인증 계산 성공과 구분한다.','앱 연결/해제, 부착 상태 및 검색 결과'),
1756:('CRA 인증 완료 없는 BLE 연결, timeout30초','1. 인증 타임아웃을30초로 설정하고 내부기를 부착한다.\n2. nRFConnect 연결 후 CRA를 완료하지 않는다.\n3. 연결/타이머 시작/Status/해제 시각을 같은 시간축으로 측정한다.','CRA 미완료 상태가30초에타임아웃되어 BLE 해제되어야 한다. 시작/reset/허용오차 미확정 범위는 판정 대기. 180초 무송수신/60초연결제한과 구분한다.','연결/타이머 시작/타임아웃/해제 시각과Status'),
1757:('리모콘auth_type1/매핑2, 각시험키00×16, timeout30초','1. 두 앱을 독립 연결한다.\n2. NonceA/B와계산 응답/Confirm/Status를 실제 인증 순서에 따라 대조한다.\n3. 인증 완료 후30초 경과까지 연결 유지, 탈착 후 미검색을 확인한다.','리모콘/매핑 각각 정의된CRA가 성공하고 Status1, 진행 중Status2를 구분해야 한다. 30초 이후 연결 유지/탈착 후 미검색. 앱별 키역할/직렬화·독립 계산 벡터가 미확정이면 암호 계산 판정 대기. 연결 유지 자체만으로 계산 통과를 판정하지 않는다.','앱유형/전체 인증값·Status, 인증완료/연결유지/탈착/검색'),
1758:('QCC시험키00×16 유지, client만01×16; 타입1리모콘/2매핑','1. 정상 공개 시험키를 QCC에 유지한다.\n2. 클라이언트 한쪽 키만01×16으로 바꿔 앱별 독립 연결한다.\n3. 인증 실패/Status/앱의 해제 및 반환 데이터를 확인한다.','인증 실패이며 앱이 연결을 해제하고 앱에서 추가 인증 성공 결과를 보내지 않아야 한다. 실제 키역할/변경 대상 확인이 미확정이면 실행/판정 대기. 양쪽 키를같게바꾼 조건과 구분한다.','양쪽 시험키 역할/타입, 실패Status/반환값과해제 주체'),
1759:('시험키00×16, auth_type1=매핑/2=리모콘의 타입불일치','1. 공개 시험키를 정상 조건으로 복원한다.\n2. 앱유형과 auth_type을 반대로 설정하여 각각 연결한다.\n3. QCC의 실패Status와 연결 해제를 확인한다.','타입 불일치가 인증 실패로 처리되고 QCC가 연결을 해제해야 한다. Status0(실패)만 알리며 인증 비밀을 노출하지 않아야 한다.','실제 앱유형/auth_type, 실패Status 및 QCC해제'),
1760:('리모콘auth_type1, 공개 시험shared_service_key00×16','1. 보안 NVM에서리모콘키를 읽는 경로를 확인한다.\n2. 확인된Challenge/Nonce직렬화의 독립AES-CMAC예상값과 실제 전바이트를 비교한다.','TD_PS_KEY_AUTH_SHARED_SERVICE_KEY의16byte키를AES-CMAC인증계산에 사용하고 CRA가정의된순서로 진행되어야 한다. 실제nonce/계산입력·독립CMAC벡터가 미확정이면 구체 계산값 판정 대기.','NVM키 역할/길이, nonce/계산입력 및 전체CMAC/인증상태'),
1761:('매핑auth_type2, 공개 clinician_master_key00×16와시험serial_num, timeout30초','1. NVM의clinician_master_key와일련번호입력을 확인한다.\n2. SHA-256전체digest→앞16byte clinic_service_key→AES-CMAC의계산경계를 독립예상벡터와 비교한다.','clinic_service_key=[0:15] of SHA256(clinician_master_key,serial_num). 인증값계산은AES-CMAC으로 구분한다. 키/일련번호 직렬화와SRS 알고리즘역할 정합성이 미확정이면 digest/CMAC의구체 계산판정 대기.','전체입력 직렬화,32byte digest/16byte파생키 및 전체CMAC/인증상태'),
1762:('NVM시험RAND10byte,BD_ADDR6byte,암호화된shared_service_key의평문00×16','1. 공개시험 RAND/BD_ADDR와확정암호문을 확인한다.\n2. enc_key=BD_ADDR||RAND의16byte도출경계와AES-128복호화를 독립벡터와 비교한다.','복호화 결과16byte=00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00. BD_ADDR직렬화/AES모드·IV/암호문이 미확정이면 계산판정 대기.','RAND/BD_ADDR 직렬화, 암호문/도출키 및16byte복호화값'),
1763:('격리 시험PS_USR40/43/44와보안NVM접근 인터페이스','1. PSTool에서영역/길이와암호화제조값 존재를 확인한다.\n2. 소스의읽기인터페이스와쓰기인터페이스부재를 분석한다.','PS_USR40=RAND10byte,43=clinician_master_key16byte,44=shared_service_key16byte. 펌웨어보안NVM접근은읽기만지원. 생산설치키와파생clinic_service_key를구분한다. 펌웨어읽기전용이 PSTool/디버거접근차단을뜻하지 않는다.','PS위치/길이/값존재, 인터페이스 및 저장대상/접근권한'),
1764:('리모콘auth_type1,시험키00×16,시험RAND/BD_ADDR와PS저장암호문','1. 저장암호문을읽고확정도출/AES계약에따라복호화한다.\n2. 독립암호문/평문벡터와 전량비교한다.\n3. PSTool저장값과복호화값의 차이를 확인한다.','복호화평문16byte=00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00. 저장암호문은평문과달라야 하며그차이만으로AES모드/보호완료를판정하지않는다. 실제암호문/직렬화/독립벡터미확정 범위는판정대기.','PS저장암호문, RAND/주소/도출경계 및16byte평문')}
# Keep the existing entire byte vectors for SPI and GAIA; normalize presentation
# without changing encoding or fabricating unresolved fields.
for r in [1735,1753,1754]:
 k=old('SUVC','K'+str(r)).strip().replace('원문 shutdown_ind','shutdown_ind').replace('원문 편이 정의','편이 정의').replace('원문 1=Left/0=Right 및 반대 문장','양이1=Left/0=Right와0=Left/1=Right의정의').replace('실제QCC','실제 QCC').replace('원문','')
 k=k.replace('예상/판정:','통과 기준:')
 pair('SUVC','K'+str(r),k,'13/14: 전체 기존 예상 바이트/RTT 보존, 실제 wire 계약 미확정 유지')
 inputs=k.split('입력: ',1)[1].split('\n',1)[0]
 proc=old('SUVC','J'+str(r)).strip().split('\n각 입력',1)[0]
 conditions[r]=(inputs,proc,None,'입력/수신 전량 바이트, 저장 상태/전체 RTT')
pair('SUVC','K1735',old('SUVC','K1735').replace('원문 shutdown_ind','shutdown_ind'),'1/13/14: 출처 수식어만 제거, 전체 바이트/RTT 및 전송 경계 불변')
for r,(inp,proc,expected,observation) in sorted(conditions.items()):
 group=next(g for g in CFG['SUVC']['groups'] if g['rows'][0]<=r<=g['rows'][1]);num=r-group['rows'][0]+1
 pre='QCC 시험 이미지/버전과 해당 유닛의 입력/출력 관찰 경계. 각 조건을 독립 적용할 수 있는 시험 구성.'
 if r in [1727,1734,1736,1737,1751,1763]:pre+=' 설정/초기화 또는 접근 인터페이스 분석 대상 소스와 설계.'
 if 1739<=r<=1755:pre+=' nRFConnect 또는 Classic 상대/스캔 도구와 연결 상태 관찰 경로.'
 if r>=1756:pre+=' 리모콘/매핑 시험 앱과 격리된 공개 시험키/시각 관찰 구성.'
 if r in [1729,1731]:pre+=' PPK2와 공급/ADC 전압 관찰 경로.'
 pair('SUVC','H'+str(r),pre,'11: 실제 입력/실행은 절차, 준비는 사전 구성')
 pair('SUVC','J'+str(r),'입력: '+inp+'\n'+proc,'11/13: 동일 조건의 구체 입력과 순서')
 if expected is not None:pair('SUVC','K'+str(r),f'조건 [{num}]\n입력: {inp}\n통과 기준: {expected}','6/13/14: 실제 기능 기대값/미확정 범위')
 limit=notice('SUVC','I'+str(r))
 if r>=1756:limit='생산 비밀키 대신 격리된 공개 시험키를 사용하는 조건.\n'+limit
 pair('SUVC','I'+str(r),limit or '이 시험은 지정된 입력 경계/동작 범위의 확인이다. 다른 단계의 검증으로 확대하지 않는다.','11/18: 조건별 제한, 불필요한 공통 키/로그 지시 제거')
 put('SUVR','L'+str(r),f'검증 [{num}]\n입력: {inp}\n실행 상태: 미실행.\n관찰: {observation} 미수집/미제공.\n[QCC 자료 입력] 이 조건의 실제 실행 결과와 관찰 근거.','15/16/18: 검증번호 및 실제 결과 대기, 작성 지시 제거')

# Newly learned facts are stated as function criteria; notices still retained.
for r,msg in {
1727:'PIO 핀 번호/실제 방향·출력 초기값과 전체 QCC RTT 형식.',
1731:'10% 허용오차의 상대오차/%p 의미 및 충전 중 Empty의 절전 우선순위.',
1732:'shutdown_ind 논리값/실제 핀 매핑과 충전 중 deep sleep 계약.',
1733:'일련번호 PS 데이터 형식/길이와 정식 직접 요구 추적.',
1737:'Standalone/TWS 설계와 A/B/C 기대 동작은 반영함. 모드 코드/실제 좌우 배정/peer 전달·복구 및 실측은 미비.',
1739:'장치명 구분자/공백 및 시험 식별번호 형식, L/R별 전체 문자열.',
1745:'37/72%는 재현 입력으로 정의함. 실제 BLE 연결/해제 및 전체 QCC 로그는 미제공.',
1755:'1:1 연결/검색 동작의 실제 관찰 자료. CRA 성공은 별도 계산/인증 조건.',
1756:'30초 타이머 시작/reset/허용오차와 실제 Status/해제 시각.',
1757:'진행 중2/성공1/실패0/타임아웃3의 상태 정의는 반영함. 실제 전체 인증값/로그와 계산 벡터는 미비.',
1758:'불일치키 시험의 앱별 키 역할/변경 대상과 실제 실패/해제 결과.',
1759:'타입 불일치의 실제 인증 상태/실패Status 및 QCC 해제 결과.',
1762:'RAND/BD_ADDR 직렬화와 AES-128 모드/IV, 독립 도출/암호문 벡터.'}.items():
 pair('SUVC','I'+str(r),('생산 비밀키 대신 격리된 공개 시험키를 사용하는 조건.\n' if r>=1756 else '')+'[QCC 자료 입력] '+msg,'22: 접두어 유지, 반영된 설계와 실제 남은 자료 구분')
pair('SUVC','F1736','A/T','9/10: 여섯 상태 저장/조회 수치의 정량 비교')
pair('SUVC','F1737','A/I/D','9/10: 생산 모드 분석/검사와 오디오·데이터 기능 시연')

# Integration: same fixed expected byte vectors, distinct transport/security paths.
for r in range(131,145):
 inp=old('SITC','M'+str(r)).split('입력: ',1)[1].split('\n',1)[0]
 pair('SITC','J'+str(r),'QCC 시험 이미지/버전, 대상 모듈/연결 상대, 입력/출력 관찰 경계. 각 조건의 연결/상태를 독립 적용할 수 있는 시험 구성.','11: 통합 준비/절차 분리')
 proc=old('SITC','L'+str(r)).strip().split('\nQCC 입력/',1)[0]
 pair('SITC','L'+str(r),'1. 해당 조건의 초기 상태/연결을 복원한다.\n2. 입력: '+inp+'\n3. '+proc,'11/13: 실제 연결/입력/관찰 수행 순서')
 criterion=old('SITC','M'+str(r)).strip().replace('예상/판정:','통과 기준:').replace('원문','')
 pair('SITC','M'+str(r),criterion,'1/13: 병합 과정 표현 제거, 기능/전체 예상값 보존')
 group=next(g for g in CFG['SITC']['groups'] if g['rows'][0]<=r<=g['rows'][1]);num=r-group['rows'][0]+1
 observations={131:'논리20byte/물리21byte 전량 송수신, E8300 상태/ACK/전체 RTT',132:'논리20byte/물리21byte 전량 송수신, E8300 상태/ACK/전체 RTT',133:'충전4상태와0/1 변환',134:'부착/연결 표시, 광고/프로파일 연결 및해제',135:'물리핀/논리값, 절전/마지막 SPI와 시각',136:'네 AUTH 교환의 방향/길이/전체바이트',137:'전체 인증값/Status와 일반제어 차단/허용',138:'연결/타이머 시작/인증 완료/Status/해제 시각',139:'마지막TX/RX/조회/해제 시각과reset',140:'실패1..5횟수,6회/제한/60초경계 시각과허용',141:'Classic/I2S 프레임/배열, DIO29/CFX버퍼 및지그 출력',142:'편이 전체 요청/응답과저장위치/잔량',143:'양이 전체 요청/응답과위치별routing/잔량',144:'NVM전량/키도출/복호화/digest/CMAC'}
 put('SITR','N'+str(r),f'검증 [{num}]\n입력: {inp}\n실행 상태: 미실행.\n관찰: {observations[r]} 미수집/미제공.\n'+('[QCC/E8300 자료 입력]' if r in [131,132,133,134,135,137,138,139,140,141] else '[QCC 자료 입력]')+' 이 조건의 실제 실행 결과와 관찰 근거.','15/16/18: 현재 미실행 관찰과 검증번호')
pair('SITC','M133','조건 [3]\n입력: 미연결/연결/충전 중/비충전4상태를 각각 발생\n통과 기준: 네 상태가 확정 변환표에 따라0x34의charger_connected0/1로 변환되어야 한다. 상태 이름은 확정되어 있으며 enum코드와0/1 변환/실제 전바이트는 미확정이므로 판정 대기.','6/22: 알려진4상태 이름 반영, 미확정 변환만 대기')
e=old('SITC','E134').strip().replace('기존E8300 결과의범위를유지한다.','').replace('SYS46/67/68','SW-FW-SYS-Test-ID-46(등록 내부기 연결)/67(기동 자극 억제)/68(종료 자극 억제)')
pair('SITC','E134',e,'3/18: QCC 입력 지원과 실제 E8300 기능 시험 범위/이름')
pair('SITC','D134','SI-ID-QCC-001\nSI-ID-QCC-002\nSI-ID-QCC-003','3: PIO005/Power006의 하드웨어 인터페이스 모듈 포함')
pair('SITC','B136','QCC','5: AUTH central↔QCC 경로, E8300 경로 아님')
pair('SITC','G136','QCC 외부기와 BLE central/중계 시험 경계','5: 인증 중계 실제 장치 경계')
pair('SITC','H136','AUTH 서비스와 BLE central 교환','5: E8300 인증 실행으로 확대하지 않음')

# System call uplink: same requirement69/upperFW15, separately identified test82.
pair('SSTC','E282','SW-FW-RID-69','1/3: 기존 Classic 오디오 요구로 통합')
pair('SSTC','F282','SYS-RID-SP-FW-15','1/3: 실제 Classic 오디오 상위 추적')
pair('SSTC','C282','DMIC2 통화 오디오의 Bluetooth Classic 송신','5: 실제 통화 상향 경로')
pair('SSTC','G282','DMIC2가 E8300/QCC 양쪽에 연결된 외부기, QCC 통화 송신 시험 이미지와 Bluetooth Classic 통화 상대, 수신 녹음/관찰 수단. 독립1000Hz/무음 조건의 시험 구성.','5/11: 확인된 DMIC2 연결과 사전 구성')
pair('SSTC','H282','[QCC/E8300 자료 입력] DMIC2의 E8300/QCC 양쪽 연결과 QCC 통화 수음/송신 경로는 반영함. QCC 핀/클록, 통화 프로파일/코덱, 두 경로의 동시 동작 조건과 실제 수집값은 미비. 정량 품질/지연 허용범위는 미확정.','22: 접두어 유지, 사용자 확인 경로와 실제 미비 구분')
pair('SSTC','I282','1. Classic 통화 연결과 DMIC2 입력 경로를 확인한다.\n2. DMIC2에1000Hz 시험음원을 입력한다.\n3. 통화 상대의 수신 오디오/데이터와 송신 연결 상태를 관찰하고 독립 무음 조건과 비교한다.\n4. DMIC2→E8300 입력 및 QCC→E8300 I2S 하향을 별도로 구분한다.','5/11: 통화 상향 절차, 별도 기존 입력/하향 경로')
pair('SSTC','J282','조건 [1]\n입력: DMIC2에1000Hz 시험음원, Classic 통화 연결.\n통과 기준: QCC가 DMIC2를 수음하여 통화 상대에게 송신하고 상대에서 입력에 대응하는 오디오를 확인해야 한다. 정량 허용범위를 새로 만들지 않고 기능 시연(D)으로 확인한다. 기존 SW-FW-SYS-Test-ID-69(Classic 수신/자극)의 결과는 이 송신 조건의 증거로 확대하지 않는다.','9/13/18: 정성 기능 시연과 수신 시험 범위 구분')
pair('SSTC','G283','같은 DMIC2 통화 송신/상대 수신 관찰 구성. 외부 시험음원 없는 독립 조건.','11: 무음 사전 구성')
pair('SSTC','I283','1. 외부 시험음원 없는 상태에서 같은 Classic 통화를 연결한다.\n2. 상대 수신 오디오/패킷과 송신 활성 상태를 관찰한다.\n3. 동시 동작을 검증할 경우 별도 QCC→E8300 I2S 하향을 적용하고 확인된 동시 동작 계약과 비교한다.','5/11: 무음과 별도 동시 하향 경로')
pair('SSTC','J283','조건 [2]\n입력: DMIC2 외부 시험음원 없음, 같은 Classic 통화 연결.\n통과 기준: 상대 수신 오디오와 송신 상태가 입력 무음/정의된 mute 동작에 대응해야 한다. 통화 상향과 QCC→E8300 I2S 하향의 동시 허용/영향은 별도 확정 계약에 맞아야 한다. mute/동시 계약 미확정 범위는 판정 대기.','5/13: 무음/mute 동작과 미확정 동시 조건')
for r,obs in [(282,'DMIC2 입력/통화 상향/상대 수신 오디오와 연결 상태'),(283,'무음 입력/상대 수신/mute/별도 동시 하향 영향')]:
 put('SSTR','K'+str(r),f'검증 [{r-281}]\n입력: '+('DMIC2에1000Hz 시험음원' if r==282 else 'DMIC2 외부 시험음원 없음')+' 및Classic 통화 연결.\n실행 상태: 미실행.\n관찰: '+obs+' 미수집/미제공.\n[QCC/E8300 자료 입력] 실제 통화 송신/수신 관찰 근거와 적용 프로파일/코덱/동시 동작 조건.','15/16/18: 송신 결과 미실행 유지')
pair('SSTC','G285','사용자 배포용 외부기 설명서의 현재 파일/버전/언어/대상제품. 안전/사용/문제 해결 정보를 검토할 제품 사용 흐름과 검토 기준.','11: 설명서 사전 준비')
pair('SSTC','F285','SYS-RID-ACDOC-17\nSYS-RID-ACDOC-18\nSYS-RID-ACDOC-20','1/3: 매트릭스 실제 설명서 상위 추적 반영')
pair('SSTC','H285','[E8300 보완] 제품 사용 설명서의 실제 해당 절/내용, 배포 대상 및 검토 결과. 상위 ACDOC-17/18/20 추적은 반영함.','22: 상위 미등록 해소, 설명서 실제 검토 대기')
pair('SSTC','J285','조건 [1]\n입력: 사용자에게 배포될 외부기 설명서 현재본.\n통과 기준: 경고/안전사항, 안전한 작동법, 자세한 동작방법이 포함되고 안전 예방조치/사용/문제 해결 방법이 대상 제품의 실제 사용 흐름과 맞아야 한다. 각 내용의 실제 위치/본문과 제품 적합성을 확인한다.','13/16: 매트릭스 기본/상세 설명서 기능 기준')
put('SSTR','K285','검증 [1]\n실행 상태: 미실행.\n관찰: 사용자 배포 설명서 현재본/버전과 경고/안전사항/작동법/동작방법 및 문제 해결의 실제 내용/위치 미제공.\n[E8300 보완] 제품 설명서 현재본과 실제 검토 결과.','15/16: 실제 설명서 확인 대기')

# QCC complete output formats are genuinely missing; retain the existing Q tag
# while naming that remaining evidence at the condition itself.
for r in range(1727,1765):
 v=PLAN['SUVC']['sheets'][CFG['SUVC']['sheet']].get('I'+str(r),old('SUVC','I'+str(r))).strip()
 if '전체 QCC RTT' not in v:v+='\n[QCC 자료 입력] 이 조건의 전체 QCC RTT 출력 형식/문자열과 실제 실행 결과.'
 pair('SUVC','I'+str(r),v,'14/22: 없는 전체 QCC 출력은 추정하지 않고 해당 조건에 표시')

# Cybersecurity auxiliary trace shares the same now-confirmed manual requirement.
for ck in ['SSTC','SSTR']:
 s='사이버보안 시험 연계'
 v=old(ck,'G38',s).replace('정식 상위 추적 미제공','SYS-RID-ACDOC-17\nSYS-RID-ACDOC-18\nSYS-RID-ACDOC-20')
 put(ck,'G38',v,'3: SYS83 실제 상위 추적과 보조 연결 일치',s)

out={'books':PLAN,'baseline':'review-baseline.json','source_mapping_task_only':str(HERE/'source-read.json'),'product_execution':'새 제품 실행/수집/통과 생성 없음','all_old_prep_actions_preserved_in_procedure':True}
(HERE/'changes.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:{'cells':sum(map(len,b['sheets'].values())),'sheets':list(b['sheets'])} for k,b in PLAN.items()},ensure_ascii=True))
