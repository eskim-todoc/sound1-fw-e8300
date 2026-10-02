"""Remove confirmed page-leading separators, or verify/rasterize the final QA."""
from copy import deepcopy
from hashlib import sha256
from pathlib import Path
import argparse
import json
import re
import zipfile
from lxml import etree as E

T=Path(__file__).resolve().parent
Q=T/'qa/design-readability/design-intent-and-placeholders'
N={'w':'http://schemas.openxmlformats.org/wordprocessingml/2006/main',
   'wp':'http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing',
   'a':'http://schemas.openxmlformats.org/drawingml/2006/main',
   'r':'http://schemas.openxmlformats.org/officeDocument/2006/relationships'}
def q(t):return '{'+N['w']+'}'+t
def txt(e):return ''.join(e.xpath('.//w:t[not(ancestor::w:del)]/text()',namespaces=N))
def pack(p):
    with zipfile.ZipFile(p) as z:return {n:z.read(n) for n in z.namelist()}
def root(p):return E.fromstring(pack(p)['word/document.xml'])
def paras(r):return [txt(p) for p in r.xpath('./w:body//w:p[not(ancestor::w:sdt)]',namespaces=N) if txt(p)]
def save(parts):
    with zipfile.ZipFile(Q/'candidate.docx','w',zipfile.ZIP_DEFLATED) as z:
        for k,v in parts.items():z.writestr(k,v)

def clean(reset=False):
    layout=json.loads((Q/'unit-gap-layout.json').read_text(encoding='utf-8-sig'))
    # A leading blank on the same page or an unnecessary separator before an
    # explicit heading page break is an authoring gap, not an image paragraph.
    top=min(x['top'] for x in layout if x['top']>=0)
    selected=[x['unit'] for x in layout if x['prevText']=='' and x['prevStyle'].startswith('TDC 본문') and
              ((x['prevPage']==x['page'] and x['prevTop']<=top+0.5) or x['pageBreakBefore']!=0)]
    if reset and (Q/'page-gap-changes.json').exists():
        # Rebuilding from the unchanged baseline also restores gaps already
        # confirmed by native Word. Reapply only those exact completed IDs.
        selected=list(dict.fromkeys(selected+json.loads((Q/'page-gap-changes.json').read_text(encoding='utf8'))))
    parts=pack(Q/'candidate.docx');r=E.fromstring(parts['word/document.xml']);body=r.find(q('body'))
    changes=[]
    # Native accepted headings may merge old pending paragraph fragments.
    for e in list(body):
        if e.tag!=q('p') or not txt(e).startswith('SU-ID-'):continue
        # Rejected paragraph marks join fragments such as "SU-ID-CM3-" in
        # Word's final view. Match the completed heading, never just its prefix.
        full='';c=e
        while c is not None:
            full+=txt(c)
            if not c.xpath('./w:pPr/w:rPr/w:del',namespaces=N):break
            c=c.getnext()
        if full not in selected:continue
        b=e.getprevious()
        while b is not None and b.xpath('./w:pPr/w:rPr/w:del',namespaces=N):b=b.getprevious()
        if b is None or b.tag!=q('p') or txt(b) or b.xpath('.//w:drawing|.//w:fldChar|.//w:delText|.//w:br',namespaces=N):continue
        assert b.xpath('./w:pPr/w:rPr/w:ins',namespaces=N), 'Not an authored inserted gap.'
        body.remove(b);changes.append(full)
    parts['word/document.xml']=E.tostring(r,encoding='UTF-8',xml_declaration=True,standalone=True)
    save(parts)
    prior=[]
    if not reset and (Q/'page-gap-changes.json').exists():prior=json.loads((Q/'page-gap-changes.json').read_text(encoding='utf8'))
    (Q/'page-gap-changes.json').write_text(json.dumps(prior+changes,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'selected':len(selected),'removed':len(changes)},ensure_ascii=True))

def verify():
    from PIL import Image,ImageDraw
    import fitz
    report=json.loads((Q/'patch.json').read_text(encoding='utf8'))
    old=pack(Q/'before.docx');new=pack(Q/'candidate.docx');r=E.fromstring(new['word/document.xml'])
    a=root(Q/'candidate-accepted.docx');b=root(Q/'before-accepted.docx')
    inverse={v['after']:v['before'] for v in report['changes'] if 'before' in v}
    added={v['after'] for v in report['changes'] if 'before' not in v}
    assert [inverse.get(t,t) for t in paras(a) if t not in added]==paras(b),'Unexpected body change'
    assert paras(root(Q/'candidate-rejected.docx'))==paras(root(Q/'before-rejected.docx')),'Old source recovery changed'
    beforeheads=json.loads((Q/'before-headings.json').read_text(encoding='utf-8-sig'))
    heads=json.loads((Q/'candidate-headings.json').read_text(encoding='utf-8-sig'))
    assert [(x['number'],x['style'],x['text']) for x in heads]==[(x['number'],x['style'],x['text']) for x in beforeheads]
    assert len(heads)==162
    units=[p for p in a.findall('w:body/w:p',N) if txt(p).startswith('SU-ID-')];assert len(units)==115
    texts='\n'.join(paras(a))
    assert not re.search(r'isd_ver|현재 빌드|현재 구현|현재 활성화된|구형|기상|완결성|복호화 문맥|자동 안전 정지로 표현|임의 확정',texts)
    assert '·' not in texts
    red=[p for p in r.findall('w:body/w:p',N) if txt(p).startswith('[자료 추가 위치]')]
    assert len(red)==4
    for p in red:
        assert all(t.getparent().find('w:rPr/w:color',N).get(q('val'))=='C00000' for t in p.xpath('.//w:t[not(ancestor::w:del)]',namespaces=N) if t.text)
    assert len(a.findall('w:body/w:tbl',N))-3==236
    for u in units:
        nexts=[];e=u.getnext()
        while e is not None and not (e.tag==q('p') and txt(e).startswith('SU-ID-')):
            nexts.append(txt(e));e=e.getnext()
        assert any('/ 유닛 설계 내용' in x for x in nexts),txt(u)
    allowed={'word/document.xml','word/settings.xml'}|{i['part'] for i in report['images']}
    assert set(new)==set(old)
    assert all(new[k]==v for k,v in old.items() if k not in allowed),'Unrelated package part changed'
    assert E.fromstring(new['word/settings.xml']).find(q('trackRevisions')) is not None
    assert r.xpath('//w:ins',namespaces=N) and r.xpath('//w:del',namespaces=N)
    m={'m':'http://schemas.openxmlformats.org/officeDocument/2006/math'}
    assert [E.tostring(x) for x in E.fromstring(old['word/document.xml']).xpath('//m:oMath',namespaces=m)]==[E.tostring(x) for x in r.xpath('//m:oMath',namespaces=m)]
    rels={e.get('Id'):e.get('Target') for e in E.fromstring(new['word/_rels/document.xml.rels'])}
    flows=[]
    body=r.find(q('body'))
    for p in body.findall(q('p')):
        if not re.match(r'그림\s+\d+\.\s+SU-ID-',txt(p)):continue
        d=p.getprevious();extent=d.find('.//wp:extent',N)
        assert int(extent.get('cx'))==4320000
        rid=d.xpath('.//a:blip/@r:embed',namespaces=N)[0]
        from io import BytesIO
        im=Image.open(BytesIO(new['word/'+rels[rid]]))
        assert abs(int(extent.get('cy'))/int(extent.get('cx'))-im.height/im.width)<0.0001,txt(p)
        flows.append(txt(p))
    assert len(flows)==34
    pdf=fitz.open(Q/'candidate.pdf');pages=Q/'pages';boards=Q/'boards';pages.mkdir(exist_ok=True);boards.mkdir(exist_ok=True)
    short=[];redpages=[];outbounds=[]
    for i,p in enumerate(pdf):
        p.get_pixmap(matrix=fitz.Matrix(96/72,96/72),alpha=False).save(str(pages/f'page-{i+1:03}.png'))
        s=p.get_text()
        if len(s.strip())<100:short.append(i+1)
        if '[자료 추가 위치]' in s:redpages.append(i+1)
        for block in p.get_text('dict')['blocks']:
            if block['bbox'][2]>p.rect.width+1 or block['bbox'][3]>p.rect.height+1:outbounds.append(i+1)
    for start in range(0,len(pdf),4):
        imgs=[Image.open(pages/f'page-{i+1:03}.png').convert('RGB') for i in range(start,min(start+4,len(pdf)))]
        w=max(im.width for im in imgs);h=max(im.height for im in imgs)
        board=Image.new('RGB',(w*2,h*2+48),'#ddd');draw=ImageDraw.Draw(board)
        for j,im in enumerate(imgs):
            x=j%2*w;y=j//2*(h+24);draw.text((x+6,y+4),f'Page {start+j+1}',fill='black');board.paste(im,(x,y+24))
        board.save(boards/f'board-{start//4+1:02}.png')
    assert not short and not outbounds,(short,outbounds)
    checks={'headings':len(heads),'units':len(units),'body_tables':236,'flows':34,'red_markers':4,'pages':len(pdf),
            'red_pages':redpages,'short_pages':short,'out_of_page_blocks':outbounds,
            'numbering_unchanged':True,'source_rejection_unchanged':True,'formulas_unchanged':True,
            'candidate_sha256':sha256((Q/'candidate.docx').read_bytes()).hexdigest(),
            'unrelated_parts_unchanged':True}
    (Q/'checks.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(checks,ensure_ascii=True))

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('action',choices=['clean','verify']);parser.add_argument('--reset',action='store_true');arg=parser.parse_args()
    clean(arg.reset) if arg.action=='clean' else verify()
