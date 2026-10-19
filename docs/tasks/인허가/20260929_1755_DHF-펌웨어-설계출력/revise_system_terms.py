"""시스템 시험의 용어만 수정하고 원본 셀 서식과 다른 기록을 보존한다."""
import copy
import json
import re
import shutil
import sys
from pathlib import Path

import revise_system_readability as base

QA = base.TASK / 'qa/system-terminology-rev0'
REPLACEMENTS = [
    ('512표본에 창 함수를', '512개 샘플에 해닝 윈도우를'),
    ('256개 크기 값을', '256개 주파수 성분의 크기를'),
    ('디지털 표본 입력', '디지털 샘플 입력'),
    ('시험자가 실벽 되는가?', '시험자가 식별되는가?'),
    ('추적이 가증한가?', '추적이 가능한가?'),
]


def read(name):
    return json.loads((QA / name).read_text('utf-8-sig'))


def write(name, data):
    (QA / name).write_text(json.dumps(data, ensure_ascii=False, indent=2), 'utf8')


def prepare():
    QA.mkdir(parents=True, exist_ok=True)
    assert not (QA / 'baseline.json').exists(), '기존 기준 사본을 덮어쓰지 않는다.'
    previous = json.loads((base.TASK / 'qa/system-concise-rev0/baseline.json').read_text('utf8'))
    baseline = {'files': {}, 'protected': {p: base.reader.sha(Path(p)) for p in previous['protected']}}
    for kind, record in previous['files'].items():
        path = Path(record['path'])
        baseline['files'][kind] = {'path': str(path), 'sha256': base.reader.sha(path)}
        shutil.copy2(path, QA / f'{kind}-before.xlsx')
    changes = []
    for sheet, data in base.reader.extract(QA / 'system-before.xlsx')['sheets'].items():
        for cell, before in data['cells'].items():
            after = before
            for old, new in REPLACEMENTS:
                after = after.replace(old, new)
            if before != after:
                changes.append({'sheet': sheet, 'cell': cell, 'before': before, 'after': after})
    write('baseline.json', baseline)
    write('plan.json', {'changes': changes, 'views': [['fft', 'I53:K53'], ['i2s', 'J80:K80']], 'inspectRange': 'J53:K53'})
    print(json.dumps(changes, ensure_ascii=False, indent=2))


def build():
    plan = read('plan.json')
    authored = base.reader.extract(QA / 'system-authored.xlsx')['sheets']
    for change in plan['changes']:
        assert authored[change['sheet']]['cells'][change['cell']] == change['after']
    for kind in read('baseline.json')['files']:
        package = base.native.parts(QA / f'{kind}-before.xlsx')
        roots = {}
        for change in base.expected_changes(kind, plan):
            sheet = base.CRITERION if kind == 'criterion' and change['sheet'] == base.MAIN else change['sheet']
            part = base.native.sheet_path(package, sheet)
            if part not in roots:
                roots[part] = base.native.parse(package[part])
            cell = next(c for c in roots[part].findall('.//s:c', base.native.N) if c.get('r') == change['cell'])
            base.bridge.inline(cell, change['after'])
        for part, root in roots.items():
            package[part] = base.native.serialize(root)
        base.bridge.save(package, QA / f'{kind}-candidate.xlsx')
    verify()


def verify(published=False):
    baseline, plan = read('baseline.json'), read('plan.json')
    counts = {}
    finals = {}
    for kind, record in baseline['files'].items():
        old_path = QA / f'{kind}-before.xlsx'
        new_path = Path(record['path']) if published else QA / f'{kind}-candidate.xlsx'
        old, new = base.reader.extract(old_path), base.reader.extract(new_path)
        expected = {(base.CRITERION if kind == 'criterion' and x['sheet'] == base.MAIN else x['sheet'], x['cell']): x for x in base.expected_changes(kind, plan)}
        actual = set()
        assert old['sheets'].keys() == new['sheets'].keys()
        changed_parts = set()
        for name, sheet in old['sheets'].items():
            target = new['sheets'][name]
            assert sheet['merges'] == target['merges'] and sheet['dimension'] == target['dimension']
            for address in sheet['cells'].keys() | target['cells'].keys():
                before, after = sheet['cells'].get(address), target['cells'].get(address)
                if before != after:
                    entry = expected[(name, address)]
                    assert (before, after) == (entry['before'], entry['after'])
                    assert re.findall(r'\d+', before) == re.findall(r'\d+', after)
                    actual.add((name, address))
                    changed_parts.add(sheet['part'])
        assert actual == expected.keys()
        p0, p1 = base.native.parts(old_path), base.native.parts(new_path)
        assert p0.keys() == p1.keys()
        for part, raw in p0.items():
            if part not in changed_parts:
                assert raw == p1[part], part
                continue
            r0, r1 = base.native.parse(raw), base.native.parse(p1[part])
            cells0 = {c.get('r'): c for c in r0.findall('.//s:c', base.native.N)}
            name = next(n for n, s in old['sheets'].items() if s['part'] == part)
            # 변경된 문자열만 원복했을 때 XML 전체가 같아야 한다.
            for cell in r1.findall('.//s:c', base.native.N):
                if (name, cell.get('r')) in expected:
                    assert cell.get('s') == cells0[cell.get('r')].get('s')
                    cell.getparent().replace(cell, copy.deepcopy(cells0[cell.get('r')]))
            assert base.native.serialize(r0) == base.native.serialize(r1), part
        counts[kind] = {'changedCells': len(actual), 'sha256': new['sha256']}
        finals[kind] = new['sheets']
    r = finals['system'][base.MAIN]['cells']
    c = finals['criterion'][base.CRITERION]['cells']
    for a in r.keys() | c.keys():
        if re.fullmatch(r'[A-J]\d+', a) and a != 'B2':
            assert r.get(a, '') == c.get(a, ''), a
    assert all(base.reader.sha(Path(p)) == digest for p, digest in baseline['protected'].items())
    write('published-verification.json' if published else 'verification.json', {'files': counts, 'commonColumnDifferences': 0, 'unchangedProtectedFiles': len(baseline['protected']), 'layoutAndOtherPartsUnchanged': True})
    print(json.dumps(counts, ensure_ascii=False))


def publish():
    verify()
    baseline = read('baseline.json')
    # 사용자가 검토 중 저장한 내용이 있으면 교체하지 않는다.
    for kind, rec in baseline['files'].items():
        assert base.reader.sha(Path(rec['path'])) == rec['sha256']
        check = read(f'{kind}-native-check.json')
        assert check['readOnly'] and all(x['needed'] <= x['current'] for x in check['measurements'])
    for kind, rec in baseline['files'].items():
        shutil.copyfile(QA / f'{kind}-candidate.xlsx', rec['path'])
    verify(published=True)


if __name__ == '__main__':
    {'prepare': prepare, 'build': build, 'verify': verify, 'publish': publish}[sys.argv[1]]()
