"""Unify generated table numbering, preserve tracked packages and field history."""
from pathlib import Path
from copy import deepcopy
import zipfile,re,json
from datetime import datetime,timezone
from lxml import etree as E
T=Path(__file__).resolve().parent;Q=T/'qa/design-readability/structured-units'
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main';NS={'w':W};q=lambda t:'{'+W+'}'+t
def read(p):
    with zipfile.ZipFile(p) as z:return {n:z.read(n) for n in z.namelist()}
def write(p,parts):
    with zipfile.ZipFile(p,'w',zipfile.ZIP_DEFLATED) as z:
        for n,v in parts.items():z.writestr(n,v)
parts=read(Q/'candidate.docx');doc=E.fromstring(parts['word/document.xml']);styles=E.fromstring(parts['word/styles.xml'])
names={s.get(q('styleId')):s.find(q('name')).get(q('val')) for s in styles.findall(q('style'))}
ids={v:k for k,v in names.items()}
body=doc.find(q('body'));started=False;depth=1;added=[]
rid=max([int(v) for v in doc.xpath('//@w:id',namespaces=NS) if v.isdigit()]+[0])+1
stamp=datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
for block in list(body):
    text=''.join(block.xpath('.//w:t[not(ancestor::w:del)]/text()',namespaces=NS))
    if block.tag==q('p'):
        st=block.find('./'+q('pPr')+'/'+q('pStyle'));name=names.get(st.get(q('val')) if st is not None else '','')
        if text=='서론' and name=='TDC 제목 1':started=True
        if started and re.fullmatch('TDC 제목 [1-6]',name):depth=int(name[-1])
    if block.tag!=q('tbl') or not started:continue
    previous=block.getprevious();prev=''.join(previous.xpath('.//w:t[not(ancestor::w:del)]/text()',namespaces=NS))
    if prev.startswith('표 '):continue
    if prev not in ['프로젝트','표준 및 규제']:continue
    title='프로젝트 참조 문서' if prev=='프로젝트' else '적용 표준 및 규제'
    p=E.Element(q('p'));pp=E.SubElement(p,q('pPr'));E.SubElement(pp,q('pStyle'),{q('val'):ids['TDC 표 캡션 '+str(depth)]})
    mark=E.SubElement(pp,q('rPr'));E.SubElement(mark,q('ins'),{q('id'):str(rid),q('author'):'김은수',q('date'):stamp});rid+=1
    ins=E.SubElement(p,q('ins'),{q('id'):str(rid),q('author'):'김은수',q('date'):stamp});rid+=1
    def run(tag,text=None,**attrs):
        r=E.SubElement(ins,q('r'));x=E.SubElement(r,q(tag),{q(k):str(v) for k,v in attrs.items()});x.text=text
        if tag in ['t','instrText']:x.set('{http://www.w3.org/XML/1998/namespace}space','preserve')
    run('t','표 ');run('fldChar',fldCharType='begin');run('instrText',' SEQ 표 \\* ARABIC ');run('fldChar',fldCharType='separate');run('t','1');run('fldChar',fldCharType='end');run('t','. '+title)
    body.insert(body.index(block),p);added.append(title)
count=0;converted=[]
for p in doc.xpath('./w:body/w:p',namespaces=NS):
    st=p.find('./'+q('pPr')+'/'+q('pStyle'));name=names.get(st.get(q('val')) if st is not None else '','')
    if not name.startswith('TDC 표 캡션'):continue
    text=''.join(p.xpath('.//w:t[not(ancestor::w:del)]/text()',namespaces=NS))
    if not re.match('표 \\d+\\.',text):continue
    count+=1
    fields=p.xpath('.//w:instrText[not(ancestor::w:del)]',namespaces=NS)
    if any('SEQ' in (f.text or '') for f in fields):continue
    nodes=p.xpath('.//w:t[not(ancestor::w:del)]',namespaces=NS)
    first=nodes[0];m=re.match(r'표 (\d+)(.*)',first.text or '')
    assert m is not None,text
    run=first.getparent();parent=run.getparent()
    assert parent.tag==q('ins'),('Original untracked caption requires separate redline',text,parent.tag)
    first.text='표 ';first.set('{http://www.w3.org/XML/1998/namespace}space','preserve')
    offset=parent.index(run)+1
    runs=[]
    def r():
        x=E.Element(q('r'));rp=run.find(q('rPr'))
        if rp is not None:x.append(deepcopy(rp))
        runs.append(x);return x
    E.SubElement(r(),q('fldChar'),{q('fldCharType'):'begin'})
    E.SubElement(r(),q('instrText')).text=' SEQ 표 \\* ARABIC '
    E.SubElement(r(),q('fldChar'),{q('fldCharType'):'separate'})
    E.SubElement(r(),q('t')).text=str(count)
    E.SubElement(r(),q('fldChar'),{q('fldCharType'):'end'})
    E.SubElement(r(),q('t')).text=m.group(2)
    for i,x in enumerate(runs):parent.insert(offset+i,x)
    converted.append(text)
assert count==218,count
# Number cache for every caption is generated display data, not design content.
counter=0
for p in doc.xpath('./w:body/w:p',namespaces=NS):
    seq=p.xpath('.//w:instrText[not(ancestor::w:del) and contains(text(),"SEQ 표")]',namespaces=NS)
    if not seq:continue
    counter+=1;separate=False
    for r in p.xpath('.//w:r[not(ancestor::w:del)]',namespaces=NS):
        fld=r.find(q('fldChar'))
        if fld is not None:
            if fld.get(q('fldCharType'))=='separate':separate=True
            if fld.get(q('fldCharType'))=='end':separate=False
        elif separate:
            t=r.find(q('t'))
            if t is not None and (t.text or '').isdigit():t.text=str(counter)
# Remove transient bookmarks from the published document.
ids=set()
for x in doc.xpath('.//w:bookmarkStart[starts-with(@w:name,"UnitFields_")]',namespaces=NS):ids.add(x.get(q('id')));x.getparent().remove(x)
for x in doc.xpath('.//w:bookmarkEnd',namespaces=NS):
    if x.get(q('id')) in ids:x.getparent().remove(x)
parts['word/document.xml']=E.tostring(doc,xml_declaration=True,encoding='UTF-8',standalone=True)
write(Q/'candidate.docx',parts)
(Q/'finish.json').write_text(json.dumps({'captions':count,'legacy_caption_fields_added':converted,'new_reference_captions':added},ensure_ascii=False,indent=2),encoding='utf8')
print('Table captions:',count,'legacy converted:',len(converted))
