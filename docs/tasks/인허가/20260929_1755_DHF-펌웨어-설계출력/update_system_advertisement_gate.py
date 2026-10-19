"""기존 시스템 시험46에 연결 전/후 광고 검색을 추가한다.

문자열은 Artifact Tool에서 작성한다. 원본의 병합/서식/그림을 보존하는
결합 단계는 작성된 문자열만 원본 OOXML 패키지에 옮긴다.
제품 코드, 참조 원본 및 다른 단계의 시험 문서는 변경하지 않는다.
"""
import json
import re
import shutil
import sys
from pathlib import Path

import inspect_system_cybersecurity as reader
import preserve_aligned_report_packages as native
import preserve_system_cybersecurity as bridge

TASK = Path(__file__).resolve().parent
QA = TASK / 'qa/system-advertisement-gate'
MAIN = '보고서_시스템 시험'
CRITERION = '기준서_시스템 시험'
HELPER = '시험 보조자료'


def padded(text):
    return '\n' + text.strip() + '\n'


def prepare():
    QA.mkdir(parents=True, exist_ok=True)
    previous = json.loads((TASK / 'qa/system-cybersecurity-rev0/publication.json').read_text('utf8'))
    paths = {x['kind']: Path(x['path']) for x in previous['files']}
    baseline = {'files': {}, 'protected': {}}
    for kind, path in paths.items():
        backup = QA / f'{kind}-before.xlsx'
        if backup.exists():
            assert reader.sha(backup) == reader.sha(path), 'Existing baseline differs; do not overwrite it.'
        else:
            shutil.copy2(path, backup)
        baseline['files'][kind] = {'path': str(path), 'sha256': reader.sha(path)}
    old = json.loads((TASK / 'qa/system-cybersecurity-rev0/baseline.json').read_text('utf8'))
    for filename in old['protected']:
        path = Path(filename)
        baseline['protected'][filename] = reader.sha(path)
    for key, record in old['sources'].items():
        if key not in ['system', 'unit']:
            baseline['protected'][record['path']] = reader.sha(Path(record['path']))

    before = reader.extract(QA / 'system-before.xlsx')
    cells = before['sheets'][MAIN]['cells']
    assert cells['B188'].strip() == 'SW-FW-SYS-Test-ID-46'
    # 일반 기동 조건에서 검색한다. 연결된 리모콘/매핑 예외와 혼동하지 않는다.
    # 새 광고 확인 기록은 기존 내부기 연결 결과와 구분한다.
    updates = {'C188': '등록된 내부이식장치와의 연결 및 내부기 연결 상태에 따른 블루투스 광고'}
    updates['G188'] = cells['G188'].strip() + '\n블루투스 광고 검색 장치를 준비한다. 일반 동작 모드에서 리모콘/매핑 프로그램/OTA와의 연결을 해제하고 시험한다. 검색 목록을 새로 갱신해 현재 광고를 확인한다.'
    updates['I188'] = (
        '1. 내부기를 부착하지 않은 상태에서 외부기 전원을 켜고 블루투스 광고를 검색한다.\n'
        '2. 외부기 맵에 등록된 내부기를 부착한다. 내부기 연결을 확인한 후 블루투스 광고를 다시 검색한다.\n'
        '3. 외부기 전원을 다시 켜 초기 상태에서 맵에 등록되지 않은 내부기를 부착한다. 연결 허용/거부 및 광고 검색 결과를 확인한다.\n'
        '등록 제조번호와 검출 제조번호, 자극 허용/출력 상태를 함께 확인한다. 제조사 기본 사용자 또는 마스터 내부기 조건은 일반 사용자 조건과 구분한다. LED는 연결 상태 확인을 위한 보조 표시로 기록한다.'
    )
    updates['J188'] = (
        '조건 [1] 내부기 미연결로 기동한 후 맵에 등록된 내부기를 부착\n'
        '내부기 미연결 상태: 외부기의 블루투스 광고가 검색되지 않아야 한다.\n'
        '등록 내부기 연결 후: 근접 무선 연결이 성립하고 외부기의 블루투스 광고가 검색되어야 한다. 외부기 녹색 LED 점멸을 확인한다.\n\n'
        '통과 기준\n'
        '등록된 내부기와는 연결되어야 하며 등록되지 않은 내부기와는 연결되지 않아야 한다. 내부기와 연결된 상태에서만 블루투스 광고가 검색되어야 한다. LED 색상만으로 연결 허용/거부를 판정하지 않는다.'
    )
    updates['J189'] = (
        '조건 [2] 초기 상태에서 맵에 등록되지 않은 내부기를 부착\n'
        '근접 무선 연결이 허용되지 않고 블루투스 광고가 검색되지 않아야 한다. 외부기 녹색 LED 점등을 확인한다.'
    )
    updates['K188'] = (
        '검증 [1] / 맵에 등록된 내부기 부착\n'
        '기존 연결 시험 결과: 근접 무선 연결 성립, 녹색 LED 점멸.\n'
        '추가한 광고 확인: 내부기 미연결 기동 시 검색되지 않는지, 등록 내부기 연결 후 검색되는지 확인한 결과를 기록한다. 현재 광고 검색 결과는 기록되지 않았다.\n'
        '(사진필요) 내부기 미연결 기동 및 등록 내부기 연결 후의 블루투스 광고 검색 화면.'
    )
    updates['K189'] = (
        '검증 [2] / 맵에 등록되지 않은 내부기 부착\n'
        '기존 연결 시험 결과: 근접 무선 연결 거부, 녹색 LED 점등.\n'
        '추가한 광고 확인: 초기 상태에서 미등록 내부기를 부착했을 때 광고가 검색되지 않는지 확인한 결과를 기록한다. 현재 광고 검색 결과는 기록되지 않았다.\n'
        '(사진필요) 등록/미등록 내부기의 연결 상태 및 LED 사진, 미등록 내부기 부착 시 블루투스 광고 검색 화면.'
    )
    updates['L188'] = '기존 연결 시험: 통과\n광고 확인: 대기'
    changes = [{'sheet': MAIN, 'cell': address, 'after': padded(value)} for address, value in updates.items()]
    for row in [188, 189]:
        condition = updates[f'J{row}'].split('\n\n통과 기준')[0]
        changes.extend([
            {'sheet': HELPER, 'cell': f'B{row}', 'after': condition},
            {'sheet': HELPER, 'cell': f'C{row}', 'after': updates[f'K{row}']},
            {'sheet': HELPER, 'cell': f'D{row}', 'after': '기존 연결 시험: 통과\n광고 확인 및 사진 첨부 대기'},
        ])
    heights = [
        {'sheet': MAIN, 'row': 188, 'height': 195},
        {'sheet': MAIN, 'row': 189, 'height': 120},
        {'sheet': HELPER, 'row': 188, 'height': 66},
        {'sheet': HELPER, 'row': 189, 'height': 66},
    ]
    plan = {'changes': changes, 'heights': heights,
            'scope': 'Existing test46 only; keep IDs/rows/merges and cybersecurity sheet unchanged.'}
    (QA / 'baseline.json').write_text(json.dumps(baseline, ensure_ascii=False, indent=2), 'utf8')
    (QA / 'plan.json').write_text(json.dumps(plan, ensure_ascii=False, indent=2), 'utf8')
    print(f'Prepared {len(changes)} report cells and common criteria cells.')


def build():
    plan = json.loads((QA / 'plan.json').read_text('utf8'))
    authored = reader.extract(QA / 'system-authored.xlsx')
    for kind in ['system', 'criterion']:
        package = native.parts(QA / f'{kind}-before.xlsx')
        changes = plan['changes'] if kind == 'system' else [x for x in plan['changes'] if x['sheet'] == MAIN and x['cell'][0] not in 'KL']
        for source_name in {x['sheet'] for x in changes}:
            name = CRITERION if kind == 'criterion' else source_name
            part = native.sheet_path(package, name)
            root = native.parse(package[part])
            all_cells = {x.get('r'): x for x in root.findall('.//s:c', native.N)}
            for change in [x for x in changes if x['sheet'] == source_name]:
                text = authored['sheets'][source_name]['cells'][change['cell']]
                assert text == change['after']
                bridge.inline(all_cells[change['cell']], text)
            for item in [x for x in plan['heights'] if x['sheet'] == source_name]:
                row = root.find(f's:sheetData/s:row[@r="{item["row"]}"]', native.N)
                row.set('ht', str(item['height']))
                row.set('customHeight', '1')
            package[part] = native.serialize(root)
        bridge.save(package, QA / f'{kind}-candidate.xlsx')
    print('Preserved original packages and changed only selected cells/heights.')


def verify(published=False):
    plan = json.loads((QA / 'plan.json').read_text('utf8'))
    baseline = json.loads((QA / 'baseline.json').read_text('utf8'))
    counts = {}
    for kind, record in baseline['files'].items():
        current = Path(record['path']) if published else QA / f'{kind}-candidate.xlsx'
        old = reader.extract(QA / f'{kind}-before.xlsx')
        new = reader.extract(current)
        expected = plan['changes'] if kind == 'system' else [x for x in plan['changes'] if x['sheet'] == MAIN and x['cell'][0] not in 'KL']
        mapping = {(CRITERION if kind == 'criterion' else x['sheet'], x['cell']): x['after'] for x in expected}
        assert list(old['sheets']) == list(new['sheets'])
        actual = []
        for name, source in old['sheets'].items():
            target = new['sheets'][name]
            assert source['merges'] == target['merges'] and source['dimension'] == target['dimension']
            for address in source['cells'].keys() | target['cells'].keys():
                left, right = source['cells'].get(address, ''), target['cells'].get(address, '')
                if left != right:
                    assert (name, address) in mapping, ('Unexpected cell', kind, name, address)
                    actual.append((name, address))
                if (name, address) in mapping:
                    assert right == mapping[(name, address)]
        assert len(actual) == len(expected)
        original_package, new_package = native.parts(QA / f'{kind}-before.xlsx'), native.parts(current)
        changed_parts = {native.sheet_path(new_package, name) for name, _ in mapping}
        assert original_package.keys() == new_package.keys()
        for part, raw in original_package.items():
            if part not in changed_parts:
                assert new_package[part] == raw, ('Unexpected package change', kind, part)
                continue
            left, right = native.parse(raw), native.parse(new_package[part])
            changed_addresses = {a for n, a in mapping if native.sheet_path(new_package, n) == part}
            for address in changed_addresses:
                a = left.find(f'.//s:c[@r="{address}"]', native.N)
                b = right.find(f'.//s:c[@r="{address}"]', native.N)
                assert a.attrib == b.attrib or (a.get('s') == b.get('s') and a.get('r') == b.get('r'))
                for c in [a, b]:
                    for child in list(c):
                        c.remove(child)
                    c.attrib.pop('t', None)
            for root in [left, right]:
                for row in root.findall('s:sheetData/s:row', native.N):
                    row.attrib.pop('ht', None)
                    row.attrib.pop('customHeight', None)
            assert native.serialize(left) == native.serialize(right), ('Unrelated sheet structure changed', kind, part)
        main_name = MAIN if kind == 'system' else CRITERION
        ids = [v.strip() for a, v in new['sheets'][main_name]['cells'].items() if re.fullmatch(r'B\d+', a) and re.fullmatch(r'SW-FW-SYS-Test-ID-\d+', v.strip())]
        assert len(ids) == len(set(ids)) == 81
        counts[kind] = {'changedCells': len(actual), 'ids': len(ids), 'sha256': new['sha256']}
    report = reader.extract(Path(baseline['files']['system']['path']) if published else QA / 'system-candidate.xlsx')
    criterion = reader.extract(Path(baseline['files']['criterion']['path']) if published else QA / 'criterion-candidate.xlsx')
    rc, cc = report['sheets'][MAIN]['cells'], criterion['sheets'][CRITERION]['cells']
    for address in rc.keys() | cc.keys():
        if re.fullmatch(r'[A-J]\d+', address) and address != 'B2':
            assert rc.get(address, '') == cc.get(address, ''), ('Common value mismatch', address)
    for filename, digest in baseline['protected'].items():
        assert reader.sha(Path(filename)) == digest, ('Protected source changed', filename)
    output = {'files': counts, 'protectedFiles': len(baseline['protected']), 'commonColumnsEqual': True,
              'newAdvertisingResult': 'not yet recorded', 'newBoardTests': False}
    (QA / ('published-verification.json' if published else 'verification.json')).write_text(json.dumps(output, ensure_ascii=False, indent=2), 'utf8')
    print(json.dumps(output, ensure_ascii=False))


def publish():
    verify()
    baseline = json.loads((QA / 'baseline.json').read_text('utf8'))
    for kind in baseline['files']:
        check = json.loads((QA / f'{kind}-native-check.json').read_text('utf-8-sig'))
        assert check['readOnly'] and check['hash'] == reader.sha(QA / f'{kind}-candidate.xlsx')
        assert all(x['needed'] <= x['current'] + 1 for x in check['measurements'])
    for record in baseline['files'].values():
        assert reader.sha(Path(record['path'])) == record['sha256'], 'User changed target after snapshot'
    for kind, record in baseline['files'].items():
        shutil.copyfile(QA / f'{kind}-candidate.xlsx', record['path'])
    verify(published=True)


if __name__ == '__main__':
    {'prepare': prepare, 'build': build, 'verify': verify, 'publish': publish}[sys.argv[1]]()
