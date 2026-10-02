"""Import revised styles and bookmark current unit fields for native redlining."""
from pathlib import Path
from copy import deepcopy
from datetime import datetime,timezone
import zipfile,re,json,hashlib
from lxml import etree as E
T=Path(__file__).resolve().parent;ROOT=T.parents[3]
QA=T/'qa/design-readability/structured-units';QA.mkdir(parents=True,exist_ok=True)
LIVE=ROOT/'docs/참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어/SDDD-FW-TD24A 소프트웨어 상세 설계서.docx'
TPL=Path('E:/workspace/rules-for-codex/지침/양식/Word/TDC 공통 스타일.dotx')
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main';q=lambda t:'{'+W+'}'+t
NS={'w':W,'wp':'http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing','a':'http://schemas.openxmlformats.org/drawingml/2006/main'}
def child(p,t,**a):
    e=p.find(q(t))
    if e is None:e=E.SubElement(p,q(t))
    for k,v in a.items():e.set(q(k),str(v))
    return e
def read(p):
    with zipfile.ZipFile(p) as z:return {n:z.read(n) for n in z.namelist()}
def write(p,parts):
    with zipfile.ZipFile(p,'w',zipfile.ZIP_DEFLATED) as z:
        for n,v in parts.items():z.writestr(n,v)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def txt(p):return ''.join(p.xpath('.//w:t[not(ancestor::w:del)]/text()',namespaces=NS))
baseline=QA/'before.docx'
if not baseline.exists():baseline.write_bytes(LIVE.read_bytes())
assert sha(LIVE)==sha(baseline),'External edit'
parts=read(baseline);tp=read(TPL);doc=E.fromstring(parts['word/document.xml']);styles=E.fromstring(parts['word/styles.xml']);ts=E.fromstring(tp['word/styles.xml']);tn=E.fromstring(tp['word/numbering.xml']);num=E.fromstring(parts['word/numbering.xml'])
current={s.find(q('name')).get(q('val')):s for s in styles.findall(q('style'))}
oldnames={s.get(q('styleId')):name for name,s in current.items()}
source={s.get(q('styleId')):s for s in ts.findall(q('style'))}
custom=[s for s in ts.findall(q('style')) if s.find(q('name')).get(q('val')).startswith('TDC ')]
ids={s.find(q('name')).get(q('val')):(current[s.find(q('name')).get(q('val'))].get(q('styleId')) if s.find(q('name')).get(q('val')) in current else 'TDCNew'+str(i)) for i,s in enumerate(custom)}
remap={s.get(q('styleId')):ids[s.find(q('name')).get(q('val'))] for s in custom}
def resolved(sid,tag):
    chain=[]
    while sid:
        s=source[sid];chain.append(s);b=s.find(q('basedOn'));sid=b.get(q('val')) if b is not None else None
    default=ts.find('./'+q('docDefaults')+'/'+q(tag+'Default')+'/'+q(tag))
    result=deepcopy(default) if default is not None else E.Element(q(tag))
    for s in reversed(chain):
        pr=s.find(q(tag))
        if pr is not None:
            for x in pr:
                old=result.find(x.tag)
                if old is not None:result.remove(old)
                result.append(deepcopy(x))
    return result
for s in custom:
    name=s.find(q('name')).get(q('val'));n=deepcopy(s);n.set(q('styleId'),ids[name])
    for tag in ['basedOn','link','pPr','rPr','tblPr','trPr','tcPr']:
        for x in n.findall(q(tag)):n.remove(x)
    for tag in ['pPr','rPr']+(['tblPr','trPr','tcPr'] if s.get(q('type'))=='table' else []):n.append(resolved(s.get(q('styleId')),tag))
    for x in n.findall(q('next')):x.set(q('val'),remap.get(x.get(q('val')),ids['TDC 본문 1']))
    if s.get(q('type'))=='paragraph':
        pp=child(n,'pPr');child(pp,'outlineLvl',val=9);child(pp,'keepNext',val=1 if name.startswith('TDC 제목') or name.startswith('TDC 표 캡션') or name.startswith('TDC 그림 ') and '캡션' not in name else 0)
        child(pp,'keepLines',val=int(name.startswith('TDC 제목') or '캡션' in name));child(pp,'pageBreakBefore',val=0)
        if name.startswith('TDC 제목 '):
            i=int(name[-1]);child(pp,'outlineLvl',val=i-1);np=child(pp,'numPr');child(np,'numId',val=7);child(np,'ilvl',val=i-1)
    if name in current:styles.replace(current[name],n)
    else:styles.append(n)
# Keep numbering IDs/text/restart, update only positions and linked style.
aid=num.xpath('./w:num[@w:numId="7"]/w:abstractNumId/@w:val',namespaces=NS)[0]
abstract=num.xpath('./w:abstractNum[@w:abstractNumId="'+aid+'"]',namespaces=NS)[0]
for lv in abstract.findall(q('lvl')):
    i=int(lv.get(q('ilvl')))+1
    if i>6:continue
    child(lv,'pStyle',val=ids[f'TDC 제목 {i}'])
    sourcelevel=tn.xpath('.//w:lvl[w:pStyle/@w:val="'+next(s.get(q('styleId')) for s in custom if s.find(q('name')).get(q('val'))==f'TDC 제목 {i}')+'"]',namespaces=NS)[0]
    for tag in ['pPr','rPr']:
        for old in lv.findall(q(tag)):lv.remove(old)
        pr=sourcelevel.find(q(tag))
        if pr is not None:lv.append(deepcopy(pr))
rid=max([int(v) for v in doc.xpath('//@w:id',namespaces=NS) if v.isdigit()]+[0])+1
stamp=datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
def remember(pr,tag):
    global rid
    if pr.find(q(tag+'Change')) is not None:return
    old=deepcopy(pr)
    for x in list(old.iter()):
        if x is not old and x.tag in {q(v) for v in ['ins','del','pPrChange','rPrChange','tblPrChange','tcPrChange','trPrChange']} and x.getparent() is not None:x.getparent().remove(x)
    ch=E.SubElement(pr,q(tag+'Change'),{q('id'):str(rid),q('author'):'김은수',q('date'):stamp});rid+=1;ch.append(old)
def assign(p,name):
    pp=child(p,'pPr');remember(pp,'pPr');child(pp,'pStyle',val=ids[name])
    for tag in ['ind','spacing','tabs','jc','contextualSpacing','keepNext','keepLines']:
        for x in pp.findall(q(tag)):pp.remove(x)
    if not name.startswith('TDC 제목'):child(pp,'outlineLvl',val=9)
expected=json.loads((QA/'units-before.json').read_text(encoding='utf8'))
body=doc.find(q('body'));started=False;depth=1;unit=None;groups={};images=[];unit_index=0
for block in list(body):
    if block.tag==q('p'):
        text=txt(block);pp=child(block,'pPr');ps=pp.find(q('pStyle'));name=oldnames.get(ps.get(q('val')) if ps is not None else '')
        if text=='서론' and name=='TDC 제목 1':started=True
        if not started:continue
        if name and re.fullmatch('TDC 제목 [1-6]',name):
            depth=int(name[-1]);assign(block,name)
        elif name and name.startswith('TDC 본문'):assign(block,'TDC 본문 '+str(depth))
        elif name in ['TDC 표 캡션','TDC 주석','TDC 그림','TDC 그림 캡션','TDC 수식']:assign(block,name+' '+str(depth))
        if text.startswith('목적 :'):
            unit=expected[unit_index]['id'];unit_index+=1;groups[unit]=[];depth=4
        if unit and re.match('목적 :|입력 :|출력 :|처리 :|동작 조건 :|인터페이스|구현 위치 :|입출력 예 :',text):groups[unit].append(block)
        drawings=block.xpath('.//wp:inline[not(ancestor::w:del)]',namespaces=NS)
        if drawings:
            for drawing in drawings:
                ext=drawing.find('{'+NS['wp']+'}extent');width=int(ext.get('cx'))/360000;height=int(ext.get('cy'))/360000
                nxt=block.getnext()
                flow=bool(nxt is not None and 'SU-ID-' in txt(nxt) and '순서도' in txt(nxt))
                # Preserve image data. Scale by individual geometry, with the small
                # branch text remaining at least 9pt (existing 9.35pt at 16cm).
                available=16.6-(depth-1)*.25
                wanted=min(width,available)
                if flow:wanted=max(15.4,min(wanted,15.4 if height<12 else 15.6 if height<17 else 15.8))
                ratio=wanted/width
                for e in [ext]+drawing.xpath('.//a:xfrm/a:ext',namespaces=NS):e.set('cx',str(round(int(e.get('cx'))*ratio)));e.set('cy',str(round(int(e.get('cy'))*ratio)))
                # Short/medium diagrams flow after their unit rather than all
                # starting a forced page. Tall diagrams use a visible image paragraph.
                if flow:child(pp,'pageBreakBefore',val=int(height*ratio>17.5))
                images.append({'unit':unit,'width_cm':round(wanted,2),'height_cm':round(height*ratio,2),'old_height_cm':round(height,2),'new_page':flow and height*ratio>17.5})
        if not text and not drawings and pp.find(q('sectPr')) is None and not block.xpath('.//w:delText',namespaces=NS):
            child(pp,'spacing',before=0,after=0,line=240,lineRule='auto')
    elif started and block.tag==q('tbl'):
        pp=child(block,'tblPr');remember(pp,'tblPr');child(pp,'tblStyle',val=ids['TDC 표 '+str(depth)]);child(pp,'jc',val='left');child(pp,'tblInd',w=round((depth-1)*.25*1440/2.54)+100,type='dxa')
        available=round((16.6-(depth-1)*.25)*1440/2.54)
        grid=block.find(q('tblGrid'));cols=grid.findall(q('gridCol'));oldtotal=sum(int(x.get(q('w'))) for x in cols)
        if oldtotal>available:
            for c in cols:c.set(q('w'),str(round(int(c.get(q('w')))*available/oldtotal)))
            for tcw in block.xpath('./w:tr/w:tc/w:tcPr/w:tcW[@w:type="dxa"]',namespaces=NS):tcw.set(q('w'),str(round(int(tcw.get(q('w')))*available/oldtotal)))
        child(pp,'tblW',w=min(oldtotal,available),type='dxa')

bookid=max([int(x) for x in doc.xpath('.//w:bookmarkStart/@w:id',namespaces=NS) if x.isdigit()]+[0])+1
manifest=[]
for uid,ps in groups.items():
    if not ps:continue
    name='UnitFields_'+uid.replace('-','_');first,last=ps[0],ps[-1]
    start=E.Element(q('bookmarkStart'),{q('id'):str(bookid),q('name'):name});end=E.Element(q('bookmarkEnd'),{q('id'):str(bookid)});bookid+=1
    first.insert(1,start);last.append(end)
    manifest.append({'unit':uid,'bookmark':name,'fields':[txt(p) for p in ps]})
expected=json.loads((QA/'units-before.json').read_text(encoding='utf8'))
assert len(manifest)==115,(len(manifest),[u['id'] for u in expected if u['id'] not in groups])
for n,r in [('word/document.xml',doc),('word/styles.xml',styles),('word/numbering.xml',num)]:parts[n]=E.tostring(r,xml_declaration=True,encoding='UTF-8',standalone=True)
write(QA/'styled.docx',parts)
(QA/'prepare.json').write_text(json.dumps({'styles':ids,'bookmarks':manifest,'images':images,'baseline_sha256':sha(baseline),'template_sha256':sha(TPL)},ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps({'units':len(manifest),'images':len(images),'styles':len(ids)},ensure_ascii=False))
