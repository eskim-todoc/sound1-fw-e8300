"""Inspect and apply the author-requested QCC presentation cleanup only."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
from lxml import etree as E
import copy, json, hashlib, sys, re
from datetime import datetime, timezone

TASK = Path(__file__).resolve().parent
QA = TASK / 'qa/sad-qcc-cleanup-20261016'
ROOT = Path('G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF/03_설계 출력/SW/외부기')
TARGET = ROOT / '[E8300-QCC] SAD-FW-TD24A 소프트웨어 아키텍쳐 기술서.docx'
W = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'
N = {'w': W}
q = lambda s: '{'+W+'}'+s

def visible(e):
    return not e.xpath('ancestor::w:del|ancestor::w:moveFrom|ancestor-or-self::w:tr[w:trPr/w:del]', namespaces=N)

def text(e):
    return ''.join((n.text or '') if n.tag == q('t') else '\n' for n in e.iter() if n.tag in [q('t'),q('br')] and visible(n))

def load(p):
    with ZipFile(p) as z:
        return {i.filename:z.read(i) for i in z.infolist()}

def inspect():
    QA.mkdir(parents=True, exist_ok=True)
    parts=load(TARGET); root=E.fromstring(parts['word/document.xml'])
    ps=root.xpath('//w:body//w:p', namespaces=N)
    data=[]
    for i,p in enumerate(ps):
        if not visible(p): continue
        t=text(p)
        data.append({'index':i,'text':t,'style':p.xpath('string(w:pPr/w:pStyle/@w:val)',namespaces=N),
                     'runs': [{'text':text(r), 'color':r.xpath('string(w:rPr/w:color/@w:val)',namespaces=N),
                               'size':r.xpath('string(w:rPr/w:sz/@w:val)',namespaces=N),
                               'underline':r.xpath('string(w:rPr/w:u/@w:val)',namespaces=N),
                               'parent':E.QName(r.getparent()).localname}
                              for r in p.xpath('.//w:r', namespaces=N) if visible(r) and text(r)]})
    (QA/'inspection.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
    for x in data:
        if 264 <= x['index'] <=384 or x['index']>=1380:
            print(x['index'],x['style'],x['text'])
    for p in (ROOT/'서석일_작성').glob('QCC_SAD*.docx'):
        rt=E.fromstring(load(p)['word/document.xml'])
        s='\n'.join(text(p) for p in rt.xpath('//w:body//w:p',namespaces=N) if visible(p))
        (QA/'source-sad.txt').write_text(s,encoding='utf-8')
        print('\nQCC_SOURCE\n'+s)

def edit():
    QA.mkdir(parents=True, exist_ok=True)
    before_file=QA/'before.docx'
    if not before_file.exists(): before_file.write_bytes(TARGET.read_bytes())
    raw=before_file.read_bytes()
    allowed={hashlib.sha256(raw).hexdigest()}
    published=QA/'publication.json'
    if published.exists():allowed.add(json.loads(published.read_text(encoding='utf8'))['sha256_candidate'])
    assert hashlib.sha256(TARGET.read_bytes()).hexdigest() in allowed, 'Target changed after snapshot'
    parts=load(before_file); before=E.fromstring(parts['word/document.xml'])
    root=copy.deepcopy(before); ps=root.xpath('//w:body//w:p',namespaces=N)
    old_ids=set(root.xpath('//@w:id',namespaces=N))
    seq=max(int(x) for x in old_ids if x.isdigit())+1
    first=seq
    stamp=datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
    colored=[]; replacements=[]
    def revision(tag):
        nonlocal seq
        e=E.Element(q(tag),{q('id'):str(seq),q('author'):'김은수',q('date'):stamp});seq+=1
        return e
    def props(r):
        p=r.find(q('rPr'))
        if p is None:p=E.Element(q('rPr'));r.insert(0,p)
        return p
    def red(r):
        p=props(r)
        if p.xpath('w:color[@w:val="C00000"]',namespaces=N):return
        assert p.find(q('rPrChange')) is None
        old=copy.deepcopy(p)
        c=p.find(q('color'))
        if c is None:c=E.SubElement(p,q('color'))
        c.attrib.clear();c.set(q('val'),'C00000')
        change=revision('rPrChange');change.append(old);p.append(change)
    def color_spans(p,spans):
        cursor=0
        for r in list(p.xpath('.//w:r',namespaces=N)):
            if not visible(r):continue
            rt=text(r); start=cursor;cursor+=len(rt)
            hits=[(max(a,start)-start,min(b,cursor)-start) for a,b in spans if a<cursor and b>start]
            if not hits:continue
            # Break/text runs are split without changing the character sequence.
            cuts=sorted({0,len(rt),*(v for pair in hits for v in pair)})
            assert all(c.tag in [q('rPr'),q('t'),q('br'),q('lastRenderedPageBreak')] for c in r)
            parent=r.getparent(); at=parent.index(r);parent.remove(r)
            for a,b in zip(cuts,cuts[1:]):
                if a==b:continue
                nr=E.Element(q('r'),r.attrib)
                rp=r.find(q('rPr'))
                if rp is not None:nr.append(copy.deepcopy(rp))
                if a==0:
                    for extra in r.findall(q('lastRenderedPageBreak')):nr.append(copy.deepcopy(extra))
                for j,line in enumerate(rt[a:b].split('\n')):
                    if j:E.SubElement(nr,q('br'))
                    if line:
                        t=E.SubElement(nr,q('t'));t.set('{http://www.w3.org/XML/1998/namespace}space','preserve');t.text=line
                if any(x<=a and b<=y for x,y in hits):red(nr)
                parent.insert(at,nr);at+=1
        colored.append(ps.index(p))
    def whole(i):
        p=ps[i];color_spans(p,[(0,len(text(p)))])
    def phrase(i,s):
        p=ps[i];t=text(p);a=t.index(s);color_spans(p,[(a,a+len(s))])
    phrase(200,'DMIC2는 QCC에도 연결되어 전화 통화 상향 입력으로 사용한다.')
    phrase(265,text(ps[265]).split(' E8300은')[0])
    for i in [273,274,275,278,320,323,325,327,329,331,334,336,338,340,343,345,347,349,351,354,356,358,383,1418]:whole(i)
    phrase(372,'QCC BLE Data 메시지는 type 1바이트와 data 0~19바이트로 총 1~20바이트이다.')
    phrase(375,'DMIC2는 QCC에도 연결되며 전화 통화 시 QCC가 수음한 음성을 통화 상대에게 송신한다. QCC의 Classic 수신 오디오는 I2S 24bit/16kHz로 CFX에 전달한다.')
    trace=root.xpath('//w:body/w:tbl',namespaces=N)[8]
    for p in trace.xpath('.//w:p',namespaces=N):
        if not visible(p):continue
        spans=[m.span() for m in re.finditer(r'(?:SI|SU)-ID-QCC-\d{3}\n[^\n]+',text(p))]
        if spans:color_spans(p,spans)
    # Move the outstanding marker after the already supplied connection statement.
    for i in [218,381,1208,1392]:
        p=ps[i];r=next(r for r in p.xpath('.//w:r',namespaces=N) if visible(r) and text(r).startswith('[QCC 자료 입력]'))
        oldtext=text(r)
        if i==218:
            done='DMIC2의 두 칩 입력 연결은 확인되었다.'
            rest=oldtext.replace('[QCC 자료 입력] ','',1).replace('DMIC2의 두 칩 입력 연결 사실과 구현/시험 근거의 확인 상태는 구분한다.','구현/시험 근거의 확인 상태는 구분한다.')
        else:
            done='DMIC2의 QCC 입력 연결은 확인되었다.'
            rest=oldtext.split(done+' ',1)[1]
        parent=r.getparent();assert parent.tag==q('ins') and parent.getparent()is p
        d=revision('del');parent.replace(r,d)
        for t in r.iter(q('t')):t.tag=q('delText')
        d.append(r)
        ins=revision('ins');p.insert(p.index(parent)+1,ins)
        a=E.SubElement(ins,q('r'));ap=E.SubElement(a,q('rPr'))
        for tag,val in [('color','C00000'),('sz','20'),('szCs','20'),('u','none')]:E.SubElement(ap,q(tag),{q('val'):val})
        E.SubElement(a,q('t')).text=done
        b=E.SubElement(ins,q('r'));b.append(copy.deepcopy(r.find(q('rPr'))));E.SubElement(b,q('br'))
        E.SubElement(b,q('t')).text='[QCC 자료 입력] '+rest
        replacements.append({'paragraph':i,'before':oldtext,'after':done+'\n[QCC 자료 입력] '+rest})
    # Delete the redundant QCC-only reverse table but retain its rejectable history.
    table=root.xpath('//w:body/w:tbl',namespaces=N)[9]
    assert '상위 요구사항 ID' in text(table) and 'SU-ID-QCC-014' in text(table)
    table_units=set(re.findall(r'SU-ID-QCC-\d{3}',text(table)))
    assert table_units <= set(re.findall(r'SU-ID-QCC-\d{3}',text(trace)))
    for row in table.findall(q('tr')):
        pr=row.find(q('trPr'))
        if pr is None:pr=E.Element(q('trPr'));row.insert(0,pr)
        pr.append(revision('del'))
    cap=ps[1294];assert text(cap)=='표 3. 소프트웨어 유닛과 상위 요구사항 연결'
    for r in list(cap.xpath('.//w:r',namespaces=N)):
        if not visible(r):continue
        parent=r.getparent();d=revision('del');parent.replace(r,d)
        for t in r.iter(q('t')):t.tag=q('delText')
        for t in r.iter(q('instrText')):t.tag=q('delInstrText')
        d.append(r)
    pr=cap.find(q('pPr'));rp=pr.find(q('rPr'))
    if rp is None:rp=E.SubElement(pr,q('rPr'))
    rp.append(revision('del'))
    # SEQ field result is a display cache, not authored source text.
    result=next(t for t in ps[1400].iter(q('t')) if visible(t) and t.text=='4')
    result.text='3'
    # New revisions can be rejected without losing prior changes or visible content.
    reject=copy.deepcopy(root)
    for e in reversed(list(reject.iter())):
        if not e.get(q('id'),'').isdigit() or int(e.get(q('id')))<first:continue
        par=e.getparent()
        if par is None:continue
        if e.tag==q('ins'):par.remove(e)
        elif e.tag==q('del'):
            if par.tag in [q('trPr'),q('rPr')]:par.remove(e)
            else:
                at=par.index(e)
                for child in list(e):
                    for t in child.iter(q('delText')):t.tag=q('t')
                    for t in child.iter(q('delInstrText')):t.tag=q('instrText')
                    e.remove(child);par.insert(at,child);at+=1
                par.remove(e)
        elif e.tag==q('rPrChange'):
            old=e.find(q('rPr'));run=par.getparent();run.replace(par,copy.deepcopy(old))
    # Only the cached following table number differs on rejecting the patch.
    rps=reject.xpath('//w:body//w:p',namespaces=N)
    rt=next(t for t in rps[1400].iter(q('t')) if visible(t) and t.text=='3');rt.text='4'
    before_text=[text(p) for p in before.xpath('//w:body//w:p',namespaces=N) if visible(p)]
    reject_text=[text(p) for p in reject.xpath('//w:body//w:p',namespaces=N) if visible(p)]
    assert before_text==reject_text
    assert old_ids <= set(root.xpath('//@w:id',namespaces=N))
    before_revs={e.get(q('id')):(e.tag,e.get(q('author')),e.get(q('date'))) for e in before.iter() if e.get(q('id')) and E.QName(e).localname in ['ins','del','rPrChange','pPrChange']}
    after_revs={e.get(q('id')):(e.tag,e.get(q('author')),e.get(q('date'))) for e in root.iter() if e.get(q('id')) and E.QName(e).localname in ['ins','del','rPrChange','pPrChange']}
    assert all(after_revs[k]==v for k,v in before_revs.items())
    for name,rtree in [('candidate.docx',root),('reject-new.docx',reject)]:
        with ZipFile(QA/name,'w',ZIP_DEFLATED) as z:
            for key,val in parts.items():z.writestr(key,E.tostring(rtree,encoding='UTF-8',xml_declaration=True,standalone=True) if key=='word/document.xml' else val)
    pending=[]
    for i,p in enumerate(root.xpath('//w:body//w:p',namespaces=N)):
        if visible(p) and '[QCC' in text(p):pending.append({'paragraph':i,'text':text(p).split('[QCC',1)[1]})
    record={'target':str(TARGET),'sha256_before':hashlib.sha256(raw).hexdigest(),'sha256_candidate':hashlib.sha256((QA/'candidate.docx').read_bytes()).hexdigest(),
            'colored_paragraphs':colored,'completed_statement_prefix_removed_at':[218,381,1208,1392],
            'table3_deleted_rows':15,'qcc_units_retained_in_table2':sorted(table_units),
            'old_revision_metadata_preserved':True,'reject_new_visible_text_equals_before':True,
            'unchanged_other_package_parts':True,'new_revision_start':first,'new_revision_end':seq-1,
            'replacements':replacements,'pending':pending}
    (QA/'changes.json').write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in record.items() if k not in ['pending','colored_paragraphs','replacements']},ensure_ascii=False,indent=2))
    print('Red paragraphs',len(colored),'pending notices',len(pending))

if __name__ == '__main__':
    edit() if len(sys.argv)>1 and sys.argv[1]=='edit' else inspect()
