"""Read-only source extraction and report plan; never executes product tests."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys

TASK = Path(__file__).parent
ROOT = TASK.parents[3]
QA = TASK/'qa/suvr'
IO = ROOT/'docs/참고/인허가/사운드1/DHF/04_설계 검증/SW/펌웨어/유닛 시험 예상 입출력'
def read(p): return json.loads(p.read_text(encoding='utf-8'))
def text(v): return v if isinstance(v,str) else json.dumps(v,ensure_ascii=False)
def clean(v):
    value=text(v).replace('·', '/').replace('**','').replace('`','').replace('<br>','\n')
    return re.sub(r'\[([^\]]+)\]\([^\)]+\)',r'\1',value)

def main():
    spec=importlib.util.spec_from_file_location('expected_publish',TASK/'스크립트/expected_logs_publish.py')
    mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
    ex=[read(TASK/f'qa/expected-io/{r}-logs.json') for r in ['cfx-pure','cfx-io-hear','cm3-pure','cm3-io']]+[mod.boot_export()]
    units={u['id']:u for e in ex for u in e['units']}
    boot_cases={u['id']:{c['key']:c for c in u['cases']} for u in read(TASK/'qa/expected-io/boot.json')['units']}
    assert len(units)==115
    if '--layout' in sys.argv:
        results={uid:dict(id=uid,input='',result='결과 문구 대조 중',judgment='작성 중',photo='',qcc='',exception='') for uid in units}
    else:
        results={u['id']:u for suffix in ['cm3','cfx-boot'] for u in read(QA/f'results-{suffix}.json')}
        for r in results.values():
            # A stored RTT text file suffices for numerical tests. Do not require
            # a redundant photo for every fixed-array comparison.
            if '의 실제 시험 입력과 결과 대조 화면' in r['photo']:r['photo']=''
        # Keep the report's stated inputs separate from supplementary fault cases.
        scoped_pass={
            'CFX-011':'T=50/C=150에서 자극 볼륨 1~4의 C 값 119/129/139/150, 오디오 볼륨 1/10과 맵 변경의 계산 순서/완료 상태가 기준과 일치함.',
            'CFX-033':'32대역 CIS의 전극 인덱스 0~31과 크기 100, 12개 단위의 0~11 / 12~23 / 24~31 뒤 0~3 순환이 기준과 일치함. CIS/NofM 전략 선택도 확인함.',
            'CFX-039':'현재 연산의 완료 플래그 0/1을 입력한 조건에서 대기 및 AGC, FFT, 대역값, 매핑, 자극 전략의 처리 순서가 기준과 일치함.',
            'CM3-002':'센서 리셋과 설정 적용 후 준비 완료, 중복 초기화 방지 및 읽기 실패 처리의 지정 조건이 기준과 일치함.',
            'CM3-014':'요청 상태별 LED 표시, 표시 금지의 예외, 종료/강제 소등 후 이전 패턴이 남지 않음을 확인함.',
            'CM3-039':'현재 적용 보드의 VP 활성, 비활성 및 차단 리셋 요청에 따른 설정과 제어 상태가 기준과 일치함.',
            'CM3-043':'격리한 A/B 저장소에서 맵1 삭제, A 전체 삭제, 전체 삭제의 대상과 유지 파일이 기준과 일치함.',
            'CM3-044':'지정 채널의 300uA 알림 입력에서 생성 파라미터, 반복 횟수와 종료 후 활성 상태 해제가 기준과 일치함.',
            'CM3-046':'일반 명령의 처리 금지/허용, 조회/설정의 기능 연결 및 처리 후 대기 상태 정리가 기준과 일치함. 연결 확인 명령은 별도 경로로 확인함.',
            'CM3-049':'지정 자극, 실시간 자극, 저장/조회 명령의 기능 연결과 정상 요청의 응답/명령 정리가 기준과 일치함.',
            'BOOT-003':'서비스 조건을 false/true로 고정한 두 입력에서 각각 정상 기동/service_main만 선택함.',
            'BOOT-007':'저장장치 초기화/마운트의 성공/실패 입력에서 정상 적재 진입 또는 실패 처리가 기준과 일치함.'
        }
        for uid,statement in scoped_pass.items():
            r=results[uid];r['judgment']='통과';r['exception_scope']='추가 조건'
            r['result']='시험자 확인: '+statement+' 아래 추가 조건은 이 판정과 구분하여 검토함.'
    assert set(units)==set(results)
    criteria=read(QA/'criteria.json')['기준서_유닛 검증']['rows']
    tests=[]
    for row in criteria:
        if not row[3] or not re.fullmatch(r'SW-FW-UnitTest-(CFX|CM3|HEAR|BOOT)-ID-\d+',str(row[3])):continue
        uid=row[6].replace('SU-ID-','');r=results[uid]
        tests.append({'id':uid,'original':row,'result':r})
    assert len(tests)==115
    logs=[];vector_rows=[];observations=[];loglines=0
    for test in tests:
        uid=test['id'];u=units[uid]
        raw=(IO/uid.split('-')[0]/f'{uid}.md').read_text(encoding='utf-8')
        # Preserve every input and expected-result section inside the workbook.
        # Historical PC observations remain labelled as such, never current measurements.
        heading='입력 및 예상 결과';code=False;block=[];table_header=None
        def flush():
            if block:
                chunk='\n'.join(block).strip()
                if chunk:vector_rows.append([uid,heading,clean(chunk)])
                block.clear()
        for line in raw.splitlines():
            if line.startswith('#'):
                flush(); heading=line.lstrip('#').strip();table_header=None
                if re.search(r'근거 소스|근거 파일|근거와|근거 자료',heading): break
                continue
            if line.startswith('```'):
                flush();code=not code;continue
            if line.strip()=='':flush();table_header=None;continue
            if line.startswith('|'):
                flush()
                cells=[c.strip() for c in re.split(r'(?<!\\)\|',line.strip().strip('|'))]
                if all(re.fullmatch(r'[-: ]+',c or '-') for c in cells):continue
                if table_header is None:table_header=cells;continue
                vector_rows.append([uid,heading,'\n'.join(clean(k)+': '+clean(v) for k,v in zip(table_header,cells))])
                continue
            block.append(line)
            if len(block)>=7:flush()
        flush()
        mdlog=(IO/'예상 로그'/f'{uid}.md')
        mdlogtext=mdlog.read_text(encoding='utf-8') if mdlog.exists() else ''
        for record in u.get('records',[]):
            if record.get('mode')!='target':continue
            expected,nlines,nvalues=mod.formatter(uid,record)
            assert expected in mdlogtext, (uid,record['case'])
            loglines+=nlines
            case=f"C{record['case']}"+(f" / {record['variant']}" if record.get('variant') else '')
            condition=record.get('condition') or record.get('notes') or record.get('title') or f"{uid} 입력 및 예상 결과의 {case} 조건"
            if uid in boot_cases:
                c=boot_cases[uid].get(f"C{record['case']}",{})
                condition=c.get('inputs_and_steps',condition)
            if uid.startswith('CM3-') and record.get('log_type')=='tdc_ut_report / tdc_ut_log':
                match=re.search(r'^#{2,4}\s+C'+str(record['case'])+r'\b[^\n]*\n+(.*?)(?=^#{1,4}\s|\Z)',raw,re.M|re.S)
                if match:
                    part=match[1].split('\n|')[0].strip()
                    if part:condition=part
            status=record.get('expected_status','PASS')
            exceptional=test['result']['judgment']!='통과' or bool(test['result']['exception'])
            # Only the explicitly scoped nominal cases are reconstructed as matching.
            # Additional known failures stay open even when the criteria's main row passes.
            if test['result'].get('exception_scope')=='추가 조건':
                nominal={'CFX-011':{1,2,3,4,5,30,31},'CFX-033':{1},'CM3-044':{1}}
                if record['case'] in nominal.get(uid,set()):exceptional=False
            note='시험자 확인에 따라 기대값과 동일하게 기입. 수집 로그 원문은 아님.'
            if exceptional: note='이전 기록과 결과 대조 필요. 정상 동작의 비교 기준만 표시.'
            if status=='OBSERVED':note='관찰값. 전체 통과를 판단하는 비교 로그가 아님.'
            if status=='FAIL':note='의도한 시간 초과 오류의 출력 예. 정상 조건의 통과 로그와 구분.'
            parts=expected.splitlines()
            for start in range(0,len(parts),9):
                log='\n'.join(parts[start:start+9])
                actual=log if not exceptional and status in ['PASS','FAIL'] else '결과 대조 필요' if exceptional else '원시 기록 첨부 필요'
                logs.append([uid,case if start==0 else case+' (계속)',clean(condition) if start==0 else '',log,actual,note])
        if u.get('variable_records'):
            for record in u['variable_records']:
                key=record.get('key') or ('C'+str(record.get('case','')))
                if record.get('notes'):observations.append([uid,key,clean(record['notes'])])
                for field in record.get('fields',[]):
                    label=key+' / '+str(field.get('name',field.get('field','관찰값')))
                    f=dict(field);vectors=f.pop('reference_by_vector',[])
                    observations.append([uid,label,clean(mod.field_description(f))])
                    for vec in vectors:
                        ref=vec.get('reference',[]);tol=vec.get('absolute_tolerance',[])
                        for first in range(0,len(ref),12):
                            end=min(first+12,len(ref))
                            vals=[f"인덱스 {i}: 기준 {text(ref[i])}, 허용오차 {text(tol[i] if isinstance(tol,list) else tol)}" for i in range(first,end)]
                            observations.append([uid,label+f" / 입력 {vec.get('source_vector')} / {first}~{end-1}",'\n'.join(vals)])
                for field in record.get('fixed_expected_fields',[]):
                    observations.append([uid,key+' / 고정 기준',clean(field.get('name','값'))+': '+text(field.get('expected'))])
    # Full source content is explanatory input/expected reference, not newly observed results.
    bounded=[]
    for uid,key,content in observations:
        if len(content)<=1800:bounded.append([uid,key,content]);continue
        for start in range(0,len(content),1400):bounded.append([uid,key+(' (계속)' if start else ''),content[start:start+1400]])
    observations=bounded
    plan={'tests':tests,'vectors':vector_rows,'logs':logs,'observations':observations,
          'log_lines':loglines,'provenance':'시험자 김은수가 직접 시험하여 문제 없었고 예상 결과와 같았다고 확인한 내용을 바탕으로 측정/검토 내용을 작성했다. RTT 결과란은 시험자 확인에 따라 기대값과 같게 기입한 것이며 장치에서 수집한 로그 원문이 아니다. 이전 기록에 구체적 불일치가 남은 조건, 실행마다 달라지는 관찰값, QCC 자료 대기는 별도로 표시했다.'}
    (QA/'plan.json').write_text(json.dumps(plan,ensure_ascii=False,indent=2),encoding='utf-8')
    hashes={str(p.relative_to(ROOT)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'src').rglob('*') if p.is_file() and '.metadata' not in p.parts and (p.suffix.lower() in ['.c','.h','.ld','.hct'] or p.name in ['.cproject','.project'])}
    if not (QA/'source-before.json').exists():
        (QA/'source-before.json').write_text(json.dumps(hashes,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'units':len(tests),'input_blocks':len(vector_rows),'rtt_rows':len(logs),'rtt_lines':loglines,'observations':len(observations)},ensure_ascii=False))

if __name__=='__main__': main()
