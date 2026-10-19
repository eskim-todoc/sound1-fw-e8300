import argparse,json,re,shutil
from pathlib import Path
from lxml import etree as E
from editor import Editor,ROOT,NS,q,visible,sha
from content import UNITS,MODULES,SCOPE,AUDIO,AUDIO_GAP,BOUNDARY,test_description,trace_description

def pp(e,text,level):return e.p(text,level=level)
def missing(e,text,level=2):return e.p(text,level=level,missing=True)
def source_correspondence():
    old=json.loads((ROOT.parents[2]/'evidence/qcc_design.json').read_text(encoding='utf-8'))
    return [{'id':u['id'],'original_id':u['original_id'],'title':u['title'],'source':u['source'],'source_corrections_local_only':u['missing_or_conflicting']}for u in old['units']]

def author_sad(commit=False):
    e=Editor('SAD')
    e.figure(1,ROOT/'figures/시스템 블록도.png','DMIC2 공통 입력과 QCC 통화 상향 경로')
    e.figure(2,ROOT/'figures/소프트웨어 구조도.png','E8300 코어와 QCC 4모듈/14유닛의 기능 경계')
    e.replace(54,'인공달팽이관 시스템 전체의 하드웨어 플랫폼은 아래 <그림 1.>과 같다. 외부장치 Sound1의 소프트웨어는 E8300과 QCC5181에서 동작한다. DMIC1/DMIC2는 E8300에 연결되고 DMIC2는 QCC에도 연결되어 전화 통화 상향 입력으로 사용한다.',level=1)
    e.insert_after(e.block(71),[missing(e,'[QCC 자료 입력] QCC 4모듈/14유닛의 실제 소스/빌드/라이브러리 설정과 실행/측정 결과를 확인해야 한다. DMIC2의 두 칩 입력 연결 사실과 구현/시험 근거의 확인 상태는 구분한다.',1)])
    e.insert_after(e.block(117),[pp(e,SCOPE,2),e.cap('장치와 기능의 경계',2),e.table(BOUNDARY,level=2,widths=[1700,3100,4416]),e.blank(2)])
    e.replace_range(136,139,[],'중복 QCC 내부/원문 병합 설명 삭제; 실제 기능 경계는 QCC 아키텍쳐 서론에 배치')
    part=[]
    for m in MODULES:
        part.extend([e.p(m['id']+' '+m['title'],level=3,heading=True),pp(e,m['purpose'],3),pp(e,'소프트웨어 항목은 다음과 같다.',3)])
        for n in m['units']:
            u=UNITS[n-1]
            part.extend([e.p(u['title'].replace(' 유닛',' 항목'),level=4,heading=True),pp(e,u['id']+'은 '+u['purpose']+' '+test_description(u),4)])
    e.replace_range(140,151,part,'4모듈과 14개 기능 항목: 목적/동작 및 실제 유닛/시험 ID 기능 설명')
    e.replace(155,AUDIO,level=2)
    e.replace(156,'[QCC 자료 입력] QCC shutdown_ind의 정상/종료 논리와 실제 PIO 번호/전압/반전, 기동/종료 대기 및 I2S enable→I2S_FLAG Low 유효의 물리 대응을 확인해야 한다.',level=2)
    e.replace(157,'[QCC 자료 입력] 충전 4상태를 0x34 charger_connected 0/1로 변환하는 표, 0x34~0x36의 통보 시점/오류 처리, 실제 QCC 송신 length 필드/21byte 변환 및 GAIA 주소/type별 데이터 정의와 실행 확인이 필요하다.',level=2)
    e.replace(158,AUDIO_GAP,level=2)
    e.replace(159,'시스템 관리는 PIO/전원/장치 정보를 수집하여 Bluetooth 활성과 SPI 중계를 조정한다. AUTH service는 보안 처리의 Challenge/Response/Confirm/Status를 전달하며 일반 BLE Data는 인증/권한 조건을 만족한 메시지를 시스템 관리에 전달한다. 보안 처리는 toolbox와 보안 NVM을 사용하고 보안 NVM은 HW NVM을 통해 저장 영역에 접근한다. Classic 수신 오디오는 I2S로 E8300에 전달하고 전화 통화 상향은 DMIC2→QCC→통화 상대 경로로 처리한다.',level=2)
    # Preserve E8300 entries in the requirement-centric table and append all applicable QCC IDs.
    t=e.block(171);selected={}
    for u in UNITS:
        for rid in u['rids']:selected.setdefault(rid,[]).append(u)
    for row in t.findall('w:tr',NS):
        if row.find('w:trPr/w:del',NS)is not None:continue
        cells=row.findall('w:tc',NS)
        if len(cells)!=3:continue
        match=re.fullmatch(r'SW-FW-RID-(\d+)',visible(cells[0]).strip())
        if not match:continue
        rid=int(match.group(1))
        if rid==82:
            pr=row.find('w:trPr',NS)
            if pr is None:pr=E.SubElement(row,q('trPr'));row.insert(0,pr)
            e.rev('del',pr);continue
        if rid in selected:
            us=selected[rid];ms=list(dict.fromkeys(u['module_id']for u in us))
            for cn,prefix,extra in[(1,'SI-ID-QCC-', '\n'.join(m['id']+' '+m['title']for m in MODULES if m['id']in ms)),(2,'SU-ID-QCC-', '\n'.join(u['id']+' '+u['title']for u in us))]:
                old=visible(cells[cn]);base=old.split(prefix,1)[0]
                base=re.split(r'\[(?:QCC|E8300)',base,1)[0].rstrip()
                text=(base+'\n'if base else'')+extra
                if cn==2:
                    note=re.search(r'\[(?:QCC|E8300).*',old,re.S)
                    if note:
                        gap=note.group(0).replace('원문 HMAC/ENC 명칭','HMAC/ENC 표기')
                        if rid==69:gap=AUDIO_GAP
                        text+='\n'+gap
                e.cell(cells[cn],text)
            e.changes.append({'table':'요구사항→모듈→유닛','id':'SW-FW-RID-'+str(rid),'module_ids':ms,'unit_ids':[u['id']for u in us]})
        elif rid==83:
            e.cell(cells[1],'제품 사용 설명서 / 실행 모듈 배정 대상 아님')
            e.cell(cells[2],'제품 문서 항목 / 실행 유닛 배정 대상 아님\n[E8300 보완] 제품 사용 설명서의 안전/작동법/동작방법 및 문제 해결 내용, 배포 대상과 검토 결과를 확인해야 한다.')
    e.replace_range(170,170,[e.cap('소프트웨어 요구사항과 상세 설계 유닛 연결',1)],'수동 표 번호를 같은 표 캡션 SEQ 체계로 연결')
    e.replace_range(172,174,[e.cap('소프트웨어 유닛과 상위 요구사항 연결',1),e.table([['상세 설계 유닛 ID / 이름','모듈 ID / 이름','상위 요구사항 ID']]+[[u['id']+' '+u['title'],u['module_id']+' '+next(m['title']for m in MODULES if m['id']==u['module_id']),', '.join('SW-FW-RID-'+str(n)for n in u['rids'])]for u in UNITS],level=1,widths=[3000,2700,3938]),e.blank(1)],'원문 ID 대응 메모를 제거하고 실제 유닛→모듈→요구사항 추적 유지')
    e.replace(175,AUDIO_GAP,level=1)
    e.replace(176,'[E8300 보완] SW-FW-RID-83은 상위 SYS-RID-ACDOC-17/18/20의 사용 설명서 요구사항에 연결된다. 제품 사용 설명서의 안전/작동법/동작방법 및 문제 해결 내용, 배포 대상과 검토 결과를 확인해야 한다.',level=1)
    e.replace_range(182,182,[e.cap('지원 소프트웨어 목록',1)],'지원 소프트웨어 표 캡션 SEQ 적용')
    e.replace(184,'QCC는 Qualcomm BSP와 내장 라이브러리의 시스템 초기화, Classic/TWS/BLE, 전원 및 NVM 기능을 Application API로 사용한다. 내장 라이브러리의 내부 구현은 수정하지 않고 기능 인터페이스로 연결한다.',level=1)
    e.replace(189,'SW-FW-RID-1~81 및 SW-FW-RID-83을 모듈/상세 설계 유닛 또는 제품 문서 항목에 연결한다. SW-FW-RID-69의 Classic 수신과 DMIC2 통화 송신은 각각의 실행 경로와 시험 조건으로 확인한다. 제품 사용 설명서 및 미확정 설계/검증 계약은 해당 미비 안내에서 확인하며 완료로 판정하지 않는다.',level=1)
    e.replace(192,'E8300의 공유 메모리/SPI/I2S/제어 핀과 QCC 모듈의 상태/명령/오디오/인증 경로를 연결한다. 종료 신호, 충전 변환과 무선/오디오/보안 계약의 미확정 조건은 검토 및 제품 시험에서 보류한다. 아키텍쳐의 연결성 검토와 실제 구현/통합시험의 판정은 구분한다.',level=1)
    e.replace_range(195,195,[pp(e,'E8300의 지원 소프트웨어는 목록에 제품/버전/출처로 식별하여 관리한다. QCC 지원 소프트웨어는 정확한 제품/버전/구성과 운영 검토 자료를 확인한 뒤 동일한 관리 범위에 연결한다.',1),missing(e,'[QCC 자료 입력] QCC 지원 소프트웨어의 정확한 제품/버전, 사용 구성/API 및 운영 검토 자료가 필요하다.',1)],'SOUP 식별 상태와 QCC 운영 검토 자료 대기 구분')
    return e.finish(commit)

def author_sddd(commit=False):
    e=Editor('SDDD')
    e.figure(1,ROOT/'figures/시스템 블록도.png','DMIC2 공통 입력과 QCC 통화 상향 경로')
    e.figure(5,ROOT/'figures/SDDD 블루투스 연계.png','QCC 4모듈/14유닛과 E8300 명령/오디오 연계')
    e.replace(51,'외부장치 Sound1은 E8300의 CFX/HEAR/Cortex-M3와 별도 칩 QCC5181 및 주변장치로 구성된다. E8300의 부트로더는 하드웨어와 실행 이미지를 준비하고 응용 코어를 기동한다. DMIC1/DMIC2는 E8300에 연결되고 DMIC2는 QCC에도 연결되어 전화 통화 상향 입력으로 사용한다.',level=1)
    e.replace(57,'각 기능 모듈은 역할과 처리 완료 시점에 따라 유닛으로 나눈다. CFX/HEAR/CM3와 부트로더의 입력/출력/처리 및 QCC의 시스템 관리/HW 인터페이스/블루투스/보안 4모듈과14유닛을 아래에 정의한다.',level=1)
    e.insert_after(e.block(94),[missing(e,'[QCC 자료 입력] QCC 4모듈/14유닛의 실제 소스/빌드/라이브러리 설정과 실행/측정 결과를 확인해야 한다. DMIC2의 두 칩 입력 연결은 확인되었으며 미비 상태는 구현/시험 근거에 적용한다.',2)])
    e.replace_range(96,96,[e.cap('장치와 기능의 경계',2),e.table(BOUNDARY,level=2,widths=[1700,3100,4416]),e.blank(2)],'DMIC2와 오디오 양방향을 포함한 기능 경계; 표2 기준선/사용 가능 폭')
    e.replace(102,'QCC와 E8300 사이의 제어 명령/응답은 SPI로 전달하고 수신 오디오는 I2S로 CFX에 전달한다. 통화 상향은 DMIC2→QCC→상대 장치 경로이다. QCC의 시스템 관리/블루투스/보안 기능은 명령 중계와 인증을 연결하고 HW 인터페이스는 PIO/전원/NVM 접근을 담당한다.',level=2)
    # Architecture descriptions outside the QCC unit section are standalone behavior descriptions.
    for n,b in enumerate(e.blocks,1):
        if b.tag==q('p')and visible(b).startswith('기존 그림의 장치/칩 경계를 유지하고'):
            e.replace(n,'외부기에는 E8300과 별도 칩 QCC5181이 있다. E8300은 CFX/HEAR 오디오 처리와 CM3 내부기 제어/자극 실행을 수행하고 QCC는 무선 송수신, 전원/PIO/NVM과 인증을 처리한다. DMIC2는 두 칩에 연결되며 QCC의 통화 상향과 E8300으로 보내는 I2S 하향을 구분한다.',level=2)
    e.replace(2552,'QCC와 E8300은 SPI 명령/응답, I2S 오디오 및 제어 핀으로 연결한다. CM3는 무선 명령을 실행하고 상태/응답을 SPI로 전달하며 CFX는 마이크와 I2S 입력 상태에 따라 신호처리 경로를 선택한다.',level=2)
    e.replace(2558,AUDIO,level=2)
    e.replace(2559,'[QCC 자료 입력] QCC shutdown_ind의 정상/종료 논리와 실제 PIO 번호/전압/반전, 기동/종료 대기 및 I2S enable→I2S_FLAG Low 유효의 물리 대응을 확인해야 한다.',level=2)
    e.replace(2560,'[QCC 자료 입력] 충전 4상태→0x34 charger_connected 0/1 변환, 0x34~0x36 통보/오류 처리, 실제 QCC 송신 length 필드/21byte 변환 및 GAIA 주소/type별 데이터 정의와 실행 확인이 필요하다.',level=2)
    e.replace(2561,AUDIO_GAP,level=2)
    part=[e.p('QCC 소프트웨어 상세 설계',level=2,heading=True),pp(e,SCOPE,2)]
    prototype=e.block(152)
    for m in MODULES:
        part.extend([e.p(m['id']+' '+m['title'],level=3,heading=True),pp(e,m['purpose'],3)])
        for n in m['units']:
            u=UNITS[n-1]
            if part[-1].tag==q('p')and visible(part[-1]):part.append(e.blank(4))
            part.extend([e.p(u['id']+' '+u['title'],level=4,heading=True),e.p(u['purpose'],level=4,keep=True),e.cap(u['title']+' / 유닛 설계 내용',4)])
            rows=[['항목','설계 내용'],['입력','\n'.join(u['input'])],['출력','\n'.join(u['output'])],['처리','\n'.join(str(i)+'. '+s for i,s in enumerate(u['process'],1))]]
            part.extend([e.table(rows,level=4,widths=[1304,7512],prototype=prototype),e.blank(4),e.cap(u['title']+' / 구현 조건',4)])
            impl=[['구분','구현 조건'],['동작 조건','\n'.join(u['impl'])],['요구사항/검증 연결',trace_description(u)]]
            part.extend([e.table(impl,level=4,widths=[1304,7512],prototype=prototype),e.blank(4),missing(e,'[QCC 자료 입력] '+u['gap'],4)])
    part.extend([e.blank(2),e.p('인증 키와 비휘발성 저장 데이터',level=3,heading=True),pp(e,'생산 시 설치하는 키, 실행 중 파생하는 키와 저장 복호화의 키 재료는 역할과 수명 주기가 다르다. 제조 키의 저장/읽기, 매핑 키 파생과 CRA 인증값 계산을 각각 연결한다.',3),e.cap('키와 저장 데이터의 역할',3),e.table([
        ['데이터','사용 역할','저장/처리 경로'],['shared_service_key','리모콘 인증용 제조 설치 키, 16byte','PS_USR44 → 보안 NVM/복호화 → CRA'],['clinician_master_key','의료진 매핑 키의 제조 설치 master key, 16byte','PS_USR43 → 보안 NVM/복호화 → SHA-256 키 파생'],['clinic_service_key','제조번호와 master key로 파생한 매핑 인증 키','SHA-256 결과 앞 16byte → CRA 인증값 계산'],['RAND / BD_ADDR','NVM 복호화 키 재료: 10byte/6byte','PS_USR40 RAND와 장치 주소 → toolbox']],level=3,widths=[2100,2800,4116]),e.blank(3),missing(e,'[QCC 자료 입력] 실제 AUTH UUID/메시지 길이/엔디언/권한, HMAC/ENC와 AES-CMAC의 역할/직렬화, NVM 키 이름/권한과 RAND 복호화 bootstrap 및 검증 벡터를 확인해야 한다.',3),missing(e,'[QCC 자료 입력] QCC 자체 OTA의 이미지 인증/오류 복구와 E8300 이미지 전달 경로, QCC 부트 이미지 무결성 방식/활성화/검사 실패 처리는 상세 계약과 구현/측정 확인이 필요하다.',3),missing(e,'[QCC/E8300 자료 입력] 무제어 명령 조건과 BLE 무송수신 180초의 감시/reset 범위, 인증 실패 누적과 60초 제한/재접속 경계 및 E8300 통보 영향을 함께 확인해야 한다.',3)])
    e.replace_range(2562,2619,part,'QCC 4모듈/14유닛의 목적 본문, 동일 E8300 I/O/처리 표 및 구현 조건/검증 연결')
    e.replace(2621,'상세 설계의 검토는 유닛의 목적, 입력/출력/처리와 동작 조건이 상위 요구사항에 연결되는지 확인한다. 연결 데이터의 길이/단위, 초기 상태와 오류 처리도 함께 확인한다. 구현 확인과 실제 제품 시험 결과는 해당 유닛/통합/시스템 시험 기록에서 별도로 판정한다.',level=1)
    e.replace(2623,'유닛 사이 데이터의 단위/저장 형식/처리 완료/초기 상태/오류 처리는 서로 일치해야 한다. 종료 신호, 충전 변환, AUTH/GAIA 포맷과 키/난수 bootstrap 등 미확정 계약은 해당 미비 안내와 시험 판정에서 유지한다. 같은 입력의 예상 배열/수치/시간과 실제 관찰값을 해당 기준서/보고서의 같은 조건에서 비교한다.',level=1)
    # Normal spacing is required even for blank paragraphs retained outside the rewritten QCC section.
    for n,b in enumerate(e.blocks,1):
        if b.get('{http://schemas.microsoft.com/office/word/2010/wordml}paraId')in ['44A710C3','2BAF0515']:
            ps=b.find('w:pPr/w:pStyle',NS);sty=ps.get(q('val'))if ps is not None else e.styles['TDC 본문 1']
            e.delete(b);e.insert_after(b,[e.p(style=sty)])
            e.changes.append({'before_body_block':n,'paragraph_id':b.get('{http://schemas.microsoft.com/office/word/2010/wordml}paraId'),'operation':'tracked replacement of 3/4pt blank with normal current style blank'})
    return e.finish(commit)

def author_sip(commit=False):
    e=Editor('SIP')
    e.figure(1,ROOT/'figures/소프트웨어 구조도.png','E8300 코어와 QCC 모듈/오디오 기능 경계')
    e.insert_after(e.block(60),[missing(e,'[QCC 자료 입력] QCC 4모듈/14유닛의 실제 소스/빌드/라이브러리 설정과 유닛/통합 실행 및 측정 결과가 필요하다. DMIC2 입력 연결은 확인되었으며 이 안내는 구현/시험 근거의 확인 상태를 나타낸다.',1)])
    e.replace(92,'PIO/Power/NVM을 먼저 준비하고 보안 키 읽기/파생/인증 계산을 연결한다. 초기화와 상태 처리에는 E8300의 준비/종료 신호 및 내부기 연결 정보를 반영한다. 하위 기능이 준비된 후 Bluetooth 검색/연결과 SPI 중계를 연결한다.',level=4)
    e.replace(98,'Classic, BLE, AUTH service와 BLE Data를 단계적으로 연결한다. AUTH service는 보안 처리/툴박스/보안 NVM을 사용하고 인증된 BLE 데이터는 시스템 상태 유닛을 통해 SPI로 중계한다. Classic 수신 오디오는 I2S로 E8300에 전달하며 통화 상향은 DMIC2→QCC→통화 상대 경로로 연결한다.',level=4)
    e.replace(99,'구현은 HW 접근과 시스템/무선/보안 기능의 의존 순서를 따른다. QCC의 4모듈/14유닛과 E8300의 실행 경로는 아래의 준비 조건 및 통합 단계에 연결한다.',level=3)
    old=e.block(100)
    rows=[['모듈','구현 대상','사전 준비 조건'],['SI-ID-QCC-002 HW 인터페이스','SU-ID-QCC-005 PIO / 006 Power / 007 NVM','회로/핀 설정과 초기값, 전압-잔량 기준, NVM 식별자/길이/오류 정책'],['SI-ID-QCC-004 보안','SU-ID-QCC-012 보안 처리 / 013 toolbox / 014 보안 NVM','공개 시험키/벡터, 생산 설치 키/파생 키의 역할, 난수/복호화 bootstrap과 접근 권한'],['SI-ID-QCC-001 시스템 관리','SU-ID-QCC-001 초기화 / 002 시스템 상태 / 003 장치 상태 / 004 Bluetooth 상태','HW 기능/라이브러리 준비, E8300 제어 신호와 SPI 정보, 생산 Standalone/TWS 구성'],['SI-ID-QCC-003 블루투스','SU-ID-QCC-008 Classic / 009 BLE / 010 AUTH / 011 BLE Data','무선 상대와 서비스/메시지 계약, 보안/중계 준비, I2S 및 DMIC2 통화 입력 설정']]
    e.replace_range(100,100,[e.cap('QCC 구현의 사전 준비 조건',3),e.table(rows,level=3,widths=[2000,3100,3916]),e.blank(3)],'사전 구성과 실제 통합 동작을 분리한 준비 조건')
    e.replace(154,'하위 유닛을 연결한 뒤 시스템 상태, 인증/데이터와 오디오 경로를 순차 통합한다. 각 단계는 아래의 기능과 연결 범위를 해당 통합 시험 조건에서 확인한다. 실패나 상충이 발견되면 관련 유닛/요구사항/시험 기준을 갱신하고 같은 입력으로 재확인한다.',level=3)
    stage=[['단계 / 대상','연결 및 확인 내용','시험 연결 / 보류 조건'],
      ['QCC-I-01 HW 기반\nSU-ID-QCC-005/006/007','전원 인가 후 PIO 방향/초기값과 Power/NVM을 연결한다. 충전 연결/분리 및 배터리 전압별 잔량/전원 상태를 SPI 중계에 연결하고 읽기/쓰기 실패를 확인한다.','SW-FW-IntegrationTest-ID-018: 잔량80/40, charger 1/0, lid1/2와 0x34/ACK 전량 비교. 019: 연결/종료 신호. 충전4상태→binary와 실제 length/21byte 변환은 미확정.'],
      ['QCC-I-02 보안 기반\nSU-ID-QCC-007/013/014','공개 시험키/RAND/BD_ADDR를 사용하는 격리된 저장 영역을 준비한다. 제조 저장값 읽기/복호화, SHA-256 매핑 키 파생과 AES-CMAC 인증값 계산을 각각 연결한다.','SW-FW-IntegrationTest-ID-024: NVM 위치/길이, 파생 중간값/복호화값/인증값 전량 비교. 직렬화/모드/IV/무결성/난수 bootstrap과 독립 시험 벡터 필요.'],
      ['QCC-I-03 시스템 상태\nSU-ID-QCC-001/002/003/004/005/006','정해진 초기화 순서와 E8300 준비/종료, 내부기 연결 및 Bluetooth 상태를 연결한다. 생산 Standalone/TWS 구성, L+R 또는 L/R 처리와 TWS 1대 단독 사용을 확인한다.','SW-FW-IntegrationTest-ID-019: 부착/탈착/재부착과 종료 신호, 023: 편이/양이 데이터. QCC PIO 물리 대응/대기/디바운스와 구성 우선순위 필요.'],
      ['QCC-I-04 BLE 인증\nSU-ID-QCC-009/010/012/013/014','페어링 후 AUTH 접근을 연결한다. 타입1리모콘/2매핑의 Challenge→Response→Confirm→Status와 invalid/wrong key/미완료를 확인하고 인증 전후 일반 제어 접근을 비교한다.','SW-FW-IntegrationTest-ID-020: 네 방향의 시험 패턴 중계. 021: 실제 CRA와 제어 게이트/30초 타임아웃. 중계 패턴 통과를 CRA 성공으로 확대하지 않는다. UUID/길이/권한/타이머 계약 필요.'],
      ['QCC-I-05 BLE 데이터\nSU-ID-QCC-002/004/009/011/012','인증/권한에 따른 GAIA type/data와 SPI 중계를 연결한다. 유효1~20byte와 잘못된 길이/주소/종류, 연결 해제/재접속, 무송수신180초와 연속5회 인증 실패의60초 제한을 확인한다.','SW-FW-IntegrationTest-ID-020/021/023: 패턴 중계, CRA/시간 제한과 편이/양이 전량 데이터. GAIA 좌우/nn/CRC와 타이머 reset/실패 누적/제한 종료 경계 필요.'],
      ['QCC-I-06 오디오\nSU-ID-QCC-002/003/004/005/008','Classic 수신→I2S24bit/16kHz→E8300 CFX 처리를 연결하고 OFF→ON→OFF, 0x36 종류와 I2S_FLAG를 확인한다. 별도로 DMIC2→QCC 통화 수음→상대 송신을 연결한다. Standalone/TWS의 오디오 구성과 송수신 동시 조건도 확인한다.','SW-FW-IntegrationTest-ID-022: 수신/I2S/CFX 경계. 시스템 시험 ID69는 수신, ID82는 DMIC2 통화 송신이며 요구사항은 SW-FW-RID-69로 연결한다. 송신 시험 결과는 수신 결과와 별도로 판정한다. 프로파일/코덱/클록/상대 및 실측 필요.'],
      ['QCC-I-07 전체 및 갱신\n4모듈 / E8300','전원/LED/오디오 상태, 리모콘/매핑 요청→실행→응답과 종료/충전/연결 전환을 전체 시스템에 연결한다. QCC 자체 OTA, E8300 이미지 전달과 각 칩의 부트 검사 경로를 구분한다.','SW-FW-IntegrationTest-ID-018~024 및 해당 기존 E8300 조건. 실제 통합/제품 시험 결과와 QCC OTA/부트의 인증/오류/복구 계약 필요. 기존 E8300 결과는 해당 실행 범위로 유지한다.']]
    e.replace_range(155,155,[e.cap('QCC와 E8300의 통합 단계',3),e.table(stage,level=3,widths=[1800,3700,3516]),e.blank(3)],'통합7단계와 실제 시험의 목적/입력 경계')
    e.replace(159,AUDIO,level=3)
    e.replace(160,'[QCC 자료 입력] QCC shutdown_ind의 정상/종료 논리와 실제 PIO 번호/전압/반전, 기동/종료 대기 및 I2S enable→I2S_FLAG Low 유효의 물리 대응을 확인해야 한다.',level=3)
    e.replace(161,'[QCC 자료 입력] 충전4상태→0x34 charger_connected0/1 변환, 통보/오류 처리, 실제 QCC 송신 length 필드와21byte 변환 및 GAIA 주소/type별 데이터 정의와 실행 결과가 필요하다.',level=3)
    e.replace(162,AUDIO_GAP,level=3)
    e.replace(165,'시험 방법은 통과를 판단할 관찰/측정 근거에 따라 I/A/D/T로 선택한다. 수치/메시지/시간의 정량 비교는 T이며 코드 분석은 물리 실측을 대신하지 않는다. 예상 SPI/BLE 데이터, 분석 결과, 시험자 확인에 따른 기록과 실제 수집 자료를 구분한다. 미실행 조건은 미실행/판정 대기로 유지한다.',level=3)
    return e.finish(commit)

if __name__=='__main__':
    a=argparse.ArgumentParser();a.add_argument('code',choices=['SAD','SDDD','SIP','all']);a.add_argument('--commit',action='store_true');args=a.parse_args()
    fs={'SAD':author_sad,'SDDD':author_sddd,'SIP':author_sip}
    reports=[fs[x](args.commit)for x in fs if args.code in[x,'all']]
    (ROOT/'source-correspondence-local.json').write_text(json.dumps(source_correspondence(),ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps([{k:v for k,v in r.items()if k in['code','path','candidate','before_sha256','after_sha256','new_revision_count','reject_new_c14n_equals_before','committed']}for r in reports],ensure_ascii=True))
