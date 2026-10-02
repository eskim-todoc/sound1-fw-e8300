"""Apply the requested style pack to current body blocks and normalize table gaps."""
from compact_unit_style import *

parts=read(Q/'interim.docx');raw=E.fromstring(parts['word/document.xml']);st=E.fromstring(parts['word/styles.xml'])
styles={s.get(q('styleId')):s for s in st.findall(q('style'))}
names={sid:s.find(q('name')).get(q('val')) for sid,s in styles.items()};ids={n:sid for sid,n in names.items()}
for sid,s in styles.items():
    if names[sid].startswith('TDC 표') and s.get(q('type'))=='table':
        child(child(s,'trPr'),'jc',val='left')
        child(child(s,'tblPr'),'jc',val='left')
rid=max([int(v) for v in raw.xpath('//@w:id',namespaces=NS) if v.isdigit()]+[0])+1
stamp=datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
purposes={u['fields']['목적'] for u in json.loads((Q/'fragments.json').read_text(encoding='utf8'))}
review_page_starts={'SU-ID-CFX-011','SU-ID-HEAR-002','SU-ID-CM3-007'}
def attrs():
    global rid
    a={q('id'):str(rid),q('author'):'김은수',q('date'):stamp};rid+=1;return a
def remember(pr,tag):
    if pr.find(q(tag+'Change')) is not None:return
    old=deepcopy(pr)
    for x in list(old.iter()):
        if x is not old and x.tag in {q(v) for v in ['ins','del','pPrChange','rPrChange','tblPrChange','tcPrChange','trPrChange']} and x.getparent() is not None:x.getparent().remove(x)
    ch=E.SubElement(pr,q(tag+'Change'),attrs());ch.append(old)
def name(p):
    x=p.find('./'+q('pPr')+'/'+q('pStyle'));return names.get(x.get(q('val')) if x is not None else '','')
def blank(level):
    p=E.Element(q('p'));pp=el(p,'pPr');el(pp,'pStyle',val=ids['TDC 본문 '+str(level)])
    el(pp,'spacing',before=0,after=0,line=300,lineRule='auto');el(pp,'keepNext',val=0);el(pp,'outlineLvl',val=9)
    rp=el(pp,'rPr');el(rp,'sz',val=20);el(rp,'szCs',val=20);E.SubElement(rp,q('ins'),attrs())
    return p
body=raw.find(q('body'));started=False;depth=1;table_count=0;new_blanks=0;captions=0;images=[]
for b in list(body):
    if b.tag==q('p'):
        tx=txt(b);style=name(b)
        if tx=='서론':started=True
        if not started:continue
        m=re.fullmatch('TDC 제목 ([1-6])',style)
        if m:depth=int(m[1])
        level=int(style[-1]) if re.search(' [1-6]$',style) else depth
        pp=child(b,'pPr');remember(pp,'pPr')
        for tag in ['ind','tabs','spacing']:
            for x in pp.findall(q(tag)):pp.remove(x)
        if tx in purposes:
            child(pp,'keepNext',val=1)
        if m and tx.split(',')[0] in review_page_starts:
            child(pp,'pageBreakBefore',val=1)
        if not m and not tx and not b.xpath('.//wp:inline|.//w:sectPr',namespaces=NS):
            child(pp,'spacing',before=0,after=0,line=300,lineRule='auto');child(pp,'keepNext',val=0)
        for rp in b.xpath('.//w:rPr[not(ancestor::w:del) and not(ancestor::w:rPrChange) and not(ancestor::w:pPrChange)]',namespaces=NS):
            # Leave embedded mathematical runs unchanged.
            if any(E.QName(a).namespace=='http://schemas.openxmlformats.org/officeDocument/2006/math' for a in rp.iterancestors()):continue
            remember(rp,'rPr')
            for tag in ['sz','szCs']:
                child(rp,tag,val=SIZES[level-1]*2 if m else 20)
        for inline in b.xpath('.//wp:inline[not(ancestor::w:del)]',namespaces=NS):
            ext=inline.find('{'+NS['wp']+'}extent');width=int(ext.get('cx'))/360000;height=int(ext.get('cy'))/360000
            wanted=min(width,16.6-ANCHORS[level-1]);ratio=wanted/width
            for e in [ext]+inline.xpath('.//a:xfrm/a:ext',namespaces=NS):e.set('cx',str(round(int(e.get('cx'))*ratio)));e.set('cy',str(round(int(e.get('cy'))*ratio)))
            images.append({'width_cm':wanted,'height_cm':height*ratio,'level':level})
        instructions=''.join(b.xpath('.//w:instrText[not(ancestor::w:del)]/text()',namespaces=NS))
        seq=bool(re.search(r'\bSEQ\s+표\b',instructions))
        simple=b.xpath('.//w:fldSimple[not(ancestor::w:del) and contains(@w:instr,"SEQ 표")]',namespaces=NS)
        if (seq or simple) and tx.startswith('표'):
            captions+=1;sep=False
            for f in simple:
                for t in f.findall('.//'+q('t')):
                    if (t.text or '').isdigit():t.text=str(captions)
            for r in b.xpath('.//w:r[not(ancestor::w:del)]',namespaces=NS):
                fld=r.find(q('fldChar'))
                if fld is not None:
                    if fld.get(q('fldCharType'))=='separate':sep=True
                    if fld.get(q('fldCharType'))=='end':sep=False
                elif sep:
                    t=r.find(q('t'))
                    if t is not None and (t.text or '').isdigit():t.text=str(captions)
    elif started and b.tag==q('tbl'):
        rows=[r for r in b.findall(q('tr')) if r.find('./'+q('trPr')+'/'+q('del')) is None]
        if not rows:continue
        table_count+=1;tp=child(b,'tblPr');remember(tp,'tblPr')
        s=tp.find(q('tblStyle'));sn=names.get(s.get(q('val')) if s is not None else '','')
        level=int(sn[-1]) if re.fullmatch('TDC 표 [1-6]',sn) else depth
        child(tp,'tblStyle',val=ids['TDC 표 '+str(level)]);child(tp,'tblInd',w=tw(ANCHORS[level-1])+100,type='dxa')
        grid=b.find(q('tblGrid'));cols=grid.findall(q('gridCol'));total=sum(int(c.get(q('w'))) for c in cols);available=tw(16.6-ANCHORS[level-1]);ratio=min(1,available/total)
        if ratio<1:
            for c in cols:c.set(q('w'),str(round(int(c.get(q('w')))*ratio)))
            for c in b.xpath('./w:tr/w:tc/w:tcPr/w:tcW[@w:type="dxa"]',namespaces=NS):c.set(q('w'),str(round(int(c.get(q('w')))*ratio)))
            child(tp,'tblW',w=available,type='dxa')
        for ri,row in enumerate(rows):
            for ci,cell in enumerate(row.findall(q('tc'))):
                cp=child(cell,'tcPr');remember(cp,'tcPr');child(cp,'vAlign',val='center')
                label=(ri==0 or ci==0)
                if label:child(cp,'shd',val='clear',color='auto',fill='F5F5F5')
                for p in cell.findall(q('p')):
                    pp=child(p,'pPr');remember(pp,'pPr');child(pp,'jc',val='center' if label else 'left');child(pp,'keepNext',val=1 if ri==0 else 0)
                    for x in pp.findall(q('ind')):pp.remove(x)
                    for rp in p.xpath('.//w:rPr[not(ancestor::w:del) and not(ancestor::w:rPrChange) and not(ancestor::w:pPrChange)]',namespaces=NS):
                        remember(rp,'rPr');child(rp,'sz',val=20);child(rp,'szCs',val=20)
        nxt=b.getnext()
        usable=nxt is not None and nxt.tag==q('p') and not txt(nxt) and not nxt.xpath('.//wp:inline|.//w:sectPr|./w:pPr/w:rPr/w:del',namespaces=NS)
        if not usable:
            body.insert(body.index(b)+1,blank(level));new_blanks+=1
        else:
            pp=child(nxt,'pPr');remember(pp,'pPr');child(pp,'pStyle',val=ids['TDC 본문 '+str(level)]);child(pp,'spacing',before=0,after=0,line=300,lineRule='auto');child(pp,'keepNext',val=0)
            rp=child(pp,'rPr');remember(rp,'rPr');child(rp,'sz',val=20);child(rp,'szCs',val=20)
for x in raw.xpath('.//w:bookmarkStart[starts-with(@w:name,"CompactUnit_")]',namespaces=NS):
    bid=x.get(q('id'));x.getparent().remove(x)
    for end in raw.xpath('.//w:bookmarkEnd[@w:id="'+bid+'"]',namespaces=NS):end.getparent().remove(end)
parts['word/document.xml']=xmlbytes(raw);parts['word/styles.xml']=xmlbytes(st)
write(Q/'candidate.docx',parts)
jsonout(Q/'finish.json',{'body_tables':table_count,'captions':captions,'new_table_gaps':new_blanks,'images':images})
print(json.dumps({'body_tables':table_count,'captions':captions,'new_table_gaps':new_blanks,'images':len(images)}))
