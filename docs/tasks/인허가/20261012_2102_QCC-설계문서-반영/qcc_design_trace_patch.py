from pathlib import Path
from copy import deepcopy
from datetime import datetime, timezone
import hashlib
import json
import zipfile
from lxml import etree as ET

TASK = Path(__file__).parent
BASE = Path('G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF/03_설계 출력/SW/외부기')
PATH = next(BASE.glob('[[]E8300-QCC] SDDD*.docx'))
ORIGINAL = next(BASE.glob('[[]E8300] SDDD*.docx'))
NS = {'w': 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'}
W = '{' + NS['w'] + '}'
NEW_TEXT = 'SW-FW-UnitTest-QCC-ID-4는 시스템 초기화와 SPI 상태 전달의 연계 지원 시험이다. 주 유닛 시험 SW-FW-UnitTest-QCC-ID-5와 구분하여 연결한다.'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()

before_hash = sha(PATH)
original_hash = sha(ORIGINAL)
before_file = PATH.read_bytes()
with zipfile.ZipFile(PATH) as z:
    entries = [(deepcopy(i), z.read(i.filename)) for i in z.infolist()]
raw = dict((i.filename, data) for i, data in entries)['word/document.xml']
root = ET.fromstring(raw)
matches = []
for index, table in enumerate(root.xpath('//w:body//w:tbl', namespaces=NS), 1):
    text = ''.join(table.xpath('.//w:t[not(ancestor::w:del)]/text()', namespaces=NS))
    if '원문 SW-FW-DS-ID-002 / 시스템 상태 유닛' in text:
        for row_index, row in enumerate(table.findall(W + 'tr'), 1):
            cells = row.findall(W + 'tc')
            if len(cells) == 2 and ''.join(cells[0].xpath('.//w:t/text()', namespaces=NS)) == '유닛 시험 연결':
                matches.append((index, row_index, cells[1]))
assert len(matches) == 1, len(matches)
table_index, row_index, cell = matches[0]
paragraphs = cell.findall(W + 'p')
assert len(paragraphs) == 1
paragraph = paragraphs[0]
before_text = ''.join(paragraph.xpath('.//w:t[not(ancestor::w:del)]/text()', namespaces=NS))
assert before_text == 'SW-FW-UnitTest-QCC-ID-5', before_text
revision_id = str(max(int(v) for v in root.xpath('//@w:id', namespaces=NS) if v.isdigit()) + 1)
date = datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace('+00:00', 'Z')
insert = ET.SubElement(paragraph, W + 'ins', {W + 'id': revision_id, W + 'author': '김은수', W + 'date': date})
run = ET.SubElement(insert, W + 'r')
props = ET.SubElement(run, W + 'rPr')
ET.SubElement(props, W + 'rFonts', {W + 'ascii': '맑은 고딕', W + 'hAnsi': '맑은 고딕', W + 'eastAsia': '맑은 고딕'})
ET.SubElement(props, W + 'sz', {W + 'val': '20'})
ET.SubElement(props, W + 'szCs', {W + 'val': '20'})
ET.SubElement(run, W + 'br')
ET.SubElement(run, W + 't').text = NEW_TEXT
assert not insert.xpath('ancestor::w:ins|ancestor::w:del', namespaces=NS)
rejected = deepcopy(root)
rejected_insert = rejected.xpath('//w:ins[@w:id="' + revision_id + '"]', namespaces=NS)
assert len(rejected_insert) == 1
rejected_insert[0].getparent().remove(rejected_insert[0])
assert ET.tostring(rejected, method='c14n') == ET.tostring(ET.fromstring(raw), method='c14n')
backup = TASK / 'qa/qcc_design/SDDD-before-unit002-support-trace.docx'
backup.write_bytes(before_file)
temp = TASK / 'qa/qcc_design/SDDD-unit002-support-trace.docx'
with zipfile.ZipFile(temp, 'w') as z:
    for info, data in entries:
        z.writestr(info, ET.tostring(root, encoding='UTF-8', xml_declaration=True, standalone=True) if info.filename == 'word/document.xml' else data)
with zipfile.ZipFile(temp) as z:
    assert all(z.read(i.filename) == data for i, data in entries if i.filename != 'word/document.xml')
assert sha(PATH) == before_hash, 'Concurrent target edit detected'
PATH.write_bytes(temp.read_bytes())
assert sha(ORIGINAL) == original_hash
record = {
    'date': date, 'path': str(PATH), 'sha256_before': before_hash, 'sha256_after': sha(PATH),
    'locator': {'unit': 'SU-ID-QCC-002 시스템 상태 유닛', 'source_unit': 'SW-FW-DS-ID-002', 'body_table_1based': table_index, 'table_row_1based': row_index, 'cell_1based': 2, 'row_title': '유닛 시험 연결'},
    'before': before_text, 'inserted': NEW_TEXT,
    'tracking': {'revision_id': revision_id, 'author': '김은수', 'type': 'w:ins direct paragraph child', 'nested_in_prior_ins_del': False, 'reject_new_c14n_equals_before': True},
    'package': {'only_document_xml_changed': True, 'Office_fields_other_parts_and_prior_revisions_preserved': True},
    'original': {'path': str(ORIGINAL), 'sha256_before': original_hash, 'sha256_after': sha(ORIGINAL), 'unchanged': True},
    'native_render': 'root final render pending; heading/TOC text unchanged'
}
evidence_path = TASK / 'evidence/qcc_design_authored.json'
evidence = json.loads(evidence_path.read_text('utf-8'))
evidence.setdefault('post_office_targeted_patches', []).append(record)
evidence_path.write_text(json.dumps(evidence, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(record, ensure_ascii=True))
