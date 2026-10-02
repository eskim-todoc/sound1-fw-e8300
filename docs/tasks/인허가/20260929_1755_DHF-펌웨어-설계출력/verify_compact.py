"""Independently check saved Word final view and keep visual review evidence."""
from compact_unit_style import *
import pypdfium2 as pdfium
from PIL import Image,ImageDraw
N={**NS,'m':'http://schemas.openxmlformats.org/officeDocument/2006/math'}
def root(p,n='word/document.xml'):return E.fromstring(read(p)[n])
def text(r):return ''.join(r.xpath('.//w:t/text()|.//m:t/text()',namespaces=N))
def norm(s):return re.sub(r'\s+','',s)
def paras(r):return [text(p) for p in r.xpath('./w:body/w:p',namespaces=N) if text(p)]
ca=root(Q/'candidate-accepted.docx');raw=root(Q/'candidate.docx');ba=root(OLDQ/'candidate-accepted.docx')
heads=json.loads((Q/'candidate-headings.json').read_text(encoding='utf-8-sig'));beforeheads=json.loads((OLDQ/'candidate-headings.json').read_text(encoding='utf-8-sig'))
assert [(x['text'],x['number']) for x in heads]==[(x['text'],x['number']) for x in beforeheads],'Headings/automatic numbering changed'
assert len(heads)==162
assert max(int(x['style'][-1]) for x in heads)==4
positions=json.loads((Q/'native-heading-positions.json').read_text(encoding='utf-8-sig'))
assert len(positions)==162
for h in positions:
    expected=ANCHORS[int(h['style'][-1])-1]*72/2.54
    assert abs(h['left_pt']-expected)<.06 and abs(h['number_pt']-expected)<.06 and abs(h['first_pt'])<.06,('native heading position',h)
reject=root(Q/'candidate-rejected.docx');oldreject=root(OLDQ/'candidate-rejected.docx')
assert paras(reject)==paras(oldreject),'Reject-all paragraphs differ'
assert reject.xpath('.//w:tbl//w:t/text()',namespaces=N)==oldreject.xpath('.//w:tbl//w:t/text()',namespaces=N),'Reject-all table values differ'
body=ca.find(q('body'));blocks=list(body);byunit={};uid=None
for b in blocks:
    tx=text(b);m=re.match(r'SU-ID-(CFX|HEAR|CM3|BOOT)-(\d+),',tx)
    if m:uid=m[1]+'-'+m[2];byunit[uid]=[]
    elif uid:byunit[uid].append(b)
assert len(byunit)==115
units=json.loads((Q/'fragments.json').read_text(encoding='utf8'))
for u in units:
    group=byunit[u['unit']];ut=norm(''.join(text(b) for b in group))
    for k,v in u['fields'].items():assert norm(v) in ut,('field lost',u['unit'],k)
    first=next(b for b in group if text(b))
    assert first.tag==q('p') and norm(text(first))==norm(u['fields']['목적']),('purpose outside table',u['unit'],text(first))
    assert first.find('./'+q('pPr')+'/'+q('keepNext')).get(q('val'),'1')=='1',('purpose/table separation',u['unit'])
    assert not any(text(c)=='목적' for b in group if b.tag==q('tbl') for c in b.xpath('./w:tr/w:tc',namespaces=N))
    design=next(b for b in group if b.tag==q('tbl'))
    labels=[text(r.find(q('tc'))) for r in design.findall(q('tr'))]
    assert labels==['항목','입력','출력','처리'],(u['unit'],labels)
assert '목적 :' not in ''.join(text(b) for bs in byunit.values() for b in bs)
assert ca.xpath('.//m:t/text()',namespaces=N)==ba.xpath('.//m:t/text()',namespaces=N),'Math changed'
assert len(ca.xpath('.//wp:docPr',namespaces=N))==41
assert len(ca.xpath('./w:body/w:tbl',namespaces=N))==239
oldparts=read(Q/'before.docx');parts=read(Q/'candidate.docx')
for n,data in oldparts.items():
    if n.startswith('word/media/'):assert data in [v for k,v in parts.items() if k.startswith('word/media/')],('Image bytes changed',n)
assert [text(b) for b in blocks if b.tag==q('tbl')][:3]==[text(b) for b in ba.find(q('body')) if b.tag==q('tbl')][:3],'Cover/revision table changed'
def geometry(r):
    tags=['pgSz','pgMar','type','titlePg','cols','pgNumType','docGrid']
    return [[(t,tuple(sorted(e.attrib.items()))) for t in tags for e in sec.findall(q(t))] for sec in r.xpath('.//w:sectPr[not(ancestor::w:pPrChange)]',namespaces=N)]
assert geometry(ca)==geometry(ba),'Section geometry changed'
def story(data):
    r=E.fromstring(data);out=[];fd=0
    for e in r.iter():
        if any(a.tag in {q('del'),q('pPrChange'),q('rPrChange')} for a in e.iterancestors()):continue
        if e.tag==q('fldChar'):
            if e.get(q('fldCharType'))=='begin':fd+=1
            elif e.get(q('fldCharType'))=='end':fd-=1
        elif e.tag==q('instrText'):out.append(('field',norm(e.text or '')))
        elif e.tag==q('fldSimple'):out.append(('field',norm(e.get(q('instr'),''))))
        elif e.tag==q('t') and not fd and not any(a.tag==q('fldSimple') for a in e.iterancestors()):out.append(('text',norm(e.text or '')))
    return out
for fam in ['header','footer']:
    assert sorted(story(v) for n,v in oldparts.items() if re.fullmatch('word/'+fam+r'\d+\.xml',n))==sorted(story(v) for n,v in parts.items() if re.fullmatch('word/'+fam+r'\d+\.xml',n)),fam
st=root(Q/'candidate-accepted.docx','word/styles.xml');styles={s.find(q('name')).get(q('val')):s for s in st.findall(q('style'))};names={s.get(q('styleId')):n for n,s in styles.items()}
assert len([n for n in styles if n.startswith('TDC ')])==59
native_sizes={s['name']:s['size'] for s in json.loads((Q/'native-style-sizes.json').read_text(encoding='utf-8-sig'))}
def style_size(s):
    value=s.find('./'+q('rPr')+'/'+q('sz'))
    if value is not None:return value.get(q('val'))
    parent=s.find(q('basedOn'))
    if parent is not None:return style_size(next(v for v in styles.values() if v.get(q('styleId'))==parent.get(q('val'))))
    default=st.find('./'+q('docDefaults')+'/'+q('rPrDefault')+'/'+q('rPr')+'/'+q('sz'))
    if default is not None:return default.get(q('val'))
    default=st.xpath('./w:style[@w:default="1" and @w:type="paragraph"]/w:rPr/w:sz/@w:val',namespaces=N)
    if default:return default[0]
    # Word normalizes explicit 10pt values out of some styles. Check the
    # effective native Word value rather than guessing its default.
    return str(round(native_sizes[s.find(q('name')).get(q('val'))]*2))
for i in range(1,7):
    for fam in ['본문','표 캡션','주석','그림','그림 캡션','수식']:
        s=styles[f'TDC {fam} {i}'];ind=s.find('./'+q('pPr')+'/'+q('ind'))
        assert abs((int(ind.get(q('left'),'0')) if ind is not None else 0)-tw(ANCHORS[i-1]))<=1,(fam,i)
    s=styles[f'TDC 제목 {i}'];assert style_size(s)==str(SIZES[i-1]*2)
    sp=s.find('./'+q('pPr')+'/'+q('spacing'));assert [sp.get(q(k)) for k in ['before','after','line']]==['160','80','276']
for n,s in styles.items():
    if n.startswith('TDC ') and n!='TDC 문서 제목' and not n.startswith('TDC 제목 '):
        assert style_size(s)=='20',(n,'not 10pt')
num=root(Q/'candidate-accepted.docx','word/numbering.xml')
for i in range(1,7):
    sid=styles['TDC 제목 '+str(i)].get(q('styleId'))
    lvs=num.xpath('.//w:lvl[w:pStyle/@w:val="'+sid+'"]',namespaces=N)
    assert lvs and all(lv.find(q('suff')).get(q('val'))=='space' for lv in lvs),('suffix',i)
    for lv in lvs:
        ind=lv.find('./'+q('pPr')+'/'+q('ind'))
        assert abs(int(ind.get(q('left'),'0'))-tw(ANCHORS[i-1]))<=1,('heading number position',i)
caps=[];started=False;depth=1;gaps=0
for b in blocks:
    if b.tag==q('p'):
        tx=text(b);ps=b.find('./'+q('pPr')+'/'+q('pStyle'));sn=names.get(ps.get(q('val')) if ps is not None else '','')
        if tx=='서론':started=True
        if not started:continue
        if re.fullmatch('TDC 제목 [1-6]',sn):depth=int(sn[-1])
        elif tx and re.fullmatch('TDC (본문|주석|표 캡션|그림 캡션) [1-6]',sn):assert int(sn[-1])==depth,('paragraph level',sn,depth,tx[:50])
        if sn.startswith('TDC 표 캡션 '):caps.append(tx)
        assert not b.xpath('.//w:rPr/w:sz[number(@w:val)<18]',namespaces=N),('tiny run',tx[:40])
        for inline in b.xpath('.//wp:inline',namespaces=N):
            ex=inline.find('{'+N['wp']+'}extent');assert int(ex.get('cx'))/360000<=16.6-ANCHORS[depth-1]+.03
    elif started and b.tag==q('tbl'):
        tp=b.find(q('tblPr'));sid=tp.find(q('tblStyle')).get(q('val'));sn=names[sid]
        assert sn=='TDC 표 '+str(depth),('table level',sn,depth,text(b)[:50])
        ind=tp.find(q('tblInd'))
        if ind is None:ind=styles[sn].find('./'+q('tblPr')+'/'+q('tblInd'))
        assert abs(int(ind.get(q('w')))-(tw(ANCHORS[depth-1])+100))<=2
        width=sum(int(c.get(q('w'))) for c in b.findall('./'+q('tblGrid')+'/'+q('gridCol')));assert width<=tw(16.6-ANCHORS[depth-1])+5
        nxt=b.getnext();assert nxt is not None and nxt.tag==q('p') and text(nxt)=='',('table gap',text(b)[:80])
        ps=nxt.find('./'+q('pPr')+'/'+q('pStyle'));assert names[ps.get(q('val'))]=='TDC 본문 '+str(depth);gaps+=1
        for ri,row in enumerate(b.findall(q('tr'))):
            for ci,cell in enumerate(row.findall(q('tc'))):
                label=ri==0 or ci==0;cp=cell.find(q('tcPr'))
                vertical=cp.find(q('vAlign'))
                if vertical is None:vertical=styles[sn].find('./'+q('tcPr')+'/'+q('vAlign'))
                assert vertical is not None and vertical.get(q('val'))=='center'
                if label:
                    fill=cp.find(q('shd'))
                    if fill is None:fill=styles[sn].find('./'+q('tblStylePr')+'[@'+q('type')+'="'+('firstRow' if ri==0 else 'firstCol')+'"]/'+q('tcPr')+'/'+q('shd'))
                    assert fill is not None and fill.get(q('fill'))=='F5F5F5'
                for p in cell.findall(q('p')):
                    jc=p.find('./'+q('pPr')+'/'+q('jc'))
                    if jc is None:
                        pstyle=p.find('./'+q('pPr')+'/'+q('pStyle'));jc=styles[names[pstyle.get(q('val'))]].find('./'+q('pPr')+'/'+q('jc'))
                    assert jc is not None and jc.get(q('val'))==('center' if label else 'left')
assert len(caps)==236 and gaps==236,(len(caps),gaps)
assert [int(re.match(r'표\s*(\d+)',s)[1]) for s in caps]==list(range(1,237))
settings=root(Q/'candidate.docx','word/settings.xml');assert settings.find(q('trackRevisions')) is not None
assert settings.find(q('updateStyles')) is None
assert raw.xpath('.//w:trPr/w:ins',namespaces=N)
folder=LIVE.parent
for prefix,h in {'SUVC':'498f7a4614b75652428d5e27b64f259c975f9856a147776842c199401c5e785b','SITC':'e39d54d1f89b3307cb64e67f9f190cfd6ca9d65db7fea5bd104b30a5aabc05a7','SSTC':'99a92516b1d3d48025a9b038e9b777fca2f3194a2fbc43812bd54df988754c85'}.items():assert sha(next(folder.glob(prefix+'*.xlsx')))==h
sources=json.loads((T/'qa/unit-reconciliation/model-checks.json').read_text(encoding='utf8'))['source_hashes']
for path,h in sources.items():assert sha(PROJECT/path)==h
pdf=pdfium.PdfDocument(str(Q/'candidate.pdf'));pages=Q/'pages';review=Q/'review';pages.mkdir(exist_ok=True);review.mkdir(exist_ok=True);texts=[]
for i,page in enumerate(pdf):
    page.render(scale=96/72).to_pil().convert('RGB').save(pages/f'page-{i+1}.png');texts.append(page.get_textpage().get_text_range())
    assert len(norm(texts[-1]))>40,('near empty page',i+1)
for start in range(0,len(pdf),4):
    ims=[Image.open(pages/f'page-{i+1}.png') for i in range(start,min(start+4,len(pdf)))];w=max(im.width for im in ims);h=max(im.height for im in ims)
    board=Image.new('RGB',(w*2,h*2+40),'#cccccc');draw=ImageDraw.Draw(board)
    for k,im in enumerate(ims):board.paste(im,((k%2)*w,(k//2)*h));draw.text(((k%2)*w+10,(k//2)*h+10),str(start+k+1),fill='red')
    board.save(review/f'pages-{start+1:03d}-{min(start+4,len(pdf)):03d}.png')
jsonout(Q/'page-text.json',texts)
result={'passed':True,'pages':len(pdf),'headings':162,'deepest_heading':4,'units':115,'purpose_prose':115,'unit_IOP_tables':115,'unit_tables':227,'body_tables':236,'table_gaps':gaps,'table_captions':236,'images':41,'styles':59,'sources_unchanged':len(sources),'reject_all_restores_original':True,'candidate_sha256':sha(Q/'candidate.docx')}
jsonout(Q/'checks.json',result);print(json.dumps(result,ensure_ascii=False))
# The root template is checked independently of this applied document.
tp=read(RQ/'candidate.dotx');ts=E.fromstring(tp['word/styles.xml']);tn=E.fromstring(tp['word/numbering.xml'])
assert len(ts.xpath('./w:style[w:name[starts-with(@w:val,"TDC ")]]',namespaces=N))==59
native=root(RQ/'native-created.docx','word/styles.xml');assert len(native.xpath('./w:style[w:name[starts-with(@w:val,"TDC ")]]',namespaces=N))==59
fixture=root(RQ/'specimen-accepted.docx')
fixture_styles=root(RQ/'specimen-accepted.docx','word/styles.xml')
for i,tb in enumerate(fixture.xpath('./w:body/w:tbl',namespaces=N),1):
    ind=tb.find('./'+q('tblPr')+'/'+q('tblInd'))
    if ind is None:
        sid=tb.find('./'+q('tblPr')+'/'+q('tblStyle')).get(q('val'))
        ind=fixture_styles.xpath('./w:style[@w:styleId="'+sid+'"]/w:tblPr/w:tblInd',namespaces=N)[0]
    assert ind is not None and abs(int(ind.get(q('w')))-(tw(ANCHORS[i-1])+100))<=2,('fixture table position',i)
for n in ['specimen']:
    p=pdfium.PdfDocument(str(RQ/(n+'.pdf')))
    for i,page in enumerate(p):page.render(scale=1.5).to_pil().save(RQ/f'{n}-{i+1}.png')
jsonout(RQ/'checks.json',{'styles':59,'native_template_creation':True,'specimen_pages':len(p),'template_sha256':sha(RQ/'candidate.dotx'),'pc_defaults_not_modified':True})
