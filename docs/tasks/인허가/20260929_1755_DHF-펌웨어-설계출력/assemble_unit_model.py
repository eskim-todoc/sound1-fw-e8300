"""Build deterministic lineage and symbol evidence without editing product code."""
from pathlib import Path
from collections import Counter,defaultdict
import re,json,hashlib
from unit_model import UNITS,BASE,T
import unit_model_cm3
import unit_model_boot

ROOT=T.parents[3]
QA=T/'qa/unit-reconciliation'
MODULE_IDS={('CFX','시스템 제어 모듈'):'SI-ID-CFX-001',('CFX','신호처리 모듈'):'SI-ID-CFX-002',
 ('CFX','내부 자극 장치 실행 모듈'):'SI-ID-CFX-003',('HEAR','신호처리 모듈'):'SI-ID-HEAR-001',
 ('CM3','시스템 제어 모듈'):'SI-ID-CM3-001',('CM3','신호처리 모듈'):'SI-ID-CM3-002',
 ('CM3','내부 자극 장치 실행 모듈'):'SI-ID-CM3-003',('CM3','블루투스 통신 모듈'):'SI-ID-CM3-004',
 ('BOOT','부트 제어 및 이미지 적재 모듈'):'SI-ID-BOOT-001',('BOOT','무선 갱신 모듈'):'SI-ID-BOOT-002'}
SOURCE_DIRS={'CFX':'src/1__cfx','CM3':'src/2__cm3','BOOT':'src/0__bootloader','HEAR':'src/3__hear'}
ALIASES={'NRF_ON_Command':'NRF_On_Command'}
DECLARATION_ONLY={'isMappingProgramConnected':'헤더 선언만 있으며 정의/호출 없음. mappingControl 반환 구조체로 현재 상태를 확인한다.'}
NOT_ACTIVE={
 'CFX-015':'구형 배터리 LSAD 초기화. 현 정상 잔량/충전 판단은 QCC 정보 수신으로 대체한다. 레거시 소스 존재와 실행 경로 채택은 구분한다.',
 'CFX-045':'가속도 센서는 현 사용자 입력 장치가 아니다. Sound1의 터치 센서 초기화/판정 시험으로 별도 구성한다.',
 'CFX-048':'텔레코일 입력 경로를 현재 제품에서 사용하지 않는다. 오디오 블록의 절대값 시험은 HEAR 최대값 유닛으로 유지한다.',
 'CFX-049':'텔레코일 근접 감지 기능은 현재 제품에 없다. 새 I2S 수신 상태 판정은 다른 입력 조건으로 시험한다.',
 'CM3-013':'가속도 센서 초기화는 적용하지 않는다. 터치 센서 초기화 및 상태 판정을 새 항목으로 정의한다.',
}
for i in range(1,19):NOT_ACTIVE[f'NRF-{i:03}']='QCC 내부 자료 대기. NRF 내부 코드/시험을 이름만 바꾸어 QCC 구현으로 취급하지 않는다.'

def code_text(path):
    raw=path.read_bytes()
    for enc in ['utf8','cp949']:
        try:return raw.decode(enc)
        except UnicodeDecodeError:pass
    raise ValueError(path)

def build():
    # Number in the same order the SAD-module-grouped document is read.
    core_order=['CFX','HEAR','CM3','BOOT']
    module_order=list(MODULE_IDS)
    UNITS.sort(key=lambda u:(core_order.index(u['core']),module_order.index((u['core'],u['module']))))
    # SAD assigns channel selection/ordering to signal processing, packet assembly to internal-device execution.
    expected_modules={
        'CIS 전략과 전략 선택':'신호처리 모듈',
        'NofM 대역 선택':'신호처리 모듈',
        'NofM 선택 대역 교차 배치':'신호처리 모듈',
        '자극 패킷 헤더 구성':'내부 자극 장치 실행 모듈',
        'NofM 패킷 구성과 분할 출력':'내부 자극 장치 실행 모듈'}
    for name,module in expected_modules.items():
        match=[u for u in UNITS if u['core']=='CFX' and u['name']==name]
        assert len(match)==1 and match[0]['module']==module,(name,module)
    seq=Counter()
    for u in UNITS:
        seq[u['core']]+=1;u['id']=seq[u['core']]
        u['su']=f"SU-ID-{u['core']}-{u['id']:03}"
        u['test']=f"SW-FW-UnitTest-{u['core']}-ID-{u['id']}"
    records={}
    for core,folder in SOURCE_DIRS.items():
        records[core]=[(p,code_text(p)) for p in (ROOT/folder).rglob('*') if p.suffix in ('.c','.h','.hct') and not any(k in p.parts for k in ['Debug','Release'])]
    member_map={};missing=[];source_hashes={}
    for u in UNITS:
        u['si']=MODULE_IDS[(u['core'],u['module'])]
        refs=[]
        for fn in u['symbols']:
            fn=ALIASES.get(fn,fn)
            if fn in DECLARATION_ONLY:continue
            pattern=re.compile(r'\b'+re.escape(fn)+r'\s*\([^;{}]*?\)\s*\{',re.S)
            hits=[]
            for p,text in records[u['core']]:
                # Preserve line numbering when masking comments.
                stripped=re.sub(r'/\*.*?\*/|//[^\n]*',lambda m: ''.join('\n' if x=='\n' else ' ' for x in m.group()),text,flags=re.S)
                for m in pattern.finditer(stripped):
                    hits.append(dict(path=p.relative_to(ROOT).as_posix(),line=stripped.count('\n',0,m.start())+1,symbol=fn))
            if fn=='I2C_0_IRQHandler' and not hits:
                p=ROOT/'src/2__cm3/Cortex-M3-src/systemControl/driver_i2c.c'
                text=code_text(p)
                assert 'void I2C_0_IRQHandler(void)\n#else\nvoid i2c_comm(void)\n#endif\n{' in text
                hits=[dict(path=p.relative_to(ROOT).as_posix(),line=text[:text.index('void I2C_0_IRQHandler(void)')].count('\n')+1,symbol='I2C_0_IRQHandler / i2c_comm (조건부 컴파일)')]
            if not hits:missing.append((u['su'],fn))
            refs.extend(hits)
        if u['anchor']:
            assert (ROOT/u['anchor']).exists(),u['anchor']
            refs.append(dict(path=u['anchor'],line=1,symbol='처리 영역'))
        u['refs']=refs
        u['symbols']=[ALIASES.get(s,s) for s in u['symbols'] if s not in DECLARATION_ONLY]
        for ref in refs:
            source_hashes[ref['path']]=hashlib.sha256((ROOT/ref['path']).read_bytes()).hexdigest()
        for old in u['members']:
            assert old not in member_map,old
            member_map[old]=u['su']
    all_members={u['su_id'] for u in BASE['supplement_units']}
    assert set(member_map)==all_members,(all_members-set(member_map),set(member_map)-all_members)
    original=defaultdict(list)
    for u in UNITS:
        for old in u['old']:original[old].append(u['su'])
    all_original={f'CFX-{i:03}' for i in range(1,62)}|{f'CM3-{i:03}' for i in range(1,108)}|{f'NRF-{i:03}' for i in range(1,19)}
    all_original.remove('CFX-055');all_original.update(['CFX-055a','CFX-055b'])
    missing_orig=sorted(all_original-set(original)-set(NOT_ACTIVE))
    results=dict(counts=dict(Counter(u['core'] for u in UNITS)),unit_count=len(UNITS),first_draft_members=len(member_map),
       missing_symbols=missing,missing_original=missing_orig,source_hashes=source_hashes)
    (QA/'model.json').write_text(json.dumps(UNITS,ensure_ascii=False,indent=2),encoding='utf8')
    (QA/'lineage.json').write_text(json.dumps(dict(first_draft=member_map,original=original,not_active=NOT_ACTIVE),ensure_ascii=False,indent=2),encoding='utf8')
    (QA/'model-checks.json').write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({k:v for k,v in results.items() if k!='source_hashes'},ensure_ascii=False,indent=2))

if __name__=='__main__':build()
