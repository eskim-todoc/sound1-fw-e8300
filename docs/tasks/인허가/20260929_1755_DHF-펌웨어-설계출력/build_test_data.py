"""Read-only extraction and explicit test adaptation plan for artifact/native authoring."""
from build_architecture import *
import sys,textwrap
sys.path.insert(0,str(ROOT/'docs/tasks/인허가/20260909_1734_suvr-sound1/스크립트'))
import suvc_rows
units=suvc_rows.build()
inputdir=ROOT/'docs/참고/인허가/사운드1/DHF/02_설계 입력/SW/펌웨어'
inp=openpyxl.load_workbook(next(inputdir.glob('*SSTC*.xlsx')),data_only=True)['기준서_시스템 시험']
inputrows={int(r[1].value.rsplit('-',1)[1]):[c.value for c in r[:10]] for r in inp if isinstance(r[1].value,str) and re.fullmatch(r'SW-FW-SYS-Test-ID-\d+',r[1].value)}
assert len(inputrows)==81

def source(kind):return next(x for x in INV if x['name'].startswith(kind+'-'))
def filled_rows(sheet,start,col):
    rows=[];b=c=None
    for row in sheet['values'][start-1:]:
        if not row[col]:continue
        rr=list(row)
        if rr[1]:b=rr[1]
        if rr[2]:c=rr[2]
        rr[1]=b;rr[2]=c;rows.append(rr)
    return rows
def diff(old,new,key):
    oldby={r[key]:r for r in old};entries=[];used=set()
    for r in new:
        o=oldby.get(r[key]);used.add(r[key])
        if o==r:entries.append({'mark':'','values':r})
        else:
            if o:entries.append({'mark':'[-]','values':o})
            entries.append({'mark':'[+]','values':r})
    for o in old:
        if o[key] not in used:entries.append({'mark':'[-]','values':o})
    return entries
def clean(r):return [x.replace('·','/').replace('SND1','TD24A').replace('TD20B','TD24A') if isinstance(x,str) else x for x in r]

plans=[]
# Unit tests retain the prepared Sound1 test vectors and exact checked unit IDs.
kind='SUVC';item=source(kind);s=item['sheets'][1]
baseline=openpyxl.load_workbook(ROOT/'docs/참고/인허가/사운드1/[04] SUVC-FW-SND1 소프트웨어 유닛 검증 기준서.xlsx',data_only=True).worksheets[1]
rows=[];core=module=None
for row in baseline:
    r=[c.value for c in row[:11]]
    if not isinstance(r[3],str) or not r[3].startswith('SW-FW-UnitTest-'):continue
    core=r[1] or core;module=r[2] or module;r[1]=core;r[2]=module;rows.append(clean(r))
assert len(rows)==325
assert {r[6] for r in rows}==set(json.loads((T/'detail_ids.json').read_text(encoding='utf8')))
# QCC internals intentionally have no invented unit cases. Retain a separate insertion section in metadata.
old=filled_rows(s,30,3)
plans.append({'kind':kind,'source':item['source'],'name':item['name'].replace('TD20B','TD24A'),'sheet':s['name'],'start':30,'end':223,'cols':11,'key':3,'rows':rows,'diff':diff(old,rows,3)})

# Integration test IDs 001..014 retained, with QCC former-core tests redefined as interface tests.
kind='SITC';item=source(kind);s=item['sheets'][1]
old=[list(r) for r in s['values'][24:38]];rows=[]
def uids(core,si=None,mod=None,block=None):
    found=[x['su_id'] for x in units if x['core']==core and (si is None or x['si_id']==si) and (mod is None or x['mod']==mod) and (block is None or x['block']==block)]
    assert found,(core,si,mod,block)
    return '\n'.join(found)
si_map={1:'SI-ID-CFX-001',2:'SI-ID-CFX-003',3:'SI-ID-CFX-002',4:'SI-ID-CFX-006',5:'SI-ID-CFX-005',6:'SI-ID-CFX-002',7:'SI-ID-CM3-001',8:'SI-ID-CM3-005',9:'SI-ID-CM3-006',10:'SI-ID-CM3-006',11:'SI-ID-CM3-002'}
for i,o in enumerate(old,1):
    r=clean(o);r[1]='CFX' if i<=6 else 'CM3' if i<=11 else 'E8300/QCC'
    if i<=11:
        r[3]=si_map[i];r[5]=uids(r[1],si=si_map[i])
        r[7]=''
    if i==1:
        r[9]='- 부트로더로 하드웨어 초기화와 이미지 적재를 마친다.\n- CFX 초기화 완료 지점에 중단점을 설정한다.'
        r[10]='- CM3 초기화가 완료되어 CFX 실행이 허용된 상태로 시험한다.'
    if i==2:
        r[9]='- 정상 동작 중인 Sound1과 디버거를 준비한다.';r[10]='NA'
        r[11]='- CM3에서 저전력 전환을 요청한다.\n- CFX의 오디오 입출력 정지와 대기 진입을 확인한다.\n- 기상 조건을 적용하여 초기화 및 처리가 재개되는지 확인한다.'
        r[12]='- 대기 중 오디오 처리와 자극 출력이 정지한다.\n- 기상 후 필요한 설정을 복원하고 정상 동작으로 복귀한다.'
    if i==3:
        r[6]='I2S 오디오 입력 장치'
        r[4]='오디오 입력 상태에 따른 처리 경로 전환';r[9]='- I2S 오디오 입력과 마이크 입력을 준비한다.';r[10]='NA'
        r[11]='- I2S 입력을 시작하고 정지한다.\n- 입력 플래그와 마이크 사용 수, 믹싱 경로를 확인한다.'
        r[12]='- I2S 입력 중에는 마이크 1개와 I2S 경로를 혼합한다.\n- 스트리밍 종료 후에는 설정된 마이크 경로로 복귀한다.'
    if i==9:r[4]='매핑 명령과 자극 제어 연계'
    if i>=12:
        r[3]='SI-ID-CM3-006' if i!=13 else 'SI-ID-CFX-004'
        r[5]=uids('CM3',si='SI-ID-CM3-006') if i!=13 else uids('CFX',si='SI-ID-CFX-004')
        r[6]='E8300 / QCC5181';r[7]='';r[8]='T';r[10]='NA'
        if i==12:
            r[4]='QCC 시스템 정보와 CM3 상태 처리';r[9]='- QCC 또는 동일 SPI 시험 송신기로 시스템 정보 명령을 전송한다.'
            r[11]='- 0x34로 배터리 80%, 충전기 연결 1, 뚜껑 열림 1을 전달한다.\n- 저장 상태와 응답을 확인한다.\n- 0x36에 연결 1, 종류 1과 종류 2를 각각 전달한다.'
            r[12]='- 배터리 80%, 충전 상태, 뚜껑 열림이 반영되고 0x34 / 1 응답이 준비된다.\n- 크래들 여부는 종류 1일 때 1, 종류 2일 때 0이며 0x36 / 1 응답이 준비된다.'
        elif i==13:
            r[4]='QCC I2S 오디오와 CFX 혼합';r[9]='- 마이크와 I2S 버퍼를 조회한다.\n- 마이크와 크래들 게인 설정을 서로 다르게 지정한다.'
            r[11]='- I2S 오디오를 입력하고 크래들/일반 스트리밍 상태를 각각 설정한다.\n- 매핑 연결 전후의 게인과 혼합 출력을 확인한다.'
            r[12]='- 크래들은 마이크 Gain_A와 I2S Gain_B가 적용된다.\n- 일반 스트리밍의 I2S 게인은 1이다.\n- 매핑 연결 중에는 두 경로 모두 게인 1을 적용한다.\n- 혼합 신호가 PCM 자극 출력으로 이어진다.'
        else:
            r[3]='SI-ID-CM3-003';r[5]=uids('CM3',si='SI-ID-CM3-003')
            r[4]='내부기 상태 및 QCC 제어 핀';r[9]='- ISD_CHECK와 QCC_CTRL 핀을 측정한다.'
            r[11]='- 내부기 연결/해제를 수행한다.\n- QCC 정상 동작과 종료 제어를 수행한다.'
            r[12]='- ISD_CHECK는 연결 확정 시 High, 해제 시 Low이다.\n- QCC_CTRL은 정상 동작 요청 시 High, 종료 요청 시 Low이다.'
    rows.append(r)
for number,core,si,title,prep,steps,expected in [
 (15,'CFX','SI-ID-CFX-005','빔포밍과 N-of-M 신호처리 통합','- 2개 마이크 입력과 N-of-M 맵을 준비한다.','- 좌/우 설정을 변경하여 지연 마이크 경로를 확인한다.\n- 채널별 크기를 서로 다르게 입력하여 선택 채널과 PCM 출력을 비교한다.','- 좌/우 설정에 따라 지연 마이크가 바뀐다.\n- 크기가 큰 N개 채널만 선택되고 선택되지 않은 채널의 자극 크기는 0이다.'),
 (16,'BOOT','SI-ID-BOOT-003','이미지 검사와 코어 기동','- 정상 이미지와 CRC 오류 이미지를 준비한다.','- 각 이미지로 기동하여 검사 결과와 코어 실행 여부를 확인한다.','- 정상 이미지만 실행 대상으로 적재되며 오류 이미지를 정상 이미지로 실행하지 않는다.'),
 (17,'BOOT','SI-ID-BOOT-005','갱신 및 대체 이미지 선택','- 기본/대체 이미지와 상태 파일을 준비한다.','- 갱신 이미지 수신과 재기동을 수행한다.\n- 대체 이미지 검증 실패 및 기동 시도 상한 조건을 확인한다.','- 상태 파일에 따른 이미지가 선택된다.\n- 검증 실패 또는 시도 상한 조건에서 기본 이미지로 복귀한다.')]:
    rows.append([None,core,f'SW-FW-IntegrationTest-ID-{number:03d}',si,title,uids(core,si=si),'Sound1',None,'T',prep,'NA',steps,expected])
plans.append({'kind':kind,'source':item['source'],'name':item['name'].replace('TD20B','TD24A'),'sheet':s['name'],'start':25,'end':38,'cols':13,'key':2,'rows':rows,'diff':diff(old,rows,2)})

# System tests keep the output-stage detailed procedure except where the product changed.
kind='SSTC';item=source(kind);s=item['sheets'][1]
old=[list(r) for r in s['values'][33:101]];rows=[]
oldby={int(r[1].rsplit('-',1)[1]):r for r in old}
for n,ir in inputrows.items():
    r=clean(oldby[n]) if n in oldby else clean(ir)
    r[1]=ir[1];r[2]=ir[2];r[3]=ir[3];r[4]=ir[4];r[5]=ir[5]
    if n==1:
        r[7]='- 활성화된 자극 채널수 : 32개'
        r[9]='- 32채널 사용 시 전체 자극률이 20kHz 이상이어야 한다.'
    if n==10:
        r[8]='- 시험 맵에서 활성 채널 수와 각 채널의 분석 주파수 범위를 확인한다.\n- 각 분석 대역 안의 순음을 입력하고 해당 채널의 자극 출력을 확인한다.'
        r[9]='- 설정된 1~32개 분석 대역에 따라 입력 주파수에 해당하는 채널의 자극 출력이 확인되어야 한다.'
    if n==11:
        r[8]='- CIS 맵을 적용하고 채널별 출력 순서를 확인한다.\n- N-of-M 맵을 적용하고 N보다 많은 분석 대역에 서로 다른 크기의 신호를 입력한다.\n- 32채널 시험 내부기 지그에서 채널별 출력을 확인한다.'
        r[9]='- CIS의 자극 순서는 맵에 설정된 순서와 일치해야 한다.\n- N-of-M은 입력 크기에 따라 N개 채널을 선택하여 출력하고 선택되지 않은 채널은 자극하지 않아야 한다. 선택 채널의 출력 순서는 설정된 N-of-M 순서와 일치해야 한다.'
    if n in (6,16):r[7]=None
    if n in (17,19,21,22,42,43,47,48,49,51,52,54,61,62,63,66) or n>=69:
        r[6]=ir[6];r[7]=ir[7];r[9]=ir[9]
        r[8]=ir[8] or '- 시험 준비 조건을 적용한다.\n- 각 조건에서 동작과 상태를 확인하여 통과 기준과 비교한다.'
    if n==15:
        r[6]='- 마이크 2개와 I2S 입력을 준비한다.';r[7]=None
        r[8]='- 각 마이크에 소리를 입력하여 입력 버퍼를 확인한다.\n- QCC에서 I2S 오디오를 입력하여 입력 버퍼와 자극 출력을 확인한다.'
        r[9]='- 2개 이상의 오디오 입력 소스를 처리할 수 있어야 한다.\n- I2S 입력에 따른 자극 출력이 확인되어야 한다.'
    if n==19:r[8]='- 터치 센서에 지정된 사용자 입력을 가한다.\n- 전원 모드와 사용자 제어의 상태 변화를 확인한다.'
    if n in (21,22,62):r[8]='- LED 표시를 허용하고 내부기/매핑 연결 및 오류 표시를 해제한다.\n- 충전기 연결/해제와 배터리 잔량 80%, 79%, 10%, 9% 조건을 적용한다.\n- 충전 중에는 크래들 뚜껑을 열고 LED를 확인한다.'
    if n in (23,28,34,55):
        for j in (6,7,8,9):
            if isinstance(r[j],str):r[j]=re.sub(r'3\.3\s*[vV]','저전압 알림 조건',r[j])
    if n==29:r[8]='- QCC에서 배터리 잔량 정보를 전달한다.\n- 리모콘으로 배터리 상태를 조회하여 전달한 값과 응답을 비교한다.'
    if n==42:r[8]='- 터치 입력을 가하여 I2C 수신과 입력 이벤트 처리를 확인한다.'
    if n==43:
        r[8]=(oldby[n][8] or '').replace('PCMI','PMIC').replace('Pmic','PMIC')
    if n==50:r[8]='- 기록 대상 이벤트를 발생시킨다.\n- 감사기록에서 해당 이벤트를 확인한다.'
    if n==54:r[8]='- 정상 실행파일과 CRC 오류 실행파일로 각각 기동한다.\n- 무결성 검사 결과와 실행 여부를 확인한다.'
    if n==56:r[9]=(r[9] or '').replace('10으로','한번에 10으로').replace('한번에 한번에','한번에')
    if n==57:r[8]='- EPTC-TD24A 필수성능시험 기준서에 따라 시험한다.';r[9]='- EPTC-TD24A의 해당 필수성능 합격 기준을 만족해야 한다.'
    if n==69:r[8]='- Bluetooth Classic으로 오디오 스트리밍을 시작한다.\n- 내부기 시험 지그에서 자극 출력을 확인한다.'
    if n==70:r[8]='- BLE 연결 후 제어 명령을 전송한다.\n- 명령 처리와 응답 수신을 확인한다.'
    rows.append(r)
assert {r[4] for r in rows}=={f'SW-FW-RID-{n}' for n in range(1,82)}
plans.append({'kind':kind,'source':item['source'],'name':item['name'].replace('TD20B','TD24A').replace('_ver2',''),'sheet':s['name'],'start':34,'end':101,'cols':10,'key':1,'rows':rows,'diff':diff(old,rows,1)})

# Continuation lines avoid Excel's 409-point row height ceiling. No text or vector is omitted.
def splitrow(r):
    chunks=[]
    for value in r:
        if isinstance(value,str) and len(value)>430:
            lines=[];buf=''
            for line in value.splitlines(keepends=True):
                for bit in textwrap.wrap(line,width=420,replace_whitespace=False,drop_whitespace=False) or ['']:
                    if len(buf)+len(bit)>430:lines.append(buf);buf=''
                    buf+=bit
            if buf:lines.append(buf)
            chunks.append(lines)
        else:chunks.append([value])
    return [[ch[i] if i<len(ch) else None for ch in chunks] for i in range(max(map(len,chunks)))]
for plan in plans:
    plan['physical']=[]
    for row in plan['rows']:
        for k,part in enumerate(splitrow(row)):
            if k:
                part[plan['key']]=row[plan['key']]
            plan['physical'].append(part)
    plan['diffphysical']=[]
    for entry in plan['diff']:
        for k,part in enumerate(splitrow(entry['values'])):
            if k:part[plan['key']]=entry['values'][plan['key']]
            plan['diffphysical'].append({'mark':entry['mark'],'values':part})
(T/'test_plan.json').write_text(json.dumps(plans,ensure_ascii=False,indent=2),encoding='utf8')
print([(p['kind'],len(p['rows']),len(p['physical'])) for p in plans])
