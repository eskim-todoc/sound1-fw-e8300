from pathlib import Path
from copy import deepcopy
from io import BytesIO
import sys, json, re, hashlib, zipfile, posixpath
from lxml import etree as E

H = Path(__file__).resolve().parent
TARGET = Path('G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF/04_설계 검증/SW/외부기/[E8300-QCC] [완료 - QCC 입력 대기] SSTR-FW-TD24A 소프트웨어 시스템 시험 보고서.xlsx')
BASE_SHA = '50C8DEF81BA74982C33E537DEB8F14B92A19479A8667E92BF035CF630A6E793D'
S = '{http://schemas.openxmlformats.org/spreadsheetml/2006/main}'
R = '{http://schemas.openxmlformats.org/officeDocument/2006/relationships}'
SHEET = '보고서_시스템 시험'
COLS = 'BCDEFGHIJKL'

def digest(data):
    return hashlib.sha256(data).hexdigest().upper()

def save(name, value):
    (H / name).write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

def xml(node):
    return E.tostring(node, encoding='UTF-8', xml_declaration=True, standalone=True)

def sig(node):
    return (node.tag, tuple(sorted(node.attrib.items())), node.text or '', tuple(sig(x) for x in node))

class Book:
    def __init__(self, path):
        self.data = Path(path).read_bytes()
        self.sha = digest(self.data)
        self.z = zipfile.ZipFile(BytesIO(self.data))
        self.parts = {i.filename: self.z.read(i) for i in self.z.infolist()}
        self.styles = E.fromstring(self.parts['xl/styles.xml'])
        wb = E.fromstring(self.parts['xl/workbook.xml'])
        rel = {x.get('Id'): x.get('Target') for x in E.fromstring(self.parts['xl/_rels/workbook.xml.rels'])}
        self.names = {x.get('name'): resolve('xl/workbook.xml', rel[x.get(R + 'id')]) for x in wb.find(S + 'sheets')}
        self.sheets = {n: E.fromstring(self.parts[p]) for n, p in self.names.items()}
        self.cells = {n: {c.get('r'): c for c in sh.findall(S + 'sheetData/' + S + 'row/' + S + 'c')} for n, sh in self.sheets.items()}
        self.strings = list(E.fromstring(self.parts['xl/sharedStrings.xml'])) if 'xl/sharedStrings.xml' in self.parts else []

    def value(self, sheet, address):
        c = self.cells[sheet].get(address)
        if c is None:
            return ''
        t = c.get('t')
        el = self.strings[int(c.findtext(S + 'v', '0'))] if t == 's' else c.find(S + 'is') if t == 'inlineStr' else None
        return ''.join(x.text or '' for x in el.iter(S + 't')) if el is not None else c.findtext(S + 'v', '')

    def fill(self, sheet, address):
        c = self.cells[sheet].get(address)
        if c is None:
            return None
        xf = self.styles.find(S + 'cellXfs')[int(c.get('s', '0'))]
        return self.styles.find(S + 'fills')[int(xf.get('fillId', '0'))]

def resolve(part, target):
    return posixpath.normpath(target.lstrip('/')) if target.startswith('/') else posixpath.normpath(posixpath.join(posixpath.dirname(part), target))

def write_zip(book, path, replacements=None, removed=None):
    replacements, removed = replacements or {}, removed or set()
    with zipfile.ZipFile(path, 'w') as z:
        for info in book.z.infolist():
            if info.filename not in removed:
                z.writestr(info, replacements.get(info.filename, book.parts[info.filename]))

def color(fill):
    p = fill.find(S + 'patternFill') if fill is not None else None
    f = p.find(S + 'fgColor') if p is not None else None
    return f.get('rgb', '').upper() if f is not None else ''

def bounds(ref):
    a, *rest = ref.split(':')
    b = rest[0] if rest else a
    return re.fullmatch(r'([A-Z]+)(\d+)', a).groups(), re.fullmatch(r'([A-Z]+)(\d+)', b).groups()

def prepare():
    data = TARGET.read_bytes()
    assert digest(data) == BASE_SHA, 'Source changed before snapshot'
    (H / 'before.xlsx').write_bytes(data)
    b = Book(H / 'before.xlsx')
    starts = []
    for addr in b.cells[SHEET]:
        if not re.fullmatch(r'B\d+', addr):
            continue
        m = re.fullmatch(r'\s*SW-FW-SYS-Test-ID-(\d+)\s*', b.value(SHEET, addr))
        if m:
            starts.append((int(addr[1:]), int(m[1])))
    starts.sort()
    assert len(starts) == 83 and sorted(i for r, i in starts) == list(range(1, 84))
    merges = [x.get('ref') for x in b.sheets[SHEET].find(S + 'mergeCells')]
    last = starts[-1][0]
    for ref in merges:
        (c1, r1), (c2, r2) = bounds(ref)
        if int(r1) <= starts[-1][0] <= int(r2):
            last = max(last, int(r2))
    blocks = [{'order': j + 1, 'id': i, 'start': r, 'end': starts[j + 1][0] - 1 if j + 1 < len(starts) else last,
               'color': 'FFFFFF' if j % 2 == 0 else 'F5F5F5'} for j, (r, i) in enumerate(starts)]
    preserve, pink_ranges, pink_ids = set(), [], []
    for block in blocks:
        addr = 'B' + str(block['start'])
        if color(b.fill(SHEET, addr)) != 'FFFFEBFF':
            continue
        region = next((ref for ref in merges if bounds(ref)[0] == ('B', str(block['start'])) and bounds(ref)[1][0] == 'B'), addr)
        (c1, r1), (c2, r2) = bounds(region)
        addresses = ['B' + str(r) for r in range(int(r1), int(r2) + 1)]
        preserve.update(addresses)
        pink_ranges.append({'id': block['id'], 'range': region, 'addresses': addresses})
        pink_ids.append(block['id'])
    alladdresses = [f'{col}{r}' for block in blocks for r in range(block['start'], block['end'] + 1) for col in COLS]
    assert all(a in b.cells[SHEET] for a in alladdresses), 'Source has missing body cell; inspect before authoring'
    plan = {'target': str(TARGET), 'sha': BASE_SHA, 'sheet': SHEET, 'columns': 'B:L', 'firstRow': starts[0][0], 'lastRow': last,
            'blocks': blocks, 'preservedPinkRanges': pink_ranges, 'preserveAddresses': sorted(preserve, key=lambda a: int(a[1:])),
            'pinkIDs': pink_ids, 'pinkPolicy': 'User confirmed: preserve original pink TEST ID B merged areas including subordinate cells; band other body cells by physical TEST order.',
            'marker': 'Root already executed edit marker once for all three files; do not rerun.', 'authorInput': 'author-input.xlsx'}
    save('plan.json', plan)
    prefixes = ['[QCC 자료 입력]', '[QCC/E8300 자료 입력]', '[E8300 보완]']
    notices = []
    for block in blocks:
        for row in range(block['start'], block['end'] + 1):
            for col in COLS:
                addr = f'{col}{row}'
                text = b.value(SHEET, addr)
                for prefix in prefixes:
                    if prefix in text:
                        notices.append({'prefix': prefix, 'cell': addr, 'testID': f"SW-FW-SYS-Test-ID-{block['id']}", 'text': text})
    save('notices.json', {'target': str(TARGET), 'sha': BASE_SHA, 'sheet': SHEET, 'range': f'B{starts[0][0]}:L{last}', 'notices': notices})
    # This import-only copy omits images; none of these parts are used for final packaging.
    removed = {p for p in b.parts if p.startswith(('xl/drawings/', 'xl/media/'))}
    patch = {}
    for part, contents in b.parts.items():
        if part in removed:
            continue
        if part.endswith('.rels'):
            root = E.fromstring(contents)
            parent = posixpath.join(posixpath.dirname(posixpath.dirname(part)), posixpath.basename(part)[:-5])
            gone = []
            for rel in list(root):
                if rel.get('TargetMode') != 'External' and resolve(parent, rel.get('Target')) in removed:
                    gone.append(rel.get('Id'))
                    root.remove(rel)
            if gone:
                patch[part] = xml(root)
                if parent.startswith('xl/worksheets/') and parent in b.parts:
                    sheet = E.fromstring(patch.get(parent, b.parts[parent]))
                    for node in list(sheet):
                        if node.get(R + 'id') in gone:
                            sheet.remove(node)
                    patch[parent] = xml(sheet)
    ct = E.fromstring(b.parts['[Content_Types].xml'])
    for node in list(ct):
        if node.get('PartName', '').lstrip('/') in removed:
            ct.remove(node)
    patch['[Content_Types].xml'] = xml(ct)
    write_zip(b, H / 'author-input.xlsx', patch, removed)
    save('prepare.json', {'sourceSHA': b.sha, 'sourceBytes': len(data), 'tests': len(blocks), 'bodyRange': f'B{starts[0][0]}:L{last}',
                          'pinkTESTs': len(pink_ids), 'preservedCells': len(preserve), 'notices': len(notices), 'removedAuthorOnlyParts': len(removed),
                          'finalPackagePolicy': 'Original package; fill-only style additions and main worksheet s attributes only.'})
    print(json.dumps({'status': 'prepared', 'tests': len(blocks), 'range': f'B{starts[0][0]}:L{last}', 'pinkTESTs': len(pink_ids), 'preservedCells': len(preserve)}))

def integrate():
    b, a = Book(H / 'before.xlsx'), Book(H / 'authored.xlsx')
    p = json.loads((H / 'plan.json').read_text(encoding='utf-8'))
    assert b.sha == p['sha'] and digest(TARGET.read_bytes()) == p['sha'], 'Source hash guard'
    xfs, fills = b.styles.find(S + 'cellXfs'), b.styles.find(S + 'fills')
    ids = {}
    for co in ['FFFFFF', 'F5F5F5']:
        ids[co] = next(i for i, f in enumerate(fills) if color(f) == 'FF' + co and f.find(S + 'patternFill').get('patternType') == 'solid')
    preserve = set(p['preserveAddresses'])
    cache, changed, authored = {}, [], []
    for block in p['blocks']:
        for row in range(block['start'], block['end'] + 1):
            for col in COLS:
                addr = f'{col}{row}'
                if addr in preserve:
                    continue
                assert color(a.fill(SHEET, addr))[-6:] == block['color'], (addr, 'Artifact fill not authored')
                authored.append(addr)
                c = b.cells[SHEET][addr]
                old, fid = int(c.get('s', '0')), ids[block['color']]
                key = (old, fid)
                if key not in cache:
                    xf = deepcopy(xfs[old]); xf.set('fillId', str(fid)); xf.set('applyFill', '1')
                    same = next((i for i, x in enumerate(xfs) if sig(x) == sig(xf)), None)
                    if same is None:
                        same = len(xfs); xfs.append(xf)
                    cache[key] = same
                if cache[key] != old:
                    changed.append(addr)
                    c.set('s', str(cache[key]))
    xfs.set('count', str(len(xfs)))
    write_zip(b, H / 'candidate.xlsx', {'xl/styles.xml': xml(b.styles), b.names[SHEET]: xml(b.sheets[SHEET])})
    save('authoring.json', {'artifactFileSHA': a.sha, 'checkedAuthoredCells': len(authored), 'changedStyleCells': len(changed), 'pinkPreservedCells': len(preserve)})
    verify()

def verify():
    b, a = Book(H / 'before.xlsx'), Book(H / 'candidate.xlsx')
    p = json.loads((H / 'plan.json').read_text(encoding='utf-8'))
    assert b.sha == p['sha']
    assert b.parts.keys() == a.parts.keys()
    allowed = {'xl/styles.xml', b.names[SHEET]}
    unchanged = [n for n in b.parts if n not in allowed]
    assert all(b.parts[n] == a.parts[n] for n in unchanged), 'A non-main/style package part changed'
    bx, ax = b.styles.find(S + 'cellXfs'), a.styles.find(S + 'cellXfs')
    assert all(sig(x) == sig(ax[i]) for i, x in enumerate(bx)), 'An original style changed'
    bs, ass = deepcopy(b.styles), deepcopy(a.styles)
    bs.remove(bs.find(S + 'cellXfs')); ass.remove(ass.find(S + 'cellXfs'))
    assert sig(bs) == sig(ass), 'Non-cellXfs styles changed'
    expected = {f'{col}{r}': block['color'] for block in p['blocks'] for r in range(block['start'], block['end'] + 1) for col in COLS}
    preserve = set(p['preserveAddresses'])
    assert a.cells[SHEET].keys() == b.cells[SHEET].keys()
    for addr, cell in a.cells[SHEET].items():
        orig = b.cells[SHEET][addr]
        ac, bc = deepcopy(cell), deepcopy(orig)
        if addr in expected and addr not in preserve:
            assert color(a.fill(SHEET, addr)) == 'FF' + expected[addr], (addr, 'wrong fill')
            af, bf = deepcopy(ax[int(cell.get('s', '0'))]), deepcopy(bx[int(orig.get('s', '0'))])
            for attr in ['fillId', 'applyFill']:
                af.attrib.pop(attr, None); bf.attrib.pop(attr, None)
            assert sig(af) == sig(bf), (addr, 'non-fill style changed')
            ac.attrib.pop('s', None); bc.attrib.pop('s', None)
        assert sig(ac) == sig(bc), (addr, 'cell content or protected style changed')
    ash, bsh = deepcopy(a.sheets[SHEET]), deepcopy(b.sheets[SHEET])
    ash.remove(ash.find(S + 'sheetData')); bsh.remove(bsh.find(S + 'sheetData'))
    assert sig(ash) == sig(bsh), 'Sheet layout/drawings/merges changed'
    assert [dict(r.attrib) for r in a.sheets[SHEET].find(S + 'sheetData')] == [dict(r.attrib) for r in b.sheets[SHEET].find(S + 'sheetData')], 'Rows changed'
    protected = [{'part': n, 'sha256': digest(a.parts[n])} for n in unchanged if n.startswith(('xl/media/', 'xl/drawings/'))]
    result = {'status': 'PASS', 'target': p['target'], 'sourceSHA': b.sha, 'candidateSHA': a.sha, 'sheet': SHEET,
              'range': f"B{p['firstRow']}:L{p['lastRow']}", 'tests': len(p['blocks']), 'cellsChecked': len(expected), 'bandedCells': len(expected) - len(preserve),
              'pinkPreservedCells': len(preserve), 'pinkPreservedTESTs': len(p['pinkIDs']), 'rowHeightsColumnsMergesPrint': 'unchanged',
              'textFormulasNonFillStyles': 'unchanged', 'otherSheetsAndPackageParts': 'byte-identical', 'unchangedPackageParts': len(unchanged),
              'pictureParts': protected, 'originalStylesUnchanged': len(bx), 'newStyles': len(ax) - len(bx), 'nativeVisualQA': 'root/design agent pending'}
    save('validation.json', result)
    print(json.dumps({'status': 'PASS', 'candidateSHA': a.sha, 'tests': len(p['blocks']), 'pinkPreservedCells': len(preserve), 'picturePartsPreserved': len(protected)}))

if __name__ == '__main__':
    {'prepare': prepare, 'integrate': integrate, 'verify': verify}[sys.argv[1]]()
