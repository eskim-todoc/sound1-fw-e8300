"""Keep requirement text separate from owner-specific missing-material notices."""
from pathlib import Path
from copy import deepcopy
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib
import json
import re
import shutil
import sys
from lxml import etree as E

TASK = Path(__file__).resolve().parent
ROOT = TASK.parents[3]
DHF = ROOT / 'docs/참고/인허가/사운드1/DHF'
QA = TASK / 'qa/missing-material-plan-input-20261012'
S = 'http://schemas.openxmlformats.org/spreadsheetml/2006/main'
NS = {'s': S}
q = lambda name: '{' + S + '}' + name
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()


def package(path):
    with ZipFile(path) as z:
        return {name: z.read(name) for name in z.namelist()}


def string_table(pkg):
    if 'xl/sharedStrings.xml' not in pkg:
        return []
    return [''.join(si.xpath('.//s:t/text()', namespaces=NS))
            for si in E.fromstring(pkg['xl/sharedStrings.xml'])]


def value(c, strings):
    if c.get('t') == 's':
        return strings[int(c.find(q('v')).text)]
    if c.get('t') == 'inlineStr':
        return ''.join(c.xpath('.//s:is//s:t/text()', namespaces=NS))
    return c.findtext(q('v'))


def cell(root, address):
    return root.find(f'.//s:c[@r="{address}"]', NS)


def dump_json(name, data):
    (QA / name).write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf8')


def prepare():
    QA.mkdir(parents=True, exist_ok=True)
    files = sorted(p for folder in ('01_설계 계획', '02_설계 입력')
                   for p in (DHF / folder).rglob('*') if p.is_file())
    scoped = [p for p in files if p.name.startswith('[김은수 작성] [Rev.0]')]
    assert len(scoped) == 9
    targets = [p for p in scoped if p.suffix == '.xlsx']
    assert len(targets) == 1
    target = targets[0]
    before = QA / 'before.xlsx'
    if before.exists():
        assert sha(before) == sha(target), 'Target changed after baseline'
    else:
        shutil.copyfile(target, before)
    pkg = package(before)
    tree = E.fromstring(pkg['xl/worksheets/sheet2.xml'])
    styles = E.fromstring(pkg['xl/styles.xml'])
    fonts, xfs = styles.find(q('fonts')), styles.find(q('cellXfs'))
    strings = string_table(pkg)
    base = fonts[int(xfs[int(cell(tree, 'F54').get('s', '0'))].get('fontId', '0'))]
    size = float(base.find(q('sz')).get('val'))
    assert size == 10
    entries = []
    for c in tree.iter(q('c')):
        text = value(c, strings)
        if isinstance(text, str) and text.startswith(('[QCC 단독]', '[E8300/QCC ')):
            row = c.getparent()
            rid = cell(tree, 'G' + row.get('r'))
            note = ('[QCC 자료 입력] QCC 측 설계/검증 자료를 입력한다.'
                    if text.startswith('[QCC 단독]')
                    else '[QCC 자료 입력] QCC 측 연계 설계/검증 자료를 입력한다.')
            oldfont = fonts[int(xfs[int(c.get('s', '0'))].get('fontId', '0'))]
            entries.append({'cell': c.get('r'), 'row': int(row.get('r')),
                            'before': text, 'note': note, 'after': text + '\n\n' + note,
                            'id': value(rid, strings), 'base_size': size,
                            'font_name': oldfont.find(q('name')).get('val'),
                            'height': float(row.get('ht', '15'))})
    assert len(entries) == 12
    plan = {'target': str(target), 'sha256': sha(target),
            'sheet': '소프트웨어 입력 요구사항 사양서', 'edits': entries,
            'scoped': [str(p) for p in scoped],
            'protected': {str(p): sha(p) for p in files if p != target}}
    dump_json('plan.json', plan)
    print(json.dumps(plan['edits'], ensure_ascii=False, indent=2))


def merge_native():
    plan = json.loads((QA / 'plan.json').read_text(encoding='utf8'))
    original, authored = package(QA / 'before.xlsx'), package(QA / 'artifact.xlsx')
    tree = E.fromstring(original['xl/worksheets/sheet2.xml'])
    styles = E.fromstring(original['xl/styles.xml'])
    atree = E.fromstring(authored['xl/worksheets/sheet2.xml'])
    astrings = string_table(authored)
    fonts, xfs = styles.find(q('fonts')), styles.find(q('cellXfs'))
    base = fonts[int(xfs[int(cell(tree, 'F54').get('s', '0'))].get('fontId', '0'))]
    measured = {}
    if (QA / 'native-check.json').exists():
        measured = {x['cell']: x for x in json.loads((QA / 'native-check.json').read_text(encoding='utf-8-sig'))}
    for item in plan['edits']:
        c, ac = cell(tree, item['cell']), cell(atree, item['cell'])
        assert value(ac, astrings) == item['after'], 'Artifact text authoring differs'
        xf = deepcopy(xfs[int(c.get('s', '0'))])
        font = deepcopy(base)
        # Keep the ordinary requirement text at the local 10pt/body weight.
        font_id = next((i for i, f in enumerate(fonts) if E.tostring(f) == E.tostring(font)), len(fonts))
        if font_id == len(fonts):
            fonts.append(font)
        xf.set('fontId', str(font_id)); xf.set('applyFont', '1')
        xf_id = next((i for i, f in enumerate(xfs) if E.tostring(f) == E.tostring(xf)), len(xfs))
        if xf_id == len(xfs):
            xfs.append(xf)
        c.set('s', str(xf_id)); c.set('t', 'inlineStr')
        for child in list(c):
            c.remove(child)
        inline = E.SubElement(c, q('is'))
        ordinary = E.SubElement(inline, q('r'))
        E.SubElement(ordinary, q('t'), {'{http://www.w3.org/XML/1998/namespace}space': 'preserve'}).text = item['before'] + '\n\n'
        notice = E.SubElement(inline, q('r'))
        rpr = E.SubElement(notice, q('rPr'))
        E.SubElement(rpr, q('rFont'), {'val': item['font_name']})
        E.SubElement(rpr, q('sz'), {'val': str(item['base_size'] + 2)})
        E.SubElement(rpr, q('color'), {'rgb': 'FFC00000'})
        E.SubElement(rpr, q('u'), {'val': 'single'})
        if base.find(q('b')) is not None:
            rpr.append(deepcopy(base.find(q('b'))))
        E.SubElement(notice, q('t')).text = item['note']
        if item['cell'] in measured:
            needed = measured[item['cell']]['needed'] + 4
            row = c.getparent()
            if needed > float(row.get('ht', '15')):
                assert needed <= 409.5
                row.set('ht', str(needed)); row.set('customHeight', '1')
    xfs.set('count', str(len(xfs))); fonts.set('count', str(len(fonts)))
    original['xl/styles.xml'] = E.tostring(styles, encoding='UTF-8', xml_declaration=True, standalone=True)
    original['xl/worksheets/sheet2.xml'] = E.tostring(tree, encoding='UTF-8', xml_declaration=True, standalone=True)
    with ZipFile(QA / 'candidate.xlsx', 'w', ZIP_DEFLATED) as z:
        for name, data in original.items():
            z.writestr(name, data)
    print('Candidate: 12 separate QCC notices, original workbook features preserved.')


def verify(publish=False):
    plan = json.loads((QA / 'plan.json').read_text(encoding='utf8'))
    a, b = package(QA / 'before.xlsx'), package(QA / 'candidate.xlsx')
    assert set(a) == set(b)
    changes = [n for n in a if a[n] != b[n]]
    assert set(changes) == {'xl/styles.xml', 'xl/worksheets/sheet2.xml'}
    ta, tb = E.fromstring(a['xl/worksheets/sheet2.xml']), E.fromstring(b['xl/worksheets/sheet2.xml'])
    sa, sb = string_table(a), string_table(b)
    styles = E.fromstring(b['xl/styles.xml'])
    fonts, xfs = styles.find(q('fonts')), styles.find(q('cellXfs'))
    height_changes = []
    for item in plan['edits']:
        ca, cb = cell(ta, item['cell']), cell(tb, item['cell'])
        assert value(ca, sa) == item['before'] and value(cb, sb) == item['after']
        ordinary, notice = cb.findall('s:is/s:r', NS)
        assert ordinary.find(q('t')).text == item['before'] + '\n\n'
        assert notice.find(q('t')).text == item['note']
        rpr = notice.find(q('rPr'))
        assert rpr.find(q('color')).get('rgb') == 'FFC00000'
        assert float(rpr.find(q('sz')).get('val')) == item['base_size'] + 2
        assert rpr.find(q('u')).get('val') == 'single'
        base_font = fonts[int(xfs[int(cb.get('s'))].get('fontId'))]
        assert base_font.find(q('b')) is None and rpr.find(q('b')) is None
        assert float(base_font.find(q('sz')).get('val')) == item['base_size']
        xa = E.fromstring(a['xl/styles.xml']).find(q('cellXfs'))[int(ca.get('s'))]
        xb = deepcopy(xfs[int(cb.get('s'))]); xb.set('fontId', xa.get('fontId')); xb.attrib.pop('applyFont', None)
        xa2 = deepcopy(xa); xa2.attrib.pop('applyFont', None)
        assert E.tostring(xa2) == E.tostring(xb), (item['cell'], 'non-font style differs')
        # Put the candidate cell back to baseline, then prove all other XML is identical.
        cb.getparent().replace(cb, deepcopy(ca))
    for ra, rb in zip(ta.findall('.//s:row', NS), tb.findall('.//s:row', NS)):
        if ra.attrib != rb.attrib:
            assert int(ra.get('r')) in {x['row'] for x in plan['edits']}
            height_changes.append([ra.get('r'), ra.get('ht'), rb.get('ht')])
            rb.attrib.clear(); rb.attrib.update(ra.attrib)
    assert E.tostring(ta) == E.tostring(tb), 'Unrelated worksheet content differs'
    for path, digest in plan['protected'].items():
        assert sha(Path(path)) == digest, 'Out-of-scope file changed: ' + path
    checks = {'scope_documents': 9, 'unchanged_word_documents': 8, 'QCC_notices': 12,
              'existing_requirements_IDs_formulas_merges_preserved': True,
              'protected_files': len(plan['protected']), 'changed_parts': changes,
              'height_changes': height_changes}
    if publish:
        native = json.loads((QA / 'native-check.json').read_text(encoding='utf-8-sig'))
        assert len(native) == 12 and all(x['needed'] <= x['current'] + 1 for x in native)
        visual = json.loads((QA / 'visual-review.json').read_text(encoding='utf8'))
        assert visual['issues'] == []
        target = Path(plan['target'])
        assert sha(target) == plan['sha256'], 'Target externally edited'
        pending = target.with_name(target.name + '.pending')
        assert not pending.exists()
        shutil.copyfile(QA / 'candidate.xlsx', pending)
        assert sha(target) == plan['sha256']
        pending.replace(target)
        assert sha(target) == sha(QA / 'candidate.xlsx')
        checks.update({'target': str(target), 'sha256': sha(target)})
    dump_json('publication.json' if publish else 'checks.json', checks)
    print(json.dumps(checks, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    if '--merge' in sys.argv:
        merge_native()
    elif '--verify' in sys.argv:
        verify()
    elif '--publish' in sys.argv:
        verify(True)
    else:
        prepare()
