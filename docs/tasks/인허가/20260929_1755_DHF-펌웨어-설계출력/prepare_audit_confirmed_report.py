"""Record tester-confirmed results without changing test criteria or product code."""
import json
import re
import shutil
import openpyxl
from prepare_audit_expected_report import ROOT, TASK, DHF, REPORT, sha

QA = TASK / 'qa/audit-report-confirmed-results-20261012'


def plan():
    QA.mkdir(parents=True, exist_ok=True)
    before = QA / 'report-before.xlsx'
    if before.exists():
        assert sha(before) == sha(REPORT), 'Report changed after baseline'
    else:
        shutil.copyfile(REPORT, before)
    protected = {str(p): sha(p) for p in DHF.rglob('*')
                 if p.is_file() and p.suffix in ('.xlsx', '.docx')
                 and p != REPORT and not p.name.startswith('~$')}
    sources = {str(p): sha(p) for folder in ('src', 'tests')
               for p in (ROOT / folder).rglob('*') if p.is_file()}
    (QA / 'baseline.json').write_text(json.dumps({
        'report': str(REPORT), 'sha256': sha(REPORT), 'protected': protected, 'sources': sources,
    }, ensure_ascii=False, indent=2), encoding='utf8')
    wb = openpyxl.load_workbook(before, rich_text=True)
    ws = wb.worksheets[1]
    edits = {}
    pad = lambda s: '\n' + s.strip() + '\n'
    def put(addr, value):
        value = pad(value)
        if value != str(ws[addr].value or ''):
            edits[addr] = value

    # Criteria stay unchanged. The later tester confirmation supplies the report results.
    for first, last in ((789, 792), (809, 811), (1208, 1213)):
        for row in range(first, last+1):
            addr = f'L{row}'
            value = str(ws[addr].value or '')
            value = re.sub(r'\n판정: 출력/동작이 기준과 일치하여 통과\.', '', value)
            if value:
                put(addr, value)
    for addr in ('M789', 'M809', 'M1208'):
        put(addr, '통과')
    put('I809', '''조건 [3]은 감사기록 읽기 함수의 독립 시험이다.
조건 [4]~[8]은 조회 명령의 접근 제한 시험이다.''')

    put('L792', '''검증 [4]
파일 없음 / 식별자 오류 / 다음 위치=255 / 기록 수=256:
파일 크기=4,096바이트, 식별자=EVENTLOG.TXT,
기록 수=0, 다음 위치=0, 반환=0. 기록 영역은 모두 0.
정상 기록 1개: 크기=4,096바이트, 기록 수=1,
다음 위치=1, 반환=0, 파일 내용 동일, 파일 쓰기=0회.

검증 [5]
저장 실패 입력 / 초기화 함수 반환=-1.

검증 [6]
열기 실패 입력 / 초기화 함수 반환=-1.

검증 [7]
쓰기 실패 입력 / 초기화 함수 반환=-1.
4,095바이트 쓰기 / 초기화 함수 반환=-1.

검증 [8]
닫기 실패 입력 / 초기화 함수 반환=-1.''')
    previous_2 = str(ws['L810'].value).split('\n\n검증 [3]')[0].strip()
    previous_2 = previous_2.replace('\n판정: 출력/동작이 기준과 일치하여 통과.', '')
    header = '[LOG] IDENT=EVENTLOG.TXT, WRITE INDEX=1, ENTITY COUNT=1'
    record = '      <  0> : TIME {26-05-29-13-14-15} ADDR {12:34:56:78:9A:BC} EVENT {00000040}'
    put('L810', previous_2 + '\n\n검증 [3]\n'
        '입력 기록 시간=2026-05-29 13:14:15.\n'
        '--dump_log 입력 / 감사기록 읽기 함수 호출. 출력 결과:\n'
        + header + '\n' + record + '\n반환=0, 파일 쓰기=0회, 조회 전후 파일 동일.\n'
        '읽기 실패 입력 / 반환=-1.')
    put('L811', '''검증 [4]
입력 기록 시간=2026-05-29 13:14:15.
조회=허용, 읽기=1회, 반환=0, 기록 출력=1건.
''' + header + '\n' + record + '''

검증 [5]
조회=차단, 읽기=0회, 기록 출력=0건.

검증 [6]
정상 연결: 차단, 읽기=0회, 기록 출력=0건.
미연결: 차단, 읽기=0회, 기록 출력=0건.

검증 [7]
연결 해제 후: 차단, 추가 읽기=0회, 추가 출력=0건.

검증 [8]
정보 미확인: 차단, 읽기=0회, 기록 출력=0건.
초기화 중: 차단, 읽기=0회, 기록 출력=0건.

검증 [4]~[8]: 파일 쓰기=0회, 파일 내용 동일.
차단 시 읽기 함수 호출=0회, 반환값 없음.''')
    previous_4 = str(ws['L1212'].value).split('\n\n검증 [5]')[0].strip()
    previous_4 = previous_4.replace('\n판정: 출력/동작이 기준과 일치하여 통과.', '')
    put('L1212', previous_4 + '''

검증 [5]
입력: 시간={26,5,29,13,14,15}, 주소=12:34:56:78:9A:BC.
처리 순서: 시간 갱신 → 주소 설정 → 연결 이벤트 저장.
연결 확인은 일반 명령 처리와 구분되어 실행됐다.
저장 시간=26-05-29-13-14-15, 이벤트=0x40,
기록 수=1, 다음 위치=1, 반환=0.
999ms: 시간=26-05-29-13-14-15.
1,000ms: 시간=26-05-29-13-14-16.
다음 위치=254 / 기록 수=255에서 1회 저장:
다음 위치=0, 기록 수=255.''')
    put('L1213', '''검증 [6]
닫기 실패 입력 / 기록 함수 반환=-1.

검증 [7]
열기 실패 입력 / 기록 함수 반환=-1.

검증 [8]
읽기 실패 / 읽은 크기=4,095바이트 / 식별자 오류 /
다음 위치=255 / 기록 수=256: 각각 기록 함수 반환=-1.

검증 [9]
쓰기 실패 / 기록 크기=4,095바이트: 각각 기록 함수 반환=-1.

검증 [10] 입력 시간 → 기록 시간
26-05-29-13-14-59 → 26-05-29-13-15-00
26-05-29-23-59-59 → 26-05-30-00-00-00
26-05-31-23-59-59 → 26-06-01-00-00-00
28-02-28-23-59-59 → 28-02-29-00-00-00
26-02-28-23-59-59 → 26-03-01-00-00-00
26-12-31-23-59-59 → 27-01-01-00-00-00''')
    output = {'edits': edits, 'fills': [], 'heights': {}, 'clear_marker_fonts': [],
              'criteria_date': '2026-05-02', 'result_date': '2026-05-29',
              'mode': 'tester-confirmed-results',
              'test_confirmation': '은수님이 다른 시험자가 실행한 결과가 모두 기준/예상값과 일치한다고 확인함.',
              'prior_records': '../audit-report-dates-format-20261012/report-before.xlsx',
              'renders': ['K791:M792', 'K809:M811', 'K1212:M1213']}
    (QA / 'sheet-edits.json').write_text(json.dumps(output, ensure_ascii=False, indent=2), encoding='utf8')
    print(json.dumps({'baseline': sha(REPORT), 'edits': list(edits),
                      'result_basis': output['test_confirmation']}, ensure_ascii=False))


if __name__ == '__main__':
    plan()
