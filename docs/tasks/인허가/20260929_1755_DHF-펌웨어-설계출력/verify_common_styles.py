"""Content, numbering, portable styles, pagination and revision preservation checks."""
from pathlib import Path
from copy import deepcopy
from collections import Counter
from lxml import etree as E
import json, zipfile, hashlib, re
import pypdfium2 as pdfium
from PIL import Image, ImageDraw

T=Path(__file__).resolve().parent
QA=T/'qa/design-readability/common-style'
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main'
N={'w':W,'m':'http://schemas.openxmlformats.org/officeDocument/2006/math',
   'wp':'http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing'}
q=lambda x:'{'+W+'}'+x
def parts(p):
    with zipfile.ZipFile(p) as z:return {n:z.read(n) for n in z.namelist()}
def root(p,name='word/document.xml'):return E.fromstring(parts(p)[name])
def texts(r):return [''.join(p.xpath('.//w:t/text()|.//m:t/text()',namespaces=N)) for p in r.xpath('./w:body/*[not(self::w:sdt)]',namespaces=N)]
before=root(QA/'before.docx');candidate=root(QA/'candidate.docx')
ba=root(QA/'before-accepted.docx');ca=root(QA/'candidate-accepted.docx')
assert texts(ba)==texts(ca),'Accepted main text/paragraph topology changed.'
for path in ['.//w:t[not(ancestor::w:sdt)]/text()','.//w:delText/text()','.//w:instrText/text()','.//m:t/text()']:
    assert before.xpath(path,namespaces=N)==candidate.xpath(path,namespaces=N),path
for tag in ['ins','del']:
    old=before.findall('.//'+q(tag));new=candidate.findall('.//'+q(tag))
    assert len(old)==len(new),(tag,len(old),len(new))
    assert [(x.get(q('id')),x.get(q('author')),x.get(q('date'))) for x in old]==[(x.get(q('id')),x.get(q('author')),x.get(q('date'))) for x in new]
for tag in ['pPrChange','rPrChange']:
    new={x.get(q('id')):x for x in candidate.findall('.//'+q(tag))}
    for x in before.findall('.//'+q(tag)):
        assert E.tostring(x,method='c14n')==E.tostring(new[x.get(q('id'))],method='c14n'),('Historical formatting snapshot changed',tag,x.get(q('id')))
bheads=json.loads((QA/'before-headings.json').read_text(encoding='utf-8-sig'))
cheads=json.loads((QA/'candidate-headings.json').read_text(encoding='utf-8-sig'))
assert [(x['text'],x['number']) for x in bheads]==[(x['text'],x['number']) for x in cheads]
assert len(cheads)==162
unit=[x for x in cheads if x['text'].startswith('SU-ID-')]
assert len(unit)==115,len(unit)
assert len(ca.xpath('.//wp:docPr',namespaces=N))==len(ba.xpath('.//wp:docPr',namespaces=N))
styles=root(QA/'candidate-accepted.docx','word/styles.xml')
lookup={s.get(q('styleId')):s.find(q('name')).get(q('val')) for s in styles.findall(q('style'))}
names=[n for n in lookup.values() if n.startswith('TDC ')]
assert len(names)==21
for p in ca.xpath('./w:body/w:p',namespaces=N):
    st=p.find('./'+q('pPr')+'/'+q('pStyle'))
    name=lookup.get(st.get(q('val')) if st is not None else '','')
    if name.startswith('TDC '):
        for sz in p.xpath('./w:r/w:rPr/w:sz/@w:val',namespaces=N):assert int(sz)>=18,(name,sz)
        assert not p.xpath('./w:r/w:rPr/w:vanish[not(@w:val="0")]',namespaces=N)
        if not name.startswith('TDC 제목'):
            outline=p.find('./'+q('pPr')+'/'+q('outlineLvl'))
            assert outline is None or outline.get(q('val'))=='9'
oldparts=parts(QA/'before.docx');newparts=parts(QA/'candidate.docx')
for name,data in oldparts.items():
    if name not in ['word/document.xml','word/styles.xml','word/numbering.xml','word/settings.xml']:assert newparts[name]==data,name
# Numeric meaning, start and overrides are identical. Only style pointers/positions differ.
def number_semantics(p):
    r=root(p,'word/numbering.xml')
    for x in list(r.iter()):
        if x.tag in {q(t) for t in ['pPr','rPr','pStyle']} and x.getparent() is not None:x.getparent().remove(x)
    return E.tostring(r,method='c14n')
assert number_semantics(QA/'before.docx')==number_semantics(QA/'candidate.docx')
assert (QA/'candidate-rejected.docx').exists() and (QA/'before-rejected.docx').exists(), 'Native reject-all QA snapshots required.'
assert texts(root(QA/'before-rejected.docx'))==texts(root(QA/'candidate-rejected.docx')),'Reject-all source text changed.'
pdf=pdfium.PdfDocument(QA/'candidate.pdf')
pages=QA/'pages';pages.mkdir(exist_ok=True)
contacts=QA/'review';contacts.mkdir(exist_ok=True)
page_text=[]
for i in range(len(pdf)):
    pg=pdf[i]
    im=pg.render(scale=96/72).to_pil().convert('RGB')
    im.save(pages/f'page-{i+1}.png')
    tx=pg.get_textpage().get_text_range();page_text.append(tx)
for offset in range(0,len(pdf),4):
    ims=[Image.open(pages/f'page-{i+1}.png').convert('RGB') for i in range(offset,min(offset+4,len(pdf)))]
    w=max(im.width for im in ims);h=max(im.height for im in ims)
    canvas=Image.new('RGB',(w*2+30,h*2+70),'#bcbcbc');draw=ImageDraw.Draw(canvas)
    for k,im in enumerate(ims):
        x=10+(k%2)*(w+10);y=25+(k//2)*(h+30)
        canvas.paste(im,(x,y));draw.text((x,y-17),str(offset+k+1),fill='black')
    canvas.save(contacts/f'pages-{offset+1:03}-{min(offset+4,len(pdf)):03}.png')
(QA/'page-text.json').write_text(json.dumps(page_text,ensure_ascii=False),encoding='utf8')
result={'content_same':True,'old_revisions_preserved':True,'heading_numbers_same':True,
        'reject_all_source_text_same':True,'headers_footers_media_unchanged':True,
        'numbering_semantics_same':True,
        'portable_styles':len(names),'headings':len(cheads),'units':len(unit),'pages':len(pdf),
        'figures':len(ca.xpath('.//wp:docPr',namespaces=N)),
        'before_sha256':hashlib.sha256((QA/'before.docx').read_bytes()).hexdigest(),
        'candidate_sha256':hashlib.sha256((QA/'candidate.docx').read_bytes()).hexdigest()}
(QA/'checks.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps(result,ensure_ascii=False))
