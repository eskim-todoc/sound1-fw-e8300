"""Remove six authorized SRS notices, preserving all unrelated workbook parts."""
from copy import deepcopy
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib
import json
import re
import shutil
import sys
from lxml import etree as E
from review_srs_materials import TASK, DHF, S, NS, find_file, workbook

QA = TASK / 'qa/srs-remove-six-notices-matrix-review-20261012'
IDS = {50: 'F58', 51: 'F59', 54: 'F63', 73: 'F66', 79: 'F72', 59: 'F83'}
q = lambda n: '{' + S + '}' + n
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()


def dump(name, value):
    (QA / name).write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding='utf8')


def package(path):
    with ZipFile(path) as z:
        return {n: z.read(n) for n in z.namelist()}


def cell(root, addr):
    return root.find(f'.//s:c[@r="{addr}"]', NS)


def cell_text(c, pkg):
    if c.get('t') == 's':
        shared = E.fromstring(pkg['xl/sharedStrings.xml'])
        return ''.join(shared[int(c.findtext(q('v')))].xpath('.//s:t/text()', namespaces=NS))
    if c.get('t') == 'inlineStr':
        return ''.join(c.xpath('.//s:t/text()', namespaces=NS))
    return c.findtext(q('v'))


def prepare(export_path):
    assert not (QA / 'plan.json').exists(), 'Keep the previous execution baseline'
    QA.mkdir(parents=True, exist_ok=True)
    target = find_file('02_설계 입력', 'SRS')
    data = workbook(target)
    sheet = data['소프트웨어 입력 요구사항 사양서']
    edits = []
    for rid, addr in IDS.items():
        assert sheet['cells']['G' + addr[1:]].strip() == f'SW-FW-RID-{rid}'
        before = sheet['cells'][addr]
        assert before.count('[E8300 보완]') == 1
        requirement, note = before.split('\n\n[E8300 보완]', 1)
        assert requirement.strip() and note.strip()
        edits.append({'id': rid, 'cell': addr, 'before': before, 'after': requirement,
                      'removed': '[E8300 보완]' + note})
    protected = {str(p): sha(p) for p in DHF.rglob('*')
                 if p.is_file() and p != target and not p.name.startswith('~$')}
    plan = {'target': str(target), 'sha256': sha(target), 'sheet': '소프트웨어 입력 요구사항 사양서',
            'part': sheet['part'], 'edits': edits, 'protected': protected}
    shutil.copyfile(target, QA / 'srs-before.xlsx')
    shutil.copyfile(export_path, QA / 'matrix-current-readonly.xlsx')
    dump('plan.json', plan)
    evidence = {'matrix': workbook(QA / 'matrix-current-readonly.xlsx'), 'SRS': data}
    for code, folder in [('SSTC', '03_설계 출력'), ('SSTR', '04_설계 검증')]:
        p = find_file(folder, code)
        evidence[code] = workbook(p)
        evidence[code + '_path'] = str(p)
    dump('current-readonly-evidence.json', evidence)
    print(json.dumps({'target': str(target), 'hash': plan['sha256'], 'edits': edits,
                      'protected_files': len(protected)}, ensure_ascii=False, indent=2))


def merge():
    plan = json.loads((QA / 'plan.json').read_text(encoding='utf8'))
    pkg = package(QA / 'srs-before.xlsx')
    artifact = package(QA / 'artifact.xlsx')
    original = E.fromstring(pkg[plan['part']])
    authored_sheet = workbook(QA / 'artifact.xlsx')[plan['sheet']]
    authored = E.fromstring(artifact[authored_sheet['part']])
    for edit in plan['edits']:
        assert cell_text(cell(authored, edit['cell']), artifact) == edit['after']
        c = cell(original, edit['cell'])
        c.set('t', 'inlineStr')
        for child in list(c):
            c.remove(child)
        inline = E.SubElement(c, q('is'))
        E.SubElement(inline, q('t'), {'{http://www.w3.org/XML/1998/namespace}space': 'preserve'}).text = edit['after']
    pkg[plan['part']] = E.tostring(original, encoding='UTF-8', xml_declaration=True, standalone=True)
    with ZipFile(QA / 'candidate.xlsx', 'w', ZIP_DEFLATED) as z:
        for name, content in pkg.items():
            z.writestr(name, content)
    print('Six cell values merged; all original styles and row heights preserved.')


def verify(publish=False):
    plan = json.loads((QA / 'plan.json').read_text(encoding='utf8'))
    a, b = package(QA / 'srs-before.xlsx'), package(QA / 'candidate.xlsx')
    assert set(a) == set(b)
    changed = [n for n in a if a[n] != b[n]]
    assert changed == [plan['part']], changed
    ra, rb = E.fromstring(a[plan['part']]), E.fromstring(b[plan['part']])
    for edit in plan['edits']:
        ca, cb = cell(ra, edit['cell']), cell(rb, edit['cell'])
        assert ca.get('s') == cb.get('s')
        assert cell_text(ca, a) == edit['before']
        assert cell_text(cb, b) == edit['after']
        assert '[E8300 보완]' not in cell_text(cb, b)
        cb.getparent().replace(cb, deepcopy(ca))
    assert E.tostring(ra) == E.tostring(rb), 'Unrelated sheet XML changed'
    cells = workbook(QA / 'candidate.xlsx')[plan['sheet']]['cells']
    ids = {v.strip() for a, v in cells.items() if a.startswith('G') and re.fullmatch(r'SW-FW-RID-\d+', v.strip())}
    assert ids == {f'SW-FW-RID-{n}' for n in range(1, 82)}
    upper = {int(x) for v in cells.values() for x in re.findall(r'SYS-RID-SP-FW-(\d+)', v)}
    assert upper == set(range(1, 51))
    counts = {p: sum(p in v for v in cells.values()) for p in ['[QCC 자료 입력]', '[E8300 보완]', '[QCC/E8300 자료 입력]']}
    assert list(counts.values()) == [13, 1, 2], counts
    for path, digest in plan['protected'].items():
        assert sha(Path(path)) == digest, 'External change: ' + path
    result = {'edited_cells': [e['cell'] for e in plan['edits']], 'removed_notice_ids': list(IDS),
              'notice_counts': counts, 'software_ids': len(ids), 'upper_ids': len(upper),
              'changed_parts': changed, 'all_other_cells_styles_merges_formulas_preserved': True,
              'protected_document_files': len(plan['protected'])}
    if publish:
        native = json.loads((QA / 'native-check.json').read_text(encoding='utf-8-sig'))
        assert len(native) == 6 and all(x['fits'] for x in native)
        assert json.loads((QA / 'visual-review.json').read_text(encoding='utf8'))['issues'] == []
        target = Path(plan['target'])
        assert sha(target) == plan['sha256'], 'External target edit'
        pending = target.with_name(target.name + '.pending')
        assert not pending.exists()
        shutil.copyfile(QA / 'candidate.xlsx', pending)
        assert sha(target) == plan['sha256']
        pending.replace(target)
        assert sha(target) == sha(QA / 'candidate.xlsx')
        result['sha256'] = sha(target)
    dump('publication.json' if publish else 'checks.json', result)
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    if '--merge' in sys.argv:
        merge()
    elif '--verify' in sys.argv:
        verify()
    elif '--publish' in sys.argv:
        verify(True)
    else:
        prepare(sys.argv[1])
