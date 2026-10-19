"""Publish full expected log text using the unchanged production test formatter.

Only a documentation helper runs. No product function or hardware test runs.
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import re
import subprocess
from urllib.parse import quote

ROOT=Path(__file__).resolve().parents[5]
TASK=Path(__file__).resolve().parents[1]
QA=TASK/'qa/expected-io'
OUT=ROOT/'docs/참고/인허가/사운드1/DHF/04_설계 검증/SW/펌웨어/유닛 시험 예상 입출력'
LOGS=OUT/'예상 로그'
EXE=ROOT/'tmp/expected-io/expected_log_formatter.exe'

def read(p):return json.loads(p.read_text(encoding='utf-8'))
def pretty(value):
    return value if isinstance(value,str) else json.dumps(value,ensure_ascii=False,separators=(',',':'))
def table(value):return pretty(value).replace('|','\\|').replace('\n','<br>')

def field_description(field):
    names={'index':'배열 인덱스','index_range':'배열 범위','range':'확인 범위','reason':'변동 이유',
           'unit':'단위','lower_index':'하한 인덱스','upper_index':'상한 인덱스',
           'variable_reason':'변동 이유','raw_range':'원시값 범위','comparison':'비교 방법',
           'reference_by_vector':'벡터별 기준','compared_raw_indices':'비교 인덱스',
           'unchecked_values':'별도 확인값','expected_relation':'값 사이의 조건'}
    return '<br>'.join(names.get(k,k)+': '+table(v) for k,v in field.items() if k not in ('name','field'))

def boot_export():
    result=[]
    for u in read(QA/'boot.json')['units']:
        rows=[];pc=[]
        for c in u['cases']:
            if not re.fullmatch(r'C\d+',c['key']):continue
            values=[f['value'] for f in c['expected_fields']]
            assert all(type(x) is int for x in values)
            record=dict(case=int(c['key'][1:]),expected=values,mode='pc',log_type='BOOT ACTUAL/EXPECTED',notes=c['title'],known_failure=c.get('known_failure',False))
            pc.append(record)
            if c.get('target_report_pass_if_all_match'):
                rows.append({**record,'mode':'target','log_type':'tdc_ut_log'})
        result.append(dict(id=u['id'],file=u['file'],records=rows,pc_only_records=pc,variable_records=[],notes='BOOT HOST는 PC 전체 벡터, RTT는 타깃에 연결된 RAM 부분집합이다. 실제 BOOT를 실행한 로그가 아니다.'))
    return dict(role='boot',units=result)

def formatter(id_,record):
    core,n=id_.split('-');expected=record['expected'];case=int(record['case'])
    values=record.get('emitted_values',expected)
    status=record.get('expected_status','PASS')
    compare=status!='OBSERVED'
    failures=sum(a!=b for a,b in zip(values,expected)) if compare else 0
    assert len(values)==len(expected)
    assert status==('OBSERVED' if not compare else 'FAIL' if failures else 'PASS')
    assert values and all(type(x) is int and -(2**31)<=x<2**31 for x in values)
    source=f'{core} {int(n)} {case} {len(values)} {int(compare)}\n'+ ' '.join(map(str,values))+'\n'+' '.join(map(str,expected))+'\n'
    run=subprocess.run([str(EXE)],input=source.encode('ascii'),capture_output=True,check=True,timeout=10)
    raw=run.stdout
    assert raw.endswith(b'\r\n') and b'\r\r' not in raw
    lines=raw.splitlines(keepends=True)
    assert all(len(line)<=60 and line.endswith(b'\r\n') for line in lines)
    decoded=raw.decode('ascii');got=[]
    for line in decoded.splitlines():
        m=re.fullmatch(r'\[UT-[A-Z0-9]+-\d{3}\] C\d+ A\[(\d+):(\d+)\]=([-\d,]+)',line)
        if m:
            assert line.startswith(f'[UT-{core}-{int(n):03d}] C{case} ')
            nums=list(map(int,m[3].split(',')))
            assert int(m[1])==len(got) and int(m[2])==len(got)+len(nums)-1
            got.extend(nums)
    assert got==values
    mismatch_lines=[line for line in decoded.splitlines() if re.search(r' I\d+ A=',line)]
    assert len(mismatch_lines)==failures
    assert decoded.splitlines()[-1]==f'[UT-{core}-{int(n):03d}] C{case} {status} N={len(values)} F={failures}'
    return decoded.replace('\r\n','\n').rstrip(),len(lines),len(values)

def pc_text(id_,record):
    if record.get('expected_log') is not None:return pretty(record['expected_log'])
    if record.get('expected_text') is not None:return record['expected_text'].rstrip('\n')
    if record.get('log_type')=='literal_stdout':return record['text']
    if record.get('log_lines') is not None:return '\n'.join(record['log_lines'])
    vals=record.get('expected');kind=record.get('log_type','')
    if vals is None:return None
    if kind=='BOOT ACTUAL/EXPECTED':
        data=[-999,int(id_.split('-')[1]),int(record['case']),len(vals),*vals]
        return 'ACTUAL '+' '.join(map(str,data))+'\nEXPECTED '+' '.join(map(str,data))
    if 'tdc_ut_cm3_extra_host.c' in kind:
        data=dict(unit=int(record.get('unit',id_.split('-')[1])),case=int(record['case']),field=record['field'],actual=vals,expected=vals,known_failure=record.get('known_failure',False))
        return 'RECORD '+json.dumps(data,separators=(',',':'))
    return None

def record_label(record):
    return (f"C{record['case']}" if record.get('case') is not None else record.get('key','출력 확인'))+(f" / {record['scenario']}" if record.get('scenario') else '')

def unique_pc(records):
    seen=set();result=[]
    for r in records:
        key=json.dumps({k:r.get(k) for k in ('log_type','case','key','scenario','variant','expected','expected_text','log_lines','text')},sort_keys=True,ensure_ascii=False)
        if key not in seen:seen.add(key);result.append(r)
    return result

def render_pc(id_,records,blocks):
    lines=['## PC에서 확인하는 출력','',
      'PC 출력은 RTT와 구분한다. 시나리오별 초기 상태와 실행 조건은 유닛 설명 문서에 있다. 배열을 출력하지 않는 실행기는 요약 줄만 대조하며, PC 출력에는 RTT의 60바이트 한도를 적용하지 않는다.','']
    for b in blocks:
        lines += ['### '+b['label'],'']
        if b.get('notes'):lines += [pretty(b['notes']),'']
        lines += ['```text',b['text'].rstrip(),'```','']
    groups={};special=[]
    for r in records:
        if r.get('log_type') in ('pc_host_unit_summary','pc_extension_summary') and blocks:continue
        text=pc_text(id_,r)
        if text is None:special.append(r);continue
        key=r.get('scenario') or r.get('log_type','출력')
        groups.setdefault(key,[]).append((r,text))
    for key,rows in groups.items():
        label='기본 출력' if key in ('stdout-literal','literal_stdout') else 'PC 전체 벡터' if key=='BOOT ACTUAL/EXPECTED' else key
        lines += ['### '+label,'']
        if any(r.get('known_failure') for r,_ in rows):
            lines += ['아래 actual은 통과 조건의 예상값이다. 현재 알려진 불일치가 있으면 현재 실행 로그와 다르다. known_failure는 시험 시나리오에 붙은 기존 분류이며 제품 합격 표시가 아니다.','']
        text='\n'.join(t for _,t in rows)
        if key=='BOOT ACTUAL/EXPECTED':
            count=sum(len(r['expected'])+4 for r,_ in rows)
            text=f'STATUS {int(id_.split("-")[1])} 0 {count} 1 0\n'+text
        lines += ['```text',text,'```','']
    for r in special:
        lines += ['### '+record_label(r),'']
        if r.get('notes'):lines += [pretty(r['notes']),'']
        if r.get('log_type')=='no-output/assert-only':
            lines += ['전용 출력 문자열이 없는 assert 시험이다. 다음 값은 비교 기준이며 RTT 메시지로 출력되지 않는다.','',
                      '| 확인 항목 | 예상값 |','|---|---|']
            for k in ('expected','injected_predicates','raw_expected','failed','overflow'):
                if k in r:lines.append(f'| {k} | {table(r[k])} |')
            lines.append('')
        elif r.get('expected') is None:
            lines += ['관찰 전용이므로 출력값 전체 일치로 판정하지 않는다. 값의 의미와 확인 범위는 유닛 문서의 해당 조건을 따른다.','']
        elif r.get('variant')=='pc-address-layout':
            lines += ['PC 주소 배치용 입력이다. 아래는 pass=1/2 반복 실행별 예상 RECORD다. 타깃 주소에 그대로 적용하지 않는다.','', '```text']
            for repeat in (1,2):
                data=dict(pass_=repeat,unit=int(id_.split('-')[1]),case=r['case'],actual=r['expected'],expected=r['expected'])
                data={'pass' if k=='pass_' else k:v for k,v in data.items()}
                lines.append('RECORD '+json.dumps(data,separators=(',',':')))
            lines += ['```','']
        else:
            raise AssertionError(('Missing exact PC output treatment',id_,r.get('log_type'),r.get('case')))
    return lines

def publish_tools():
    # Fixed synthetic inputs already specified by the test-tool input document.
    # These are formatter/adapter fixtures, not product units or board evidence.
    def r(case,values,expected=None,status='PASS'):
        return dict(case=case,expected=values if expected is None else expected,
                    emitted_values=values,expected_status=status)
    alternating=[2147483647 if i%2==0 else -2147483648 for i in range(256)]
    expectation=alternating.copy();expectation[-1]=0
    sections=[
      ('L1 / 256개 배열과 의도한 불일치',[('HEAR-999',r(999,alternating,expectation,'FAIL'))]),
      ('L2 / 관찰 전용',[('CFX-001',r(1,[2147483647],status='OBSERVED'))]),
      ('R2 / 정상과 불일치 두 조건',[('CM3-027',r(1,[0,127,255])),('CM3-027',r(2,[-20],[-21],'FAIL'))]),
      ('R3 / 같은 배열의 관찰 전용',[('CM3-027',r(1,[0,127,255],status='OBSERVED')),('CM3-027',r(2,[-20],status='OBSERVED'))]),
      ('R7 / CFX 24비트 부호 복원',[('CFX-017',r(1,[-8388608,-1,8388607]))]),
      ('R8 / HEAR 코어명 전달',[('HEAR-017',r(1,[-8388608,-1,8388607]))]),
      ('R10 / 수치 구간의 원시 관찰',[('CFX-023',r(10,[-20,-21,-19],status='OBSERVED')),('CFX-023',r(11,[40,39,41],status='OBSERVED'))]),
      ('R11 / 헤더 없는 원시 관찰',[('HEAR-002',r(4,[-8388608,-1,8388607],status='OBSERVED'))]),
      ('B2 / BOOT 정상과 불일치',[('BOOT-014',r(1,[-2147483648,2147483647])),('BOOT-011',r(6,[0],[-1],'FAIL'))]),
    ]
    lines=['# 시험 도구용 예상 출력 전문','',
      '[입력과 실행 조건](../공통/시험%20도구%20자체의%20입력과%20예상%20결과.md)','',
      '로그 생성기/전달기 안에서 캡처해 확인하는 문자열이다. 실제 제품 유닛의 시험 결과가 아니다. 오류 주입 조건에서 FAIL이 나오는 것이 도구의 정상 동작이다. PC 콘솔에는 아래 캡처 배열 대신 맨 끝의 도구 완료 문구만 표시할 수 있다.','',
      '각 절은 별도 조건이다. 잘못된 인자/취소 조건에는 완료 로그가 없으므로 입력 문서의 반환값, 중단 위치, CRLF 및 상태를 비교한다. 그런 조건에 가짜 PASS 문자열을 추가하지 않았다.','']
    nrecords=nlines=nvalues=0
    for title,records in sections:
        output=[]
        for id_,record in records:
            text,count,size=formatter(id_,record);output.append(text)
            nrecords+=1;nlines+=count;nvalues+=size
        lines += ['## '+title,'','```text','\n'.join(output),'```','']
    lines += ['## PC 도구 완료 문구','','서로 다른 실행 파일/검사기가 내는 줄이다. 모든 유닛의 합격 표시가 아니다.','', '```text',
      'RTT_LOG PASS: 60B, ASCII, full/partial retry, all 256 values, mismatch, observation',
      'REPORT HOST_PASS: metadata, cancellation, 24bit sign, retry, FAIL/PASS',
      'BOOT_REPORT HOST_PASS: actual adapter, metadata, retry, cancel/restart, FAIL preserved',
      'RTT_WRAPPERS HOST_PASS: truncation, negative, empty, nested buffer','```','']
    (LOGS/'공통-시험도구.md').write_text('\n'.join(lines),encoding='utf-8')
    return dict(records=nrecords,lines=nlines,values=nvalues)

def main():
    roles=('cfx-pure','cfx-io-hear','cm3-pure','cm3-io')
    inputs=[QA/(r+'-logs.json') for r in roles]
    assert all(p.exists() for p in inputs),'Expected-log export not complete'
    exports=[read(p) for p in inputs]+[boot_export()]
    # Keep the archived numeric input manifest, but derive current test-boundary
    # readiness instead of reviving the old CM3-051 SPI-wait warning.
    from expected_io_cm3_io import build_log_export, validate_log_export
    cm3_manifest=read(QA/'cm3-io.json')
    exports[3]=build_log_export(cm3_manifest)
    validate_log_export(cm3_manifest,exports[3])
    units=[u for e in exports for u in e['units']]
    assert len(units)==115 and len({u['id'] for u in units})==115
    LOGS.mkdir(parents=True,exist_ok=True)
    counts=[]
    index=['# 바로 비교하는 예상 로그 전문','',
      '고정 입력이 기대대로 처리되었을 때 보게 될 로그다. 실제 시험 코드가 쓰는 로그 포매터를 그대로 사용해 숫자/인덱스/줄 나눔을 만들었다. 제품 함수를 실행하거나 장치 결과를 수집한 것은 아니다.', '',
      '**비교 방법:** 같은 유닛/입력 조건의 RTT 로그를 아래 문서의 코드 블록과 나란히 놓는다. A 배열 모든 값, 인덱스 범위, 마지막 PASS의 N/F까지 같아야 한다. 현재 알려진 실패가 있는 유닛에는 기대대로 수정/동작했을 때의 비교 기준임을 따로 표시했다.', '',
      'SDK 원본 계산값, HEAR 블록 지수/원시 FFT, 외부 입력을 읽는 관찰 로그는 하나의 문자열로 고정할 수 없다. 해당 문서의 변동값 표에 구간/필드/이유를 적었다. 이 값들을 임의의 숫자로 채워 통과 예시로 만들지 않았다.', '',
      '- [공통 시험 도구의 캡처 문자열과 완료 문구](공통-시험도구.md)',
      '- [감사기록의 조건별 RECORD 전문](../공통/감사기록.md)', '',
      '| 유닛 | 고정 RTT 조건 | PC 출력 조건 | 변동 관찰 조건 |', '|---|---:|---:|---:|']
    for u in sorted(units,key=lambda x:(x['id'].split('-')[0],int(x['id'].split('-')[1]))):
        id_=u['id'];core=id_.split('-')[0]
        target=[r for r in u.get('records',[]) if r.get('mode')=='target']
        pc=unique_pc([r for r in u.get('records',[]) if r.get('mode')=='pc']+u.get('pc_only_records',[]))
        blocks=u.get('pc_output_blocks',[])
        variable=u.get('variable_records',[])
        if not target and not pc and not variable and not blocks:continue
        lines=[f'# {id_} 예상 로그 전문','',f'[입력과 예상 결과 설명](../{core}/{id_}.md)','',
          '고정값 RTT는 배열값과 줄 나눔까지 대조한다. 통과 예상, 관찰 전용, 오류 조건을 구분했으며 일부 값이나 중간 줄을 생략하지 않았다. 실제 로그는 CRLF이며 화면에서는 일반 줄 바꿈으로 표시된다.', '',
          '기존 코드의 알려진 불일치/관찰 전용 조건은 원래 유닛 문서에서 별도로 확인한다. 여기의 PASS 예문이 현재 제품의 합격 결과라는 뜻은 아니다.', '']
        if u.get('notes'):lines += [pretty(u['notes']),'']
        if u.get('target_blocked'):
            lines += ['> **현재 타깃 일괄 실행 중단 조건:** '+pretty(u.get('target_blocked_reason',u.get('reason','유닛 문서의 실행 한계를 확인한다.'))),'',
              '다음 RTT 전문은 통과 조건의 비교 기준이다. 현재 타깃에서 그대로 끝까지 출력된다는 뜻이 아니다. PC 모의 실행과 구분한다.','']
        total_lines=total_values=0
        if target:
            lines += ['## RTT 예상 전문','',
              '같은 실행 경로의 고정 비교 줄을 한 블록에 모았다. 각 C 번호의 입력/실행 방법은 위 유닛 설명 링크에서 확인한다. HEAR는 벡터별 별도 실행이며 원시 관찰 로그는 아래에서 따로 대조한다.','']
            seen=set()
            groups={}
            for r in target:
                key=(int(r['case']),r.get('variant',''))
                assert key not in seen,(id_,key);seen.add(key)
                text,nlines,nvalues=formatter(id_,r)
                total_lines+=nlines;total_values+=nvalues
                group=(r.get('variant',''),r.get('expected_status','PASS'))
                groups.setdefault(group,[]).append((r,text))
            for (variant,status),rows in groups.items():
                label={'PASS':'통과 조건의 고정 비교','FAIL':'시간 초과 오류 조건','OBSERVED':'영 입력의 원시 관찰 출력'}[status]
                if variant:label+=' / '+variant
                lines += ['### '+label,'']
                if variant or status!='PASS':
                    lines += [pretty(rows[0][0].get('condition',rows[0][0].get('notes',''))),'']
                lines += ['```text','\n'.join(t for _,t in rows),'```','']
        if pc or blocks:
            lines += render_pc(id_,pc,blocks)
        if variable:
            lines += ['## 한 가지 숫자로 고정하지 않는 관찰 로그','',
              '이 절은 문자열 전체 일치 대신 필드/범위/상태로 확인한다. `<실행값>`은 표기 설명용이며 실제 RTT에 출력되는 문자열이 아니다. 같은 C 번호가 위 고정 비교와 함께 있으면 실행 경로 또는 배열이 다른 것이다.','']
            for r in variable:
                lines += [f"### C{r.get('case','-')}"+(f" / {r['variant']}" if r.get('variant') else ''),'']
                if r.get('notes'):lines += [pretty(r['notes']),'']
                fields=r.get('fields',[])
                if fields:
                    lines += ['| 확인 필드 | 범위/변동 이유 |','|---|---|']
                    for f in fields:
                        if isinstance(f,dict):
                            label=f.get('name',f.get('field',f.get('index','값')))
                            lines.append(f'| {table(label)} | {field_description(f)} |')
                        else:lines.append(f'| 값 | {table(f)} |')
                    lines.append('')
                for k in ('expected_summary','summary','format','count'):
                    if k in r:
                        label={'expected_summary':'예상 요약 줄','summary':'요약 줄','format':'출력 형식','count':'값 개수'}[k]
                        lines += [label+':','', '```text',pretty(r[k]),'```','']
        path=LOGS/(id_+'.md');path.write_text('\n'.join(lines).replace('·','/'),encoding='utf-8')
        content=path.read_text(encoding='utf-8')
        assert '### CNone' not in content and '이 항목의 확인값/출력 구조:' not in content
        for block in blocks:assert block['text'].rstrip() in content
        for record in target:
            status=record.get('expected_status','PASS')
            summary=f"[UT-{id_}] C{record['case']} {status} N={len(record['expected'])}"
            assert summary in content,(id_,summary)
        counts.append(dict(id=id_,file=path.relative_to(ROOT).as_posix(),target_records=len(target),target_lines=total_lines,target_values=total_values,pc_records=len(pc)+len(blocks),variable_records=len(variable),
                           pass_records=sum(r.get('expected_status','PASS')=='PASS' for r in target),
                           fail_records=sum(r.get('expected_status')=='FAIL' for r in target),
                           observed_records=sum(r.get('expected_status')=='OBSERVED' for r in target)))
        index.append(f'| [{id_}]({id_}.md) | {len(target)} | {len(pc)+len(blocks)} | {len(variable)} |')
    index += ['','분석/육안 전용 등 고정 출력 경로가 없는 유닛은 [전체 입력/출력 목록](../README.md)에서 확인한다.','']
    (LOGS/'README.md').write_text('\n'.join(index),encoding='utf-8')
    tools_proof=publish_tools()
    proof=dict(units=counts,formatter_source_sha256=hashlib.sha256((ROOT/'src/test/tdc_ut_log.c').read_bytes()).hexdigest(),
               export_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},
               total_target_records=sum(x['target_records'] for x in counts),total_target_values=sum(x['target_values'] for x in counts),
               total_target_lines=sum(x['target_lines'] for x in counts),line_limit_including_crlf=60,
               status_record_counts={status:sum(x[status+'_records'] for x in counts) for status in ('pass','fail','observed')},
               common_tool_captures=tools_proof,
               product_functions_executed=False,device_execution=False,expected_used_as_expected_not_measured=True)
    (QA/'full-logs-checks.json').write_text(json.dumps(proof,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in proof.items() if k not in ('units','export_sha256')},ensure_ascii=False))

if __name__=='__main__':main()
