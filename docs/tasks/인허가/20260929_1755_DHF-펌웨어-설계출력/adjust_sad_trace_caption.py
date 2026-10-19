"""추가한 표 제목이 표와 다른 페이지에 남지 않도록 묶는다."""
import zipfile
from lxml import etree as E
from inspect_sad_unit_trace import QA, N, q, text

with zipfile.ZipFile(QA / 'candidate.docx') as z:
    parts = {name: z.read(name) for name in z.namelist()}
root = E.fromstring(parts['word/document.xml'])
paragraph = next(p for p in root.findall('w:body/w:p', N) if text(p) == '표 1. 소프트웨어 요구사항과 상세 설계 유닛 연결')
paragraph.find('w:pPr/w:keepNext', N).set(q('val'), '1')
parts['word/document.xml'] = E.tostring(root, encoding='UTF-8', xml_declaration=True, standalone=True)
temp = QA / 'candidate-layout.docx'
with zipfile.ZipFile(temp, 'w', zipfile.ZIP_DEFLATED) as z:
    for name, value in parts.items(): z.writestr(name, value)
temp.replace(QA / 'candidate.docx')
