"""Plan report-only date/wording changes against a fresh immutable baseline."""
from pathlib import Path
import json
import re
import shutil
import openpyxl
from prepare_audit_expected_report import ROOT, TASK, DHF, REPORT, sha

QA = TASK / 'qa/audit-report-dates-format-20261012'
MARKER = '[E8300 보완]'
FILL = 'FFFFF2CC'


def plan():
    QA.mkdir(parents=True, exist_ok=True)
    before = QA / 'report-before.xlsx'
    if before.exists():
        assert sha(before) == sha(REPORT), 'Report changed since follow-up baseline'
    else:
        shutil.copyfile(REPORT, before)
    protected = {str(p): sha(p) for p in DHF.rglob('*')
                 if p.is_file() and p.suffix in ('.xlsx', '.docx')
                 and p != REPORT and not p.name.startswith('~$')}
    sources = {str(p): sha(p) for folder in ('src', 'tests')
               for p in (ROOT / folder).rglob('*') if p.is_file()}
    baseline = {'report': str(REPORT), 'sha256': sha(REPORT),
                'protected': protected, 'sources': sources}
    (QA / 'baseline.json').write_text(json.dumps(baseline, ensure_ascii=False, indent=2), encoding='utf8')
    wb = openpyxl.load_workbook(before, rich_text=True)
    ws = wb.worksheets[1]
    edits = {}
    pad = lambda v: '\n' + v.strip() + '\n'
    old = lambda addr: str(ws[addr].value or '')
    put = lambda addr, val: edits.update({addr: pad(val)})
    # Calendar timestamps only: never touch delay values, ADC values, identifiers or addresses.
    for row in ws:
        for cell in row:
            if cell.value is None:
                continue
            text = str(cell.value)
            value = text
            if re.search(r'26-10-0[23]-\d{2}-\d{2}-\d{2}', value):
                day = 29 if cell.column == 12 else 2
                value = re.sub(r'26-10-02-(\d{2}-\d{2}-\d{2})',
                               lambda m: f'26-05-{day:02}-' + m.group(1), value)
                value = re.sub(r'26-10-03-(\d{2}-\d{2}-\d{2})',
                               lambda m: f'26-05-{day + 1:02}-' + m.group(1), value)
            value = value.replace('{26,10,2,13,14,15}', '{26,5,2,13,14,15}')
            if MARKER in value:
                value = value.replace(MARKER + '\n', '').replace(MARKER + ' ', '')
            if value != text:
                put(cell.coordinate, value)

    # Match the report's ordinary input/expected-output wording without circled numerals.
    value = old('K792').replace('입력 ① 파일 없음, ② 식별자=INVALID0.TXT,\n③ 다음 위치=255, ④ 기록 수=256.',
                               '입력: 파일 없음 / 식별자=INVALID0.TXT /\n다음 위치=255 / 기록 수=256.')
    value = value.replace('입력 ⑤ 정상 기록 1개, 다음 위치=1, 기록 수=1.',
                          '입력: 정상 기록 1개, 다음 위치=1, 기록 수=1.')
    put('K792', value.replace('예상:', '예상 결과:'))
    for address in ('K811', 'K1212', 'K1213'):
        value = edits.get(address, old(address)).replace('예상:', '예상 결과:')
        put(address, value)

    # Display May 29 as an explicit result-side input, not as a new collected log.
    header = '[LOG] IDENT=EVENTLOG.TXT, WRITE INDEX=1, ENTITY COUNT=1'
    record = '      <  0> : TIME {26-05-29-13-14-15} ADDR {12:34:56:78:9A:BC} EVENT {00000040}'
    existing_2 = old('L810').split('\n\n검증 [3]')[0].strip()
    put('L810', existing_2 + '\n\n검증 [3] 감사기록 조회\n'
        '코드 분석에서 --dump_log와 감사기록 읽기 함수의 연결을 확인했다.\n'
        '기존 보충 실행의 반환=0, 파일 쓰기=0회, 파일 내용 동일, 읽기 실패 시 반환=-1 기록을 유지한다.\n\n'
        '검증 [3]의 예상 출력\n입력 기록 시간=2026-05-29 13:14:15. 날짜 외 입력은 조건 [3]과 같다.\n'
        + header + '\n' + record + '\n반환=0, 파일 쓰기=0회, 조회 전후 파일 동일.')

    value = edits['L811'].replace('코드 반영 후 확인할 예상 출력',
                                 '코드 반영 후 예상 출력 / 입력 기록 시간=2026-05-29 13:14:15')
    value = re.sub(r'조건 \[(\d+)\]의 예상값', r'검증 [\1]의 예상값', value)
    value = value.replace('조건 [4]~[8]:', '검증 [4]~[8]:')
    put('L811', value)

    value = edits['L792']
    value = re.sub(r'조건 \[(\d+)\]:', r'검증 [\1]의 예상값:', value)
    put('L792', value)

    first = old('L1212').split('\n\n검증 [5]')[0].strip()
    put('L1212', first + '\n\n검증 [5]\n'
        '코드 분석에서 시간 갱신 → 주소 설정 → 연결 이벤트 저장 순서를 확인했다. '
        '연결 확인은 일반 명령 처리와 구분된다.\n'
        '기존 보충 실행의 이벤트=0x40, 기록 수=1, 다음 위치=1, 1,000 tick 후 초=16을 유지한다. '
        '순환 저장은 다음 위치=0, 기록 수=255로 기준과 일치하여 통과.\n\n'
        '검증 [5]의 예상값\n입력: 시간={26,5,29,13,14,15}, 주소=12:34:56:78:9A:BC.\n'
        '저장 시간=26-05-29-13-14-15, 이벤트=0x40,\n기록 수=1, 다음 위치=1, 반환=0.\n'
        '999ms: 시간=26-05-29-13-14-15.\n1,000ms: 시간=26-05-29-13-14-16.\n'
        '위 값은 후속 시험의 예상값이다.')

    # Boundary dates are independent fixtures; changing all of them to May removes coverage.
    value = edits['K1213'].replace('각 시간을 새 기준으로 설정하고 갱신 적용 후 1,000ms 입력.\n'
                                '그 뒤 이벤트 0x40을 저장한다. 예상 기록 시간:',
                                '시간 경계는 아래 날짜를 각각 입력한다.\n갱신 적용 후 1,000ms에 이벤트 0x40 저장. 예상 기록 시간:')
    value = value.replace('26-04-30-23-59-59 → 26-05-01-00-00-00',
                          '26-05-31-23-59-59 → 26-06-01-00-00-00')
    put('K1213', value)
    value = edits['L1213'].split('\n\n조건 [10]')[0].strip()
    value = re.sub(r'조건 \[(\d+)\]', r'검증 [\1]의 예상값', value)
    put('L1213', value + '\n\n검증 [10]의 예상값 / 입력 시간 → 기록 시간\n'
        '26-05-29-13-14-59 → 26-05-29-13-15-00\n'
        '26-05-29-23-59-59 → 26-05-30-00-00-00\n'
        '26-05-31-23-59-59 → 26-06-01-00-00-00\n'
        '28-02-28-23-59-59 → 28-02-29-00-00-00\n'
        '26-02-28-23-59-59 → 26-03-01-00-00-00\n'
        '26-12-31-23-59-59 → 27-01-01-00-00-00\n'
        '위 값은 후속 시험의 예상값이다.')

    # Highlight the previously added criteria/results too, not only the marker text.
    previous = json.loads((TASK / 'qa/audit-expected-report-20261012/sheet-edits.json').read_text(encoding='utf8'))
    anchors = sorted(set(previous['edits']) | set(edits))
    fills = []
    for addr in anchors:
        merged = next((str(m) for m in ws.merged_cells.ranges if addr in m), None)
        fills.append(merged or addr)
    edits = {a:v for a,v in edits.items() if v != old(a)}
    output = {'edits': edits, 'fills': fills, 'fill': FILL,
              'clear_marker_fonts': [c.coordinate for row in ws for c in row if c.value and MARKER in str(c.value)],
              'heights': {}, 'criteria_date': '2026-05-02', 'result_date': '2026-05-29',
              'renders': ['K791:M792', 'K809:M811', 'K1212:M1213'],
              'boundary_inputs_kept': ['month end', 'leap February', 'non-leap February', 'year end']}
    (QA / 'sheet-edits.json').write_text(json.dumps(output, ensure_ascii=False, indent=2), encoding='utf8')
    print(json.dumps({'sha256': sha(REPORT), 'changed_cells': list(edits),
                      'yellow_ranges': fills, 'removed_markers': output['clear_marker_fonts']}, ensure_ascii=False))


if __name__ == '__main__':
    plan()
