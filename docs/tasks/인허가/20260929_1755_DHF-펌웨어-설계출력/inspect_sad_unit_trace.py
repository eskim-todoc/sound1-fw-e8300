"""현행 요구사항과 설계 문서의 변경 후 내용을 읽어 추적성 대조 자료를 만든다."""
import hashlib
import json
import re
import shutil
import sys
import zipfile
from pathlib import Path

from lxml import etree as E
import inspect_system_cybersecurity as xlsx

TASK = Path(__file__).resolve().parent
ROOT = TASK.parents[3]
DHF = ROOT / 'docs/참고/인허가/사운드1/DHF'
OUT = DHF / '03_설계 출력/SW/펌웨어'
QA = TASK / 'qa/sad-unit-trace-20261012'
SAD = OUT / 'SAD-FW-TD24A 소프트웨어 아키텍쳐 기술서_rev0(김은수 작성).docx'
SDDD = OUT / 'SDDD-FW-TD24A 소프트웨어 상세 설계서_rev0(김은수 작성).docx'
SRS = DHF / '02_설계 입력/SW/펌웨어/SRS-FW-TD24A 소프트웨어 요구사항 사양서_rev0(김은수 작성).xlsx'
W = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'
N = {'w': W}
q = lambda t: '{' + W + '}' + t

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def text(el):
    return ''.join(el.xpath('.//w:t[not(ancestor::w:del)]/text()', namespaces=N))

def blocks(path):
    with zipfile.ZipFile(path) as package:
        root = E.fromstring(package.read('word/document.xml'))
    result = []
    for index, block in enumerate(root.find(q('body'))):
        if block.tag == q('p'):
            if block.find('w:pPr/w:rPr/w:del', N) is not None:
                continue
            style = block.find('w:pPr/w:pStyle', N)
            result.append({'index': index, 'type': 'p', 'style': style.get(q('val')) if style is not None else '', 'text': text(block)})
        elif block.tag == q('tbl'):
            rows = []
            for row in block.findall('w:tr', N):
                if row.find('w:trPr/w:del', N) is not None:
                    continue
                rows.append(['\n'.join(text(p) for p in cell.findall('w:p', N)) for cell in row.findall('w:tc', N)])
            result.append({'index': index, 'type': 'table', 'rows': rows})
    return result

def prepare(refresh=False):
    QA.mkdir(exist_ok=True, parents=True)
    if not refresh:
        if (QA / 'before.docx').exists():
            raise FileExistsError('기존 검토 원본은 덮어쓰지 않는다.')
        shutil.copyfile(SAD, QA / 'before.docx')
    else:
        saved = json.loads((QA / 'inputs.json').read_text(encoding='utf8'))
        assert all(sha(Path(p)) == h for p, h in saved['sources'].items())
    sad = blocks(QA / 'before-accepted.docx') if refresh else blocks(SAD)
    sddd = blocks(QA / 'sddd-accepted.docx') if refresh else blocks(SDDD)
    source = xlsx.extract(SRS)
    cells = source['sheets']['소프트웨어 입력 요구사항 사양서']['cells']
    reqs = []
    for cell, value in cells.items():
        if re.fullmatch(r'SW-FW-RID-\d+', value):
            row = re.search(r'\d+', cell).group()
            reqs.append({'id': value, 'row': int(row), 'cells': {c: cells.get(c + row, '') for c in 'BCDEFGHI'}})
    reqs.sort(key=lambda r: int(r['id'].split('-')[-1]))
    units = []
    module = ''
    for block in sddd:
        if block['type'] == 'p':
            if re.match('SI-ID-', block['text']):
                module = block['text']
            match = re.match(r'(SU-ID-[A-Z0-9]+-\d+),?\s*(.*)', block['text'])
            if match:
                units.append({'id': match[1], 'name': match[2], 'module': module, 'blocks': []})
            elif units:
                units[-1]['blocks'].append(block)
        elif units:
            units[-1]['blocks'].append(block)
    data = {'sources': {str(p): sha(p) for p in [SAD, SDDD, SRS]}, 'sad': sad, 'requirements': reqs, 'units': units, 'sddd': sddd}
    (QA / 'inputs.json').write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf8')
    print(json.dumps({'requirements': len(reqs), 'units': len(units), 'sadTables': [(b['index'], len(b['rows']), b['rows'][:1]) for b in sad if b['type'] == 'table']}, ensure_ascii=False, indent=2))

if __name__ == '__main__':
    prepare('--refresh' in sys.argv)
