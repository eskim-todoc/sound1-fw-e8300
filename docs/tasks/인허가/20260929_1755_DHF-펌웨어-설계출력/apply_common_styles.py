"""Import the actual opt-in DOTX styles without replacing content or revision history."""
from pathlib import Path
from copy import deepcopy
from datetime import datetime, timezone
from collections import Counter
import hashlib, json, re, zipfile
from lxml import etree as E

TASK = Path(__file__).resolve().parent
ROOT = TASK.parents[3]
LIVE = ROOT/'docs/참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어/SDDD-FW-TD24A 소프트웨어 상세 설계서.docx'
TEMPLATE = Path('E:/workspace/rules-for-codex/지침/양식/Word/TDC 공통 스타일.dotx')
QA = TASK/'qa/design-readability/common-style'
QA.mkdir(parents=True, exist_ok=True)
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main'
NS={'w':W,'m':'http://schemas.openxmlformats.org/officeDocument/2006/math',
    'wp':'http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing'}
q=lambda x:'{'+W+'}'+x
def read(path):
    with zipfile.ZipFile(path) as z:return {n:z.read(n) for n in z.namelist()}
def xml(parts, name):return E.fromstring(parts[name])
def write(path,parts):
    with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED) as z:
        for name,data in parts.items():z.writestr(name,data)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def child(parent, tag, **vals):
    el=parent.find(q(tag))
    if el is None:el=E.SubElement(parent,q(tag))
    for k,v in vals.items():el.set(q(k),str(v))
    return el
def props(p):
    pr=p.find(q('pPr'))
    if pr is None:pr=E.Element(q('pPr'));p.insert(0,pr)
    return pr
def text(p):return ''.join(p.xpath('.//w:t[not(ancestor::w:del)]/text()',namespaces=NS))
def current_nodes(p,path):return p.xpath(path+'[not(ancestor::w:del) and not(ancestor::w:pPrChange) and not(ancestor::w:rPrChange)]',namespaces=NS)

baseline=QA/'before.docx'
if baseline.exists():
    assert sha(LIVE)==sha(baseline),'Live file changed; do not discard external edits.'
else:baseline.write_bytes(LIVE.read_bytes())
parts=read(baseline); tpl=read(TEMPLATE)
doc=xml(parts,'word/document.xml'); styles=xml(parts,'word/styles.xml')
numbering=xml(parts,'word/numbering.xml'); ts=xml(tpl,'word/styles.xml')
tn=xml(tpl,'word/numbering.xml')
source_styles={s.get(q('styleId')):s for s in ts.findall(q('style'))}
existing={s.find(q('name')).get(q('val')):s for s in styles.findall(q('style'))}
heading_levels={s.get(q('styleId')):int(name[-1]) for name,s in existing.items() if re.fullmatch('heading [1-6]',name)}
normalid=existing['Normal'].get(q('styleId'))
default_p=ts.find('./'+q('docDefaults')+'/'+q('pPrDefault')+'/'+q('pPr'))
default_r=ts.find('./'+q('docDefaults')+'/'+q('rPrDefault')+'/'+q('rPr'))
def resolved(sid, tag):
    chain=[]
    while sid:
        s=source_styles[sid];chain.append(s)
        parent=s.find(q('basedOn'));sid=parent.get(q('val')) if parent is not None else None
    result=deepcopy(default_p if tag=='pPr' else default_r) if tag in ['pPr','rPr'] else E.Element(q(tag))
    if result is None:result=E.Element(q(tag))
    for s in reversed(chain):
        pr=s.find(q(tag))
        if pr is None:continue
        for x in pr:
            old=result.find(x.tag)
            if old is not None:result.remove(old)
            result.append(deepcopy(x))
    return result
custom=[s for s in ts.findall(q('style')) if s.find(q('name')).get(q('val')).startswith('TDC ')]
assert len(custom)==21
remap={s.get(q('styleId')):'TDCCommon'+str(i+1) for i,s in enumerate(custom)}
ids={s.find(q('name')).get(q('val')):remap[s.get(q('styleId'))] for s in custom}
assert not any(v in {s.get(q('styleId')) for s in styles} for v in ids.values())
tpl_heading_lvls={}
for source in custom:
    oldid=source.get(q('styleId'));name=source.find(q('name')).get(q('val'));dest=deepcopy(source)
    dest.set(q('styleId'),remap[oldid])
    for tag in ['basedOn','link','pPr','rPr','tblPr','trPr','tcPr']:
        for x in dest.findall(q(tag)):dest.remove(x)
    # Resolve source Normal/default dependencies, preserving destination Normal and cover styles.
    for tag in ['pPr','rPr']+(['tblPr','trPr','tcPr'] if source.get(q('type'))=='table' else []):
        dest.append(resolved(oldid,tag))
    for x in dest.findall(q('next')):x.set(q('val'),remap.get(x.get(q('val')),ids['TDC 본문 1']))
    if source.get(q('type'))=='paragraph':
        pr=dest.find(q('pPr'));child(pr,'outlineLvl',val=9)
        for tag in ['keepNext','keepLines','pageBreakBefore']:
            if pr.find(q(tag)) is None:child(pr,tag,val=0)
        rp=dest.find(q('rPr'))
        # Source body inherits false from Normal implicitly; make it portable.
        for tag in ['b','bCs']:
            if rp.find(q(tag)) is None:child(rp,tag,val=0)
        child(pr,'widowControl',val=1)
        if re.fullmatch('TDC 제목 [1-6]',name):
            level=int(name[-1]);child(pr,'outlineLvl',val=level-1)
            ref=pr.find(q('numPr'));numid=ref.find(q('numId')).get(q('val'))
            aid=tn.xpath('./w:num[@w:numId="'+numid+'"]/w:abstractNumId/@w:val',namespaces=NS)[0]
            lv=tn.xpath('./w:abstractNum[@w:abstractNumId="'+aid+'"]/w:lvl[@w:ilvl="'+str(level-1)+'"]',namespaces=NS)[0]
            tpl_heading_lvls[level]=deepcopy(lv)
            for x in lv.find(q('pPr')):
                if x.tag==q('ind'):
                    old=pr.find(x.tag)
                    if old is not None:pr.remove(old)
                    pr.append(deepcopy(x))
            # Connect to the document's original outline, not the template list.
            ref.find(q('numId')).set(q('val'),'7');child(ref,'ilvl',val=level-1)
    styles.append(dest)

stamp=datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
rid=max([int(x) for x in doc.xpath('//@w:id',namespaces=NS) if x.isdigit()]+[0])+1
def remember(pr,tag):
    global rid
    changetag=tag+'Change'
    if pr.find(q(changetag)) is not None:return
    old=deepcopy(pr)
    # Property snapshots must not clone paragraph insertion/deletion markers or
    # nested historical changes; those remain in their original live locations.
    for x in list(old.iter()):
        if x is not old and x.tag in {q(t) for t in ['ins','del','pPrChange','rPrChange','tblPrChange','tcPrChange','trPrChange']}:
            if x.getparent() is not None:x.getparent().remove(x)
    ch=E.SubElement(pr,q(changetag),{q('id'):str(rid),q('author'):'김은수',q('date'):stamp});rid+=1
    ch.append(old)
def remove(pr,tags):
    for tag in tags:
        for x in pr.findall(q(tag)):pr.remove(x)
def style_para(p,name,heading=False, table=False):
    pr=props(p);remember(pr,'pPr')
    remove(pr,['pStyle','ind','spacing','tabs','jc','keepNext','keepLines','outlineLvl','snapToGrid','contextualSpacing'])
    child(pr,'pStyle',val=ids[name])
    # Old paragraph mark formatting must not override the imported style.
    mark=pr.find(q('rPr'))
    if mark is not None:
        remember(mark,'rPr');remove(mark,['rFonts','sz','szCs','color','vanish','webHidden'])
    for rp in current_nodes(p,'.//w:rPr'):
        remember(rp,'rPr');remove(rp,['rFonts','sz','szCs','color','vanish','webHidden'])
        if heading or name=='TDC 표 머리행':remove(rp,['b','bCs'])
    # Ordinary text must not inherit old heading/list behaviour from direct settings.
    if not heading:
        child(pr,'outlineLvl',val=9)
        if name.startswith('TDC 본문') or table:
            child(pr,'keepNext',val=0);child(pr,'keepLines',val=0)
    if table:child(pr,'pageBreakBefore',val=0)

body=doc.find(q('body'));started=False;depth=1;counts=Counter();headings=[]
for block in body:
    if block.tag==q('p'):
        t=text(block);pr=props(block)
        oldstyle=pr.find(q('pStyle'));level=heading_levels.get(oldstyle.get(q('val')) if oldstyle is not None else '')
        if not started:
            if t=='서론' and level==1:started=True
            else:continue
        # Historical deleted paragraphs/runs remain untouched.
        present_image=current_nodes(block,'.//w:drawing')+current_nodes(block,'.//w:pict')
        present_math=current_nodes(block,'.//m:oMathPara')+current_nodes(block,'.//m:oMath')
        if not t and not present_image and not present_math:
            deletedmark=pr.find('./'+q('rPr')+'/'+q('del'))
            if deletedmark is not None or block.xpath('.//w:delText',namespaces=NS):continue
        if level and t:
            if pr.find(q('numPr')) is None:
                # Some original headings inherit their outline from the style.
                inherited=next((s.find('./'+q('pPr')+'/'+q('numPr')) for s in existing.values() if s.get(q('styleId'))==oldstyle.get(q('val'))),None)
                if inherited is not None:pr.append(deepcopy(inherited))
                else:
                    np=child(pr,'numPr');child(np,'ilvl',val=level-1);child(np,'numId',val=7)
            depth=level;name='TDC 제목 '+str(level);style_para(block,name,True)
            headings.append({'text':t,'level':level,'numPr':E.tostring(pr.find(q('numPr'))).decode()})
        elif present_image:
            name='TDC 그림';style_para(block,name)
            # Existing flowchart new-page rule survives, without a tiny break paragraph.
        elif present_math:
            name='TDC 수식';style_para(block,name)
        elif re.match(r'<?그림\s+\d',t):
            name='TDC 그림 캡션';style_para(block,name)
        elif re.match(r'표\s+\d+\.',t):
            name='TDC 표 캡션';style_para(block,name)
        elif t.startswith('흐름 참고 :'):
            name='TDC 주석';style_para(block,name)
        else:
            name='TDC 본문 '+str(depth);style_para(block,name)
            if block.getprevious() is not None and block.getprevious().tag==q('tbl'):
                child(pr,'spacing',before=200)
            if not t:
                # Necessary empty/section-break paragraphs stay visible and normal-sized.
                child(pr,'spacing',before=0,after=0)
        counts[name]+=1
    elif started and block.tag==q('tbl'):
        counts['TDC 표']+=1
        pr=block.find(q('tblPr'));remember(pr,'tblPr')
        remove(pr,['tblStyle','tblBorders','tblCellMar','shd'])
        child(pr,'tblStyle',val=ids['TDC 표'])
        # Preserve grid widths/merges/placement. Style controls only borders and padding.
        rows=block.findall(q('tr'))
        for i,row in enumerate(rows):
            trpr=child(row,'trPr');remember(trpr,'trPr')
            for h in trpr.findall(q('trHeight')):
                if h.get(q('hRule'))=='exact':h.set(q('hRule'),'atLeast')
            if i==0:child(trpr,'tblHeader')
            for tc in row.findall(q('tc')):
                cp=child(tc,'tcPr');remember(cp,'tcPr')
                remove(cp,['tcBorders','tcMar','shd']);child(cp,'vAlign',val='center')
                for p in tc.findall(q('p')):
                    if p.xpath('.//w:delText',namespaces=NS) and not text(p):continue
                    name='TDC 표 머리행' if i==0 else 'TDC 표 본문'
                    style_para(p,name,table=True);counts[name]+=1
                    if i and len(text(p))>18:child(props(p),'jc',val='left')

# Keep original list ID, levels, start/restart and number text exactly as they were.
aid=numbering.xpath('./w:num[@w:numId="7"]/w:abstractNumId/@w:val',namespaces=NS)[0]
outline=numbering.xpath('./w:abstractNum[@w:abstractNumId="'+aid+'"]',namespaces=NS)[0]
for lv in outline.findall(q('lvl')):
    level=int(lv.get(q('ilvl')))+1
    if level>6:continue
    child(lv,'pStyle',val=ids['TDC 제목 '+str(level)])
    for tag in ['pPr','rPr']:
        remove(lv,[tag]);source=tpl_heading_lvls[level].find(q(tag))
        if source is not None:lv.append(deepcopy(source))
settings=xml(parts,'word/settings.xml');remove(settings,['updateStyles']);child(settings,'trackRevisions')
parts['word/settings.xml']=E.tostring(settings,xml_declaration=True,encoding='UTF-8',standalone=True)
for name,r in [('word/document.xml',doc),('word/styles.xml',styles),('word/numbering.xml',numbering)]:
    parts[name]=E.tostring(r,xml_declaration=True,encoding='UTF-8',standalone=True)
write(QA/'candidate.docx',parts)
report={'baseline_sha256':sha(baseline),'template_sha256':sha(TEMPLATE),'styles':ids,
        'assigned':dict(counts),'headings':headings,'body_text_unchanged':True,
        'qa_dir':str(QA),'candidate':str(QA/'candidate.docx')}
(QA/'apply.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
# Character content, field instructions, drawings and prior revisions are immutable.
old=xml(read(baseline),'word/document.xml')
for path in ['.//w:t/text()','.//w:delText/text()','.//w:instrText/text()','.//m:t/text()']:
    assert old.xpath(path,namespaces=NS)==doc.xpath(path,namespaces=NS),path
for tag in ['ins','del','pPrChange','rPrChange']:
    new_revisions={x.get(q('id')):x for x in doc.findall('.//'+q(tag))}
    for x in old.findall('.//'+q(tag)):
        match=new_revisions.get(x.get(q('id')))
        assert match is not None and x.get(q('author'))==match.get(q('author'))
for name,data in read(baseline).items():
    if name not in ['word/document.xml','word/styles.xml','word/numbering.xml','word/settings.xml']:
        assert parts[name]==data,name
assert len(headings)>=162,len(headings)  # Deleted paragraph marks merge some XML fragments in Word.
print(json.dumps({'headings':len(headings),'assigned':dict(counts),'qa_dir':str(QA)},ensure_ascii=False))
