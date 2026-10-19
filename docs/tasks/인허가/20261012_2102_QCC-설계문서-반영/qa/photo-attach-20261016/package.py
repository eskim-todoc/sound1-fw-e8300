from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
from lxml import etree as E
from copy import deepcopy as cp
import json, re, hashlib, math, sys, posixpath

HERE=Path(__file__).resolve().parent
N='http://schemas.openxmlformats.org/spreadsheetml/2006/main'
R='http://schemas.openxmlformats.org/officeDocument/2006/relationships'
P='http://schemas.openxmlformats.org/package/2006/relationships'
X='http://schemas.openxmlformats.org/drawingml/2006/spreadsheetDrawing'
A='http://schemas.openxmlformats.org/drawingml/2006/main'
NS={'s':N,'r':R,'x':X,'a':A}
MAIN='보고서_시스템 시험'; COMP=MAIN+'_비교'
def t(n):return '{'+N+'}'+n
def readj(p):return json.loads((HERE/p).read_text(encoding='utf-8-sig'))
def savej(p,o):(HERE/p).write_text(json.dumps(o,ensure_ascii=False,indent=2),encoding='utf-8')
def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def dump(n):return E.tostring(n,encoding='utf-8',xml_declaration=True,standalone=True)
def ref(a):
    m=re.fullmatch(r'([A-Z]+)(\d+)',a);return m[1],int(m[2])
def span(a):
    v=a.split(':');c1,r1=ref(v[0]);c2,r2=ref(v[-1]);return c1,r1,c2,r2
def text(runs):return ''.join(v['text'] for v in runs)
def fontdict(node):
    d={}
    if node is not None:
        for c in node:
            k=E.QName(c).localname
            d[k]=dict(c.attrib) if c.attrib else True
    return d
class Book:
    def __init__(self,path):
        self.path=Path(path)
        with ZipFile(path) as z:self.data={n:z.read(n) for n in z.namelist()}
        self.wb=E.fromstring(self.data['xl/workbook.xml'])
        rels=E.fromstring(self.data['xl/_rels/workbook.xml.rels'])
        targets={v.get('Id'):posixpath.normpath('xl/'+v.get('Target')) if not v.get('Target').startswith('/') else v.get('Target')[1:] for v in rels}
        self.paths={s.get('name'):targets[s.get('{'+R+'}id')] for s in self.wb.find(t('sheets'))}
        self.ss=list(E.fromstring(self.data['xl/sharedStrings.xml'])) if 'xl/sharedStrings.xml' in self.data else []
        self.styles=E.fromstring(self.data['xl/styles.xml']);self.fonts=self.styles.find(t('fonts'));self.xfs=self.styles.find(t('cellXfs'))
        self.sheets={k:E.fromstring(self.data[v]) for k,v in self.paths.items()}
        self.cells={k:{c.get('r'):c for c in n.findall('s:sheetData/s:row/s:c',NS)} for k,n in self.sheets.items()}
    def content(self,c):
        if c is None:return None
        if c.get('t')=='s':return self.ss[int(c.find(t('v')).text)]
        return c.find(t('is'))
    def runs(self,c):
        if c is None:return []
        base=fontdict(self.fonts[int(self.xfs[int(c.get('s','0'))].get('fontId','0'))]);co=self.content(c)
        if co is None:
            v=c.find(t('v'));return [{'text':v.text,'font':base}] if v is not None and v.text else []
        out=[]
        for el in co:
            if el.tag==t('t'):out.append({'text':el.text or '', 'font':base.copy()})
            elif el.tag==t('r'):
                f=base.copy();f.update(fontdict(el.find(t('rPr'))));out.append({'text':el.findtext(t('t')) or '', 'font':f})
        return out
    def value(self,a,sheet=MAIN):return text(self.runs(self.cells[sheet].get(a)))
    def tests(self):
        out=[]
        for a,c in self.cells[MAIN].items():
            m=re.fullmatch(r'\s*SW-FW-SYS-Test-ID-(\d+)\s*',text(self.runs(c)))
            if a.startswith('B') and m:out.append({'id':int(m[1]),'start':ref(a)[1]})
        for i,o in enumerate(out):o['end']=out[i+1]['start']-1 if i+1<len(out) else o['start']
        return out
def tidy_runs(rs):
    rs=[cp(r) for r in rs if r['text']]
    while rs and not rs[0]['text'].strip():rs.pop(0)
    while rs and not rs[-1]['text'].strip():rs.pop()
    if not rs:return []
    rs[0]['text']='\n'+rs[0]['text'].lstrip('\r\n');rs[-1]['text']=rs[-1]['text'].rstrip('\r\n')+'\n'
    return rs
def cut(rs,start,end):
    out=[];at=0
    for r in rs:
        a,b=at,at+len(r['text']);at=b
        if b<=start or a>=end:out.append(cp(r));continue
        if a<start:out.append(dict(text=r['text'][:start-a],font=cp(r['font'])))
        if b>end:out.append(dict(text=r['text'][end-a:],font=cp(r['font'])))
    return out
def replace_literal(rs,old,new,newfont=None):
    s=text(rs);p=s.find(old)
    if p<0:raise ValueError(('text not found',old,s))
    left=cut(rs,p,len(s));right=cut(rs,0,p+len(old))
    return left+[{'text':new,'font':newfont or cp(rs[0]['font'])}]+right
def inline(c,rs):
    for ch in list(c):
        if ch.tag in [t('is'),t('v'),t('f')]:c.remove(ch)
    c.set('t','inlineStr');co=E.SubElement(c,t('is'))
    for v in rs:
        if not v['text']:continue
        rn=E.SubElement(co,t('r'));fp=E.SubElement(rn,t('rPr'));f=cp(v['font'])
        if 'rFont' in f:f.pop('name',None)
        elif 'name' in f:f['rFont']=f.pop('name')
        for k in ['rFont','charset','family','b','i','strike','outline','shadow','condense','extend','color','sz','u','vertAlign','scheme']:
            if k not in f:continue
            val=f[k]
            if val is False:continue
            if isinstance(val,dict):E.SubElement(fp,t(k),**{a:str(b) for a,b in val.items()})
            elif val is True:E.SubElement(fp,t(k))
            else:E.SubElement(fp,t(k),val=str(val))
        tx=E.SubElement(rn,t('t'));tx.set('{http://www.w3.org/XML/1998/namespace}space','preserve');tx.text=v['text']

def prepare():
    q=Book(HERE/'qcc-source.xlsx');p=Book(HERE/'personal-source.xlsx')
    tests=q.tests();files=readj('photo-inventory.json')['files'];testmap={v['id']:v for v in tests};photos={};issues=[]
    for f in files:
        test=testmap[f['test']];hits=[]
        for row in range(test['start'],test['end']+1):
            val=q.value('K'+str(row));m=re.search(r'검증\s*\[\s*(\d+)\s*\]',val)
            if m and int(m[1])==f['verification']:hits.append(row)
        if len(hits)!=1:issues.append({'file':f['name'],'rows':hits});continue
        photos.setdefault(str(hits[0]),[]).append(f)
    savej('structure.json',{'tests':tests,'photos':photos,'issues':issues,'baseMerges':[x.get('ref') for x in q.sheets[MAIN].findall('s:mergeCells/s:mergeCell',NS)],'qccSHA':digest(q.path),'personalSHA':digest(p.path)})
    print(json.dumps({'photos':len(files),'mapped':sum(len(v) for v in photos.values()),'issues':issues},ensure_ascii=False))

def measure(rs,width=520):
    # Conservative line estimator for allocating picture space; native render is checked separately.
    from PIL import ImageFont
    s=text(rs);size=max([float(r['font'].get('sz',{}).get('val',11)) for r in rs] or [11])
    f=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',int(math.ceil(size*96/72)))
    lines=0
    for line in s.split('\n'):
        lines+=max(1,math.ceil(f.getlength(line)/width))
    return math.ceil(lines*(size*1.4)+8)

def plan():
    q=Book(HERE/'qcc-source.xlsx');p=Book(HERE/'personal-source.xlsx');audit=readj('edit-audit.json');st=readj('structure.json')
    decisions=readj('decisions.json');deleted=set(decisions['deleteRows']);changes={};log=[]
    for e in audit['events']:
        if not e.get('greenRanges'):continue
        assert len(e['qccTargets'])==1,e['privateCell']
        target=e['qccTargets'][0]['cell'];old=q.runs(q.cells[MAIN].get(target))
        if ref(target)[1] in deleted:continue
        if e['partialGreen']:
            if e['privateCell']=='K82':new=cp(old)+cp(e['greenRanges'])
            elif e['privateCell']=='K206':new=replace_literal(old,'손상 이미지 기동 차단 확인.','손상된 이미지로 부팅되지 않는 것을 확인.',e['greenRanges'][0]['font'])
            else:raise ValueError(('unreviewed partial',e['privateCell']))
        else:new=cp(e['runs'])
        changes[target]={'runs':tidy_runs(new),'private':e['privateCell'],'reason':'사용자 녹색 밑줄 수정','test':e['testId']}
    for cell,newtext in decisions.get('textOverrides',{}).items():
        before=changes.get(cell,{}).get('runs') or q.runs(q.cells[MAIN].get(cell))
        changes[cell]={'runs':tidy_runs([{'text':newtext,'font':cp(before[0]['font'])}]),'reason':'사용자 변경 간의 명확한 표기 정리','test':next(x['id'] for x in st['tests'] if x['start']<=ref(cell)[1]<=x['end'])}
    for row,photos in st['photos'].items():
        row=int(row)
        if row in deleted:continue
        addr=f'K{row}';rs=changes.get(addr,{}).get('runs') or q.runs(q.cells[MAIN].get(addr));s=text(rs)
        matches=list(re.finditer(r'^.*\(사진필요\).*?(?:\n|$)',s,re.M))
        for m in reversed(matches):rs=cut(rs,m.start(),m.end())
        # This exact 32-channel attachment request is now represented by the second supplied waveform.
        if row==37:
            s=text(rs);m=re.search(r'\[E8300 보완\].*32채널/750 Hz.*?(?:\n|$)',s)
            if m:rs=cut(rs,m.start(),m.end())
        ent=changes.setdefault(addr,{'reason':'해당 검증 사진 첨부와 사진 요청 정리','test':photos[0]['test']});ent['runs']=tidy_runs(rs)
    for target in decisions.get('clearCells',[]):
        if ref(target)[1] not in deleted:changes[target]={'runs':[],'reason':'사용자 지정 삭제','test':None}
    for ch in changes.values():
        for old,new in decisions.get('minorTypos',{}).items():
            while old in text(ch['runs']):ch['runs']=replace_literal(ch['runs'],old,new)
        if '조건 [3\n' in text(ch['runs']):ch['runs']=replace_literal(ch['runs'],'조건 [3\n','조건 [3]\n')
    # Keep unresolved observations verbatim and make their interpretation visible in the verdict.
    # This does not infer which conflicting measurement/input is correct.
    for issue in decisions.get('pendingAnnotations',[]):
        addr=issue['cell'];rs=cp(changes.get(addr,{}).get('runs') or q.runs(q.cells[MAIN].get(addr)))
        size=max([float(v['font'].get('sz',{}).get('val',10)) for v in rs] or [10])
        font={'rFont':{'val':'맑은 고딕'},'sz':{'val':str(size+2)},'color':{'rgb':'FF0070C0'},'u':{'val':'single'}}
        rs.append({'text':'\n'+issue['text']+'\n','font':font})
        changes[addr]={'runs':tidy_runs(rs),'reason':'조건과 결과의 불일치 확인 대기 표시','test':issue['test']}
        vaddr=issue['verdictCell'];changes[vaddr]={'runs':[{'text':'\n확인 필요\n','font':{**font,'sz':{'val':'12'}}}],'reason':'조건/결과 확인 전 판정 보류','test':issue['test']}
    for addr,addition in decisions.get('resultNotes',{}).items():
        rs=cp(changes.get(addr,{}).get('runs') or q.runs(q.cells[MAIN].get(addr)))
        font={'rFont':{'val':'맑은 고딕'},'sz':{'val':'10'},'color':{'rgb':'FF000000'}}
        if addition.startswith('[E8300 보완]'):font.update({'sz':{'val':'12'},'color':{'rgb':'FF0070C0'},'u':{'val':'single'}})
        rs.append({'text':'\n'+addition+'\n','font':font})
        changes[addr]={'runs':tidy_runs(rs),'reason':'첨부 화면의 확인 범위와 미비 자료 구분','test':next(x['id'] for x in st['tests'] if x['start']<=ref(addr)[1]<=x['end'])}
    support=readj('H/support-update-plan.json')
    for ac in support['cellTextActions']:
        if ac['sheet']==MAIN:
            addr=ac['sourceCell'];changes[addr]={'runs':tidy_runs([{'text':ac['proposedText'],'font':q.runs(q.cells[MAIN][addr])[0]['font']}]),'reason':ac['reason'],'test':73}
    # Map every surviving original row to its new physical range. All additions are photo continuation rows.
    rowmap={};rows=[];pics=[];nextrow=1
    for oldrow in range(1,303):
        if oldrow in deleted:continue
        photos=st['photos'].get(str(oldrow),[])
        rs=changes.get('K'+str(oldrow),{}).get('runs',q.runs(q.cells[MAIN].get('K'+str(oldrow))))
        textpt=measure(rs) if photos else 0
        separate=bool(photos and textpt+226.772+14>409.5)
        n=max(1,len(photos)+(1 if separate else 0));rowmap[str(oldrow)]={'first':nextrow,'last':nextrow+n-1}
        orig=q.sheets[MAIN].find(f's:sheetData/s:row[@r="{oldrow}"]',NS);oldht=float(orig.get('ht','15')) if orig is not None else 15
        for j in range(n):
            ht=oldht
            if photos:ht=min(409.5,max(textpt if j==0 else 0,15)) if separate and j==0 else (max(244,textpt+226.772+12) if j==0 else 244)
            elif j==0 and 34<=oldrow<=284:
                for col in 'JK':
                    content=changes.get(col+str(oldrow),{}).get('runs',q.runs(q.cells[MAIN].get(col+str(oldrow))))
                    ht=max(ht,min(409.5,measure(content)))
            rows.append({'old':oldrow,'new':nextrow+j,'index':j,'height':ht})
        for j,f in enumerate(photos):
            pics.append({**f,'row':nextrow+j+(1 if separate else 0),'column':10,'offsetPx':(textpt+4)*96/72 if j==0 and not separate else 8,'heightPx':8*96/2.54,'widthPx':8*96/2.54*f['width']/f['height']})
        nextrow+=n
    merges=[]
    for mr in st['baseMerges']:
        c1,r1,c2,r2=span(mr);kept=[r for r in range(r1,r2+1) if str(r) in rowmap]
        if not kept:continue
        nr1=rowmap[str(kept[0])]['first'];nr2=rowmap[str(kept[-1])]['last']
        # Result fields receive one photo per cell instead of a merged multi-photo area.
        if c1==c2=='K' and any(st['photos'].get(str(v)) for v in kept):continue
        if nr1!=nr2 or c1!=c2:merges.append(f'{c1}{nr1}:{c2}{nr2}')
    for old,rm in rowmap.items():
        if rm['first']==rm['last']:continue
        for c in 'BCDEFGHIJL':
            occupied=any(span(m)[0]<=c<=span(m)[2] and span(m)[1]<=rm['first']<=span(m)[3] for m in merges)
            if not occupied:merges.append(f'{c}{rm["first"]}:{c}{rm["last"]}')
    for addr,ch in changes.items():
        if str(ref(addr)[1]) not in rowmap:continue
        ch['newAddress']=ref(addr)[0]+str(rowmap[str(ref(addr)[1])]['first'])
        ch['oldText']=q.value(addr);ch['newText']=text(ch['runs'])
        if ch['oldText']!=ch['newText']:log.append({'test':ch['test'],'beforeCell':addr,'afterCell':ch['newAddress'],'old':ch['oldText'],'new':ch['newText'],'reason':ch['reason']})
    for row in sorted(deleted):
        for col in 'JK':
            v=q.value(f'{col}{row}')
            if v.strip():log.append({'test':next(x['id'] for x in st['tests'] if x['start']<=row<=x['end']),'beforeCell':f'{col}{row}','afterCell':'삭제','old':v,'new':'','reason':'사용자 삭제 셀/조건 행 제거'})
    other={};aux='시험 보조자료';auxdel={x['sourceRow'] for x in support['auxiliaryDeleteRows']}
    auxmap={str(old):old-sum(v<old for v in auxdel) for old in range(1,299) if old not in auxdel}
    def plain_runs(s,before):
        # Preserve each surviving character's rich-text formatting through a textual cleanup.
        from difflib import SequenceMatcher
        out=[];s0=text(before)
        for op,i,j,a,z in SequenceMatcher(None,s0,s,autojunk=False).get_opcodes():
            if op=='equal':out+=cut(cut(before,0,i),j-i,len(s0)-i)
            elif op in ['insert','replace']:
                pos=0;f=before[0]['font'] if before else {'rFont':{'val':'맑은 고딕'},'sz':{'val':'10'}}
                for rr in before:
                    if pos+len(rr['text'])>i:f=rr['font'];break
                    pos+=len(rr['text'])
                out.append({'text':s[a:z],'font':cp(f)})
        return tidy_runs(out)
    def other_set(sheet,addr,rs,reason,test=None):
        oldrow=ref(addr)[1]
        if sheet==aux and oldrow in auxdel:return
        na=ref(addr)[0]+str(auxmap[str(oldrow)]) if sheet==aux else addr
        old=q.value(addr,sheet);new=text(rs)
        if old==new:return
        other.setdefault(sheet,{})[addr]={'newAddress':na,'runs':rs,'oldText':old,'newText':new,'reason':reason,'test':test}
    for ac in support['cellTextActions']:
        if ac['sheet']==MAIN:continue
        sh,addr=ac['sheet'],ac['sourceCell'];other_set(sh,addr,plain_runs(ac['proposedText'],q.runs(q.cells[sh][addr])),ac['reason'])
    mapping=readj('aux-mapping.json')['mapped']
    if (HERE/'H/aux-unmatched-map.json').exists():mapping+=readj('H/aux-unmatched-map.json')['mapped']
    for mp in mapping:
        ar,mr,test=mp['aux'],mp['main'],mp['test']
        if ar in auxdel or mr in deleted:continue
        baddr,caddr,daddr=f'B{ar}',f'C{ar}',f'D{ar}'
        bmain=changes.get('J'+str(mr),{}).get('runs',q.runs(q.cells[MAIN].get('J'+str(mr))))
        kmain=cp(changes.get('K'+str(mr),{}).get('runs',q.runs(q.cells[MAIN].get('K'+str(mr)))))
        if not text(bmain).strip() or not text(kmain).strip():continue
        existing=other.get(aux,{}).get(caddr,{}).get('newText',q.value(caddr,aux))
        extra=existing.split('사진/자료 안내',1)[-1].strip() if '사진/자료 안내' in existing else ''
        photoitems=[ph for ph in pics if ph['test']==test and ph['verification']==int(re.search(r'조건\s*\[\s*(\d+)\s*\]',text(bmain))[1])] if re.search(r'조건\s*\[\s*(\d+)\s*\]',text(bmain)) else []
        if photoitems:
            extra='사진 '+str(len(photoitems))+'장 첨부: '+MAIN+'!'+', '.join('K'+str(v['row']) for v in photoitems)
        if extra:kmain+= [{'text':'\n사진/자료 안내\n'+extra+'\n','font':{'rFont':{'val':'맑은 고딕'},'sz':{'val':'10'},'color':{'rgb':'FF000000'}}}]
        other_set(aux,baddr,tidy_runs(bmain),'현재 본문과 보조 조건 동기화',test)
        other_set(aux,caddr,tidy_runs(kmain),'현재 본문과 보조 결과/사진 상태 동기화',test)
        tstart=next(v['start'] for v in st['tests'] if v['id']==test)
        vr=cp(changes.get('L'+str(tstart),{}).get('runs',q.runs(q.cells[MAIN].get('L'+str(tstart)))))
        if photoitems:vr+=[{'text':'\n사진 '+str(len(photoitems))+'장 본문 첨부\n','font':{'rFont':{'val':'맑은 고딕'},'sz':{'val':'10'},'color':{'rgb':'FF000000'}}}]
        other_set(aux,daddr,tidy_runs(vr),'현재 본문 판정 및 사진 첨부 상태 동기화',test)
        if mp.get('originalId')!=test:other_set(aux,'A'+str(ar),tidy_runs([{'text':f'SW-FW-SYS-Test-ID-{test}','font':q.runs(q.cells[aux]['A'+str(ar)])[0]['font']}]),'요구 내용에 맞는 시험 ID 정정',test)
    for sh,items in other.items():
        for a,ch in items.items():log.append({'test':ch['test'],'beforeCell':sh+'!'+a,'afterCell':sh+'!'+ch['newAddress'],'old':ch['oldText'],'new':ch['newText'],'reason':ch['reason']})
    for ar in sorted(auxdel):
        old='\n'.join(q.value(c+str(ar),aux).strip() for c in 'ABCD')
        log.append({'test':None,'beforeCell':aux+'!'+str(ar),'afterCell':'삭제','old':old,'new':'','reason':'본문 삭제 조건의 보조 행 제거'})
    # A compact new section in the existing comparison sheet preserves the earlier history.
    compare=[];cr=307
    for item in log:
        compare.append({**item,'row':cr});cr+=1
    plan={'status':'prepared','baseSHA':digest(q.path),'personalSHA':digest(p.path),'deleteRows':sorted(deleted),'rowMap':rowmap,'rows':rows,'merges':merges,'changes':changes,'otherChanges':other,'auxDeleteRows':sorted(auxdel),'auxRowMap':auxmap,'photos':pics,'comparisons':compare,'lastRow':nextrow-1,'compareLastRow':cr-1,'decisions':decisions}
    savej('plan.json',plan)
    print(json.dumps({'changes':len(changes),'textChanges':len(log),'deleteRows':sorted(deleted),'photos':len(pics),'lastRow':nextrow-1},ensure_ascii=False))

class StyleBridge:
    def __init__(self,base):self.b=base;self.cache={};self.elementcache={}
    def append(self,section,node):
        def sig(el):return (E.QName(el).localname,tuple(sorted(el.attrib.items())),el.text or '',tuple(sig(c) for c in el))
        parent=self.b.styles.find(t(section));raw=sig(node)
        if section not in self.elementcache:self.elementcache[section]={sig(n):i for i,n in enumerate(parent)}
        cache=self.elementcache[section]
        if raw in cache:return cache[raw]
        parent.append(cp(node));parent.set('count',str(len(parent)));cache[raw]=len(parent)-1;return len(parent)-1
    def copy_style(self,source,sid):
        key=(str(source.path),sid)
        if key in self.cache:return self.cache[key]
        xf=cp(source.xfs[sid])
        for attr,sec in [('fontId','fonts'),('fillId','fills'),('borderId','borders')]:
            xf.set(attr,str(self.append(sec,source.styles.find(t(sec))[int(xf.get(attr,'0'))])))
        xf.set('xfId','0');idx=self.append('cellXfs',xf);self.cache[key]=idx;return idx
    def alter(self,sid,alignment=None,fill=None,border=None):
        xf=cp(self.b.xfs[sid])
        if alignment is not None:
            old=xf.find(t('alignment'))
            if old is not None:xf.remove(old)
            xf.append(cp(alignment));xf.set('applyAlignment','1')
        if fill is not None:xf.set('fillId',str(self.append('fills',fill)));xf.set('applyFill','1')
        if border is not None:xf.set('borderId',str(self.append('borders',border)));xf.set('applyBorder','1')
        return self.append('cellXfs',xf)

def integrate():
    q=Book(HERE/'qcc-source.xlsx');p=Book(HERE/'personal-source.xlsx');art=Book(HERE/'authored.xlsx')
    pl=readj('plan.json');audit=readj('edit-audit.json');bridge=StyleBridge(q);data=q.data.copy();main=q.sheets[MAIN]
    origrows={int(r.get('r')):r for r in main.find(t('sheetData'))};sd=main.find(t('sheetData'));sd.clear()
    # Derive alignments and reviewed-ID colour from the author's actual first test.
    align={}
    for col in 'BCDEFGHIJKL':
        pc=p.cells[MAIN].get(f'{col}34')
        if pc is not None:align[col]=cp(p.xfs[int(pc.get('s','0'))].find(t('alignment')))
    pink={}
    for mp in audit['testRowMapping']:
        pc=p.cells[MAIN].get('B'+str(mp['privateStart']))
        if pc is not None:
            xf=p.xfs[int(pc.get('s','0'))];fill=p.styles.find(t('fills'))[int(xf.get('fillId','0'))]
            fg=fill.find('s:patternFill/s:fgColor',NS)
            if fg is not None and fg.get('rgb','').upper() in ['FFFF99CC','FFFF00FF','FFFFC0CB','FFFF99FF','FFFFCCFF','FFFF66CC']:
                pink[mp['qccStart']]=fill
            # Exact author pink is taken by comparing with the first reviewed TEST ID.
            first=p.cells[MAIN]['B34'];ff=p.xfs[int(first.get('s','0'))].get('fillId')
            if xf.get('fillId')==ff:pink[mp['qccStart']]=fill
    for rr in pl['rows']:
        old,new,j=rr['old'],rr['new'],rr['index'];src=origrows.get(old)
        row=cp(src) if src is not None else E.Element(t('row'))
        row.set('r',str(new));row.set('ht',str(round(rr['height'],3)));row.set('customHeight','1')
        if j:
            for c in list(row):
                col,_=ref(c.get('r'));c.set('r',col+str(new))
                for v in list(c):c.remove(v)
                c.attrib.pop('t',None)
        else:
            for c in row:
                col,_=ref(c.get('r'));c.set('r',col+str(new))
                if c.find(t('f')) is not None:raise ValueError('Unexpected main formula')
        for c in row:
            col,_=ref(c.get('r'));oa=col+str(old)
            if j==0 and oa in pl['changes']:
                ch=pl['changes'][oa]
                assert art.value(ch['newAddress'])==ch['newText'],oa
                inline(c,ch['runs'])
            if 34<=old<=284 and col in align:
                fill=pink.get(old) if col=='B' else None
                mapped=next((mp for mp in audit['testRowMapping'] if mp['qccStart']<=old<=mp['qccEnd']),None)
                desired=align[col]
                if mapped:
                    pc=p.cells[MAIN].get(col+str(old+mapped['privateMinusQccOffset']))
                    if pc is not None:
                        actual=p.xfs[int(pc.get('s','0'))].find(t('alignment'))
                        if actual is not None:desired=actual
                else:
                    actual=q.xfs[int(c.get('s','0'))].find(t('alignment'))
                    if actual is not None:desired=actual
                c.set('s',str(bridge.alter(int(c.get('s','0')),alignment=desired,fill=fill)))
                if j==0:
                    rs=q.runs(c)
                    if text(rs).strip():inline(c,tidy_runs(rs))
        sd.append(row)
    # Explicit common-field merges cover added photo rows; each photo remains in an individual K cell.
    mc=main.find(t('mergeCells'));mc.clear()
    for merge in pl['merges']:E.SubElement(mc,t('mergeCell'),ref=merge)
    mc.set('count',str(len(mc)))
    cells={c.get('r'):c for c in sd.findall('s:row/s:c',NS)}
    rows={int(r.get('r')):r for r in sd}
    def cell_at(col,row):
        addr=f'{col}{row}'
        if addr not in cells:
            c=E.SubElement(rows[row],t('c'),r=addr);cells[addr]=c
        return cells[addr]
    # Repair merged borders. Hidden interior boundaries are removed; exterior borders remain.
    for merge in pl['merges']:
        c1,r1,c2,r2=span(merge)
        if r1<34:continue
        anchor=cells.get(c1+str(r1));sid=int(anchor.get('s','0')) if anchor is not None else 0
        bor=q.styles.find(t('borders'))[int(q.xfs[sid].get('borderId','0'))]
        for row in range(r1,r2+1):
            for colnum in range(ord(c1),ord(c2)+1):
                col=chr(colnum);c=cell_at(col,row);newbor=cp(bor)
                for edge,keep in [('top',row==r1),('bottom',row==r2),('left',col==c1),('right',col==c2)]:
                    edgeel=newbor.find(t(edge))
                    if not keep and edgeel is not None:edgeel.clear()
                c.set('s',str(bridge.alter(int(c.get('s',str(sid))),border=newbor)))
    for row in sd:
        ordered=sorted(list(row),key=lambda c:(len(ref(c.get('r'))[0]),ref(c.get('r'))[0]))
        row[:]=ordered
    main.find(t('dimension')).set('ref',f'A1:L{pl["lastRow"]}')
    parentcols=main.find(t('cols'))
    for col in list(parentcols):
        lo,hi=int(col.get('min')),int(col.get('max'))
        if lo<=11<=hi:
            idx=list(parentcols).index(col);parentcols.remove(col);parts=[]
            if lo<11:
                before=cp(col);before.set('max','10');parts.append(before)
            current=cp(col);current.set('min','11');current.set('max','11');current.set('width','70');current.set('customWidth','1');parts.append(current)
            if hi>11:
                after=cp(col);after.set('min','12');parts.append(after)
            for item in reversed(parts):parentcols.insert(idx,item)
    rb=main.find(t('rowBreaks'))
    if rb is not None:
        for br in rb:
            old=int(br.get('id'));kept=[r for r in range(1,old+1) if str(r) in pl['rowMap']]
            br.set('id',str(pl['rowMap'][str(kept[-1])]['last']))
    # Add only the image authoring package generated by Artifact Tool; preserve original drawings elsewhere.
    dp='xl/drawings/drawingPhotoAttach.xml';drp='xl/drawings/_rels/drawingPhotoAttach.xml.rels'
    dr=E.fromstring(art.data['xl/drawings/drawing1.xml']);rels=E.fromstring(art.data['xl/drawings/_rels/drawing1.xml.rels'])
    relmap={n.get('Id'):n for n in rels}
    assert len(dr)==len(pl['photos'])==123
    for i,(anc,photo) in enumerate(zip(dr,pl['photos']),1):
        rid=anc.find('.//a:blip',NS).get('{'+R+'}embed');rel=relmap[rid]
        rt=rel.get('Target');oldtarget=rt.lstrip('/') if rt.lstrip('/').startswith('xl/') else posixpath.normpath('xl/drawings/'+rt);newtarget=f'xl/media/sstr_attach_{i:03d}.jpg'
        content=art.data[oldtarget];assert hashlib.sha256(content).hexdigest().upper()==photo['sha256'].upper()
        data[newtarget]=content;rel.set('Target',f'../media/sstr_attach_{i:03d}.jpg')
        prop=anc.find('.//x:cNvPr',NS);prop.set('name',photo['name']);prop.set('descr',photo['name'])
        ext=anc.find('x:ext',NS);ext.set('cy','2880000');ext.set('cx',str(round(2880000*photo['width']/photo['height'])))
    data[dp]=dump(dr);data[drp]=dump(rels)
    mrp=posixpath.dirname(q.paths[MAIN])+'/_rels/'+posixpath.basename(q.paths[MAIN])+'.rels'
    mrels=E.fromstring(data[mrp]) if mrp in data else E.Element('{'+P+'}Relationships',nsmap={None:P})
    E.SubElement(mrels,'{'+P+'}Relationship',Id='rIdPhotoAttach',Type=R+'/drawing',Target='../drawings/drawingPhotoAttach.xml')
    main.append(E.Element(t('drawing'),{'{'+R+'}id':'rIdPhotoAttach'}));data[mrp]=dump(mrels)
    ct=E.fromstring(data['[Content_Types].xml']);C='http://schemas.openxmlformats.org/package/2006/content-types'
    if not any(n.get('Extension')=='jpg' for n in ct):E.SubElement(ct,'{'+C+'}Default',Extension='jpg',ContentType='image/jpeg')
    E.SubElement(ct,'{'+C+'}Override',PartName='/'+dp,ContentType='application/vnd.openxmlformats-officedocument.drawing+xml');data['[Content_Types].xml']=dump(ct)
    for sh,items in pl.get('otherChanges',{}).items():
        for old,ch in items.items():
            assert art.value(ch['newAddress'],sh)==ch['newText'],(sh,old)
            inline(q.cells[sh][old],ch['runs'])
    aux='시험 보조자료';asheet=q.sheets[aux];asd=asheet.find(t('sheetData'));arm=pl['auxRowMap']
    for row in list(asd):
        old=int(row.get('r'))
        if old in pl['auxDeleteRows']:asd.remove(row);continue
        if str(old) not in arm:continue
        new=arm[str(old)];row.set('r',str(new))
        for c in row:c.set('r',ref(c.get('r'))[0]+str(new))
        needed=float(row.get('ht','15'))
        for c in row:
            col=ref(c.get('r'))[0]
            if col in ['B','C','D']:needed=max(needed,measure(q.runs(c),width={'B':950,'C':1075,'D':100}[col]))
        row.set('ht',str(min(409.5,needed)));row.set('customHeight','1')
    for tag,attr in [('mergeCells','ref')]:
        parent=asheet.find(t(tag))
        if parent is not None:
            for node in list(parent):
                c1,r1,c2,r2=span(node.get(attr));kept=[r for r in range(r1,r2+1) if str(r) in arm]
                if not kept:parent.remove(node);continue
                node.set(attr,f'{c1}{arm[str(kept[0])]}:{c2}{arm[str(kept[-1])]}')
            parent.set('count',str(len(parent)))
    asheet.find(t('dimension')).set('ref',f'A1:D{max(arm.values())}')
    for name in ['시험 보조자료','예상 그림','사이버보안 시험 연계']:data[q.paths[name]]=dump(q.sheets[name])
    # Append the dated delta to the existing comparison sheet, leaving its earlier history intact.
    co=q.sheets[COMP];cosd=co.find(t('sheetData'));comc=co.find(t('mergeCells'))
    if comc is None:comc=E.SubElement(co,t('mergeCells'))
    for row in art.sheets[COMP].find(t('sheetData')):
        if int(row.get('r'))<305:continue
        newrow=cp(row)
        for c in newrow:
            source=art.cells[COMP][c.get('r')];c.set('s',str(bridge.copy_style(art,int(c.get('s','0')))));inline(c,art.runs(source))
        cosd.append(newrow)
    for mm in art.sheets[COMP].findall('s:mergeCells/s:mergeCell',NS):comc.append(cp(mm))
    comc.set('count',str(len(comc)));co.find(t('dimension')).set('ref',f'A1:L{pl["compareLastRow"]}')
    # Both signs occur only on the dedicated comparison sheet and have distinct font colours.
    cocells={c.get('r'):c for c in cosd.findall('s:row/s:c',NS)}
    for item in pl['comparisons']:
        c=cocells[f'E{item["row"]}'];font={'name':{'val':'맑은 고딕'},'sz':{'val':'10'}}
        inline(c,[{'text':'\n[-] '+item['old'].strip()+'\n','font':{**font,'color':{'rgb':'FFC00000'}}},{'text':'[+] '+(item['new'].strip() or '(삭제)')+'\n','font':{**font,'color':{'rgb':'FF008000'}}}])
    for dn in q.wb.findall('s:definedNames/s:definedName',NS):
        if dn.get('name')=='_xlnm.Print_Area':
            if dn.get('localSheetId')==str(list(q.paths).index(MAIN)):dn.text=f"'{MAIN}'!$B$2:$L${pl['lastRow']}"
            if dn.get('localSheetId')==str(list(q.paths).index(COMP)):dn.text=f"'{COMP}'!$B$2:$L${pl['compareLastRow']}"
            if dn.get('localSheetId')==str(list(q.paths).index(aux)):dn.text=f"'{aux}'!$A$1:$D${max(arm.values())}"
    data[q.paths[MAIN]]=dump(main);data[q.paths[COMP]]=dump(co);data['xl/styles.xml']=dump(q.styles);data['xl/workbook.xml']=dump(q.wb)
    with ZipFile(HERE/'candidate.xlsx','w',ZIP_DEFLATED) as z:
        for name,content in data.items():z.writestr(name,content)
    print(json.dumps({'candidateSHA':digest(HERE/'candidate.xlsx'),'photos':len(dr),'newRows':len(sd),'reviewedPinkIDs':len(pink)}))

if __name__=='__main__':
    {'prepare':prepare,'plan':plan,'integrate':integrate}[sys.argv[1]]()
