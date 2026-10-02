"""Apply the requested shallow hierarchy without changing numbers or technical content."""
from pathlib import Path
from copy import deepcopy
from lxml import etree as E
import zipfile,json,re,hashlib

T=Path(__file__).resolve().parent
QA=T/'qa/unit-reconciliation'
REVIEW=T/'qa/design-readability'
P=QA/'SDDD-candidate.docx'
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main'
M='http://schemas.openxmlformats.org/officeDocument/2006/math'
N={'w':W,'m':M,'wp':'http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing'}
q=lambda v:'{'+W+'}'+v
def text(p):return ''.join(p.xpath('.//w:t/text()',namespaces=N))
def child(p,tag,**values):
    x=p.find(q(tag))
    if x is None:x=E.SubElement(p,q(tag))
    for k,v in values.items():x.set(q(k),str(v))
    return x
def pp(p):
    x=p.find(q('pPr'))
    if x is None:x=E.Element(q('pPr'));p.insert(0,x)
    return x
def ind(props,left,hanging=0):
    x=child(props,'ind');x.attrib.clear()
    x.set(q('left'),str(left));x.set(q('right'),'0')
    x.set(q('hanging') if hanging else q('firstLine'),str(hanging))
def font(rpr,size,bold=None):
    f=child(rpr,'rFonts');f.attrib.clear()
    for key in ['ascii','hAnsi','eastAsia','cs']:f.set(q(key),'맑은 고딕')
    child(rpr,'sz',val=size);child(rpr,'szCs',val=size)
    child(rpr,'color',val='000000')
    if bold is not None:child(rpr,'b',val='1' if bold else '0');child(rpr,'bCs',val='1' if bold else '0')
def format_heading(props,level):
    left=TEXT_LEFT[level-1];number=NUM_LEFT[level-1]
    ind(props,left,left-number)
    child(props,'spacing',before=BEFORE[level-1],after=AFTER[level-1],line=288,lineRule='auto')
    child(props,'jc',val='left');child(props,'keepNext',val=1);child(props,'keepLines',val=1)
    child(props,'widowControl',val=1);child(props,'snapToGrid',val=0)
    old=props.find(q('tabs'))
    if old is not None:props.remove(old)
    tabs=child(props,'tabs');E.SubElement(tabs,q('tab'),{q('val'):'left',q('pos'):str(left)})
    child(props,'outlineLvl',val=level-1)

# Label starts remain shallow; long automatic numbers have their own hanging area.
TEXT_LEFT=[400,650,900,1245,1330,1415]  # 0.71 / 1.15 / 1.59 / 2.20 / 2.35 / 2.50 cm
NUM_LEFT=[0,85,170,255,340,425]
SIZES=[28,25,23,22,21,21]
BEFORE=[360,320,240,320,240,200]
AFTER=[200,160,140,140,120,120]
with zipfile.ZipFile(P) as z:parts={n:z.read(n) for n in z.namelist()}
r=E.fromstring(parts['word/document.xml']);body=r.find(q('body'))
styles=E.fromstring(parts['word/styles.xml'])
styleids={};stylelevels={}
for level in range(1,7):
    hits=styles.xpath('./w:style[w:name/@w:val="heading '+str(level)+'"]',namespaces=N)
    if hits:s=hits[0]
    else:
        s=E.SubElement(styles,q('style'),{q('type'):'paragraph',q('styleId'):'SDDDHeading'+str(level)})
        child(s,'name',val='heading '+str(level));child(s,'basedOn',val='a');child(s,'next',val='a');child(s,'qFormat')
    sid=s.get(q('styleId'));styleids[level]=sid;stylelevels[sid]=level
    format_heading(child(s,'pPr'),level);font(child(s,'rPr'),SIZES[level-1],True)

start=False;level=1;chapter=0;blocks=[];block=None;heading_records=[];body_records=[]
for p in list(body):
    if p.tag!=q('p'):continue
    s=text(p)
    pr=pp(p);st=pr.find(q('pStyle'));lev=stylelevels.get(st.get(q('val')) if st is not None else '')
    if not start:
        if lev==1 and s=='서론':start=True
        else:continue
    for br in p.xpath('.//w:br[@w:type="page"]',namespaces=N):br.getparent().remove(br)
    # Original front matter also has one numbered heading with Normal style.
    # Preserve its list value while assigning the matching heading style.
    if not lev:
        numbering_id=pr.find('./'+q('numPr')+'/'+q('numId'))
        ilvl=pr.find('./'+q('numPr')+'/'+q('ilvl'))
        if numbering_id is not None and numbering_id.get(q('val'))=='7' and ilvl is not None:
            lev=int(ilvl.get(q('val')))+1
            if lev<=6:child(pr,'pStyle',val=styleids[lev])
            else:lev=None
    for node in p.xpath('.//w:t',namespaces=N):node.text=(node.text or '').replace('·',',')
    if not s and not p.xpath('.//w:drawing|.//w:pict|.//m:oMath|.//m:oMathPara|.//w:br|./w:pPr/w:sectPr',namespaces=N):
        body.remove(p);continue
    if lev:
        level=lev
        if lev==1:chapter+=1
        if block is not None:blocks.append(block);block=None
        if s.startswith('SU-ID-'):block=[p]
        format_heading(pr,lev)
        if chapter==1 and lev in [2,3]:
            child(pr,'spacing',before=220 if lev==2 else 200,after=120 if lev==2 else 100)
        for run in p.findall(q('r')):font(child(run,'rPr'),SIZES[lev-1],True)
        child(pr,'pageBreakBefore',val=1 if lev==1 or (lev==2 and chapter in [4,5,6,7]) else 0)
        heading_records.append({'text':s,'level':lev,'left':TEXT_LEFT[lev-1]})
    else:
        if block is not None:block.append(p)
        child(pr,'spacing',before=0,after=80,line=300,lineRule='auto')
        child(pr,'jc',val='left');child(pr,'keepNext',val=0);child(pr,'keepLines',val=1)
        child(pr,'widowControl',val=1);child(pr,'snapToGrid',val=0);child(pr,'outlineLvl',val=9)
        child(pr,'pageBreakBefore',val=0)
        ind(pr,TEXT_LEFT[min(level,6)-1])
        for run in p.findall(q('r')):font(child(run,'rPr'),22)
        if p.xpath('.//w:drawing|.//w:pict|.//m:oMath|.//m:oMathPara',namespaces=N):
            ind(pr,0);child(pr,'jc',val='center');child(pr,'spacing',before=120,after=160,line=300,lineRule='auto')
            if p.xpath('.//w:drawing|.//w:pict',namespaces=N):child(pr,'keepNext',val=1)
        if re.match(r'<?그림\s+\d',s):
            ind(pr,0);child(pr,'jc',val='center');child(pr,'spacing',before=0,after=240,line=276,lineRule='auto')
            for run in p.findall(q('r')):font(child(run,'rPr'),20)
        if re.match(r'표\s+\d+\.',s):
            ind(pr,400);child(pr,'keepNext',val=1);child(pr,'spacing',before=200,after=120,line=276,lineRule='auto')
        if s.startswith(('목적 :','입력 :','출력 :','처리 :','동작 조건 :','인터페이스 :','인터페이스 계속 :','구현 위치 :','입출력 예 :','흐름 참고 :')):
            runs=p.findall(q('r'))
            if len(runs)==1 and len(runs[0].findall(q('t')))==1:
                first,rest=s.split(' : ',1)
                run=runs[0];run.find(q('t')).text=first+' : '
                run.find(q('t')).set('{http://www.w3.org/XML/1998/namespace}space','preserve')
                font(child(run,'rPr'),22,True)
                other=E.SubElement(p,q('r'));font(child(other,'rPr'),22,False)
                E.SubElement(other,q('t')).text=rest.replace('·',',')
        if p.getprevious() is not None and p.getprevious().tag==q('tbl'):
            child(pr,'spacing',before=200)
        body_records.append({'text':s,'level':level,'left':int(pr.find(q('ind')).get(q('left')))})
if block is not None:blocks.append(block)
assert len(blocks)==115,len(blocks)
for block in blocks:
    flow_at=next((i for i,p in enumerate(block) if p.xpath('.//wp:docPr[starts-with(@name,"Flow ")]',namespaces=N)),None)
    prose=block if flow_at is None else block[:flow_at]
    for p in prose[:-1]:child(pp(p),'keepNext',val=1)
    child(pp(prose[-1]),'keepNext',val=0)
    child(pp(prose[-1]),'spacing',after=180)
    if flow_at is not None:
        flow_block=block[flow_at:]
        child(pp(flow_block[0]),'pageBreakBefore',val=1)
        child(pp(flow_block[0]),'spacing',before=0,after=100,line=240,lineRule='auto')
        for p in flow_block[:-1]:child(pp(p),'keepNext',val=1)
        child(pp(flow_block[-1]),'keepNext',val=0)
        child(pp(flow_block[-1]),'spacing',after=180)
        if text(flow_block[-1]).startswith('흐름 참고 :'):
            ind(pp(flow_block[-1]),400)
            child(pp(flow_block[-1]),'spacing',before=0,after=180,line=276,lineRule='auto')
            for run in flow_block[-1].findall(q('r')):font(child(run,'rPr'),20)

# Fit the unit lists without leaving a single final row on its own page.
# Keep the existing columns and 10-point table type; only trim cell padding.
for tbl in body.findall(q('tbl')):
    if '유닛 ID' not in text(tbl):continue
    pr=tbl.find(q('tblPr'));mar=child(pr,'tblCellMar')
    child(mar,'top',w=45,type='dxa');child(mar,'bottom',w=45,type='dxa')
    rows=tbl.findall(q('tr'))
    for tr in rows[-3:-1]:
        for p in tr.xpath('./w:tc/w:p',namespaces=N):child(pp(p),'keepNext',val=1)

# The original automatic numbering sequence/text stays intact. Only positions are reset.
numbering=E.fromstring(parts['word/numbering.xml'])
used=set()
for p in body.findall(q('p')):
    st=p.find('./'+q('pPr')+'/'+q('pStyle'))
    if st is not None and st.get(q('val')) in stylelevels:
        num=p.find('./'+q('pPr')+'/'+q('numPr')+'/'+q('numId'))
        if num is not None:used.add(num.get(q('val')))
for num in numbering.findall(q('num')):
    if num.get(q('numId')) not in used:continue
    aid=num.find(q('abstractNumId')).get(q('val'))
    a=numbering.xpath('./w:abstractNum[@w:abstractNumId="'+aid+'"]',namespaces=N)[0]
    for lv in a.findall(q('lvl')):
        i=int(lv.get(q('ilvl')))
        if i>=6:continue
        child(lv,'suff',val='tab')
        child(lv,'lvlJc',val='left')
        font(child(lv,'rPr'),SIZES[i],True)
        prop=child(lv,'pPr');ind(prop,TEXT_LEFT[i],TEXT_LEFT[i]-NUM_LEFT[i])
        old=prop.find(q('tabs'))
        if old is not None:prop.remove(old)
        E.SubElement(child(prop,'tabs'),q('tab'),{q('val'):'num',q('pos'):str(TEXT_LEFT[i])})
for lv in numbering.findall('.//'+q('lvl')):
    fmt=lv.find(q('numFmt'))
    if fmt is not None and fmt.get(q('val'))=='bullet':
        child(lv,'lvlText',val='-');font(child(lv,'rPr'),22)

for name,root in [('word/document.xml',r),('word/styles.xml',styles),('word/numbering.xml',numbering)]:
    parts[name]=E.tostring(root,xml_declaration=True,encoding='UTF-8',standalone=True)
with zipfile.ZipFile(P,'w',zipfile.ZIP_DEFLATED) as z:
    for name,data in parts.items():z.writestr(name,data)
manifest={'heading_styles':styleids,'heading_text_indents_twips':TEXT_LEFT,'number_indents_twips':NUM_LEFT,
          'headings':heading_records,'body_count':len(body_records),'unit_blocks':len(blocks),
          'font':'맑은 고딕','body_pt':11,'line_multiple':1.25,'paragraph_after_pt':4,
          'unit_prose_keep_together':True,'flowchart_separate_page':True,'flow_notes_in_prose':True,'core_and_module_page_break':True}
(REVIEW/'layout.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf8')
print('Headings',len(heading_records),'unit blocks',len(blocks),'body paragraphs',len(body_records))
