"""Word의 Table.Delete가 추적하지 않은 기존 표를 삭제 행으로 보존한다.

기존 행 내부의 수정/삭제 이력은 그대로 둔다. 새 행 삭제 기록을 거절하면
기존 표가 복원되고, 모든 변경을 거절하면 원래의 1.5세대 표가 복원된다.
새 유닛 연결 표의 삽입 이력은 Word가 생성한 것을 그대로 사용한다.
"""
import copy
from datetime import datetime, timezone
import zipfile
from lxml import etree as E
from inspect_sad_unit_trace import QA, q, N, text

with zipfile.ZipFile(QA / 'before.docx') as z:
    original = E.fromstring(z.read('word/document.xml'))
    original_styles = z.read('word/styles.xml')
old = next(t for t in original.findall('w:body/w:tbl', N) if 'SW-FW-RID-81' in text(t))
with zipfile.ZipFile(QA / 'candidate.docx') as z:
    parts = {name: z.read(name) for name in z.namelist()}
root = E.fromstring(parts['word/document.xml'])
body = root.find('w:body', N)
assert len(root.findall('w:body/w:tbl', N)) == 7
used = [int(n.get(q('id'))) for r in [original, root] for n in r.iter() if n.get(q('id'), '').isdigit()]
seq = max(used) + 1
old = copy.deepcopy(old)
date = datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
for row in old.findall('w:tr', N):
    props = row.find('w:trPr', N)
    if props is None:
        props = E.Element(q('trPr')); row.insert(0, props)
    deletion = E.Element(q('del'), {q('id'): str(seq), q('author'): '김은수', q('date'): date})
    seq += 1
    change = props.find('w:trPrChange', N)
    if change is None: props.append(deletion)
    else: props.insert(list(props).index(change), deletion)
intro = next(p for p in body.findall('w:p', N) if text(p).startswith('소프트웨어 요구사항을 담당 모듈과 상세 설계 유닛에 연결한다.'))
body.insert(list(body).index(intro), old)
parts['word/document.xml'] = E.tostring(root, encoding='UTF-8', xml_declaration=True, standalone=True)
# InsertXML가 사용하지 않는 표 스타일에도 kern=0 등을 덧붙일 수 있어,
# 기존 스타일 정의를 패키지 원본 그대로 유지한다. 새 문단은 모두 기존 ID를 쓴다.
parts['word/styles.xml'] = original_styles
temp = QA / 'candidate-preserved.docx'
with zipfile.ZipFile(temp, 'w', zipfile.ZIP_DEFLATED) as z:
    for name, data in parts.items(): z.writestr(name, data)
temp.replace(QA / 'candidate.docx')
print('original trace table retained as 83 tracked-deleted rows')
