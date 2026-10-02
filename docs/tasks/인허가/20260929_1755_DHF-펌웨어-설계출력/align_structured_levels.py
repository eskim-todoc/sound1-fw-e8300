"""Align final logical blocks; deleted paragraph marks can merge tracked XML fragments."""
from pathlib import Path
from copy import deepcopy
from datetime import datetime,timezone
import json,zipfile,re
from lxml import etree as E
Q=Path(__file__).resolve().parent/'qa/design-readability/structured-units'
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main';q=lambda t:'{'+W+'}'+t
N={'w':W,'wp':'http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing','m':'http://schemas.openxmlformats.org/officeDocument/2006/math','a':'http://schemas.openxmlformats.org/drawingml/2006/main'}
def read(p):
    with zipfile.ZipFile(p) as z:return {n:z.read(n) for n in z.namelist()}
def text(p):return ''.join(p.xpath('.//w:t[not(ancestor::w:del)]/text()',namespaces=N))
def child(p,t,**a):
    e=p.find(q(t))
    if e is None:e=E.SubElement(p,q(t))
    for k,v in a.items():e.set(q(k),str(v))
    return e
parts=read(Q/'candidate.docx');raw=E.fromstring(parts['word/document.xml']);accepted=E.fromstring(read(Q/'candidate-accepted.docx')['word/document.xml'])
styles=E.fromstring(parts['word/styles.xml']);ns={s.get(q('styleId')):s.find(q('name')).get(q('val')) for s in styles.findall(q('style'))};ids={v:k for k,v in ns.items()}
for s in styles.findall(q('style')):
    if s.find(q('name')).get(q('val'))=='TDC 표 머리행':child(child(s,'pPr'),'keepNext',val=1)
def name(p):
    st=p.find('./'+q('pPr')+'/'+q('pStyle'));return ns.get(st.get(q('val')) if st is not None else '','')
depth=1;started=False;paras={};tables={};pics=[];maths={}
for b in accepted.find(q('body')):
    if b.tag==q('p'):
        tx=text(b);st=name(b)
        if tx=='서론':started=True
        if not started:continue
        if re.fullmatch('TDC 제목 [1-6]',st):depth=int(st[-1])
        elif tx:paras[tx]=depth
        if b.xpath('.//wp:inline',namespaces=N):
            nxt=b.getnext();caption=text(nxt) if nxt is not None else ''
            pics.append({'level':depth,'flow':'순서도' in caption and 'SU-ID-' in caption,'caption':caption})
        if b.xpath('.//m:oMath|.//m:oMathPara',namespaces=N):maths[''.join(b.xpath('.//m:t/text()',namespaces=N))]=depth
    elif started and b.tag==q('tbl'):tables[text(b)]=depth
rid=max([int(v) for v in raw.xpath('//@w:id',namespaces=N) if v.isdigit()]+[0])+1
stamp=datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
def remember(pr,tag):
    global rid
    if pr.find(q(tag+'Change')) is not None:return
    old=deepcopy(pr)
    for x in list(old.iter()):
        if x is not old and x.tag in {q(t) for t in ['ins','del','pPrChange','rPrChange','tblPrChange']} and x.getparent() is not None:x.getparent().remove(x)
    ch=E.SubElement(pr,q(tag+'Change'),{q('id'):str(rid),q('author'):'김은수',q('date'):stamp});rid+=1;ch.append(old)
changed=[];picindex=0;mathindex=0;started=False
for b in raw.find(q('body')):
    if b.tag==q('p'):
        tx=text(b);st=name(b)
        if tx=='서론':started=True
        if not started:continue
        if b.xpath('.//wp:inline[not(ancestor::w:del)]',namespaces=N):
            info=pics[picindex];target='TDC 그림 '+str(info['level']);picindex+=1
            for inline in b.xpath('.//wp:inline[not(ancestor::w:del)]',namespaces=N):
                ext=inline.find('{'+N['wp']+'}extent');width=int(ext.get('cx'))/360000;height=int(ext.get('cy'))/360000
                wanted=min(width,16.6-(info['level']-1)*.25)
                if info['flow']:wanted=min(wanted,15.4 if height<12 else 15.6 if height<17 else 15.8)
                ratio=wanted/width
                for e in [ext]+inline.xpath('.//a:xfrm/a:ext',namespaces=N):e.set('cx',str(round(int(e.get('cx'))*ratio)));e.set('cy',str(round(int(e.get('cy'))*ratio)))
        elif b.xpath('.//m:oMath[not(ancestor::w:del)]|.//m:oMathPara[not(ancestor::w:del)]',namespaces=N):
            mt=''.join(b.xpath('.//m:t[not(ancestor::w:del)]/text()',namespaces=N))
            if mt not in maths:continue
            target='TDC 수식 '+str(maths[mt]);mathindex+=1
        elif tx in paras and re.fullmatch(r'TDC (본문|표 캡션|주석|그림 캡션) [1-6]',st):target=st[:-1]+str(paras[tx])
        else:continue
        if target!=st:
            pp=child(b,'pPr');remember(pp,'pPr');child(pp,'pStyle',val=ids[target]);changed.append({'text':tx[:90],'before':st,'after':target})
    elif started and b.tag==q('tbl') and text(b) in tables:
        level=tables[text(b)];tp=child(b,'tblPr');remember(tp,'tblPr');child(tp,'tblStyle',val=ids['TDC 표 '+str(level)]);child(tp,'tblInd',w=round((level-1)*.25*1440/2.54)+100,type='dxa')
        for row in b.findall(q('tr')):
            if row.find('./'+q('trPr')+'/'+q('del')) is not None:continue
            for p in row.findall('.//'+q('p')):
                if name(p)=='TDC 표 머리행':
                    pp=child(p,'pPr');remember(pp,'pPr');child(pp,'keepNext',val=1)
            break
parts['word/document.xml']=E.tostring(raw,xml_declaration=True,encoding='UTF-8',standalone=True)
parts['word/styles.xml']=E.tostring(styles,xml_declaration=True,encoding='UTF-8',standalone=True)
with zipfile.ZipFile(Q/'candidate.docx','w',zipfile.ZIP_DEFLATED) as z:
    for n,v in parts.items():z.writestr(n,v)
(Q/'alignment.json').write_text(json.dumps({'changed':changed,'pictures':picindex,'math_paragraphs':mathindex},ensure_ascii=False,indent=2),encoding='utf8')
print('Aligned paragraphs:',len(changed),'pictures:',picindex)
