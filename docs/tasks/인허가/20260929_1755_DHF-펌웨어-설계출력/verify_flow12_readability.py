"""Check the accepted display, retained redlines, author copy and layout evidence."""
import json
import re
import zipfile
from hashlib import sha256
from pathlib import Path
from lxml import etree as E
import fitz
from PIL import Image, ImageDraw
T = Path(__file__).resolve().parent
LIVE = T.parents[2] / '참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어/SDDD-FW-TD24A 소프트웨어 상세 설계서.docx'
QA = T / 'qa/design-readability/flow12-and-module-explanation'
N = {'w':'http://schemas.openxmlformats.org/wordprocessingml/2006/main'}
def q(s): return '{' + N['w'] + '}' + s
def text(p): return ''.join(p.xpath('.//w:t[not(ancestor::w:del)]/text()', namespaces=N))
def packed(path):
    with zipfile.ZipFile(path) as z: return {n:z.read(n) for n in z.namelist()}

report = json.loads((QA/'patch.json').read_text(encoding='utf-8'))
parts = packed(QA/'candidate.docx'); oldparts = packed(QA/'before.docx')
root = E.fromstring(parts['word/document.xml'])
before = E.fromstring(oldparts['word/document.xml'])
def xml(path): return E.fromstring(packed(path)['word/document.xml'])
a = xml(QA/'candidate-accepted.docx'); b = xml(QA/'before-accepted.docx')
def paragraphs(r):
    return [text(p) for p in r.xpath('./w:body//w:p[not(ancestor::w:sdt)]',namespaces=N) if text(p)]
back = {p['after']: p['before'] for p in report['wording']}
newparas = [back.get(s,s) for s in paragraphs(a) if s not in report['added_paragraphs']]
assert newparas == paragraphs(b), 'Unexpected accepted text change.'
bh = json.loads((QA/'before-headings.json').read_text(encoding='utf-8-sig'))
ah = json.loads((QA/'candidate-headings.json').read_text(encoding='utf-8-sig'))
assert [(x['number'],x['text'],x['style']) for x in bh] == [(x['number'],x['text'],x['style']) for x in ah]
assert len(ah) == 162
units = [p for p in a.findall('w:body/w:p',N) if text(p).startswith('SU-ID-')]
assert len(units) == 115
for p in units:
    previous = p.getprevious()
    assert previous.tag == q('p') and not text(previous), text(p)
    assert previous.find('w:pPr/w:pStyle',N).get(q('val')) == 'TDC40', text(p)
for f in report['flows']:
    assert f['after'][0] == 4320000
    assert abs(f['before'][1]/f['before'][0] - f['after'][1]/f['after'][0]) < 0.000001
assert all(parts[k] == v for k,v in oldparts.items() if k not in {'word/document.xml','word/settings.xml'})
assert root.xpath('//w:del',namespaces=N) and root.xpath('//w:ins',namespaces=N)
assert len(root.xpath('//w:del',namespaces=N)) == len(before.xpath('//w:del',namespaces=N))
assert len(root.xpath('//w:ins',namespaces=N)) > len(before.xpath('//w:ins',namespaces=N))
assert [E.tostring(e) for e in before.xpath('//m:oMath',namespaces={'m':'http://schemas.openxmlformats.org/officeDocument/2006/math'})] == [E.tostring(e) for e in root.xpath('//m:oMath',namespaces={'m':'http://schemas.openxmlformats.org/officeDocument/2006/math'})]
assert paragraphs(xml(QA/'candidate-rejected.docx')) == paragraphs(xml(QA/'before-rejected.docx'))
assert sha256(LIVE.read_bytes()).hexdigest() == report['baseline']
review = LIVE.with_name('[작성자 전용] ' + LIVE.name)
assert sha256(review.read_bytes()).hexdigest() == report['review_copy']

pdf = fitz.open(QA/'candidate.pdf')
pgdir = QA/'pages'; pgdir.mkdir(exist_ok=True)
boarddir = QA/'boards'; boarddir.mkdir(exist_ok=True)
image_pages = []; flow_pages = []; short_pages = []
for i,page in enumerate(pdf):
    path = pgdir/f'page-{i+1:03}.png'
    pix = page.get_pixmap(matrix=fitz.Matrix(96/72,96/72), alpha=False); pix.save(str(path))
    s = page.get_text()
    if len(s.strip()) < 100: short_pages.append(i+1)
    if page.get_image_info(): image_pages.append(i+1)
    if re.search(r'그림\s+\d+\.\s+SU-ID-',s): flow_pages.append(i+1)
for start in range(0,len(pdf),4):
    images = [Image.open(pgdir/f'page-{i+1:03}.png').convert('RGB') for i in range(start,min(start+4,len(pdf)))]
    width=max(im.width for im in images);height=max(im.height for im in images)
    board=Image.new('RGB',(width*2,height*2+48),'#dddddd');draw=ImageDraw.Draw(board)
    for j,im in enumerate(images):
        x=j%2*width;y=j//2*(height+24)
        draw.text((x+6,y+4),f'Page {start+j+1}',fill='black');board.paste(im,(x,y+24))
    board.save(boarddir/f'board-{start//4+1:02}.png')
checks={'headings':len(ah),'units':len(units),'flows':len(report['flows']),
        'new_unit_gaps':report['new_unit_gaps'],'added_paragraphs':len(report['added_paragraphs']),
        'wording_edits':len(report['wording']),'body_tables':len(a.findall('w:body/w:tbl',N))-3,
        'other_package_parts_unchanged':True,'original_rejection_text_unchanged':True,
        'author_copy_unchanged':True,'numbering_unchanged':True,'pages':len(pdf),
        'image_pages':image_pages,'flow_pages':flow_pages,'short_pages':short_pages,
        'candidate_sha256':sha256((QA/'candidate.docx').read_bytes()).hexdigest()}
(QA/'checks.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(checks,ensure_ascii=False))
