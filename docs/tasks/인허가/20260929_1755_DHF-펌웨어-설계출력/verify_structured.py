"""Check unit semantics, numbering, rejection, stored style levels and visual QA pages."""
from pathlib import Path
import re,json,zipfile,hashlib
from lxml import etree as E
import pypdfium2 as pdfium
from PIL import Image,ImageDraw
T=Path(__file__).resolve().parent;ROOT=T.parents[3];Q=T/'qa/design-readability/structured-units';OLD=T/'qa/design-readability/common-style'
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main';q=lambda t:'{'+W+'}'+t
N={'w':W,'m':'http://schemas.openxmlformats.org/officeDocument/2006/math','wp':'http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing','a':'http://schemas.openxmlformats.org/drawingml/2006/main'}
def parts(p):
    with zipfile.ZipFile(p) as z:return {n:z.read(n) for n in z.namelist()}
def root(p,name='word/document.xml'):return E.fromstring(parts(p)[name])
def text(r):return ''.join(r.xpath('.//w:t/text()|.//m:t/text()',namespaces=N))
def normalize(s):return re.sub(r'\s+','',s)
def mainparagraphs(r):return [text(p) for p in r.xpath('./w:body/w:p',namespaces=N) if text(p)]
ca=root(Q/'candidate-accepted.docx');raw=root(Q/'candidate.docx');ba=root(OLD/'candidate-accepted.docx')
bheads=json.loads((OLD/'candidate-headings.json').read_text(encoding='utf-8-sig'));heads=json.loads((Q/'candidate-headings.json').read_text(encoding='utf-8-sig'))
assert [(x['text'],x['number']) for x in heads]==[(x['text'],x['number']) for x in bheads]
assert len(heads)==162 and sum(x['text'].startswith('SU-ID-') for x in heads)==115
reject=root(Q/'candidate-rejected.docx');oldreject=root(OLD/'candidate-rejected.docx')
assert mainparagraphs(reject)==mainparagraphs(oldreject),'Reject-all paragraphs differ'
assert reject.xpath('.//w:tbl//w:t/text()',namespaces=N)==oldreject.xpath('.//w:tbl//w:t/text()',namespaces=N),'Reject-all table data differ'
# Unit values survive relocation to tables. Related functions may replace
# erroneous path-only interfaces; design values and calculation examples do not change.
units=json.loads((Q/'fragments.json').read_text(encoding='utf8'))
before=json.loads((Q/'units-before.json').read_text(encoding='utf8'))
body=ca.find(q('body'));blocks=list(body);unittexts={};cur=None
for b in blocks:
    tx=text(b);m=re.match(r'SU-ID-(CFX|CM3|HEAR|BOOT)-(\d+),',tx)
    if m:cur=m.group(1)+'-'+m.group(2);unittexts[cur]=''
    elif cur:unittexts[cur]+=tx
assert len(unittexts)==115
for u in units:
    ut=normalize(unittexts[u['unit']])
    for k,v in u['fields'].items():assert normalize(v) in ut,(u['unit'],k,v)
for u in before:
    for f in u['fields']:
        k,v=f.split(' :',1)
        if k.startswith('구현 위치') or k.startswith('인터페이스'):continue
        assert normalize(v) in normalize(unittexts[u['id']]),('Original field lost',u['id'],k)
assert not any('구현 위치 :' in x for x in unittexts.values())
assert not any('src/' in x for x in unittexts.values())
assert normalize('입력 혼합과 빔포밍') in normalize(text(ca)) and normalize('AGC 동작 원리') in normalize(text(ca))
assert ca.xpath('.//m:t/text()',namespaces=N)==ba.xpath('.//m:t/text()',namespaces=N),'Math changed'
assert len(ca.xpath('.//wp:docPr',namespaces=N))==41
assert len(ca.xpath('./w:body/w:tbl',namespaces=N))==221
sty=root(Q/'candidate-accepted.docx','word/styles.xml');names={s.get(q('styleId')):s.find(q('name')).get(q('val')) for s in sty.findall(q('style'))}
tdc=[s for s in sty.findall(q('style')) if s.find(q('name')).get(q('val')).startswith('TDC ')]
assert len(tdc)==59
byname={s.find(q('name')).get(q('val')):s for s in tdc}
for level in range(1,7):
    for family in ['본문','표 캡션','주석','그림','그림 캡션','수식']:
        s=byname[f'TDC {family} {level}'];ind=s.find('./'+q('pPr')+'/'+q('ind'));left=int(ind.get(q('left'),'0')) if ind is not None else 0;assert abs(left-round((level-1)*.25*1440/2.54))<=1,(family,level,left)
    h=byname['TDC 제목 '+str(level)];sp=h.find('./'+q('pPr')+'/'+q('spacing'));assert (sp.get(q('before')),sp.get(q('after')),sp.get(q('line')))==('160','80','276')
caps=[];images=[];started=False;depth=1
for b in blocks:
    if b.tag==q('p'):
        st=b.find('./'+q('pPr')+'/'+q('pStyle'));name=names.get(st.get(q('val')) if st is not None else '','');tx=text(b)
        if tx=='서론':started=True
        if not started:continue
        if re.fullmatch('TDC 제목 [1-6]',name):depth=int(name[-1])
        elif name.startswith('TDC 본문 '):assert int(name[-1])==depth or tx=='',('body level',name,depth,tx[:50])
        elif name.startswith('TDC 표 캡션 '):
            assert int(name[-1])==depth,('caption level',name,depth,tx[:80]);caps.append(tx)
        elif name.startswith('TDC 주석 '):assert int(name[-1])==depth,('note level',name,depth,tx[:60])
        if name.startswith('TDC '):
            assert not b.xpath('.//w:rPr/w:sz[number(@w:val)<18]',namespaces=N),('tiny active text',tx[:50])
            assert not b.xpath('.//w:vanish[not(@w:val="0")]',namespaces=N)
        for inline in b.xpath('.//wp:inline',namespaces=N):
            e=inline.find('{'+N['wp']+'}extent');width=int(e.get('cx'))/360000;assert width<=16.6-(depth-1)*.25+.03,(width,depth,tx)
            images.append({'width_cm':width,'height_cm':int(e.get('cy'))/360000,'level':depth})
    elif started and b.tag==q('tbl'):
        tp=b.find(q('tblPr'));st=tp.find(q('tblStyle'));name=names.get(st.get(q('val')) if st is not None else '','')
        assert name=='TDC 표 '+str(depth),('table style depth',name,depth)
        ind=tp.find(q('tblInd'))
        if ind is None:ind=byname[name].find('./'+q('tblPr')+'/'+q('tblInd'))
        assert ind is not None and abs(int(ind.get(q('w')))-(round((depth-1)*.25*1440/2.54)+100))<=2,('table indent',name,E.tostring(ind),text(b)[:80])
        width=sum(int(x.get(q('w'))) for x in b.findall('./'+q('tblGrid')+'/'+q('gridCol')))
        assert width<=round((16.6-(depth-1)*.25)*1440/2.54)+5,(depth,width)
assert len(caps)==218
assert [int(re.match(r'표 (\d+)',s).group(1)) for s in caps]==list(range(1,219))
# Native save can coalesce prior same-author insertions. Genuine tracking and
# original reject-all restoration, not raw marker count, establish preservation.
assert raw.xpath('.//w:trPr/w:ins',namespaces=N)
settings=root(Q/'candidate.docx','word/settings.xml');assert settings.find(q('trackRevisions')) is not None
assert settings.find(q('updateStyles')) is None
oldparts=parts(Q/'before.docx');newparts=parts(Q/'candidate.docx')
# Native Word may normalize XML and refresh PAGE/NUMPAGES caches. Compare
# protected layout and visible header/footer text independently of those caches.
assert [text(x) for x in ca.xpath('./w:body/w:tbl',namespaces=N)[:3]]==[text(x) for x in ba.xpath('./w:body/w:tbl',namespaces=N)[:3]],'Cover/revision tables changed'
geometry_tags=['pgSz','pgMar','type','titlePg','cols','pgNumType','docGrid']
def geometry(r):
    return [[(tag,tuple(sorted(e.attrib.items()))) for tag in geometry_tags for e in sec.findall(q(tag))] for sec in r.xpath('.//w:sectPr[not(ancestor::w:pPrChange)]',namespaces=N)]
assert geometry(ca)==geometry(ba),'Section geometry changed'
def protected_story(data):
    story=E.fromstring(data);tokens=[];field_depth=0
    for e in story.iter():
        if any(a.tag in {q('del'),q('pPrChange'),q('rPrChange')} for a in e.iterancestors()):continue
        if e.tag==q('fldChar'):
            kind=e.get(q('fldCharType'))
            if kind=='begin':field_depth+=1
            elif kind=='end':field_depth-=1
        elif e.tag==q('instrText'):tokens.append(('field',normalize(e.text or '')))
        elif e.tag==q('fldSimple'):tokens.append(('field',normalize(e.get(q('instr'),''))))
        elif e.tag==q('t') and field_depth==0 and not any(a.tag==q('fldSimple') for a in e.iterancestors()):tokens.append(('text',normalize(e.text or '')))
    return tokens
for family in ['header','footer']:
    oldstories=sorted(protected_story(v) for k,v in oldparts.items() if re.fullmatch('word/'+family+r'\d+\.xml',k))
    newstories=sorted(protected_story(v) for k,v in newparts.items() if re.fullmatch('word/'+family+r'\d+\.xml',k))
    assert oldstories==newstories,family+' content/fields changed'
for n,data in oldparts.items():
    if n.startswith('word/media/'):
        assert data in [v for k,v in newparts.items() if k.startswith('word/media/')],('Image data changed',n)
# Unrelated trial documents remain at the last approved baseline.
folder=ROOT/'docs/참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어'
hashes={'SUVC':'498f7a4614b75652428d5e27b64f259c975f9856a147776842c199401c5e785b','SITC':'e39d54d1f89b3307cb64e67f9f190cfd6ca9d65db7fea5bd104b30a5aabc05a7','SSTC':'99a92516b1d3d48025a9b038e9b777fca2f3194a2fbc43812bd54df988754c85'}
for prefix,h in hashes.items():
    file=next(folder.glob(prefix+'*.xlsx'));assert hashlib.sha256(file.read_bytes()).hexdigest()==h
source_hashes=json.loads((T/'qa/unit-reconciliation/model-checks.json').read_text(encoding='utf8'))['source_hashes']
for name,h in source_hashes.items():assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==h,('Source changed',name)
pdf=pdfium.PdfDocument(str(Q/'candidate.pdf'));pages=Q/'pages';review=Q/'review';pages.mkdir(exist_ok=True);review.mkdir(exist_ok=True)
page_text=[]
for i in range(len(pdf)):
    page=pdf[i];page.render(scale=96/72).to_pil().convert('RGB').save(pages/f'page-{i+1}.png')
    page_text.append(page.get_textpage().get_text_range())
for start in range(0,len(pdf),4):
    ims=[Image.open(pages/f'page-{i+1}.png').convert('RGB') for i in range(start,min(start+4,len(pdf)))];w=ims[0].width;h=ims[0].height
    im=Image.new('RGB',(w*2+30,h*2+70),'#bcbcbc');draw=ImageDraw.Draw(im)
    for k,img in enumerate(ims):
        x=10+(k%2)*(w+10);y=25+(k//2)*(h+30);im.paste(img,(x,y));draw.text((x,y-17),str(start+k+1),fill='black')
    im.save(review/f'pages-{start+1:03}-{min(start+4,len(pdf)):03}.png')
normalized=[normalize(x) for x in page_text]
figurepages=[]
for p in blocks:
    if p.tag!=q('p'):continue
    tx=text(p)
    if tx.startswith('그림 ') and '순서도' in tx:
        found=[i+1 for i,s in enumerate(normalized) if normalize(tx) in s];assert len(found)==1,(tx,found);figurepages.append({'caption':tx,'page':found[0]})
assert len(figurepages)==34
(Q/'page-text.json').write_text(json.dumps(page_text,ensure_ascii=False),encoding='utf8')
(Q/'flow-pages.json').write_text(json.dumps(figurepages,ensure_ascii=False,indent=2),encoding='utf8')
result={'units':115,'headings':162,'prose_units':9,'new_unit_tables':209,'body_table_captions':218,'figures':41,'flowcharts':34,'styles':59,'pages':len(pdf),'unit_values_preserved':True,'numbering_preserved':True,'cover_header_footer_geometry_preserved':True,'original_reject_all_restored':True,'unrelated_test_files_unchanged':True,'source_files_unchanged':len(source_hashes),'images':images,'baseline_sha256':hashlib.sha256((Q/'before.docx').read_bytes()).hexdigest(),'candidate_sha256':hashlib.sha256((Q/'candidate.docx').read_bytes()).hexdigest()}
(Q/'checks.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps({k:v for k,v in result.items() if k!='images'},ensure_ascii=False))
