"""Extract current, not deleted, design/test evidence for an SRS notice review."""
from pathlib import Path
from zipfile import ZipFile
import hashlib
import json
import re
import shutil
from lxml import etree as E

TASK = Path(__file__).resolve().parent
ROOT = TASK.parents[3]
DHF = ROOT / 'docs/참고/인허가/사운드1/DHF'
QA = TASK / 'qa/srs-material-review-20261012'
S = 'http://schemas.openxmlformats.org/spreadsheetml/2006/main'
W = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'
R = 'http://schemas.openxmlformats.org/officeDocument/2006/relationships'
NS = {'s': S, 'w': W}
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()


def find_file(folder, document):
    found = [p for p in (DHF / folder).rglob('*') if p.is_file()
             and p.name.startswith('[김은수 작성] [Rev.0] ' + document + '-')]
    assert len(found) == 1, (folder, document, found)
    return found[0]


def workbook(path):
    with ZipFile(path) as z:
        shared = []
        if 'xl/sharedStrings.xml' in z.namelist():
            shared = [''.join(x.xpath('.//s:t/text()', namespaces=NS))
                      for x in E.fromstring(z.read('xl/sharedStrings.xml'))]
        names = E.fromstring(z.read('xl/workbook.xml')).find('{'+S+'}sheets')
        rels = {x.get('Id'): x.get('Target') for x in E.fromstring(z.read('xl/_rels/workbook.xml.rels'))}
        sheets = {}
        for sh in names:
            part = rels[sh.get('{'+R+'}id')]
            part = part.lstrip('/') if part.startswith('/') else 'xl/' + part
            tree = E.fromstring(z.read(part))
            cells = {}
            for c in tree.iter('{'+S+'}c'):
                if c.get('t') == 's':
                    txt = shared[int(c.findtext('{'+S+'}v'))]
                elif c.get('t') == 'inlineStr':
                    txt = ''.join(c.xpath('.//s:t/text()', namespaces=NS))
                else:
                    txt = c.findtext('{'+S+'}v')
                if txt is not None:
                    cells[c.get('r')] = txt
            sheets[sh.get('name')] = {'part': part, 'cells': cells,
                'merges': [x.get('ref') for x in tree.findall('.//s:mergeCell', NS)]}
        return sheets


def visible(element):
    if element.xpath('ancestor::w:del|ancestor::w:moveFrom|ancestor-or-self::w:tr[w:trPr/w:del]', namespaces=NS):
        return False
    return True


def doc_text(element):
    return '\n'.join(''.join(x.text or '' for x in p.iter('{'+W+'}t') if visible(x))
                     for p in element.iter('{'+W+'}p') if visible(p)).strip()


def extract():
    QA.mkdir(parents=True, exist_ok=True)
    paths = {'SRS': find_file('02_설계 입력', 'SRS'),
             'SAD': find_file('03_설계 출력', 'SAD'),
             'SSTC': find_file('03_설계 출력', 'SSTC'),
             'SSTR': find_file('04_설계 검증', 'SSTR')}
    for kind, path in paths.items():
        dest = QA / (kind.lower() + '-before' + path.suffix)
        if dest.exists():
            assert sha(dest) == sha(path), 'External edit: ' + kind
        else:
            shutil.copyfile(path, dest)
    data = {'paths': {k: str(v) for k, v in paths.items()},
            'hashes': {k: sha(v) for k, v in paths.items()},
            'workbooks': {k: workbook(v) for k, v in paths.items() if v.suffix == '.xlsx'}}
    with ZipFile(paths['SAD']) as z:
        tree = E.fromstring(z.read('word/document.xml'))
        tables = []
        for i, table in enumerate(tree.iter('{'+W+'}tbl')):
            rows = [[doc_text(c) for c in row.findall('w:tc', NS)]
                    for row in table.findall('w:tr', NS) if visible(row)]
            tables.append({'index': i, 'rows': rows})
        data['architecture_tables'] = tables
        data['architecture_notices'] = [doc_text(p) for p in tree.iter('{'+W+'}p')
            if visible(p) and re.search(r'\[(?:QCC|E8300)|資料|資料|보완|입력하세요|대기', doc_text(p))]
    files = [p for p in DHF.rglob('*') if p.is_file() and not p.name.startswith('~$')]
    data['protected'] = {str(p): sha(p) for p in files if p != paths['SRS']}
    (QA / 'evidence.json').write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf8')
    for k, v in data['workbooks'].items():
        print(k, [(n, len(s['cells'])) for n, s in v.items()])
    print('SAD tables', [(x['index'], len(x['rows'])) for x in data['architecture_tables']])
    print('SAD notices', len(data['architecture_notices']))


if __name__ == '__main__':
    extract()
