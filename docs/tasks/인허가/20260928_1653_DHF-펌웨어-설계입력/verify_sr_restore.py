from pathlib import Path
from zipfile import ZipFile
from lxml import etree as E
import json,hashlib,shutil
import pypdfium2 as pdfium
TASK=Path(__file__).parent
QA=TASK/'qa-20260929'
NS={'w':'http://schemas.openxmlformats.org/wordprocessingml/2006/main'}
def text(p):return ''.join(p.xpath('.//w:t[not(ancestor::w:del)]/text()',namespaces=NS)).strip()
def load(p):
    with ZipFile(p) as z:return E.fromstring(z.read('word/document.xml'))
old=load(QA/'before.docx');new=load(QA/'updated.docx')
def split(root):
    lines=[text(p) for p in root.xpath('//w:p',namespaces=NS) if text(p)]
    at=lines.index('소프트웨어 요구사항 검증');return lines[:at],lines[at:]
pre,tail=split(new)
assert pre==split(old)[0], 'Unrelated body changed'
source=Path('G:/공유 드라이브/20_프로젝트별/01_project sound1/04_docs/99_참고용DHF/02_설계 입력/SW/펌웨어/SR-FW-TD20B 소프트웨어 요구사항.docx')
assert tail==split(load(source))[1], 'Restoration differs from original'
assert '검토 결과 :' not in tail
with ZipFile(QA/'updated.docx') as z:assert b'trackRevisions' in z.read('word/settings.xml')
for name in ('accepted','markup'):
    with pdfium.PdfDocument(QA/(name+'.pdf')) as doc:
        out=QA/name;out.mkdir(exist_ok=True)
        for i,page in enumerate(doc):page.render(scale=1.4).to_pil().save(out/f'{i+1:02}.png')
        print(name,'pages',len(doc))
print('Original sections 5/6 restored exactly; preceding accepted body unchanged; tracking retained.')
(QA/'verification.json').write_text(json.dumps({'original_sections_restored':True,'other_body_unchanged':True,'track_changes_enabled':True,'basis':'2026-09-29 사용자 실제 검증/위험분석 재평가 완료 확인'},ensure_ascii=False,indent=2),encoding='utf-8')
