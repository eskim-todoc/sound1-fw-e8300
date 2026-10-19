"""Add the agreed implant gate to design and test documents, never firmware."""
from pathlib import Path
import hashlib
import json
import shutil
import zipfile
import openpyxl
from lxml import etree as E
from inspect_audit_units import FILES, TASK
from inspect_sad_unit_trace import N, q, text, ROOT
from patch_audit_designs import edit_p
from preserve_audit_workbooks import unpack, value, text_runs

QA = TASK / 'qa/audit-query-guard-20261012'
KINDS = ('sddd', 'suvc', 'suvr')
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
pad = lambda s: '\n' + s.strip() + '\n'
PURPOSE = 'CM3의 여러 제어 기능을 같은 반복 흐름에서 순서대로 진행한다. 감사기록 조회는 등록된 내부기 정보가 실제 내부기 정보와 일치하고 정상 연결된 경우에만 허용한다.'
FIELDS = {
 '입력': '주기 이벤트, 터치/통신 상태, 등록 내부기 정보와 실제 내부기 정보의 일치 여부, 현재 내부기 연결 상태, 개발/점검용 감사기록 조회 명령',
 '출력': '각 기능 처리와 정상/절전 전환, 조회 허용 또는 차단, 허용 시 저장된 감사기록의 시간/주소/이벤트 출력',
 '처리': '입력 상태를 수집하고 시스템 제어, 맵 갱신, 내부기 실행, 통신 처리를 진행한다. 종료 조건에서는 반복 실행을 해제하고 절전 경로로 전환한다.\n--dump_log 요청마다 등록 정보 일치와 정상 연결을 확인한다. 두 조건이 모두 충족된 경우에만 감사기록 읽기 함수를 호출한다. 미연결, 정보 불일치 또는 연결 해제 시 읽기와 저장 기록 출력을 차단한다.\n허용된 조회는 파일을 읽기 전용으로 열어 크기, 식별자, 기록 수와 다음 위치를 검사한다. 유효한 기록의 시간/블루투스 주소/이벤트를 저장 배열 순서대로 RTT에 출력하고 저장 내용은 변경하지 않는다.',
 '관련 함수': 'func_normal, disable_iteration / tdc_ui_command_poll, handle_dump_log, ci_event_log_read\n연결 상태: snd_isd_interface_get_state (SU-ID-CM3-030)\n실제 내부기 정보: read_Connected_ISD_id (SU-ID-CM3-031)',
 '동작 조건': 'SU-ID-CM3-031의 실제 내부기 정보를 등록 정보와 대조하고 SU-ID-CM3-030의 현재 연결 상태를 확인한다. 정보 확인 전이나 초기화 중에는 조회를 차단한다. 연결 해제 후에는 이전 허용 상태를 사용하지 않는다.\n허용된 읽기는 파일 열기/읽기 또는 기록 정보 검사 실패 시 -1, 정상 완료 시 0을 반환한다. 조회 차단과 파일 읽기 오류를 구분한다.\n연/월/일/시/분/초, 주소 6바이트, 이벤트 4바이트를 표시한다. 기록이 0개면 식별자/기록 수/다음 위치만 출력한다. 순환 배열은 시간순으로 다시 정렬하지 않는다.'
}
NOTE = '[E8300 반영] 등록 내부기 정보 일치와 정상 연결을 모두 확인한 뒤 조회를 실행하도록 반영한다.\n[E8300 확인] 로그 외부 전송 시 기밀성 보호 조건과 출하 시 디버그 사용 조건은 별도로 확인한다.'

def prepare():
    QA.mkdir(parents=True, exist_ok=True)
    baseline = {}
    for kind in KINDS:
        p = FILES[kind]
        dst = QA / (kind + '-before' + p.suffix)
        if dst.exists():
            assert sha(dst) == sha(p), 'External edit: ' + kind
        else:
            shutil.copyfile(p, dst)
        baseline[kind] = {'path': str(p), 'sha256': sha(p)}
    dhf = FILES['sddd'].parents[3]
    baseline['protected'] = {str(p): sha(p) for p in dhf.rglob('*') if p.suffix in ('.docx','.xlsx') and not p.name.startswith('~$') and p not in [FILES[k] for k in KINDS]}
    baseline['sources'] = {str(p): sha(p) for folder in ('src','tests') for p in (ROOT/folder).rglob('*') if p.suffix.lower() in ('.c','.h','.cpp','.hpp','.py','.mjs','.ps1','.cproject','.project')}
    (QA/'baseline.json').write_text(json.dumps(baseline,ensure_ascii=False,indent=2),encoding='utf8')

    package = unpack(QA/'sddd-before.docx')
    root = E.fromstring(package['word/document.xml'])
    body = root.find(q('body'))
    start = next(i for i,p in enumerate(body) if text(p).startswith('SU-ID-CM3-005,'))
    end = next(i for i in range(start+1,len(body)) if text(body[i]).startswith('SU-ID-CM3-006,'))
    changed = []
    edit_p(body[start+1],PURPOSE)
    changed.append(start+1)
    for i in range(start+2,end):
        elem = body[i]
        if elem.tag == q('tbl'):
            for row in elem.findall('w:tr',N):
                cells = row.findall('w:tc',N)
                key = text(cells[0])
                if key in FIELDS:
                    edit_p(cells[1].find('w:p',N), FIELDS[key])
            changed.append(i)
        elif text(elem).startswith('[E8300 확인]'):
            edit_p(elem, NOTE, 'e8300')
            changed.append(i)
    assert len(changed) == 4, changed
    package['word/document.xml'] = E.tostring(root,encoding='UTF-8',xml_declaration=True,standalone=True)
    with zipfile.ZipFile(QA/'sddd.docx','w',zipfile.ZIP_DEFLATED) as z:
        for name,data in package.items(): z.writestr(name,data)
    (QA/'design-edits.json').write_text(json.dumps({'body_indices':changed,'unit':'CM3-005','purpose':PURPOSE,'fields':FIELDS,'note':NOTE},ensure_ascii=False,indent=2),encoding='utf8')

    wb = openpyxl.load_workbook(QA/'suvr-before.xlsx')
    s = wb.worksheets[1]
    old = {c.coordinate: str(c.value or '') for row in s.iter_rows(min_row=809,max_row=811) for c in row}
    fixture = '[LOG] IDENT=EVENTLOG.TXT, WRITE INDEX=1, ENTITY COUNT=1\n      <  0> : TIME {26-10-02-13-14-15} ADDR {12:34:56:78:9A:BC} EVENT {00000040}'
    gate = ('조건 [4] 정보 일치 / 정상 연결\n'
            '등록 ID=0x12345678, 실제 ID=0x12345678, 정상 연결. --dump_log 1회 입력.\n'
            '예상: 조회 허용, 읽기 함수 1회 호출, 반환=0. 출력:\n' + fixture + '\n\n'
            '조건 [5] 정보 일치 / 미연결\n'
            '동일 ID를 유지하고 연결 상태만 미연결로 바꾼 뒤 조회한다.\n'
            '예상: 조회 차단, 읽기 함수 0회 호출, 저장 기록 출력 0건.\n\n'
            '조건 [6] 정보 불일치\n'
            '등록 ID=0x12345678, 실제 ID=0x12345679. 정상 연결과 미연결 상태에서 각각 조회한다.\n'
            '예상: 두 경우 모두 차단, 읽기 함수 0회 호출, 저장 기록 출력 0건.\n\n'
            '조건 [7] 정상 조회 후 연결 해제\n'
            '조건 [4] 실행 후 연결을 해제하고 다시 조회한다.\n'
            '예상: 새 요청 차단, 읽기 함수 추가 호출 0회, 저장 기록 추가 출력 0건.\n'
            '조건 [4]~[7] 모두 파일 쓰기=0회, 조회 전후 파일 동일.')
    common = {
      'E809': pad(PURPOSE),
      'H809': pad(old['H809'] + '\n- 조회 허용 조건 시험은 등록 ID=0x12345678과 실제 ID/연결 상태를 입력해 진행한다. 정상 연결과 정보 일치는 각각 확인한다.'),
      'I809': pad('조건 [3]은 감사기록 읽기 함수의 독립 시험이다. 조건 [4]~[7]은 조회 명령의 접근 제한 시험이다.\n[E8300 반영] 조건 [4]~[7]은 조회 제한 코드 반영 후 확인한다.'),
      'J809': pad(old['J809'] + '\n- 조건 [4]~[7]의 내부기 정보와 연결 상태를 설정하고 --dump_log를 입력한다. 읽기 함수 호출 수와 저장 기록의 출력 여부를 확인한다. 연결 해제 후 다시 조회하여 이전 허용 상태가 유지되지 않는지 확인한다.'),
      'K809': pad(old['K809'].strip() + '\n\n' + old['K810'].strip()),
      'K810': old['K811'],
      'K811': pad(gate),
    }
    results = {
      'L809': pad(old['L809'].strip()+'\n\n'+old['L810'].strip()),
      'L810': old['L811'],
      'L811': pad('검증 [4] 정보 일치 / 정상 연결\n[E8300 확인] 조회 제한 코드 반영 후 허용 동작과 왼쪽의 출력 문자열을 확인한다. 결과 미기록.\n\n검증 [5] 정보 일치 / 미연결\n[E8300 확인] 코드 반영 후 차단 여부, 읽기 함수 호출 수와 기록 출력 수를 확인한다. 결과 미기록.\n\n검증 [6] 정보 불일치\n[E8300 확인] 코드 반영 후 정상 연결/미연결 두 상태의 차단 결과를 확인한다. 결과 미기록.\n\n검증 [7] 정상 조회 후 연결 해제\n[E8300 확인] 코드 반영 후 새 요청이 차단되는지 확인한다. 결과 미기록.'),
      'M809': pad('기존 조건 [1]~[3]\n통과\n\n[E8300 확인]\n추가 조건 [4]~[7]\n코드 반영 후 검증'),
    }
    plan = {'common':common,'report':results,'heights':{'809':165,'810':247.5,'811':330},'qccCells':[]}
    (QA/'sheet-edits.json').write_text(json.dumps(plan,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'snapshot':str(QA),'design_block':changed,'spreadsheet_cells':{k:len(v) for k,v in [('common',common),('results',results)]},'protected':len(baseline['protected']),'source_files':len(baseline['sources'])},ensure_ascii=False))

def preserve():
    plan=json.loads((QA/'sheet-edits.json').read_text(encoding='utf8'))
    for kind in ('suvc','suvr'):
        original=unpack(QA/(kind+'-before.xlsx'))
        authored=unpack(QA/(kind+'-artifact.xlsx'))
        ns={'s':'http://schemas.openxmlformats.org/spreadsheetml/2006/main'}
        root=E.fromstring(original['xl/worksheets/sheet2.xml'])
        src=E.fromstring(authored['xl/worksheets/sheet2.xml'])
        strings=E.fromstring(authored['xl/sharedStrings.xml']) if 'xl/sharedStrings.xml' in authored else []
        styles=E.fromstring(original['xl/styles.xml'])
        edits={**plan['common'],**(plan['report'] if kind=='suvr' else {})}
        for addr,want in edits.items():
            dst=root.find(f'.//s:c[@r="{addr}"]',ns)
            actual=value(src.find(f'.//s:c[@r="{addr}"]',ns),strings)
            assert actual==want,addr
            text_runs(dst,actual,styles)
        for row,height in plan['heights'].items():
            r=root.find(f'.//s:row[@r="{row}"]',ns)
            r.set('ht',str(height));r.set('customHeight','1')
        original['xl/worksheets/sheet2.xml']=E.tostring(root,encoding='UTF-8',xml_declaration=True,standalone=True)
        with zipfile.ZipFile(QA/(kind+'.xlsx'),'w',zipfile.ZIP_DEFLATED) as z:
            for name,data in original.items():z.writestr(name,data)
    print('Changed Artifact Tool values transplanted; other native package content preserved.')

if __name__=='__main__':
    import sys
    if '--preserve' in sys.argv: preserve()
    else: prepare()
