import package as m
from copy import deepcopy as cp
from lxml import etree as E
from zipfile import ZipFile, ZIP_DEFLATED
from pathlib import Path
import re,json,sys,math,os

H=m.HERE; MAIN=m.MAIN; COMP=m.COMP; AUX='시험 보조자료'
def trim(rs,pad=False):
    rs=cp(rs);s=m.text(rs);start=len(s)-len(s.lstrip('\r\n'));end=len(s.rstrip('\r\n'))
    if end<=start:return []
    rs=m.cut(rs,end,len(s));rs=m.cut(rs,0,start)
    if pad:rs[0]['text']='\n'+rs[0]['text'];rs[-1]['text']+='\n'
    return rs
def green(f):
    color=f.get('color',{}).get('rgb','').upper()[-6:]
    u=f.get('u');return color in ['008000','00B050','00FF00'] and bool(u) and not(isinstance(u,dict) and u.get('val')=='none')
def normalize(rs,pad=False,ten=False,black=False):
    rs=trim(rs,pad)
    for r in rs:
        if black and green(r['font']):r['font']['color']={'rgb':'FF000000'};r['font']['u']={'val':'none'}
        if ten:r['font']['sz']={'val':'10'}
    return rs
def remove_notes(rs,result=False):
    prefixes=['[QCC/E8300 자료 입력]']+(['[E8300 보완]'] if result else [])
    for prefix in prefixes:
        while prefix in m.text(rs):
            s=m.text(rs);a=s.index(prefix);z=s.find('\n',a)
            rs=m.cut(rs,a,len(s) if z<0 else z+1)
    return rs
def ensure_cell(b,sh,addr):
    if addr in b.cells[sh]:return b.cells[sh][addr]
    nr=m.ref(addr)[1];sd=b.sheets[sh].find(m.t('sheetData'));row=next((r for r in sd if int(r.get('r'))==nr),None)
    if row is None:row=E.SubElement(sd,m.t('row'),r=str(nr))
    c=E.SubElement(row,m.t('c'),r=addr);b.cells[sh][addr]=c;return c
def savebook(b,path):
    data=b.data.copy()
    for sh,root in b.sheets.items():data[b.paths[sh]]=m.dump(root)
    data['xl/styles.xml']=m.dump(b.styles);data['xl/workbook.xml']=m.dump(b.wb)
    with ZipFile(path,'w',ZIP_DEFLATED) as z:
        for name,content in data.items():z.writestr(name,content)
def fontstyle(b,bridge,sid,black=False):
    xf=cp(b.xfs[sid]);f=cp(b.fonts[int(xf.get('fontId','0'))]);fd=m.fontdict(f)
    sz=f.find(m.t('sz'))
    if sz is None:sz=E.SubElement(f,m.t('sz'))
    sz.set('val','10')
    if black and green(fd):
        for tag in ['color','u']:
            old=f.find(m.t(tag))
            if old is not None:f.remove(old)
        E.SubElement(f,m.t('color'),rgb='FF000000')
    xf.set('fontId',str(bridge.append('fonts',f)));xf.set('applyFont','1')
    return bridge.append('cellXfs',xf)

def prepare():
    b=m.Book(H/'style-before.xlsx');p=m.Book(H/'style-personal.xlsx');src=m.readj('style-source-map.json');bridge=m.StyleBridge(b)
    pink=src['pinkSelection']['testIDs'];maps=src['headerMappings']+src['conditionMappings']+src['continuationMappings']
    overrides=[]
    for v in maps:
        addr=v['targetCell'];rs=p.runs(p.cells[MAIN].get(v['sourceCell']));proposed=v['proposedText']
        if addr in ['I158','J170']:
            rs=b.runs(b.cells[MAIN][addr]);overrides.append(addr)
        elif m.text(rs)!=proposed:
            from difflib import SequenceMatcher
            s=m.text(rs)
            out=[]
            for op,i,j,k,l in SequenceMatcher(None,s,proposed,autojunk=False).get_opcodes():
                if op=='equal':out+=m.cut(m.cut(rs,j,len(s)),0,i)
                elif op in ['insert','replace']:out.append({'text':proposed[k:l],'font':cp(rs[0]['font'])})
            rs=out
        c=ensure_cell(b,MAIN,addr);m.inline(c,rs)
        align=v['source'].get('alignment')
        if align:c.set('s',str(bridge.alter(int(c.get('s','0')),alignment=E.Element(m.t('alignment'),**align))))
    corrections={'J161':[('조검 [','조건 [')],'J256':[('통과 해아','통과해야')],
      'G234':[(', f_sync/f_close 실패',' 실패')],
      'I182':[('기존 명령 처리 중 새 명령 수신 조건은 별도로 확인한다.\n','')]}
    for addr,items in corrections.items():
        rs=b.runs(b.cells[MAIN][addr])
        for old,new in items:
            if old in m.text(rs):rs=m.replace_literal(rs,old,new)
        m.inline(b.cells[MAIN][addr],rs);overrides.append(addr)
    # Joint QCC additions are outside the author's confirmed conditions restored above.
    # Remove only their obsolete wrapper; retain observed measurements and actual photo scope notes.
    for tt in b.tests():
        if tt['id'] not in pink:continue
        for r in range(tt['start'],tt['end']+1):
            a='K'+str(r);c=b.cells[MAIN].get(a)
            if c is None:continue
            rs=b.runs(c);s=m.text(rs)
            if '[QCC/E8300 자료 입력]' in s:
                for line in ['기존 E8300 관찰/작성자 확인 기록(과거 범위):\n','현재 추가/수정 기준의 실제 QCC 결과: 미실행/미수집.\n']:
                    if line in m.text(rs):rs=m.replace_literal(rs,line,'')
                m.inline(c,remove_notes(rs,result=True))
    for v in src['resultVerdictReview']['testVerdictMappings']:
        if v['test'] in [15,16,69,74,78]:m.inline(b.cells[MAIN][v['targetCell']],p.runs(p.cells[MAIN][v['sourceCell']]))
    finish_prepare(b,pink,[],{'mappingFile':'style-source-map.json','mappingSHA':m.digest(H/'style-source-map.json'),'priorityOverrides':overrides,'verdictSourceTests':[15,16,69,74,78],'pureQCCPendingVerdictsPreserved':[54,59]})

def finish_prepare(b,pink,content_changes,source_map):
    before=m.Book(H/'style-before.xlsx');bridge=m.StyleBridge(b);tests=b.tests();testrow={r:t['id'] for t in tests for r in range(t['start'],t['end']+1)}
    # Synchronize existing auxiliary rows to the current condition and observed result.
    oldplan=m.readj('plan.json');auxmaps=m.readj('aux-mapping.json')['mapped']+m.readj('H/aux-unmatched-map.json')['mapped'];auxchanged=[]
    for v in auxmaps:
        if v['test'] not in pink or str(v['aux']) not in oldplan['auxRowMap'] or str(v['main']) not in oldplan['rowMap']:continue
        ar=oldplan['auxRowMap'][str(v['aux'])];mr=oldplan['rowMap'][str(v['main'])]['first']
        for ac,mc in [('B','J'),('C','K')]:
            addr=f'{ac}{ar}';rs=b.runs(b.cells[MAIN].get(f'{mc}{mr}'));oldrs=b.runs(b.cells[AUX].get(addr));s=m.text(oldrs)
            if ac=='C' and '사진/자료 안내' in s:
                a=s.index('사진/자료 안내');extra=m.cut(oldrs,0,a)
                rs=cp(rs)+[{'text':'\n','font':cp(rs[-1]['font'] if rs else oldrs[0]['font'])}]+extra
            rs=remove_notes(rs,result=ac=='C');rs=normalize(rs,black=True)
            m.inline(ensure_cell(b,AUX,addr),rs);auxchanged.append(addr)
        # Only mirror a verdict if the author-confirmed verdict actually changed.
        test=next(t for t in tests if t['id']==v['test']);la='L'+str(test['start'])
        if before.value(la)!=b.value(la):
            rs=trim(b.runs(b.cells[MAIN][la]));oldrs=b.runs(b.cells[AUX].get(f'D{ar}'));s=m.text(oldrs)
            ph=re.search(r'사진 \d+장 본문 첨부',s)
            if ph:rs.append({'text':'\n'+ph[0],'font':{'name':{'val':'맑은 고딕'},'sz':{'val':'10'},'color':{'rgb':'FF000000'}}})
            m.inline(ensure_cell(b,AUX,f'D{ar}'),rs)
    for addr,verdict in [('D212','확인'),('D235','판정 대기')]:
        rs=b.runs(b.cells[AUX][addr]);s=m.text(rs)
        if '통과' in s:rs=m.replace_literal(rs,'통과',verdict)
        m.inline(b.cells[AUX][addr],rs)
    # All other sheets lose outer Enter padding. Body rows alone retain one blank line each side.
    stylecache={};changes=[];fontcells=0;outercells=0;blackcells=0
    for sh,cells in b.cells.items():
        for addr,c in cells.items():
            col,row=m.ref(addr);body=sh==MAIN and 34<=row<=293;reviewed=body and testrow.get(row) in pink
            rs=b.runs(c);original=m.text(rs)
            if body:
                sid=int(c.get('s','0'));key=(sid,reviewed)
                if key not in stylecache:stylecache[key]=fontstyle(b,bridge,sid,black=reviewed)
                c.set('s',str(stylecache[key]));fontcells+=1
            istext=c.get('t') in ['s','inlineStr','str']
            if istext and c.find(m.t('f')) is None:
                if reviewed:rs=remove_notes(rs,result=col=='K')
                if reviewed and any(green(r['font']) for r in rs):blackcells+=1
                rs=normalize(rs,pad=body,ten=body,black=reviewed)
                if not body and original!=m.text(rs):outercells+=1
                m.inline(c,rs)
            old=before.value(addr,sh);new=b.value(addr,sh)
            if old!=new or (body and b.runs(c)!=before.runs(before.cells[sh].get(addr))):
                changes.append({'sheet':sh,'cell':addr,'old':old,'new':new,'runs':b.runs(c),'substantive':old.strip()!=new.strip()})
    # Recompute text/photo row heights without inserting/removing any rows.
    main=b.sheets[MAIN];rows={int(r.get('r')):r for r in main.find(m.t('sheetData'))}
    merges=[m.span(v.get('ref')) for v in main.findall('s:mergeCells/s:mergeCell',m.NS)]
    widths={c['label']:c['width_pt'] for sh in m.readj('style-layout-widths.json')['sheets'] if sh['name']==MAIN for c in sh['columns']}
    def cell_span(a):
        c,r=m.ref(a)
        return next((z for z in merges if z[0]==c and z[1]==r),(c,r,c,r))
    heights={r:30.0 for r in range(34,294)};requirements=[]
    for a,c in b.cells[MAIN].items():
        col,row=m.ref(a)
        if not(34<=row<=293) or not b.value(a).strip():continue
        c1,r1,c2,r2=cell_span(a)
        width=sum(widths[chr(n)] for n in range(ord(c1),ord(c2)+1))*96/72-12
        needed=m.measure(b.runs(c),max(20,width))
        if r1==r2:heights[row]=max(heights[row],needed)
        else:requirements.append((a,r1,r2,needed))
    dpath='xl/drawings/drawing1.xml';drawing=E.fromstring(b.data[dpath]);photo_positions=[]
    for anc in drawing:
        fr=anc.find('x:from',m.NS);r=int(fr.findtext('x:row',namespaces=m.NS))+1;a='K'+str(r);rs=b.runs(b.cells[MAIN].get(a))
        off=(m.measure(rs,widths['K']*96/72-12)+4) if m.text(rs).strip() else 6
        fr.find('x:rowOff',m.NS).text=str(round(off*12700));ext=anc.find('x:ext',m.NS);ext.set('cy','2880000')
        bottom=off+2880000/12700+8;heights[r]=max(heights[r],bottom)
        photo_positions.append({'name':anc.find('.//x:cNvPr',m.NS).get('name'),'row':r,'offsetPt':off,'bottomPt':bottom})
    # Distribute a merged cell's shortfall over its rows; cap at Excel's native maximum.
    for a,r1,r2,needed in sorted(requirements,key=lambda v:v[2]-v[1]):
        missing=needed-sum(heights[r] for r in range(r1,r2+1))
        while missing>.01:
            available=[r for r in range(r1,r2+1) if heights[r]<409.5]
            if not available:raise ValueError(('merged text exceeds height',a,needed))
            step=missing/len(available)
            for r in available:
                inc=min(step,409.5-heights[r]);heights[r]+=inc;missing-=inc
    for r,ht in heights.items():
        if ht>409.5:raise ValueError(('row exceeds Excel max',r,ht))
        rows[r].set('ht',str(round(ht,2)));rows[r].set('customHeight','1')
        if rows[r].get('s'):rows[r].set('s',str(fontstyle(b,bridge,int(rows[r].get('s')),black=False)))
    b.data[dpath]=m.dump(drawing)
    # Current comparison histories are preserved; append only substantive content changes and one format rule.
    substantive=[v for v in changes if v['substantive'] and not v['sheet'].endswith('_비교')]
    last=max(int(r.get('r')) for r in b.sheets[COMP].find(m.t('sheetData')));header=last+2
    for i,v in enumerate(substantive):v['compareRow']=header+2+i
    pl={'baselineSHA':m.digest(before.path),'personalSHA':m.digest(H/'style-personal.xlsx'),'pinkIDs':pink,'changes':changes,'contentChanges':substantive,'headerRow':header,'lastRow':header+2+len(substantive),'fontCells':fontcells,'outsidePaddingRemovedCells':outercells,'greenResetCells':blackcells,'heights':heights,'photoPositions':photo_positions,'sourceMap':source_map}
    m.savej('style-plan.json',pl);savebook(b,H/'style-prepared.xlsx')
    print(json.dumps({k:pl[k] for k in ['fontCells','outsidePaddingRemovedCells','greenResetCells','headerRow','lastRow']}));print('substantive changes',len(substantive))

def integrate():
    b=m.Book(H/'style-prepared.xlsx');a=m.Book(H/'style-authored.xlsx');pl=m.readj('style-plan.json');bridge=m.StyleBridge(b)
    for ch in pl['changes']:assert a.value(ch['cell'],ch['sheet'])==ch['new'],(ch['sheet'],ch['cell'])
    co=b.sheets[COMP];sd=co.find(m.t('sheetData'));mc=co.find(m.t('mergeCells'))
    for row in a.sheets[COMP].find(m.t('sheetData')):
        if int(row.get('r'))<pl['headerRow']:continue
        nr=cp(row)
        for c in nr:
            source=a.cells[COMP][c.get('r')];c.set('s',str(bridge.copy_style(a,int(c.get('s','0')))));m.inline(c,trim(a.runs(source)))
        sd.append(nr)
    for mm in a.sheets[COMP].findall('s:mergeCells/s:mergeCell',m.NS):mc.append(cp(mm))
    added={c.get('r'):c for c in sd.findall('s:row/s:c',m.NS)}
    for ch in pl['contentChanges']:
        c=added['E'+str(ch['compareRow'])];font={'name':{'val':'맑은 고딕'},'sz':{'val':'10'}}
        m.inline(c,[{'text':'[-] '+ch['old'].strip()+'\n','font':{**font,'color':{'rgb':'FFC00000'}}},
                    {'text':'[+] '+(ch['new'].strip() or '(삭제)'),'font':{**font,'color':{'rgb':'FF008000'}}}])
    mc.set('count',str(len(mc)));co.find(m.t('dimension')).set('ref',f'A1:L{pl["lastRow"]}')
    for dn in b.wb.findall('s:definedNames/s:definedName',m.NS):
        if dn.get('name')=='_xlnm.Print_Area' and dn.get('localSheetId')==str(list(b.paths).index(COMP)):dn.text=f"'{COMP}'!$B$2:$L${pl['lastRow']}"
    savebook(b,H/'style-after.xlsx');print('candidate',m.digest(H/'style-after.xlsx'))

if __name__=='__main__':
    if sys.argv[1]=='prepare':prepare()
    if sys.argv[1]=='integrate':integrate()
