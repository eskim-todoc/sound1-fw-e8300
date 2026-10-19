"""Create the navigation document and check published expected-I/O files."""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import re
from urllib.parse import quote, unquote

ROOT=Path(__file__).resolve().parents[5]
TASK=Path(__file__).resolve().parents[1]
OUT=ROOT/'docs/참고/인허가/사운드1/DHF/04_설계 검증/SW/펌웨어/유닛 시험 예상 입출력'
QA=TASK/'qa/expected-io'
COVERAGE=TASK/'qa/source-preserving-tests/coverage/115-units.json'

def link(label,path):
    return f'[{label}]({quote(path,safe="/")})'

def main():
    coverage=json.loads(COVERAGE.read_text(encoding='utf-8'))
    rows=coverage['units'];assert len(rows)==115
    index=['# 예상 테스트 입력과 출력', '',
      '확인 기준: 2026-10-02 현재 TD24A 유닛 시험 코드와 사전 기대값. 유닛 검증 보고서를 작성할 때 입력 조건과 예상 결과를 찾는 자료다. 실제 장치 시험 결과는 아니다.', '',
      '## 먼저 볼 문서', '',
      '- '+link('유닛 시험 일괄 실행 매뉴얼','../유닛 시험 일괄 실행 매뉴얼.md')+': GUI 명령, 자동 묶음, 실제 입력 단계와 사진/로그 저장 순서.',
      '- '+link('바로 비교하는 예상 로그 전문','예상 로그/README.md')+': 실제 포매터로 만든 A 배열 전량/줄 나눔/PASS와 PC 출력. 고정할 수 없는 관찰값은 별도 구분.',
      '- '+link('실행 조건과 예상 로그 읽기','공통/실행과 로그 읽기.md')+': 초기 상태, PC/타깃 구분, RTT 배열/판정 읽는 법.',
      '- '+link('시험 도구 자체의 입력과 예상 결과','공통/시험 도구 자체의 입력과 예상 결과.md')+': 로그 길이/재시도, 전달기, 오류 주입과 검사기 시험.',
      '- '+link('감사기록 25개 조건 / 93개 비교값','공통/감사기록.md')+': RAM 파일 초기화, 기록/읽기/순환 저장과 실패 반환.',
      '- '+link('전체 검사 실행 항목과 문서 대응','공통/전체 검사 대응표.md')+': 공통 실행기의 모든 단계가 어느 문서에 설명되어 있는지 확인.', '',
      '## 범위와 사용 방법', '',
      '현재 유닛 115개(CFX 42, HEAR 2, CM3 51, BOOT 20)를 모두 연결했다. PC 고정 입력이 있는 번호는 90개이며, 일부 경로만 시험한 번호와 비활성 구형 보드 분기도 포함한다. 나머지는 코드 분석 또는 실제 장치/육안 확인 조건으로 정리했다. 115개 전체가 자동 실행되거나 통과했다는 뜻은 아니다.', '',
      '1. 아래에서 유닛을 열고 시험 환경/초기 상태를 확인한다.',
      '2. 각 조건의 입력과 실행 동작을 적용한다. 배열은 인덱스와 단위까지 같이 읽는다.',
      '3. 실행값/로그를 같은 조건의 사전 기대값과 비교한다. 범위 판정은 미리 정한 하한/상한을 사용한다.',
      '4. 현재 불일치와 관찰 전용 항목은 합격란에 옮기지 않는다. 실제 증거를 얻은 뒤 결과를 작성한다.', '',
      'CFX 계산/IO, CM3 계산/IO, HEAR, BOOT의 번호와 벡터 번호를 유지했다. PC 보충 시나리오와 반복 실행을 타깃의 기본 벡터와 구분했다. 구현 위치는 근거용이며 문서 본문에 입력/예상값을 함께 담았다.', '',
      '## 현재 불일치를 먼저 확인할 번호', '',
      '| 구분 | 확인할 항목 |', '|---|---|',
      '| 동작 의도/기준 해석 검토 | CFX-015 복사 완료, CM3-006 같은 맵 재적재, CM3-030 같은 상태 변경 알림 |',
      '| 기대값과 다른 제품 동작 | CFX-033 작은 밴드 패킷, CM3-028/029 전하량/실패 후 완료, CM3-042/043 저장 실패 응답, CM3-046/049/051 명령 처리, BOOT-007/011 실패 반환, 감사기록 C24/C25 |',
      '| 적용 조건 주의 | CM3-039는 현재 보드에서 사용하지 않는 구형 PMIC 분기. CFX-033에는 정상 CM3 설정에서 발생하지 않는 조합도 별도 포함 |',
      '| 판정 전 관찰/장치 증거 필요 | CFX 실제 SDK 수치, HEAR, LED/물리 출력/실제 저장소, 계산 실패 후 T/C 처리 정책, QCC 내부 동작 |', '',
      '**CM3-051 보완:** 일괄 실행 작업에서 시험용 SPI 완료 처리를 추가했다. R17/R18의 두 응답 사이에서 RAM 시험이 멈추지 않도록 한 것으로, 오류 응답 4필드 불일치와 기대값은 그대로다. PC 회귀/SDK 링크와 실제 보드 실행은 구분하며 RTT 전문은 여전히 사전 비교 기준이다.', '',
      '## 유닛별 목록', '']
    status={'analysis':'코드 분석','fixed_vectors':'고정 입력','partial_vectors':'부분 입력/추가 확인','target_required':'장치 실행 필요','visual_required':'육안 확인 필요','host_and_target_subset':'PC 입력/타깃 일부'}
    missing=[]
    for core in ('CFX','HEAR','CM3','BOOT'):
        group=sorted((r for r in rows if r['core']==core),key=lambda r:r['unit'])
        index += [f'### {core} ({len(group)}개)', '', '| 입력/예상값 문서 | 시험 대상 | 예상 로그 전문 | 현재 준비 방식 |','|---|---|---|---|']
        for row in group:
            id_=f"{core}-{row['unit']:03d}"
            relative=f'{core}/{id_}.md'
            if not (OUT/relative).exists():missing.append(relative)
            log_path=f'예상 로그/{id_}.md'
            log_link=link('로그 보기',log_path) if (OUT/log_path).exists() else '분석/육안 확인'
            index.append(f"| {link(id_,relative)} | {row['name'].replace('·','/')} | {log_link} | {status.get(row['kind'],row['kind'])} |")
        index.append('')
    index += ['## QCC 자료 추가 위치', '',
      '`(QCC 검증 결과 필요)`는 현재 E8300의 모의 통신 상대나 상태 입력만으로 QCC 내부 동작을 증명할 수 없는 부분이다. CM3-003/008/045~051, CFX의 I2S/오디오 경로 및 감사기록의 연계 설명과 대조한다. QCC 내부 유닛 번호나 합격 로그는 자료 없이 만들지 않았다. 자료 수령 후 대상 문서의 입력/예상 결과와 확인 범위를 보완하고 중앙 QCC 위치 목록도 함께 갱신한다.', '',
      '## 문서 작성 기준', '',
      '- 원래 기대값과 독립 수식/판정 구간을 사용한다. 현재 실행값에 맞춰 기대값을 바꾸지 않는다.',
      '- `OBSERVED`와 분석/육안 항목에 존재하지 않는 RTT 합격 로그를 붙이지 않는다.',
      '- 오실로스코프 확인이 필요한 경우에만 실제 핀/물리량 기준을 쓴다. 모의 레지스터 값으로 실제 파형을 측정한 것처럼 쓰지 않는다.',
      '- 원본 제품/시험 코드, Word/Excel 기준서와 필수성능시험 기준서는 이번 정리에서 변경하지 않았다.', '']
    OUT.mkdir(parents=True,exist_ok=True)
    if missing:
        index[2:2]=[f'> 작성 중: 유닛 문서 {115-len(missing)}/115개 생성. 아직 없는 문서 링크는 작성 완료 후 연결된다.', '']
    (OUT/'README.md').write_text('\n'.join(index),encoding='utf-8')
    suite=json.loads((TASK/'qa/source-preserving-tests/suite/checks.json').read_text(encoding='utf-8'))
    targets={
      'suite-runner-checker':'시험 도구 문서 7절 / 이전 성공 무효화, 컴파일 실패, 시간 초과, 도구 없음',
      'source-preservation-checker':'시험 도구 문서 6절 / 원본 비교기의 경로/해시 입력',
      'log-build':'시험 도구 문서 1절의 실행 파일 컴파일', 'log':'시험 도구 문서 1절 / 256값, 잘림/재시도/취소/잘못된 인자',
      'report-build':'시험 도구 문서 2절의 실행 파일 컴파일', 'report':'시험 도구 문서 2절 / 헤더, 24비트 부호, 관찰, 취소',
      'boot-report-build':'시험 도구 문서 3절의 실행 파일 컴파일', 'boot-report':'시험 도구 문서 3절 / BOOT 배출/취소/불일치',
      'check_rtt_wrappers':'시험 도구 문서 4절 / 긴 문자열, 포맷 오류, 빈 문자열, 중첩 호출',
      'check_cfx_analysis':'CFX의 분석 유닛 / 인터럽트, 초기화, 오디오 경로, 타이머/완료 경로',
      'check_cfx_vectors':'CFX-008/013/016/017/018/020/022/023/024/025/028/029/030/031/032/034/035',
      'check_cfx_numeric_reference':'CFX-023/031/032 / 독립 수치 구간과 오류 주입',
      'tdc_ut_hear_reference_check':'HEAR-001/002 / 독립 기준과 합성 RAM을 이용한 비교기/수명 시험. 실제 HEAR 출력 아님',
      'tdc_ut_cfx_io_check':'CFX-007/010/011/012/015/019/026/027/033/036/037/040',
      'tdc_ut_cm3_run':'CM3 계산/상태 벡터 문서 / 21개 ID의 고정 입력',
      'tdc_ut_boot_check':'BOOT 문서 / PC 및 RAM 부분집합',
      'cm3-io':'CM3-002/010/025/026/034~040/042/043/045~051 / PC 보충 포함',
      'cm3-io-ram':'같은 CM3 IO 문서의 타깃 RAM 지원 부분집합',
      'cm3-state':'CM3-004/006/030 / 실제 상태 함수, 사용자 관찰 경계, 복원',
      'cm3-extra':'CM3-021/022/029/031/032/033/041 / 새 프로세스의 상태/계산 호출자 조건',
      'event-log':'감사기록 문서 / 25조건, 93비교',
      'event-log-checker':'시험 도구 문서 5절 / 누락/중복/기대값 변조 거부',
      'remaining-analysis':'분석 유닛 문서 / 미자동화 경로의 심볼/설정 연결',
      'coverage':'시험 도구 문서 8절 및 전체 목록 / 115유닛 대응',
      'target-artifacts':'시험 도구 문서 8절 / 심볼/입력 해시/공유주소/RAM/BRAM/인코딩',
      'stack-budget':'시험 도구 문서 8절 / 선택 함수의 단일 스택 프레임',
      'source-preservation':'시험 도구 문서 8절의 소스 동일성 및 실행 전후 보존 검사',
    }
    for core in ('CFX','CM3','BOOT'):
        for kind in ('test','normal'):
            targets[f'{core}-{kind}']=f'시험 도구 문서 8절 / {core} '+('시험' if kind=='test' else '정상')+' SDK 빌드/링크'
    assert set(targets)=={r['name'] for r in suite['results']}
    table=['# 전체 검사 실행 항목과 문서 대응','',
      '현재 공통 실행기의 SDK/보존 검사 포함 33단계를 빠짐없이 연결했다. 제품 유닛 입력은 각 코어 문서, 공통 도구 입력은 아래 문서에서 찾는다.', '',
      '- '+link('시험 도구 자체의 입력과 예상 결과','시험 도구 자체의 입력과 예상 결과.md'),
      '- '+link('감사기록 입력과 예상 결과','감사기록.md'),
      '- '+link('115개 유닛 전체 목록','../README.md'), '',
      '각 단계가 검사 자체의 조건을 만족하면 공통 실행기에는 `<단계 이름>: HARNESS_OK`가 표시된다. 알려진 제품 실패를 정확히 재현해도 검사기는 정상일 수 있다. 단계 성공과 유닛 통과를 구분한다.', '',
      '| 순서 | 실행 단계 | 입력과 예상 결과를 볼 위치 |','|---:|---|---|']
    for i,row in enumerate(suite['results'],1):table.append(f"| {i} | `{row['name']}` | {targets[row['name']]} |")
    table += ['', '실행 기준: `tests/unit/run_suite.py`. 원본 보존 옵션을 사용할 때는 작업 시작 전에 만든 기준 파일을 지정한다. 이번 문서화를 위해 장치를 실행하거나 33단계를 다시 실행한 기록은 아니다. 단위별 사전 기대값과 이전 실행 근거를 현재 소스와 대조해 문서화했다.', '']
    (OUT/'공통/전체 검사 대응표.md').write_text('\n'.join(table),encoding='utf-8')
    bad_links=[];bad_chars=[];bad_tables=[];bad_fences=[];files=[]
    for p in OUT.rglob('*.md'):
        content=p.read_text(encoding='utf-8')
        if '\ufffd' in content:bad_chars.append(p.relative_to(OUT).as_posix())
        # Ignore fenced code; validate local Markdown destinations rather than symbol text.
        no_code=re.sub(r'```.*?```','',content,flags=re.S)
        if sum(line.startswith('```') for line in content.splitlines())%2:
            bad_fences.append(p.relative_to(OUT).as_posix())
        columns=None
        for line_number,line in enumerate(no_code.splitlines(),1):
            if line.startswith('|') and line.rstrip().endswith('|'):
                count=len(re.split(r'(?<!\\)\|',line))-2
                if columns is None:columns=count
                elif count!=columns:bad_tables.append(dict(file=p.relative_to(OUT).as_posix(),line=line_number,expected=columns,found=count))
            else:columns=None
        no_inline_code=re.sub(r'`[^`\n]*`','',no_code)
        for dest in re.findall(r'\]\(([^\n)]+)\)',no_inline_code):
            dest=dest.strip('<>').split('#')[0]
            if not dest or re.match(r'[a-z]+://',dest):continue
            target=p.parent/unquote(dest)
            if not target.exists():bad_links.append({'file':p.relative_to(OUT).as_posix(),'target':dest})
        files.append(dict(file=p.relative_to(OUT).as_posix(),bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
    manifests={};registered=[];bad_registration=[]
    for role in ('cfx-pure','cfx-io-hear','cm3-pure','cm3-io','boot','common'):
        p=QA/(role+'.json')
        manifests[role]=dict(exists=p.exists(),sha256=hashlib.sha256(p.read_bytes()).hexdigest() if p.exists() else None)
        if p.exists():
            data=json.loads(p.read_text(encoding='utf-8'))
            for unit in data.get('units',[]):
                id_=unit['id'];registered.append(id_)
                expected_file=OUT/id_.split('-')[0]/(id_+'.md')
                if (ROOT/unit['file']).resolve()!=expected_file.resolve():bad_registration.append(dict(id=id_,file=unit['file']))
    expected_ids={f"{r['core']}-{r['unit']:03d}" for r in rows}
    registration=dict(registered=len(registered),duplicates=sorted({x for x in registered if registered.count(x)>1}),missing=sorted(expected_ids-set(registered)),extra=sorted(set(registered)-expected_ids),wrong_files=bad_registration)
    report=dict(unit_count=115,missing_unit_files=missing,broken_links=bad_links,damaged_text=bad_chars,ragged_tables=bad_tables,unclosed_fences=bad_fences,
                manifests=manifests,registration=registration,files=files,bytes=sum(x['bytes'] for x in files),device_execution=False)
    (QA/'publication-checks.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(dict(unit_count=115,files=len(files),missing=len(missing),broken_links=len(bad_links),damaged_text=len(bad_chars),ragged_tables=len(bad_tables),unclosed_fences=len(bad_fences),bytes=report['bytes']),ensure_ascii=False))
    assert not missing,f'{len(missing)} missing documents; see publication-checks.json'
    assert not bad_links,f'{len(bad_links)} broken links; see publication-checks.json'
    assert not bad_chars,f'{len(bad_chars)} damaged text files'
    assert not bad_tables,f'{len(bad_tables)} malformed table rows; see publication-checks.json'
    assert not bad_fences,f'{len(bad_fences)} unclosed code fences'
    assert all(v['exists'] for v in manifests.values()),manifests
    assert len(registered)==115 and not any(registration[k] for k in ('duplicates','missing','extra','wrong_files')),registration

if __name__=='__main__':main()
