"""Prepare source-based SUVC rows and affected SITC references for native application."""
from pathlib import Path
import json,re,hashlib,shutil
from collections import defaultdict
import openpyxl

T=Path(__file__).resolve().parent;ROOT=T.parents[3];QA=T/'qa/unit-reconciliation'
OUT=ROOT/'docs/참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어'
units=json.loads((QA/'model.json').read_text(encoding='utf8'))
base=json.loads((QA/'baseline.json').read_text(encoding='utf8'))
lineage=json.loads((QA/'lineage.json').read_text(encoding='utf8'))
byid={u['su']:u for u in units}

def oldkeys(row):
    m=re.fullmatch(r'SW-FW-UnitTest-(CFX|CM3|NRF)-ID-(\d+)',str(row[3] or ''))
    if not m:return []
    core,n=m[1],int(m[2])
    if core=='CFX':
        # Original G-column has incorrect IDs at 16~23 and duplicate 055.
        if n==54:return ['CFX-054','CFX-055a']
        if n==55:return ['CFX-055b']
        n={57:58,58:59,59:57}.get(n,n)
    return [f'{core}-{n:03}']

orig=[];test_lineage=[]
for rno,row in enumerate(base['SUVC-source']['rows'],1):
    keys=oldkeys(row)
    if not keys:continue
    targets=list(dict.fromkeys(s for k in keys for s in lineage['original'].get(k,[])))
    reason=' / '.join(lineage['not_active'][k] for k in keys if k in lineage['not_active'])
    orig.append({'row':rno,'values':row,'keys':keys,'targets':targets})
    test_lineage.append([row[3],row[4],row[6],', '.join(keys),', '.join(targets) or '해당 없음',
      ', '.join(byid[x]['test'] for x in targets) or '자료 대기' if keys[0].startswith('NRF') else ', '.join(byid[x]['test'] for x in targets) or '해당 없음',
      reason or '같은 기능 역할을 현재 코어/모듈의 유닛에 연결. 원본 상세 설계 ID와 시험 ID의 숫자만으로 대응하지 않음.'])
assert len(orig)==186

def row_for(u):
    prep='- '+u['case']
    limit='- 시험 중 다른 실행 경로가 관찰 버퍼/상태를 덮어쓰지 않도록 해당 유닛의 시작 상태를 고정한다.' if u['method']=='D' else 'NA'
    proc=('- 코드 분석. ' if u['method']=='A' else '- 지정 입력으로 실행하고 반환값/버퍼/상태를 관찰한다. ')+u['steps']
    proc+='\n- 대상: '+(', '.join(u['symbols']) or u['anchor'])
    return [None,u['core'],u['module'],u['test'],u['name'],u['method'],u['su'],prep,limit,proc,'- '+u['expected']]

rows=[row_for(u) for u in units]
def split_cell(s,limit):
    if s is None:return [None]
    s=str(s);result=[]
    while len(s)>limit:
        cut=s.rfind(' ',0,limit)
        if cut<limit//2:cut=limit
        result.append(s[:cut]);s=s[cut:].lstrip(' ')
    result.append(s);return result

def physicalize(records):
    physical=[];positions=[]
    for row in records:
        # Keep all text while avoiding Excel's fixed 409-point row-height ceiling.
        chunks=[split_cell(v,{7:200,8:260,9:330,10:450}.get(i,1500)) for i,v in enumerate(row)]
        count=max(map(len,chunks));positions.append([len(physical),count])
        for k in range(count):
            values=[c[k] if k<len(c) else None for c in chunks]
            if k:
                values[0]=row[0];values[4]='계속 '+str(k+1)
            physical.append(values)
    return physical,positions
# A source row appears only once. New rows follow the first original role they inherit.
first={}
for o in orig:
    for target in o['targets']:first.setdefault(target,o['row'])
diff=[];pairs=[]
for o in orig:
    neg=list(o['values']);neg[0]='[-]';nidx=len(diff);diff.append(neg)
    for u in units:
        if first.get(u['su'])==o['row']:
            pos=row_for(u);pos[0]='[+]';pidx=len(diff);diff.append(pos);pairs.append([nidx,pidx])
for u in units:
    if u['su'] not in first:
        pos=row_for(u);pos[0]='[+]';diff.append(pos)
assert sum(r[0]=='[+]' for r in diff)==115
assert sum(r[0]=='[-]' for r in diff)==186

first_draft_rows=[['1차본 유닛 ID','1차본 유닛 이름','새 유닛 ID','새 유닛 이름','새 시험 ID']]
supp={u['su_id']:u for u in base['supplement_units']}
for old,new in lineage['first_draft'].items():first_draft_rows.append([old,supp[old]['name'],new,byid[new]['name'],byid[new]['test']])
original_rows=[['원본 시험 ID','원본 시험 이름','원본 기재 설계 ID','역할 기준 설계 ID','Sound1 유닛 ID','Sound1 시험 ID','처리 근거']]+test_lineage
code_rows=[['코어','SAD 모듈','소프트웨어 항목','유닛 ID','이름','대상 함수','구현 파일']]
for u in units:code_rows.append([u['core'],u['module'],u['si'],u['su'],u['name'],', '.join(u['symbols']), '\n'.join(dict.fromkeys(r['path'] for r in u['refs']))])

sitc=OUT/'SITC-FW-TD24A 소프트웨어 통합시험 기준서.xlsx'
before=QA/'SITC-before.xlsx'
if not before.exists():shutil.copy2(sitc,before)
allowed_hashes={hashlib.sha256(before.read_bytes()).hexdigest()}
verified=QA/'final-checks.json'
if verified.exists():allowed_hashes.add(json.loads(verified.read_text(encoding='utf8'))['sha256']['SITC'])
assert hashlib.sha256(sitc.read_bytes()).hexdigest() in allowed_hashes,'SITC changed since baseline and last verified publication'
wb=openpyxl.load_workbook(before,data_only=False);sitc_edits=[]
for sheet in wb:
    if not sheet.title.startswith('기준서_통합 시험'):continue
    for row in sheet:
        if not isinstance(row[2].value,str) or not re.fullmatch(r'SW-FW-IntegrationTest-ID-\d+',row[2].value):continue
        if sheet.title.endswith('변경 비교') and row[0].value=='[-]':continue
        old_sus=re.findall(r'SU-ID-(?:CFX|CM3|BOOT)-\d+',str(row[5].value or ''))
        if not old_sus:continue
        missing=[su for su in old_sus if su not in lineage['first_draft']]
        assert not missing,missing
        new_sus=list(dict.fromkeys(lineage['first_draft'][su] for su in old_sus))
        # Refresh affected integration links by role where the first draft omitted dependencies.
        extra={
          '013':['단일 및 두 입력 혼합','입력별 게인 혼합','입력 게인 명령 처리'],
          '015':['전면 마이크 선택','두 마이크 지연 합산 빔포밍','신호처리 경로 선택','오디오 절대값 최대값 연산','창 적용 FFT와 크기 연산'],
          '016':['응용 코어 기동','부트 실행 순서 관리','이미지 구획 처리','이미지 CRC 검사','구획 데이터 전송','프로그램 메모리 잠금'],
          '017':['OTA 세션 시작과 종료','OTA 데이터와 기동 위치 명령']
        }.get(row[2].value.rsplit('-',1)[1],[])
        # Names are checked below; unmatched entries must not be silently invented.
        for name in extra:
            matches=[u for u in units if u['name']==name]
            assert len(matches)==1,(name,matches)
            new_sus.extend(u['su'] for u in matches if u['su'] not in new_sus)
        new_sis=list(dict.fromkeys(byid[su]['si'] for su in new_sus))
        for col,new in [('D','\n'.join(new_sis)),('F','\n'.join(new_sus))]:
            addr=f'{col}{row[0].row}'
            sitc_edits.append(dict(sheet=sheet.title,cell=addr,old=sheet[addr].value,new=new,test=row[2].value))
assert len(sitc_edits)==68,len(sitc_edits)
physical,positions=physicalize(rows);diffphysical,diffpositions=physicalize(diff)
plan={'suvc':{'rows':rows,'diff':diff,'pairs':pairs,'physical':physical,'positions':positions,'diffphysical':diffphysical,'diffpositions':diffpositions,'original_rows':original_rows,'first_draft_rows':first_draft_rows,'code_rows':code_rows},'sitc':sitc_edits,
 'sitc_before_hash':hashlib.sha256(before.read_bytes()).hexdigest()}
(QA/'tables-plan.json').write_text(json.dumps(plan,ensure_ascii=False,indent=2),encoding='utf8')
print('SUVC',len(rows),'diff',len(diff),'SITC affected cells',len(sitc_edits))
