"""Owned SIP QCC presentation cleanup and evidence, 2026-10-16."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
from lxml import etree as E
import copy, hashlib, json, re, sys
from datetime import datetime, timezone

TASK = Path(__file__).resolve().parent
QA = TASK / 'qa/sip-qcc-cleanup-20261016'
ROOT = Path('G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF/03_설계 출력/SW/외부기')
TARGET = ROOT / '[E8300-QCC] SIP-FW-TD24A 소프트웨어 통합 계획서.docx'
W = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'
N = {'w': W}
q = lambda s: '{'+W+'}'+s
def sha(raw): return hashlib.sha256(raw).hexdigest()
def load(path):
    with ZipFile(path) as z: return {i.filename:z.read(i) for i in z.infolist()}
def visible(e):
    return not e.xpath('ancestor::w:del|ancestor::w:moveFrom|ancestor-or-self::w:tr[w:trPr/w:del]', namespaces=N)
def text(e):
    return ''.join((n.text or '') if n.tag==q('t') else '\n' if n.tag==q('br') else '\t'
                   for n in e.iter() if n.tag in [q('t'),q('br'),q('tab')] and visible(n))
def dump(path):
    root=E.fromstring(load(path)['word/document.xml'])
    out=[]
    for i,p in enumerate(root.xpath('//w:body//w:p',namespaces=N)):
        if not visible(p): continue
        out.append({'i':i,'text':text(p),'style':p.xpath('string(w:pPr/w:pStyle/@w:val)',namespaces=N),
                    'runs':[{'text':text(r),'parent':E.QName(r.getparent()).localname,'xml':E.tostring(r,encoding='unicode')}
                            for r in p.xpath('.//w:r',namespaces=N) if visible(r) and text(r)]})
    return out
def inspect():
    QA.mkdir(parents=True,exist_ok=True)
    raw=TARGET.read_bytes()
    before=QA/'before.docx'
    if not before.exists(): before.write_bytes(raw)
    assert before.read_bytes()==raw, 'Target changed after snapshot'
    data=dump(before)
    (QA/'inspection.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
    print('BEFORE SHA256',sha(raw))
    for x in data:
        if x['text']: print(x['i'],x['style'],x['text'])
    for p in (ROOT/'서석일_작성').glob('QCC_SIP*.docx'):
        src=dump(p)
        (QA/'source-sip.json').write_text(json.dumps(src,ensure_ascii=False,indent=2),encoding='utf-8')
        print('QCC SOURCE:',p.name)
        for x in src:
            if x['text']: print(x['i'],x['style'],x['text'])

def write_package(parts, root, name):
    with ZipFile(QA/name,'w',ZIP_DEFLATED) as z:
        for key,val in parts.items():
            z.writestr(key,E.tostring(root,encoding='UTF-8',xml_declaration=True,standalone=True) if key=='word/document.xml' else val)

def semantic_chars(root):
    result=[]
    for p in root.xpath('//w:body//w:p',namespaces=N):
        if not visible(p): continue
        chars=[]
        for r in p.xpath('.//w:r',namespaces=N):
            if not visible(r): continue
            rp=r.find(q('rPr'))
            props=[]
            if rp is not None:
                props=[E.tostring(x,method='c14n') for x in rp if x.tag!=q('rPrChange')]
            chars.extend((c,tuple(props)) for c in text(r))
        result.append(chars)
    return result

def edit():
    raw=(QA/'before.docx').read_bytes()
    assert TARGET.read_bytes()==raw,'Target changed since snapshot'
    parts=load(QA/'before.docx'); before=E.fromstring(parts['word/document.xml'])
    root=copy.deepcopy(before); ps=root.xpath('//w:body//w:p',namespaces=N)
    seq=max(int(v) for v in root.xpath('//@w:id',namespaces=N) if v.isdigit())+1
    first=seq; stamp=datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
    colored=[]; replacements=[]
    def rev(tag):
        nonlocal seq
        e=E.Element(q(tag),{q('id'):str(seq),q('author'):'김은수',q('date'):stamp});seq+=1
        return e
    def red(r):
        rp=r.find(q('rPr'))
        if rp is None:rp=E.Element(q('rPr'));r.insert(0,rp)
        c=rp.find(q('color'))
        if c is not None and c.get(q('val'))=='C00000':return
        assert rp.find(q('rPrChange')) is None,'Existing format revision in target run'
        old=copy.deepcopy(rp)
        if c is None:
            c=E.Element(q('color'))
            # Schema order: color precedes size, underline and subsequent rPr entries.
            followers=['spacing','w','kern','position','sz','szCs','highlight','u','effect','bdr','shd','fitText','vertAlign','rtl','cs','em','lang','eastAsianLayout','specVanish','oMath']
            at=next((j for j,x in enumerate(rp) if E.QName(x).localname in followers),len(rp))
            rp.insert(at,c)
        c.attrib.clear();c.set(q('val'),'C00000')
        change=rev('rPrChange');change.append(old);rp.append(change)
    def span(i,a=0,b=None):
        p=ps[i]; b=len(text(p)) if b is None else b; cursor=0
        for r in list(p.xpath('.//w:r',namespaces=N)):
            if not visible(r):continue
            rt=text(r); start=cursor;cursor+=len(rt)
            lo=max(a,start)-start; hi=min(b,cursor)-start
            if hi<=lo:continue
            assert all(c.tag in [q('rPr'),q('t'),q('br'),q('lastRenderedPageBreak')] for c in r)
            if lo==0 and hi==len(rt):red(r);continue
            cuts=sorted({0,len(rt),lo,hi});par=r.getparent();at=par.index(r);par.remove(r)
            for x,y in zip(cuts,cuts[1:]):
                if x==y:continue
                nr=E.Element(q('r'),r.attrib);rp=r.find(q('rPr'))
                if rp is not None:nr.append(copy.deepcopy(rp))
                if x==0:
                    for extra in r.findall(q('lastRenderedPageBreak')):nr.append(copy.deepcopy(extra))
                for j,line in enumerate(rt[x:y].split('\n')):
                    if j:E.SubElement(nr,q('br'))
                    if line:
                        t=E.SubElement(nr,q('t'));t.set('{http://www.w3.org/XML/1998/namespace}space','preserve');t.text=line
                if lo<=x and y<=hi:red(nr)
                par.insert(at,nr);at+=1
        colored.append({'paragraph':i,'start':a,'end':b,'text':text(p)[a:b]})
    for i in [233,240,242,247,248,250,251,253,254,256,257,329,334,335,337,338,340,341,343,344,346,347,352,355,356]:span(i)
    span(350,0,text(ps[350]).index('\nSU-ID-CFX'))
    for i,stop in [(336,'충전4상태'),(339,'직렬화'),(342,'QCC PIO'),(345,'중계 패턴 통과'),(348,'GAIA 좌우'),(354,'송신 시험 결과는'),(357,'실제 통합/제품 시험'),(394,'통합과 시스템 시험은'),(395,'AUTH 패턴 중계 시험을')]:
        span(i,0,text(ps[i]).index(stop))
    for i,phrase in [(384,'QCC BLE Data 메시지는 type 1바이트와 data 0~19바이트로 총 1~20바이트이다.'),(387,'DMIC2는 QCC에도 연결되며 전화 통화 시 QCC가 수음한 음성을 통화 상대에게 송신한다. QCC의 Classic 수신 오디오는 I2S 24bit/16kHz로 CFX에 전달한다.')]:
        a=text(ps[i]).index(phrase);span(i,a,a+len(phrase))
    # Separate only established DMIC2 connection statements from genuine outstanding evidence.
    for i,done,rest in [
        (200,'DMIC2 입력 연결은 확인되었다.','QCC 4모듈/14유닛의 실제 소스/빌드/라이브러리 설정과 유닛/통합 실행 및 측정 결과가 필요하다. 이 안내는 구현/시험 근거의 확인 상태를 나타낸다.'),
        (393,'DMIC2의 QCC 입력 연결은 확인되었다.',text(ps[393]).split('DMIC2의 QCC 입력 연결은 확인되었다. ',1)[1])]:
        p=ps[i];oldtext=text(p)
        r=next(r for r in p.xpath('.//w:r',namespaces=N) if visible(r) and text(r).startswith('[QCC 자료 입력]'))
        assert text(r)==oldtext
        par=r.getparent();assert par.tag==q('ins') and par.getparent() is p
        deletion=rev('del');par.replace(r,deletion)
        for t in r.iter(q('t')):t.tag=q('delText')
        deletion.append(r)
        ins=rev('ins');p.insert(p.index(par)+1,ins)
        a=E.SubElement(ins,q('r'));rp=copy.deepcopy(r.find(q('rPr')));a.append(rp)
        for tag,val in [('color','C00000'),('sz','20'),('szCs','20'),('u','none')]:
            e=rp.find(q(tag))
            if e is None:e=E.SubElement(rp,q(tag))
            e.attrib.clear();e.set(q('val'),val)
        E.SubElement(a,q('t')).text=done
        b=E.SubElement(ins,q('r'));b.append(copy.deepcopy(r.find(q('rPr'))));E.SubElement(b,q('br'))
        E.SubElement(b,q('t')).text='[QCC 자료 입력] '+rest
        replacements.append({'paragraph':i,'before':oldtext,'after':text(p)})
    reject=copy.deepcopy(root); accept=copy.deepcopy(root)
    for tree,mode in [(reject,'reject'),(accept,'accept')]:
        for e in reversed(list(tree.iter())):
            if not e.get(q('id'),'').isdigit() or int(e.get(q('id')))<first:continue
            par=e.getparent()
            if par is None:continue
            if e.tag==q('rPrChange'):
                if mode=='reject':par.getparent().replace(par,copy.deepcopy(e.find(q('rPr'))))
                else:par.remove(e)
            elif e.tag in [q('ins'),q('del')]:
                if (mode=='reject' and e.tag==q('ins')) or (mode=='accept' and e.tag==q('del')):par.remove(e)
                else:
                    at=par.index(e)
                    for child in list(e):
                        for t in child.iter(q('delText')):t.tag=q('t')
                        e.remove(child);par.insert(at,child);at+=1
                    par.remove(e)
    assert semantic_chars(reject)==semantic_chars(before),'Reject semantic character/property mismatch'
    assert semantic_chars(accept)==semantic_chars(root),'Accept mismatch'
    def revision_signatures(tree):
        return {e.get(q('id')):(e.tag,e.get(q('author')),e.get(q('date')),''.join(e.xpath('.//w:t/text()|.//w:delText/text()',namespaces=N)))
                for e in tree.iter() if E.QName(e).localname in ['ins','del','rPrChange','pPrChange','tblPrChange','trPrChange','tcPrChange']}
    olds=revision_signatures(before);news=revision_signatures(root)
    assert all(news[k]==v for k,v in olds.items()),'Prior revision signatures changed'
    for tag in ['tblPr','tblGrid','tcPr','drawing','pict','sectPr','instrText']:
        assert [E.tostring(x) for x in before.iter(q(tag))]==[E.tostring(x) for x in root.iter(q(tag))],tag
    for name,tree in [('candidate.docx',root),('reject-new.docx',reject),('accept-new.docx',accept)]:write_package(parts,tree,name)
    headings=[];pending=[]
    for i,p in enumerate(ps):
        if not visible(p):continue
        style=p.xpath('string(w:pPr/w:pStyle/@w:val)',namespaces=N)
        if re.fullmatch('TDC[1-6]',style):headings.append(text(p).strip())
        if '[QCC 자료 입력]' in text(p):
            pending.append({'paragraph':i,'heading':headings[-1] if headings else '', 'text':text(p).split('[QCC 자료 입력]',1)[1].strip()})
    ids=lambda tree:re.findall(r'(?:SI-ID-QCC|SU-ID-QCC|SU-ID-CFX|SW-FW-IntegrationTest-ID|SW-FW-RID|QCC-I)-\d+',text(tree))
    assert ids(before)==ids(root),'ID sequence changed'
    assert len(pending)==7
    record={'target':str(TARGET),'sha256_before':sha(raw),'sha256_candidate':sha((QA/'candidate.docx').read_bytes()),
            'new_revision_start':first,'new_revision_end':seq-1,'revision_timestamp':stamp,'author':'김은수',
            'colored_spans':colored,'replacements':replacements,'completed_statements_detached_from_prefix':2,
            'whole_notices_removed':0,'active_prefix_count_before':7,'active_prefix_count_after':7,'pending':pending,
            'old_revision_count':len(olds),'old_revision_signatures_preserved':True,'reject_new_text_and_font_equal_before':True,
            'accept_new_equals_intended':True,'ids_preserved':True,'tables_fields_drawings_sections_preserved':True,
            'other_package_parts_unchanged':True,'native_validation':'pending','visual_review':'pending'}
    (QA/'changes.json').write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in record.items() if k not in ['colored_spans','replacements','pending']},ensure_ascii=False,indent=2))

def render_qa():
    import pypdfium2 as pdfium
    from PIL import ImageChops
    records=json.loads((QA/'native-fast-validation.json').read_text(encoding='utf-8-sig'))
    for phase in ['before','candidate','reject-structured','before-reject-all','candidate-reject-all']:
        folder=QA/('render-'+phase);folder.mkdir(exist_ok=True)
        doc=pdfium.PdfDocument(str(QA/(phase+'.pdf')))
        for i in range(len(doc)):
            page=doc[i]; bitmap=page.render(scale=1.5)
            img=bitmap.to_pil().convert('RGB');img.save(folder/f'page-{i+1}.png')
            bitmap.close();page.close()
        doc.close()
    from PIL import Image
    before_imgs=sorted((QA/'render-before').glob('page-*.png'),key=lambda p:int(p.stem.split('-')[1]))
    rejected_imgs=sorted((QA/'render-reject-structured').glob('page-*.png'),key=lambda p:int(p.stem.split('-')[1]))
    assert len(before_imgs)==len(rejected_imgs),'Rejected page count differs'
    diffs=[]
    for a,b in zip(before_imgs,rejected_imgs):
        with Image.open(a) as aa,Image.open(b) as bb:
            equal=aa.size==bb.size and ImageChops.difference(aa,bb).getbbox() is None
            diffs.append({'page':int(a.stem.split('-')[1]),'equal':equal})
    before_text=(QA/'before-word-text.txt').read_text(encoding='utf-8-sig')
    rejected_text=(QA/'reject-structured-word-text.txt').read_text(encoding='utf-8-sig')
    assert before_text==rejected_text,'Word reject text differs'
    assert all(x['equal'] for x in diffs),'Word rejection pixels differ'
    candidate_text=(QA/'candidate-word-text.txt').read_text(encoding='utf-8-sig')
    expected=[x['text'] for x in dump(QA/'candidate.docx') if x['i']>=147 and x['text']]
    normalize=lambda t:re.sub(r'\s|\x07','',t)
    clean=normalize(candidate_text)
    for p in expected:assert normalize(p) in clean, 'Missing expected paragraph: '+p[:80]
    native_reject_texts=[(QA/(ph+'-word-text.txt')).read_text(encoding='utf-8-sig') for ph in ['before-reject-all','candidate-reject-all']]
    assert native_reject_texts[0]==native_reject_texts[1], 'Native reject all content differs'
    all_reject_diffs=[]
    for a in sorted((QA/'render-before-reject-all').glob('page-*.png')):
        b=QA/'render-candidate-reject-all'/a.name
        with Image.open(a) as aa,Image.open(b) as bb:
            equal=aa.size==bb.size and ImageChops.difference(aa,bb).getbbox() is None
            all_reject_diffs.append({'page':int(a.stem.split('-')[1]),'equal':equal})
    assert all(x['equal'] for x in all_reject_diffs),'Native RejectAll pixel difference'
    report={'before_pages':records[0]['pages'],'candidate_pages':records[1]['pages'],
            'word_accept_all_remaining_revisions':records[1]['revisions_after_accept'],
            'new_only_reject_method':'OOXML new revision IDs 857-971 reversed; Word accepted preview equals before',
            'new_only_reject_text_equals_before':True,'new_only_reject_word_render_pixels_equal_before':diffs,
            'native_word_reject_all_text_equals_baseline_reject_all':True,'native_word_reject_all_pixels_equal_baseline':all_reject_diffs,
            'candidate_all_expected_body_paragraphs_present':True,'visual_review':'pending'}
    (QA/'render-validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False,indent=2))

def publish():
    record=json.loads((QA/'changes.json').read_text(encoding='utf-8'))
    render=json.loads((QA/'render-validation.json').read_text(encoding='utf-8'))
    assert render['visual_review']=='pass_all_pages'
    assert sha(TARGET.read_bytes())==record['sha256_before'],'G file changed: do not overwrite'
    candidate=(QA/'candidate.docx').read_bytes()
    assert sha(candidate)==record['sha256_candidate']
    with TARGET.open('r+b') as out:
        assert sha(out.read())==record['sha256_before'],'Changed immediately before publication'
        out.seek(0);out.write(candidate);out.truncate();out.flush()
    actual=TARGET.read_bytes()
    assert actual==candidate
    record['published_sha256']=sha(actual)
    record['published_at']=datetime.now(timezone.utc).isoformat()
    record['native_validation']='pass'
    record['visual_review']='pass_all_pages'
    record['published_package_reopened']=len(load(TARGET))
    (QA/'changes.json').write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'published':str(TARGET),'before':record['sha256_before'],'after':record['published_sha256']},ensure_ascii=False,indent=2))

if __name__=='__main__':
    if len(sys.argv)>1 and sys.argv[1]=='edit':edit()
    elif len(sys.argv)>1 and sys.argv[1]=='render-qa':render_qa()
    elif len(sys.argv)>1 and sys.argv[1]=='publish':publish()
    else:inspect()
