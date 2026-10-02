"""Apply the live opt-in DOTX pack to SIP while retaining native redlines."""
from pathlib import Path
from copy import deepcopy
from datetime import datetime, timezone
from collections import Counter
import argparse, hashlib, json, re, zipfile
from lxml import etree as E

TASK = Path(__file__).resolve().parent
ROOT = TASK.parents[3]
parser=argparse.ArgumentParser()
parser.add_argument('--kind',choices=['sip','sad'],default='sip')
args=parser.parse_args()
QA = TASK / ('qa/'+args.kind+'-common-style')
filename='SIP-FW-TD24A 소프트웨어 통합 계획서.docx' if args.kind=='sip' else 'SAD-FW-TD24A 소프트웨어 아키텍쳐 기술서.docx'
LIVE = ROOT / 'docs/참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어' / filename
TPL = Path('E:/workspace/rules-for-codex/지침/양식/Word/TDC 공통 스타일.dotx')
W = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'
NS = {'w': W, 'wp': 'http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing'}
q = lambda t: '{' + W + '}' + t
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):
    with zipfile.ZipFile(p) as z: return {n: z.read(n) for n in z.namelist()}
def save(p, parts):
    with zipfile.ZipFile(p, 'w', zipfile.ZIP_DEFLATED) as z:
        for n, data in parts.items(): z.writestr(n, data)
def xml(parts, n): return E.fromstring(parts[n])
def serialize(r): return E.tostring(r, xml_declaration=True, encoding='UTF-8', standalone=True)
def child(p, t, **a):
    x = p.find(q(t))
    if x is None: x = E.SubElement(p, q(t))
    for k, v in a.items(): x.set(q(k), str(v))
    return x
def remove(p, tags):
    for t in tags:
        for x in p.findall(q(t)): p.remove(x)
def pp(p):
    x = p.find(q('pPr'))
    if x is None: x = E.Element(q('pPr')); p.insert(0, x)
    return x
def txt(p): return ''.join(p.xpath('.//w:t[not(ancestor::w:del)]/text()', namespaces=NS))
def active(p):
    return not p.xpath('./w:pPr/w:rPr/w:del', namespaces=NS) and not (not txt(p) and p.xpath('.//w:delText', namespaces=NS))

QA.mkdir(parents=True, exist_ok=True)
before = QA / 'before.docx'
if not before.exists(): before.write_bytes(LIVE.read_bytes())
assert sha(before) == sha(LIVE), 'External SIP changes: preserve and reassess.'
parts = read(before); tp = read(TPL)
doc = xml(parts, 'word/document.xml'); old_doc = deepcopy(doc)
styles = xml(parts, 'word/styles.xml'); ts = xml(tp, 'word/styles.xml')
num = xml(parts, 'word/numbering.xml'); tn = xml(tp, 'word/numbering.xml')
sources = {s.get(q('styleId')): s for s in ts.findall(q('style'))}
existing = {s.find(q('name')).get(q('val')): s for s in styles.findall(q('style'))}
old_levels = {s.get(q('styleId')): int(name[-1]) for name, s in existing.items() if re.fullmatch('heading [1-6]', name)}
customs = [s for s in sources.values() if s.find(q('name')).get(q('val')).startswith('TDC ')]
assert len(customs) == 59
ids = {s.find(q('name')).get(q('val')): 'TDCSIP' + str(i + 1) for i, s in enumerate(customs)}
remap = {s.get(q('styleId')): ids[s.find(q('name')).get(q('val'))] for s in customs}
assert not set(remap.values()) & {s.get(q('styleId')) for s in styles}

def resolved(sid, tag):
    chain = []
    while sid:
        s = sources[sid]; chain.append(s)
        base = s.find(q('basedOn')); sid = base.get(q('val')) if base is not None else None
    d = ts.find('./' + q('docDefaults') + '/' + q(tag + 'Default') + '/' + q(tag))
    out = deepcopy(d) if d is not None else E.Element(q(tag))
    for s in reversed(chain):
        pr = s.find(q(tag))
        if pr is not None:
            for x in pr:
                for old in out.findall(x.tag): out.remove(old)
                out.append(deepcopy(x))
    return out

next_aid = max(int(x.get(q('abstractNumId'))) for x in num.findall(q('abstractNum'))) + 1
next_nid = max(int(x.get(q('numId'))) for x in num.findall(q('num'))) + 1
def clone_num(nid):
    global next_aid, next_nid
    n = num.xpath('./w:num[@w:numId="' + nid + '"]', namespaces=NS)[0]
    aid = n.find(q('abstractNumId')).get(q('val'))
    a = deepcopy(num.xpath('./w:abstractNum[@w:abstractNumId="' + aid + '"]', namespaces=NS)[0])
    a.set(q('abstractNumId'), str(next_aid)); next_aid += 1
    remove(a, ['nsid', 'tmpl'])
    # Abstract definitions precede concrete lists in the OOXML schema.
    first = num.find(q('num')); num.insert(num.index(first), a)
    nn = deepcopy(n); nn.set(q('numId'), str(next_nid)); next_nid += 1
    nn.find(q('abstractNumId')).set(q('val'), a.get(q('abstractNumId'))); num.append(nn)
    return nn.get(q('numId')), a

outline_id, outline = clone_num('7')
for source in customs:
    name = source.find(q('name')).get(q('val')); sid = source.get(q('styleId'))
    dest = deepcopy(source); dest.set(q('styleId'), remap[sid])
    remove(dest, ['basedOn', 'link', 'pPr', 'rPr', 'tblPr', 'trPr', 'tcPr'])
    for tag in ['pPr', 'rPr'] + (['tblPr', 'trPr', 'tcPr'] if source.get(q('type')) == 'table' else []):
        dest.append(resolved(sid, tag))
    for x in dest.findall(q('next')): x.set(q('val'), remap.get(x.get(q('val')), ids['TDC 본문 1']))
    if source.get(q('type')) == 'paragraph':
        p = dest.find(q('pPr')); r = dest.find(q('rPr'))
        child(p, 'outlineLvl', val=int(name[-1])-1 if name.startswith('TDC 제목 ') else 9)
        for tag in ['keepNext', 'keepLines', 'pageBreakBefore']:
            if p.find(q(tag)) is None: child(p, tag, val=0)
        for tag in ['b', 'bCs']:
            if r.find(q(tag)) is None: child(r, tag, val=0)
        child(p, 'widowControl', val=1)
        if name.startswith('TDC 제목 '):
            level = int(name[-1]); ref = p.find(q('numPr'))
            t_id = ref.find(q('numId')).get(q('val'))
            t_aid = tn.xpath('./w:num[@w:numId="'+t_id+'"]/w:abstractNumId/@w:val', namespaces=NS)[0]
            t_lv = tn.xpath('./w:abstractNum[@w:abstractNumId="'+t_aid+'"]/w:lvl[@w:ilvl="'+str(level-1)+'"]', namespaces=NS)[0]
            lv = outline.find('./'+q('lvl')+'[@'+q('ilvl')+'="'+str(level-1)+'"]')
            remove(lv, ['pStyle', 'suff', 'pPr', 'rPr'])
            child(lv, 'pStyle', val=ids[name]); child(lv, 'suff', val='space')
            for tag in ['pPr', 'rPr']:
                if t_lv.find(q(tag)) is not None: lv.append(deepcopy(t_lv.find(q(tag))))
            ref.find(q('numId')).set(q('val'), outline_id)
    for ref in dest.xpath('.//w:basedOn|.//w:next|.//w:link', namespaces=NS):
        if ref.get(q('val')) in remap: ref.set(q('val'), remap[ref.get(q('val'))])
    styles.append(dest)

stamp = datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
rid = max([int(v) for v in doc.xpath('//@w:id', namespaces=NS) if v.isdigit()] + [0]) + 1
def remember(pr, tag):
    global rid
    if pr.find(q(tag+'Change')) is not None: return
    old = deepcopy(pr)
    for x in list(old.iter()):
        if x is not old and E.QName(x).localname in ['ins', 'del', 'pPrChange', 'rPrChange', 'tblPrChange', 'tcPrChange', 'trPrChange']:
            x.getparent().remove(x)
    ch = E.SubElement(pr, q(tag+'Change'), {q('id'): str(rid), q('author'): '김은수', q('date'): stamp}); rid += 1
    ch.append(old)
def style(p, name, heading=False, table=False):
    pr = pp(p); remember(pr, 'pPr')
    remove(pr, ['pStyle', 'ind', 'spacing', 'tabs', 'jc', 'keepNext', 'keepLines', 'outlineLvl', 'snapToGrid', 'contextualSpacing', 'shd'])
    child(pr, 'pStyle', val=ids[name])
    if not heading:
        child(pr, 'outlineLvl', val=9); child(pr, 'keepNext', val=0); child(pr, 'keepLines', val=0)
    if table: child(pr, 'pageBreakBefore', val=0)
    for rp in p.xpath('.//w:rPr[not(ancestor::w:del) and not(ancestor::w:pPrChange) and not(ancestor::w:rPrChange)]', namespaces=NS):
        remember(rp, 'rPr'); remove(rp, ['rFonts', 'sz', 'szCs', 'vanish', 'webHidden'])
        if heading or name == 'TDC 표 머리행': remove(rp, ['b', 'bCs', 'color'])
        # Preserve purposeful red QCC markers and ordinary bold emphasis.
    if table: child(pr, 'jc', val='center' if name=='TDC 표 머리행' else 'left')

body = doc.find(q('body')); started = False; depth = 1; counts = Counter(); headings = []; bullet_cache = {}
for block in list(body):
    if block.tag == q('p'):
        t = txt(block); pr = pp(block); ps = pr.find(q('pStyle')); old_level = old_levels.get(ps.get(q('val')) if ps is not None else '')
        if not started:
            if t == '서론' and old_level == 1: started = True
            else: continue
        if not active(block): continue
        drawing = block.xpath('.//w:drawing[not(ancestor::w:del)]|.//w:pict[not(ancestor::w:del)]', namespaces=NS)
        if old_level and t:
            np = pr.find(q('numPr'))
            depth = int(np.find(q('ilvl')).get(q('val'))) + 1 if np is not None else old_level
            name = 'TDC 제목 '+str(depth); style(block, name, heading=True)
            if np is not None: np.find(q('numId')).set(q('val'), outline_id)
            else:
                # The source's 1.4.2 heading is typed, not an automatic list.
                child(pr, 'numPr'); child(pr.find(q('numPr')), 'numId', val=0)
            headings.append({'text':t, 'level':depth})
        elif drawing:
            name = 'TDC 그림 '+str(depth); style(block, name); child(pr, 'keepNext', val=1)
        elif re.match(r'<?\s*그림\s+\d', t):
            name = 'TDC 그림 캡션 '+str(depth); style(block, name)
        elif re.match(r'표\s+(?:\d+|SOUP-\d+)\.', t):
            name = 'TDC 표 캡션 '+str(depth); style(block, name)
        elif t.startswith('[자료 추가 위치]'):
            name = 'TDC 주석 '+str(depth); style(block, name)
        else:
            name = 'TDC 본문 '+str(depth); style(block, name)
        np = pr.find(q('numPr'))
        if not old_level and np is not None and t:
            origid = np.find(q('numId')).get(q('val')); ilvl = np.find(q('ilvl')).get(q('val'))
            key = (origid, ilvl, depth)
            if key not in bullet_cache:
                nid, a = clone_num(origid); lv = a.find('./'+q('lvl')+'[@'+q('ilvl')+'="'+ilvl+'"]')
                remove(lv, ['pStyle', 'pPr', 'rPr', 'suff']); child(lv, 'suff', val='space')
                bp = E.SubElement(lv, q('pPr')); anchor = round((depth-1)*.35*1440/2.54)
                child(bp, 'ind', left=anchor, hanging=0, right=0)
                rp = E.SubElement(lv, q('rPr')); child(rp,'rFonts',ascii='맑은 고딕',hAnsi='맑은 고딕',eastAsia='맑은 고딕');child(rp,'sz',val=20)
                bullet_cache[key] = nid
            np.find(q('numId')).set(q('val'), bullet_cache[key])
        if not t and not drawing:
            # Keep visible, ordinary-size spacers. Remove bullet-only empty lines.
            if np is not None: np.find(q('numId')).set(q('val'),'0')
            child(pr,'spacing',before=0,after=0,line=300,lineRule='auto')
        counts[name] += 1
    elif started and block.tag == q('tbl'):
        pr = block.find(q('tblPr')); remember(pr,'tblPr')
        remove(pr,['tblStyle','tblBorders','tblCellMar','shd','tblInd'])
        child(pr,'tblStyle',val=ids['TDC 표 '+str(depth)])
        child(pr,'tblInd',w=round((depth-1)*.35*1440/2.54)+100,type='dxa')
        child(pr,'jc',val='left')
        # Fit the existing grid proportionally to the body-level available width.
        available = 11906-2268-round((depth-1)*.35*1440/2.54)
        grid = block.find(q('tblGrid')); widths=[int(x.get(q('w'))) for x in grid]
        total=sum(widths); new=[round(w*available/total) for w in widths];new[-1]+=available-sum(new)
        for x,w in zip(grid,new): x.set(q('w'),str(w))
        child(pr,'tblW',w=available,type='dxa'); child(pr,'tblLayout',type='fixed')
        rows=block.findall(q('tr'))
        for ri,row in enumerate(rows):
            is_header=ri==0 or row.find('./'+q('trPr')+'/'+q('tblHeader')) is not None
            tr = child(row,'trPr');remember(tr,'trPr');remove(tr,['trHeight']);child(tr,'cantSplit')
            if ri==0:child(tr,'tblHeader')
            grid_index=0
            for ci,tc in enumerate(row.findall(q('tc'))):
                cp=child(tc,'tcPr');remember(cp,'tcPr');remove(cp,['tcBorders','tcMar','shd']);child(cp,'vAlign',val='center')
                span=cp.find(q('gridSpan'));span=int(span.get(q('val'))) if span is not None else 1
                child(cp,'tcW',w=sum(new[grid_index:grid_index+span]),type='dxa');grid_index+=span
                if is_header or ci==0:child(cp,'shd',val='clear',color='auto',fill='F5F5F5')
                for p in tc.findall(q('p')):
                    if active(p):
                        name='TDC 표 머리행' if is_header else 'TDC 표 항목' if ci==0 else 'TDC 표 본문'
                        style(p,name,table=True)
                        child(pp(p),'jc',val='center' if is_header or ci==0 else 'left')
                        if len(rows)<=6 and ri<len(rows)-1:child(pp(p),'keepNext',val=1)
        counts['TDC 표 '+str(depth)]+=1
        following=block.getnext()
        if following is None or following.tag!=q('p') or txt(following):
            spacer=E.Element(q('p'));sp=pp(spacer);child(sp,'pStyle',val=ids['TDC 본문 '+str(depth)])
            child(sp,'outlineLvl',val=9);child(sp,'spacing',before=0,after=0,line=300,lineRule='auto')
            child(child(sp,'rPr'),'ins',id=rid,author='김은수',date=stamp);rid+=1
            body.insert(body.index(block)+1,spacer)

# Resolve oversized clusters of empty source paragraphs without hidden fonts.
deleted_spacers=[]; previous_empty=False; started=False
for p in list(body):
    if p.tag!=q('p'): previous_empty=False;continue
    if txt(p)=='서론':started=True
    if not started or not active(p):continue
    is_empty=not txt(p) and not p.xpath('.//w:drawing|.//w:pict|.//w:br|.//w:sectPr',namespaces=NS)
    prev=p.getprevious()
    while prev is not None and prev.tag==q('p') and not active(prev):prev=prev.getprevious()
    preserve_spacer=prev is not None and (prev.tag==q('tbl') or (prev.tag==q('p') and (prev.xpath('.//w:drawing[not(ancestor::w:del)]',namespaces=NS) or re.match(r'<?그림\s+\d',txt(prev)))))
    if is_empty and (previous_empty or not preserve_spacer):
        mark=child(pp(p),'rPr');child(mark,'del',id=rid,author='김은수',date=stamp);rid+=1;deleted_spacers.append(body.index(p))
    else: previous_empty=is_empty

# TOC is body navigation, not the protected cover/header. Keep its indent/tabs
# and field machinery; use modest spacing so one trailing entry is not stranded.
for toc in body.findall(q('sdt')):
    toc_ps=toc.xpath('.//w:p',namespaces=NS)
    for p in toc_ps:
        pr=pp(p);ps=pr.find(q('pStyle'))
        if ps is None:continue
        sid=ps.get(q('val')); source_style=next((s for s in styles if s.get(q('styleId'))==sid),None)
        if source_style is None or source_style.find(q('name')).get(q('val'))=='TOC Heading':continue
        spr=child(source_style,'pPr');child(spr,'spacing',before=0,after=0,line=276,lineRule='auto')
        srp=child(source_style,'rPr');child(srp,'sz',val=20);child(srp,'szCs',val=20)
        child(srp,'rFonts',ascii='맑은 고딕',hAnsi='맑은 고딕',eastAsia='맑은 고딕')
        remember(pr,'pPr');child(pr,'spacing',before=0,after=0,line=276,lineRule='auto')
    # Blank padding between the TOC and chapter 1 is neither a cover element
    # nor a real section break. Track its removal; break on the actual heading.
    p=toc.getnext()
    while p is not None and p.tag==q('p') and not txt(p) and not p.xpath('.//w:sectPr',namespaces=NS):
        if active(p):
            remember(pp(p),'pPr');child(pp(p),'pageBreakBefore',val=0)
            for r in list(p.findall(q('r'))):
                if r.xpath('.//w:br',namespaces=NS):
                    where=p.index(r);p.remove(r)
                    deletion=E.Element(q('del'),{q('id'):str(rid),q('author'):'김은수',q('date'):stamp});rid+=1
                    deletion.append(r);p.insert(where,deletion)
            mark=child(pp(p),'rPr');child(mark,'del',id=rid,author='김은수',date=stamp);rid+=1;deleted_spacers.append(body.index(p))
        p=p.getnext()
    if p is not None and txt(p)=='서론':
        remember(pp(p),'pPr');child(pp(p),'pageBreakBefore',val=1)

settings=xml(parts,'word/settings.xml');remove(settings,['updateStyles']);child(settings,'trackRevisions')
for n,r in [('word/document.xml',doc),('word/styles.xml',styles),('word/numbering.xml',num),('word/settings.xml',settings)]:parts[n]=serialize(r)
save(QA/'candidate.docx',parts)
for path in ['.//w:t/text()','.//w:delText/text()','.//w:instrText/text()']:
    assert old_doc.xpath(path,namespaces=NS)==doc.xpath(path,namespaces=NS),path
assert len(headings)==43 if args.kind=='sip' else len(headings)>=30
for n,data in read(before).items():
    if n not in ['word/document.xml','word/styles.xml','word/numbering.xml','word/settings.xml']:assert parts[n]==data,n
report={'before_sha256':sha(before),'template_sha256':sha(TPL),'style_count':len(customs),'styles':ids,'assigned':dict(counts),'headings':headings,'deleted_empty_spacers':deleted_spacers,'outline_id':outline_id,'candidate_sha256':sha(QA/'candidate.docx')}
(QA/'apply.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps({'styles':len(customs),'headings':len(headings),'assigned':dict(counts),'empty_spacers_removed':len(deleted_spacers)},ensure_ascii=False))
