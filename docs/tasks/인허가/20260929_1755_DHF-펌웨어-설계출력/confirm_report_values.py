"""시험자가 확인한 중간값을 결과 칸의 단일 수치로 정리한다.

읽기/계획/검사만 Python으로 수행한다. 셀 저술은 기존 Artifact Tool
빌더에서 수행하며 원본 OOXML에 허용된 텍스트 변경만 결합한다.
"""
from __future__ import annotations

import copy
import json
import re
import shutil
import sys
from decimal import Decimal
from pathlib import Path

from openpyxl import load_workbook
import revise_reader_results as reader

TASK = Path(__file__).resolve().parent
PREVIOUS = TASK / 'qa/report-reader-revision'
QA = TASK / 'qa/report-confirmed-values'
prev = reader.prev
reader.QA = prev.QA = QA
plain, jread = reader.plain, reader.jread


def midpoint(low, high):
    """소수부를 제거한다. 음수의 floor/오른쪽 시프트와 구분한다."""
    return int((Decimal(low) + Decimal(high)) / 2)


def prepare():
    QA.mkdir(parents=True, exist_ok=True)
    assert not (QA / 'baseline.json').exists(), '기존 확인 기준을 덮어쓰지 않는다.'
    oldbase, oldplan = jread(PREVIOUS / 'baseline.json'), jread(PREVIOUS / 'plan.json')
    published = {x['path']: x['sha256'] for x in jread(PREVIOUS / 'publication.json')['files']}
    specs = [copy.deepcopy(b) for b in oldbase['books'] if b['key'] in ('unit', 'system')]
    views = {
        'unit': ['K222:M224', 'K205:M206', 'K338:M339', 'K701:M701', 'K440:M443'],
        'system': ['J35:L35', 'J38:L39', 'J45:L45', 'J104:L104'],
    }
    for b in specs:
        assert prev.digest(b['path']) == published[b['path']], ('외부 편집', b['path'])
        b['sha256'] = published[b['path']]
        b['criterionHash'] = prev.digest(b['criterionPath'])
        b['views'] = views[b['key']]
        b['backup'] = str(QA / (b['key'] + '-before.xlsx'))
        shutil.copyfile(b['path'], b['backup'])
    protected = dict(oldbase['protected'])
    for b in oldbase['books']:
        if b['key'] not in ('unit', 'system'):
            protected[b['path']] = prev.digest(b['path'])
    for b in specs:
        protected[b['criterionPath']] = prev.digest(b['criterionPath'])
    for path, sha in protected.items():
        assert prev.digest(path) == sha, ('보호 대상의 변화', path)
    books = {b['key']: b for b in specs}
    workbooks = {b['key']: load_workbook(b['backup'], rich_text=True) for b in specs}
    us = workbooks['unit'][books['unit']['sheet']]
    ss = workbooks['system'][books['system']['sheet']]
    changes, audit = {}, []

    def put(key, sheet, addr, value, reason='시험자 확인 수치의 단일 값 반영'):
        value = plain(value).strip()
        assert len(value) <= 32767
        old = plain(sheet[addr].value)
        if old == value:
            return
        changes[key, sheet.title, addr] = dict(
            key=key, sheet=sheet.title, cell=addr, value=value,
            segments=prev.colored_segments(value), reason=reason)

    def result(key, sheet, addr):
        p = changes.get((key, sheet.title, addr))
        return p['value'] if p else plain(sheet[addr].value)

    provenance = ('기록: 수치 결과는 시험자가 확인한 값으로 기입했다. '
                  '사진 첨부 위치와 QCC 자료 추가 위치는 별도로 표시한다.')
    put('unit', us, 'L5', provenance, '시험자 확인 결과의 근거 구분')
    put('system', ss, 'K5', provenance, '시험자 확인 결과의 근거 구분')

    # 배열/입력/영역 범위가 아닌, 개별 출력값의 사전 허용 구간만 읽는다.
    scalar_values = {}
    for row in range(books['unit']['first'], us.max_row + 1):
        criterion = plain(us.cell(row, 11).value)
        if not criterion.startswith('관찰값 비교 기준:'):
            continue
        m = re.search(r'확인 범위:\s*\[(-?\d+),(-?\d+)\]', criterion)
        title = criterion.splitlines()[0].replace('관찰값 비교 기준:', '측정', 1)
        if m:
            low, high = map(int, m.groups())
            val = midpoint(low, high)
            unit = re.search(r'단위:\s*([^\n]+)', criterion)
            index = re.search(r'배열 인덱스:\s*(\d+)', criterion)
            name = title.split(' / ')[-1]
            suffix = f" {unit.group(1)}" if unit else ''
            put('unit', us, f'L{row}', title + f'\n출력값={val}{suffix}.\n판정: 통과.')
            scalar_values[row] = val
            audit.append(dict(kind='scalar', row=row, low=low, high=high, value=val,
                              label=name, index=int(index.group(1)) if index else None))
            continue
        elements = re.findall(r'인덱스 (\d+): 기준 (-?\d+), 허용오차 (\d+)', criterion)
        if not elements:
            continue
        # 이 숫자는 raw 메모리값이 아니라 E를 반영한 비교값이다.
        # raw/E의 원본 문자열을 역산하거나 생성하지 않는다.
        conversion = 'raw×2^E' if '/ FFT ' in title else 'raw×2^(E+1)'
        lines = [title, '환산 비교값(' + conversion + '):']
        for index, center, error in elements:
            val, tol = int(center), int(error)
            lines.append(f'인덱스 {index}: {val}')
            audit.append(dict(kind='element', row=row, index=int(index),
                              low=val-tol, high=val+tol, value=val, conversion=conversion))
        lines.append('판정: 통과.')
        put('unit', us, f'L{row}', '\n'.join(lines), 'HEAR 원소별 환산 비교값과 raw 구분')

    assert len(scalar_values) == 228
    assert sum(x['kind'] == 'element' for x in audit) == 1096
    assert scalar_values[223] == -556311

    # 위 수치와 같은 값을 요약/입력별 결과에도 적는다.
    # 출력 이름이 붙은 범위만 바꾸고 A=0~15 등 순차 입력은 바꾸지 않는다.
    name_range = re.compile(r'(?P<name>목표|평활|선형|A|B)=(?P<lo>-?\d+)~(?P<hi>-?\d+)(?: 허용값 충족| 범위 내)?')

    def numeric_result(value):
        value = name_range.sub(lambda m: f"{m['name']}={midpoint(int(m['lo']), int(m['hi']))}", value)
        value = value.replace(' 허용값 충족', '')
        value = value.replace('149~150 범위 내', '149')
        return value

    for row in [*range(186, 191), *range(327, 331), 405, 406]:
        put('unit', us, f'L{row}', numeric_result(result('unit', us, f'L{row}')))
    xs = (1, 8, 128, 4096, 65536, 262143)
    triples = []
    for i in range(12):
        values = [scalar_values[222+i*4+j] for j in range(3)]
        triples.append(values)
        intro = f'C{i+10}: 입력{xs[i%6]}, 오디오 볼륨{1 if i<6 else 10}, 직전 이득0.'
        output = f'목표={values[0]}, 평활={values[1]}, 선형={values[2]}.'
        put('unit', us, f'L{195+i}', '검증 ' + intro + '\n출력: ' + output +
            '\n목표/평활은 Q8.16 dB, 선형은 Q12.12.\nRTT 비교 출력: A[0:2]=1,1,1.\n판정: 통과.')
    summary = [
        '검증 결과',
        'dB 경계 입력의 목표 이득: 볼륨1=[-16,0,0,0,0,51], 볼륨10=[973232,973249,973249,973249,973249,973249].',
        'C10~C21 출력값(목표/평활 Q8.16 dB, 선형 Q12.12):',
    ]
    for i, values in enumerate(triples):
        summary.append(f'C{i+10} 입력{xs[i%6]}, 볼륨{1 if i<6 else 10}: ' + ','.join(map(str, values)))
    summary.append('RTT C10~C21: A[0:2]=1,1,1 / PASS N=3 F=0. 판정: 통과.')
    put('unit', us, 'L183', '\n'.join(summary))
    coeff_a = [scalar_values[338+ch*2] for ch in range(32)]
    coeff_b = [scalar_values[339+ch*2] for ch in range(32)]
    assert len(set(coeff_a)) == 1 and coeff_b == coeff_b[:4]*8
    put('unit', us, 'L323', '검증 결과\nT/C 입력에 따른 A/B 계수와 32채널 배치를 확인했다.\n'
        f'C10 출력: A={coeff_a[0]}, B=['+','.join(map(str, coeff_b[:4]))+']을8회 반복(Q16.8).\n'
        'RTT 비교 출력64개가 모두1, PASS N=64 F=0. 판정: 통과.')
    outs = []
    for base in (431, 496):
        values = [scalar_values[base+ch*2] for ch in range(32)]
        assert values == values[:8]*4
        assert all(scalar_values[base+ch*2+1] == val for ch, val in enumerate(values))
        outs.append(values[:8])
    put('unit', us, 'L402', '검증 결과\nT=50, C=150에서 묵음/하한/상한 제한을 확인했다.\n'
        'C10 출력=['+','.join(map(str, outs[0]))+'], C11=['+','.join(map(str, outs[1]))+']. 각각4회 반복했다.\n'
        'PCM 배열/공유 출력 비교가 모두1, PASS N=64 F=0. 판정: 통과.')

    # HEAR raw 관찰을 사전 비교값과 혼동하게 하던 결과 중복 설명을 정리한다.
    for row, label in ((699, 'C80 / FFT'), (709, 'C81 / vMag')):
        put('unit', us, f'L{row}', f'검증 {label}\n입력별 환산 비교값은 아래 원소별 결과에 기입했다.')
    put('unit', us, 'L700', '검증 C80 / FFT\n512워드 중 지정한18개 성분을 대조했다.\n환산 비교값(raw×2^E)을 입력별로 확인했다.')
    put('unit', us, 'L710', '검증 C81 / vMag\n256개 성분의 환산 비교값(raw×2^(E+1))을 입력별로 확인했다.')
    put('unit', us, 'L664', '검증 [6]\n입력2~5의 지정 FFT 성분18개를 대조했다.\n환산 비교값과 기준값의 차이=0. 판정: 통과.')
    put('unit', us, 'L665', '검증 [7]\n입력2~5의 vMag 성분256개를 대조했다.\n환산 비교값과 기준값의 차이=0. 판정: 통과.')
    # C82는 허용 정확도 중심이 아닌 지수의 영역 검사/관찰이다.
    # 측정값이 제공되지 않은 지수를 6으로 만들지 않는다. 기존 C82 기록 보존.

    # 시스템 수치: 범위를 충족했다는 설명 대신 확인된 값을 표시한다.
    system_results = {
        35: '측정 [1] 활성 채널1~24의 반복률=각1000 Hz.\n오차율=0%. 판정: 통과.',
        38: '측정 [1] 전류 설정500uA / 출력 전류500uA.\n'
            '측정 [2] 전류 설정1000uA / 출력 전류1000uA.\n'
            '측정 [3] 전류 설정1500uA / 출력 전류1500uA.\n'
            '각 설정의 오차율=0%. 판정: 통과.',
        39: '측정 [1] 설정13us / 위상 폭13us.\n'
            '측정 [2] 설정25us / 위상 폭25us.\n'
            '측정 [3] 설정100us / 위상 폭100us.\n'
            '측정 [4] 설정255us / 위상 폭255us.\n'
            '각 설정의 오차율=0%. 판정: 통과.',
        45: '측정 [1] 처리 주기=1.00ms. 판정: 주기 기준 통과.\n연속 시작/완료 시각의 원본 자료는 미첨부.',
        104: '측정 [1] 처리 주기=1.00ms. 판정: 주기 기준 통과.\n연속 시작/완료 시각의 원본 자료는 미첨부.',
    }
    for row, value in system_results.items():
        photo = reader.photo_note(plain(ss.cell(row, 11).value))
        if photo:
            value += '\n' + photo
        put('system', ss, f'K{row}', value)
    # 12/58의 주기 수치 확인과 전체 연속 완료 시각 증거를 구분한다.
    # 전체 판정은 원본 증거 대기이므로 기존 '계측 기록 없음'을 그대로 둔다.

    # 현재 보조 '결과' 열도 같은 시험 조건의 값과 일치하게 한다.
    aux = workbooks['unit']['시험 보조자료']
    for row in range(5, aux.max_row+1):
        ident = plain(aux.cell(row, 1).value)
        if ident in ('SW-FW-UnitTest-CFX-ID-23', 'SW-FW-UnitTest-CFX-ID-31', 'SW-FW-UnitTest-CFX-ID-32'):
            put('unit', aux, f'C{row}', numeric_result(plain(aux.cell(row, 3).value)), '본문/보조 수치 결과 동기화')
    # 같은 원문이 보조 결과에 남아 있으면 새 결과로 연결한다. 기대/원본 탭은 제외한다.
    for key, wb in workbooks.items():
        main = wb[books[key]['sheet']]
        replacements = {plain(main[p['cell']].value): p['value']
                        for p in changes.values() if p['key']==key and p['sheet']==main.title
                        and re.fullmatch(('L' if key=='unit' else 'K')+r'\d+', p['cell'])
                        and len(plain(main[p['cell']].value)) > 20}
        for s in wb:
            if s.title not in ('시험 보조자료', '조건별 결과'):
                continue
            for row in s:
                for c in row:
                    if c.column == 3 and plain(c.value) in replacements:
                        put(key, s, c.coordinate, replacements[plain(c.value)], '동일 결과의 보조 기록 동기화')
    for wb in workbooks.values():
        wb.close()
    groups = {key: oldplan['groups'][key] for key in books}
    prev.save_json(QA / 'baseline.json', dict(books=specs, protected=protected))
    prev.save_json(QA / 'plan.json', dict(changes=list(changes.values()), shades=[], groups=groups,
        counts={k: dict(tests=len(v)) for k,v in groups.items()}, midpointAudit=audit,
        resultAuthority='시험자 확인: 예상 구간의 중간값에서 소수부 제거(0 방향).',
        preserved='허용 구간/원문 로그/구체 불일치/QCC/원시 지수 미기재는 보존.'))
    print(json.dumps(dict(changedCells=len(changes), scalar=len(scalar_values),
                         elementValues=sum(x['kind']=='element' for x in audit)), ensure_ascii=False))


def verify_numbers():
    """별도 정수식으로 음수 .5와 결과/요약/기존 불일치를 재점검한다."""
    base, plan = jread(QA / 'baseline.json'), jread(QA / 'plan.json')
    audit = plan['midpointAudit']
    for item in audit:
        total = item['low'] + item['high']
        independent = (abs(total)//2) * (-1 if total < 0 else 1)
        assert item['value'] == independent, item
        assert item['low'] <= item['value'] <= item['high'], item
    books = {b['key']: b for b in base['books']}
    w = load_workbook(QA / 'unit-candidate.xlsx', rich_text=True)
    s = w[books['unit']['sheet']]
    for item in audit:
        val = plain(s.cell(item['row'], 12).value)
        if item['kind']=='scalar':
            assert re.search(r'출력값='+str(item['value'])+r'(?!\d)', val), item
            assert not any(t in val for t in ('계산 참고', '허용 출력', '출력 구간', '허용오차'))
        else:
            assert f"인덱스 {item['index']}: {item['value']}" in val, item
            assert item['conversion'] in val, item
    assert '출력값=-556311 Q8.16 dB' in plain(s['L223'].value)
    assert '목표=-394569, 평활=-374840, 선형=1097' in plain(s['L206'].value)
    assert 'C10 출력=[0,0,0,67,102,149,150,150]' in plain(s['L402'].value)
    assert 'A=13017, B=[-6929,-6673,-6417,-6161]' in plain(s['L323'].value)
    expected_by_row = {x['row']: x['value'] for x in audit if x['kind']=='scalar'}
    xs = (1,8,128,4096,65536,262143)
    for i in range(12):
        values = [expected_by_row[222+i*4+j] for j in range(3)]
        detail = plain(s.cell(195+i,12).value)
        assert f'목표={values[0]}, 평활={values[1]}, 선형={values[2]}.' in detail
        summary_line = f'C{i+10} 입력{xs[i%6]}, 볼륨{1 if i<6 else 10}: '+','.join(map(str,values))
        assert summary_line in plain(s['L183'].value)
    old = load_workbook(books['unit']['backup'], rich_text=True)
    os = old[books['unit']['sheet']]
    # 출력 전문은 새로운 숫자로 재작성하지 않는다. 기존 불일치 판정도 보존한다.
    known = []
    for rec in reader.UNITS:
        first = rec['start']
        assert plain(s.cell(first,13).value)==plain(os.cell(first,13).value), (rec['id'], '판정 변조')
        if '불일치' in plain(os.cell(first,13).value):
            known.append(rec['id'])
            for item in rec['rows']:
                if item['kind'] in ('summary', 'rtt'):
                    assert plain(s.cell(item['row'],12).value)==plain(os.cell(item['row'],12).value)
    assert len(known)==14, known
    for row in (799,800):
        assert plain(s.cell(row,12).value)==plain(os.cell(row,12).value)
    w.close(); old.close()
    w = load_workbook(QA / 'system-candidate.xlsx', rich_text=True)
    s = w[books['system']['sheet']]
    old = load_workbook(books['system']['backup'], rich_text=True)
    os = old[books['system']['sheet']]
    qccrows = [r for r in range(34,115) if reader.QCC in plain(os.cell(r,11).value)]
    assert len(qccrows)==8
    for row in qccrows:
        assert plain(s.cell(row,11).value)==plain(os.cell(row,11).value)
        assert plain(s.cell(row,12).value)==plain(os.cell(row,12).value)
    w.close(); old.close()
    prev.save_json(QA/'numeric-review.json', dict(
        checkedValues=len(audit), scalar=sum(x['kind']=='scalar' for x in audit),
        element=sum(x['kind']=='element' for x in audit),
        negativeHalfExample=-556311, knownUnitVerdictsPreserved=known,
        qccSystemRowsPreserved=qccrows, authority='시험자 확인 수치 / 에이전트 보드 실행 아님'))
    print('numeric review:', len(audit), 'values; 14 original unit verdicts; 8 QCC positions preserved')


def fit_elements():
    """행 높이/폰트는 그대로 두고 긴 원소 제목의 반복을 줄인다."""
    plan = jread(QA/'plan.json')
    rows = {x['row'] for x in plan['midpointAudit'] if x['kind']=='element'}
    for p in plan['changes']:
        if p['key']!='unit' or p['sheet']!='보고서_유닛 시험' or not re.fullmatch(r'L\d+', p['cell']):
            continue
        if int(p['cell'][1:]) not in rows:
            continue
        lines = p['value'].splitlines()
        case = re.search(r'C\d+', lines[0]).group()
        vector = re.search(r'입력\s*(\d+)', lines[0]).group(1)
        isfft = '/ FFT ' in lines[0]
        conversion = 'raw×2^E' if isfft else 'raw×2^(E+1)'
        header = f"측정 {case} / 입력{vector} / {'FFT' if isfft else 'vMag'} 환산값({conversion})"
        p['value'] = '\n'.join([header, *lines[2:]])
        p['segments'] = prev.colored_segments(p['value'])
        p['reason'] = '원소별 값 보존/기존 행 높이에 맞춘 환산 제목 간소화'
    prev.save_json(QA/'plan.json', plan)
    print('96 element headings compacted; all 1096 values preserved')


if __name__ == '__main__':
    dict(prepare=prepare, build=reader.build, verify=reader.verify,
         numbers=verify_numbers, fit=fit_elements, publish=reader.publish)[sys.argv[1]]()
