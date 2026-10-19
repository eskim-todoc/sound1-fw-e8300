"""Narrow, tracked QCC presentation edits in the assigned SDDD only."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
from lxml import etree as E
import copy, json, hashlib, re, sys
from datetime import datetime, timezone

TASK = Path(__file__).resolve().parent
QA = TASK / 'qa/sddd-qcc-cleanup-20261016'
ROOT = Path('G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF/03_설계 출력/SW/외부기')
TARGET = ROOT / '[E8300-QCC] SDDD-FW-TD24A 소프트웨어 상세 설계서.docx'
W = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'
N = {'w': W}
q = lambda s: '{'+W+'}'+s

def visible(e):
    return not e.xpath('ancestor::w:del|ancestor::w:moveFrom|ancestor-or-self::w:tr[w:trPr/w:del]', namespaces=N)

def text(e):
    return ''.join((n.text or '') if n.tag == q('t') else '\n' for n in e.iter() if n.tag in [q('t'),q('br')] and visible(n))

def load(p):
    with ZipFile(p) as z: return {i.filename:z.read(i) for i in z.infolist()}

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def write_json(name, data):
    (QA/name).write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf-8')

def inspect():
    QA.mkdir(parents=True, exist_ok=True)
    parts=load(TARGET); root=E.fromstring(parts['word/document.xml'])
    data=[]; heading=''
    for i,p in enumerate(root.xpath('//w:body//w:p', namespaces=N)):
        if not visible(p): continue
        t=text(p); style=p.xpath('string(w:pPr/w:pStyle/@w:val)',namespaces=N)
        if style.startswith('Heading') or re.match(r'SU-ID-QCC-\d+',t): heading=t
        data.append({'index':i, 'text':t, 'style':style, 'heading':heading,
                     'runs':[{'text':text(r), 'color':r.xpath('string(w:rPr/w:color/@w:val)',namespaces=N),
                              'size':r.xpath('string(w:rPr/w:sz/@w:val)',namespaces=N),
                              'underline':r.xpath('string(w:rPr/w:u/@w:val)',namespaces=N),
                              'bold':r.xpath('string(w:rPr/w:b/@w:val)',namespaces=N),
                              'parent':E.QName(r.getparent()).localname}
                             for r in p.xpath('.//w:r',namespaces=N) if visible(r) and text(r)]})
    write_json('inspection.json',data)
    write_json('baseline.json', {'path':str(TARGET),'sha256':sha(TARGET),'package_parts':len(parts),
                               'sources':{str(p):sha(p) for p in (ROOT/'서석일_작성').glob('QCC_*.docx')}})
    for x in data:
        if '[QCC' in x['text'] or 'QCC' in x['text'] and (x['style'].startswith('Heading') or len(x['text'])<80):
            print(x['index'],x['style'],x['text'])
    for p in (ROOT/'서석일_작성').glob('QCC_SDDD*.docx'):
        rt=E.fromstring(load(p)['word/document.xml'])
        source=[{'index':i,'text':text(x)} for i,x in enumerate(rt.xpath('//w:body//w:p',namespaces=N)) if visible(x)]
        write_json('source-sddd.json',source)
    print('Inspection complete',len(data),'paragraphs',len(root.xpath('//w:drawing',namespaces=N)),'drawings')

def save_package(path,parts,root):
    with ZipFile(path,'w',ZIP_DEFLATED) as z:
        for key,val in parts.items():
            z.writestr(key,E.tostring(root,encoding='UTF-8',xml_declaration=True,standalone=True) if key=='word/document.xml' else val)

def project_new(root,first,accept=False):
    root=copy.deepcopy(root)
    if not accept:
        for p in list(root.xpath('//w:p[w:pPr/w:rPr/w:ins]',namespaces=N)):
            mark=p.find('w:pPr/w:rPr/w:ins',N)
            if int(mark.get(q('id')))<first:continue
            p.getparent().remove(p)
    for e in reversed(list(root.iter())):
        eid=e.get(q('id'),'')
        if not eid.isdigit() or int(eid)<first: continue
        par=e.getparent()
        if par is None:continue
        if e.tag==q('rPrChange'):
            if accept:par.remove(e)
            else:par.getparent().replace(par,copy.deepcopy(e.find(q('rPr'))))
        elif e.tag in [q('ins'),q('del')]:
            if (e.tag==q('ins'))==accept:
                at=par.index(e)
                for child in list(e):
                    if not accept:
                        for t in child.iter(q('delText')):t.tag=q('t')
                    e.remove(child);par.insert(at,child);at+=1
            par.remove(e)
    return root

def char_signature(root):
    result=[]
    for p in root.xpath('//w:body//w:p',namespaces=N):
        if not visible(p):continue
        chars=[]
        for r in p.xpath('.//w:r',namespaces=N):
            if not visible(r):continue
            rp=r.find(q('rPr'))
            props=tuple(sorted(E.tostring(x,method='c14n').decode() for x in rp)) if rp is not None else ()
            chars.extend((c,props) for c in text(r))
        result.append(chars)
    return result

def edit():
    baseline=json.loads((QA/'baseline.json').read_text(encoding='utf-8'))
    assert sha(TARGET)==baseline['sha256'], 'Target changed since inspection'
    beforefile=QA/'before.docx'
    if not beforefile.exists(): beforefile.write_bytes(TARGET.read_bytes())
    assert sha(beforefile)==baseline['sha256']
    parts=load(beforefile);before=E.fromstring(parts['word/document.xml']);root=copy.deepcopy(before)
    ps=root.xpath('//w:body//w:p',namespaces=N)
    seq=max(int(x) for x in root.xpath('//@w:id',namespaces=N) if x.isdigit())+1
    first=seq;stamp=datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
    edits=[];colored=[]
    def rev(tag):
        nonlocal seq
        e=E.Element(q(tag),{q('id'):str(seq),q('author'):'김은수',q('date'):stamp});seq+=1
        return e
    def red(r):
        rp=r.find(q('rPr'))
        if rp is None:rp=E.Element(q('rPr'));r.insert(0,rp)
        if rp.xpath('w:color[@w:val="C00000"]',namespaces=N):return
        assert rp.find(q('rPrChange')) is None
        old=copy.deepcopy(rp);c=rp.find(q('color'))
        if c is None:c=E.SubElement(rp,q('color'))
        c.attrib.clear();c.set(q('val'),'C00000')
        ch=rev('rPrChange');ch.append(old);rp.append(ch)
    def span(i,a,b):
        p=ps[i];cursor=0
        for r in list(p.xpath('.//w:r',namespaces=N)):
            if not visible(r):continue
            rt=text(r);start=cursor;cursor+=len(rt)
            if a>=cursor or b<=start:continue
            left=max(a,start)-start;right=min(b,cursor)-start
            if left==0 and right==len(rt):red(r);continue
            assert all(c.tag in [q('rPr'),q('t'),q('br'),q('lastRenderedPageBreak')] for c in r)
            par=r.getparent();at=par.index(r);par.remove(r)
            for x,y in zip([0,left,right],[left,right,len(rt)]):
                if x==y:continue
                nr=E.Element(q('r'),r.attrib);rp=r.find(q('rPr'))
                if rp is not None:nr.append(copy.deepcopy(rp))
                for k,line in enumerate(rt[x:y].split('\n')):
                    if k:E.SubElement(nr,q('br'))
                    if line:
                        t=E.SubElement(nr,q('t'));t.set('{http://www.w3.org/XML/1998/namespace}space','preserve');t.text=line
                if x==left and y==right:red(nr)
                par.insert(at,nr);at+=1
        colored.append({'paragraph':i,'text':text(p)[a:b]})
    def phrase(i,s):
        a=text(ps[i]).index(s);span(i,a,a+len(s))
    phrase(270,'DMIC2는 QCC에도 연결되어 전화 통화 상향 입력으로 사용한다.')
    for i in [325,326,327,329,330]:span(i,0,len(text(ps[i])))
    phrase(277,'QCC의 시스템 관리/HW 인터페이스/블루투스/보안 4모듈과14유닛')
    phrase(317,'외부기에는 E8300과 별도 칩 QCC5181이 있다.')
    phrase(317,'QCC는 무선 송수신, 전원/PIO/NVM과 인증을 처리한다. DMIC2는 두 칩에 연결되며 QCC의 통화 상향과 E8300으로 보내는 I2S 하향을 구분한다.')
    phrase(4886,'QCC BLE Data 메시지는 type 1바이트와 data 0~19바이트로 총 1~20바이트이다.')
    phrase(4889,'DMIC2는 QCC에도 연결되며 전화 통화 시 QCC가 수음한 음성을 통화 상대에게 송신한다. QCC의 Classic 수신 오디오는 I2S 24bit/16kHz로 CFX에 전달한다.')
    for i in range(4896,5233):
        p=ps[i];t=text(p);style=p.xpath('string(w:pPr/w:pStyle/@w:val)',namespaces=N)
        if not t or '[QCC' in t or style in ['TDC0','TDCe']:continue
        if i==4897:
            span(i,0,t.index(' E8300은'));continue
        if i==5187:
            span(i,0,t.index('\n'));continue
        span(i,0,len(t))
    # Three mixed notices retain their exact sentences; only their scopes/styles separate.
    for i in [315,4895,5079]:
        p=ps[i];rs=[r for r in p.xpath('.//w:r',namespaces=N) if visible(r) and text(r)]
        assert len(rs)==1
        r=rs[0];oldtext=text(r);par=r.getparent()
        assert par.tag==q('ins') and par.getparent()is p
        if i==315:
            pending,done=oldtext.split(' DMIC2',1);done='DMIC2'+done
        else:
            done='DMIC2의 QCC 입력 연결은 확인되었다.'
            pending='[QCC 자료 입력] '+oldtext.split(done+' ',1)[1]
        d=rev('del');par.replace(r,d)
        for t in r.iter(q('t')):t.tag=q('delText')
        d.append(r)
        done_p=E.Element(q('p'))
        done_pr=copy.deepcopy(p.find(q('pPr')))
        for prop in list(done_pr):
            if prop.tag in [q('rPr'),q('pPrChange')]:done_pr.remove(prop)
        keep=E.SubElement(done_pr,q('keepNext'))
        mark_rp=E.SubElement(done_pr,q('rPr'));mark_rp.append(rev('ins'))
        done_p.append(done_pr);p.addprevious(done_p)
        for outp,s,done_flag in [(done_p,done,True),(p,pending,False)]:
            ins=rev('ins');outp.append(ins)
            nr=E.SubElement(ins,q('r'));rp=copy.deepcopy(r.find(q('rPr')));nr.append(rp)
            if done_flag:
                for tag,val in [('sz','20'),('szCs','20'),('u','none')]:
                    el=rp.find(q(tag))
                    if el is None:el=E.SubElement(rp,q(tag))
                    el.set(q('val'),val)
            t=E.SubElement(nr,q('t'))
            t.set('{http://www.w3.org/XML/1998/namespace}space','preserve')
            t.text=s
        edits.append({'paragraph':i,'before':oldtext,'after':done+'\n'+pending})
    rejected=project_new(root,first);accepted=project_new(root,first,True)
    assert [text(p) for p in before.xpath('//w:body//w:p',namespaces=N)]==[text(p) for p in rejected.xpath('//w:body//w:p',namespaces=N)]
    assert char_signature(before)==char_signature(rejected), 'Reject must restore text and character properties'
    # Paragraph/table/section/figure properties and every other ZIP part are untouched.
    retained=copy.deepcopy(root)
    for p in list(retained.xpath('//w:p[w:pPr/w:rPr/w:ins]',namespaces=N)):
        if int(p.find('w:pPr/w:rPr/w:ins',N).get(q('id')))>=first:p.getparent().remove(p)
    for xp in ['//w:pPr','//w:tblPr','//w:tblGrid','//w:tcPr','//w:trPr','//w:drawing','//w:pict','//w:sectPr']:
        assert [E.tostring(x) for x in before.xpath(xp,namespaces=N)]==[E.tostring(x) for x in retained.xpath(xp,namespaces=N)],xp
    revtags=['ins','del','rPrChange','pPrChange','tblPrChange','trPrChange','tcPrChange']
    def metas(rt):
        return [(e.tag,dict(e.attrib)) for e in rt.iter() if E.QName(e).localname in revtags and int(e.get(q('id'),'0'))<first]
    assert metas(before)==metas(root)
    # Full prior-revision contents are identical after rejecting just this operation.
    def signatures(rt):
        return [(e.tag,dict(e.attrib),''.join(e.itertext())) for e in rt.iter() if E.QName(e).localname in revtags]
    assert signatures(before)==signatures(rejected)
    for name,rt in [('candidate.docx',root),('reject-new.docx',rejected),('accept-new.docx',accepted)]:save_package(QA/name,parts,rt)
    ps=root.xpath('//w:body//w:p',namespaces=N)
    heading='';pending=[]
    for i,p in enumerate(ps):
        if not visible(p):continue
        t=text(p);sty=p.xpath('string(w:pPr/w:pStyle/@w:val)',namespaces=N)
        if sty in ['TDC1','TDC2','TDC3','TDC4'] and t:heading=t
        if '[QCC' in t:
            s=t[t.index('[QCC'):].split('\n')[0]
            for r in p.xpath('.//w:r',namespaces=N):
                if visible(r) and text(r).lstrip('\n').startswith('[QCC'):
                    assert r.xpath('string(w:rPr/w:sz/@w:val)',namespaces=N)=='24'
                    assert r.xpath('string(w:rPr/w:u/@w:val)',namespaces=N)=='single'
                    assert r.xpath('string(w:rPr/w:color/@w:val)',namespaces=N)==('7030A0' if '[QCC/E8300' in text(r) else 'C00000')
            pending.append({'paragraph':i,'title_or_id':heading,'text':s})
    before_t='\n'.join(text(x) for x in before.xpath('//w:body//w:p',namespaces=N))
    after_t='\n'.join(text(x) for x in ps)
    ids=lambda t:sorted(re.findall(r'(?:SI|SU)-ID-[A-Z0-9]+-\d+|SW-FW-(?:RID|UnitTest-QCC-ID)-\d+',t))
    assert ids(before_t)==ids(after_t)
    report={'target':str(TARGET),'sha256_before':sha(beforefile),'sha256_candidate':sha(QA/'candidate.docx'),
            'new_revision_start':first,'new_revision_end':seq-1,'author':'김은수',
            'completed_prefix_relocated_count':2,'fully_removed_notice_count':0,'mixed_notices_normalized_count':3,
            'qcc_prefix_count_before':before_t.count('[QCC 자료 입력]'),'qcc_prefix_count_after':after_t.count('[QCC 자료 입력]'),
            'joint_prefix_count_after':after_t.count('[QCC/E8300 자료 입력]'),
            'colored':colored,'mixed_notice_edits':edits,'pending':pending,
            'validation':{'new_rejection_restores_text_and_run_properties':True,'old_revision_signatures_preserved':True,
                          'all_ids_preserved':True,'drawings_preserved':len(root.xpath('//w:drawing',namespaces=N)),
                          'table_widths_grid_cells_paragraphs_sections_preserved':True,'other_package_parts_unchanged':True,
                          'product_test_performed':False,'render':'pending'},
            'source_evidence':{'QCC_SDDD':'4 modules / 14 unit descriptions, AUTH/CRA/NVM/Classic/BLE definitions',
                               'QCC_SUVC':'PIO/preInit, PS identifiers and lengths, CRA 30 s and test-connection definitions',
                               'DMIC2':'2026-10-13 user confirmation retained in the existing task record'}}
    write_json('changes.json',report)
    print(json.dumps({k:v for k,v in report.items() if k not in ['colored','mixed_notice_edits','pending']},ensure_ascii=False,indent=2))

def publish():
    report=json.loads((QA/'changes.json').read_text(encoding='utf-8'))
    assert report['validation']['render']=='passed'
    assert sha(TARGET)==report['sha256_before'],'Source changed; do not overwrite'
    assert sha(QA/'candidate.docx')==report['sha256_candidate']
    baseline=json.loads((QA/'baseline.json').read_text(encoding='utf-8'))
    assert all(sha(p)==h for p,h in baseline['sources'].items())
    TARGET.write_bytes((QA/'candidate.docx').read_bytes())
    assert sha(TARGET)==report['sha256_candidate']
    assert load(TARGET)==load(QA/'candidate.docx')
    report.update({'published':True,'sha256_final':sha(TARGET),'published_at':datetime.now(timezone.utc).isoformat(),
                   'source_hash_rechecked_before_publish':True,'reference_documents_unchanged':True})
    write_json('changes.json',report)
    print(json.dumps({k:v for k,v in report.items() if k not in ['colored','mixed_notice_edits','pending']},ensure_ascii=False,indent=2))

if __name__=='__main__':
    {'inspect':inspect,'edit':edit,'publish':publish}[sys.argv[1] if len(sys.argv)>1 else 'inspect']()
