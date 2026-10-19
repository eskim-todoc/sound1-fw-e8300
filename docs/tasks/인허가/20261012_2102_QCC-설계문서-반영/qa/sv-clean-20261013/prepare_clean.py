"""Prepare Artifact Tool authoring, then preserve original OOXML in local candidates.

Never writes a G source. Root owns backup, Artifact authoring and native Office QA.
"""
from pathlib import Path
from lxml import etree as E
import argparse,copy,hashlib,json,posixpath,re,uuid,zipfile

H=Path(__file__).resolve().parent
NS='http://schemas.openxmlformats.org/spreadsheetml/2006/main'
N={'x':NS}; RN='http://schemas.openxmlformats.org/officeDocument/2006/relationships'
PK='http://schemas.openxmlformats.org/package/2006/relationships'
CT='http://schemas.openxmlformats.org/package/2006/content-types'
GREEN={'008000','FF008000'}
SUPPORTED={'drawing','comments','vmlDrawing','printerSettings','image','hyperlink'}
MARK=re.compile(r'\[([+-])\]')
def tag(s):return '{'+NS+'}'+s
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def load(p):return json.loads(Path(p).read_text(encoding='utf-8'))
def save(p,o):Path(p).write_text(json.dumps(o,ensure_ascii=False,indent=2),encoding='utf-8')
def pack(path):
    with zipfile.ZipFile(path) as z:
        assert z.testzip() is None
        return {i.filename:z.read(i.filename) for i in z.infolist()},z.infolist()
def xml(b):return E.fromstring(b)
def dump(n):return E.tostring(n,encoding='UTF-8',xml_declaration=True,standalone=True)
def texts(n):return ''.join(n.xpath('.//x:t/text()',namespaces=N))
def relpart(p):return posixpath.join(posixpath.dirname(p),'_rels',posixpath.basename(p)+'.rels')
def target(p,t):return posixpath.normpath(t[1:] if t.startswith('/') else posixpath.join(posixpath.dirname(p),t))
def relative(p,t):return posixpath.relpath(t,posixpath.dirname(p))
def sheets(d):
    links={a.get('Id'):a.get('Target') for a in xml(d['xl/_rels/workbook.xml.rels'])}
    return [{'name':s.get('name'),'id':s.get('sheetId'),'index':i,'part':target('xl/workbook.xml',links[s.get('{'+RN+'}id')])} for i,s in enumerate(xml(d['xl/workbook.xml']).find('x:sheets',N))]
def sst(d):return list(xml(d['xl/sharedStrings.xml'])) if 'xl/sharedStrings.xml' in d else []
def container(c,ss):return ss[int(c.findtext('x:v',namespaces=N))] if c.get('t')=='s' else c.find('x:is',N)
def value(c,ss):
    co=container(c,ss)
    return texts(co) if co is not None else c.findtext('x:v',namespaces=N) or ''
def cellmap(d,p):
    ss=sst(d);r=xml(d[p])
    return r,{c.get('r'):c for c in r.findall('.//x:sheetData/x:row/x:c',N)},ss
def intervals(v,code):
    """Return accepted original-character slices without normalizing whitespace."""
    spans=[];offset=0;old=False
    for line in v.splitlines(keepends=True):
        m=MARK.match(line)
        if m:
            assert line.startswith(('[-] ','[+] ')),('unsupported marker separator',code,line)
            old=m[1]=='-'
            if not old:spans.append((offset+4,offset+len(line)))
        elif MARK.search(line):raise AssertionError(('unanchored marker',code,line))
        elif not (code=='SRS' and old):spans.append((offset,offset+len(line)))
        offset+=len(line)
    accepted=''.join(v[a:b] for a,b in spans)
    if code=='SRS' and not accepted.strip():return [],''
    return spans,accepted
def green(co):return co is not None and any(a.get('rgb','').upper() in GREEN for a in co.findall('x:r/x:rPr/x:color',N))
def basecolor(d,c):
    r=xml(d['xl/styles.xml']);xf=r.find('x:cellXfs',N)[int(c.get('s','0'))]
    f=r.find('x:fonts',N)[int(xf.get('fontId','0'))];color=f.find('x:color',N)
    return copy.deepcopy(color) if color is not None else E.Element(tag('color'),rgb='FF000000')
def accepted_cell(d,c,ss,code):
    co=container(c,ss);before=value(c,ss);spans,after=intervals(before,code)
    assert c.find('x:f',N) is None,('text patch must not change formula',code,c.get('r'))
    new=copy.deepcopy(c)
    for a in list(new):
        if E.QName(a).localname in ['is','v']:new.remove(a)
    new.set('t','inlineStr');isn=E.Element(tag('is'))
    # Keep is before extLst; x:c child order is f,v,is,extLst.
    ext=new.find('x:extLst',N)
    if ext is None:new.append(isn)
    else:new.insert(list(new).index(ext),isn)
    source_runs=co.findall('x:r',N) if co is not None else []
    if not source_runs:
        if after:
            t=E.SubElement(isn,tag('t'));t.set('{http://www.w3.org/XML/1998/namespace}space','preserve');t.text=after
    else:
        offset=0
        for src in source_runs:
            t=src.findtext('x:t',namespaces=N) or '';end=offset+len(t)
            for a,b in spans:
                lo=max(a,offset);hi=min(b,end)
                if hi<=lo:continue
                run=copy.deepcopy(src);tt=run.find('x:t',N);tt.text=t[lo-offset:hi-offset]
                rp=run.find('x:rPr',N)
                col=rp.find('x:color',N) if rp is not None else None
                if col is not None and col.get('rgb','').upper() in GREEN:
                    at=list(rp).index(col);rp.remove(col);rp.insert(at,basecolor(d,c))
                isn.append(run)
            offset=end
    assert texts(isn)==after
    assert all(texts(r) for r in isn.findall('x:r',N)),('empty rich run',code,c.get('r'))
    return new,after
def walkrels(d,start):
    seen=set();out=[]
    def visit(p):
        if p in seen:return
        seen.add(p)
        rp=relpart(p)
        if rp not in d:return
        for a in xml(d[rp]):
            kind=a.get('Type').rsplit('/',1)[-1]
            assert kind in SUPPORTED,('unsupported relationship',start,p,kind)
            t=None if a.get('TargetMode')=='External' else target(p,a.get('Target'))
            if t is not None:
                assert t in d,('missing relationship target',rp,t)
                visit(t)
            out.append({'part':p,'relationship':dict(a.attrib),'targetPart':t})
    visit(start);return out
def planpath(args):return Path(args.plan or H/'plan.json')
def prepare(args):
    mp=Path(args.manifest or H/'source-final-output-manifest.json')
    if not mp.exists():mp=H.parent/'review-20261013'/'final-output-manifest.json'
    manifest=load(mp);tests=load(H/'tests-audit.json');srs=load(H/'srs-audit.json')
    testcurrent={(f['key'],s['sheet'],c['cell']):c['public_final_text'] for f in tests['files'] for s in f['sheets'] for c in s['marker_cell_details']}
    srscurrent={c['cell']:c['expected_final_text_after_stateful_filter_and_one_marker_separator_space'] for c in srs['marker_cells']}
    books=[]
    for f in manifest['files']:
        if not f['path'].lower().endswith('.xlsx'):continue
        code=f['code'];path=Path(f['path']);expected=f['sha256'].upper()
        assert sha(path)==expected,(code,'source hash guard')
        d,_=pack(path);ss=sst(d);planned=[]
        for s in sheets(d):
            sr,cells,_=cellmap(d,s['part']);changes=[]
            for addr,c in cells.items():
                before=value(c,ss);co=container(c,ss)
                if not MARK.search(before) and not green(co):continue
                _,after=accepted_cell(d,c,ss,code)
                if MARK.search(before):
                    audit=srscurrent[addr] if code=='SRS' else testcurrent[(code,s['name'],addr)]
                    assert after==audit,(code,s['name'],addr,'audit projection mismatch')
                changes.append({'cell':addr,'before':before,'after':after,'colorOnly':before==after})
            if changes:
                name=s['name']+'_비교';assert len(name)<=31 and name not in [x['name'] for x in sheets(d)]
                # Worksheet unique IDs can be cloned only with fresh IDs; all cells remain exact.
                graph=walkrels(d,s['part'])
                planned.append({'name':s['name'],'compareName':name,'range':sr.find('x:dimension',N).get('ref'),'changes':changes,'sourcePart':s['part'],'sourceSheetId':s['id'],'sourceIndex':s['index'],'sourcePartSHA256':hashlib.sha256(d[s['part']]).hexdigest().upper(),'relationships':graph})
        assert sha(path)==expected
        books.append({'code':code,'path':str(path),'sourceSHA256':expected,'backupPath':str(Path(args.backup_dir or H/'backups')/(code+'.xlsx')),'artifactPath':str(Path(args.artifact_dir or H/'authored')/(code+'-authored.xlsx')),'candidatePath':str(H/'candidates'/(code+'.xlsx')),'sheets':planned})
    assert len(books)==7
    p={'sourceManifest':str(mp),'sourceManifestSHA256':sha(mp),'status':'Prepared metadata/plain accepted cell plan; no source writes or final candidate yet','books':books}
    save(planpath(args),p)
    print(json.dumps({'status':p['status'],'books':[{'code':b['code'],'sheets':[{'name':s['name'],'compareName':s['compareName'],'changes':len(s['changes']),'colorOnly':sum(c['colorOnly'] for c in s['changes'])} for s in b['sheets']]} for b in books]},ensure_ascii=True))

def layout_attributes(original,spec):
    result=dict(original);result['ht']=str(spec['height']);result['customHeight']='1'
    if 'hidden' in spec:result['hidden']='1' if spec['hidden'] else '0'
    return result
def apply_layout(body,rows):
    for spec in rows:
        pattern=rb'<row\b[^>]*\br="'+str(spec['row']).encode()+rb'"[^>]*>'
        def patch(m):
            header=m[0][:-1]
            for key,val in layout_attributes({},spec).items():
                attribute=key.encode();replacement=b' '+attribute+b'="'+val.encode()+b'"'
                if re.search(rb'\s'+attribute+rb'="[^"]*"',header):header=re.sub(rb'\s'+attribute+rb'="[^"]*"',lambda _:replacement,header,count=1)
                else:header+=replacement
            return header+b'>'
        body,n=re.subn(pattern,patch,body,count=1);assert n==1,('missing layout row',spec['row'])
    return body

def finalize(args):
    plan=load(planpath(args));folder=H/'candidates';folder.mkdir(exist_ok=True)
    unselected={b['code']:sha(b['candidatePath']) for b in plan['books'] if args.code and b['code']!=args.code}
    requested_layout=load(args.layout_plan) if args.layout_plan else None
    if requested_layout:
        assert args.code=='SRS' and requested_layout['code']=='SRS'
        assert requested_layout['sheet']=='소프트웨어 입력 요구사항 사양서'
        assert requested_layout['rows']==[{'row':50,'height':0,'hidden':True},{'row':62,'height':15},{'row':68,'height':15},{'row':81,'height':15}],('unsupported layout plan',requested_layout['rows'])
    for book in plan['books']:
        if args.code and book['code']!=args.code:continue
        code=book['code'];assert sha(book['path'])==book['sourceSHA256']
        previous_candidate=pack(book['candidatePath'])[0] if requested_layout else None
        previous_candidate_sha=sha(book['candidatePath']) if requested_layout else None
        layout=requested_layout if code=='SRS' and requested_layout else book.get('mainRowLayout')
        if layout:
            layout_stage,_=pack(layout['artifactPath']);layout_sheets={s['name']:s for s in sheets(layout_stage)}
            layout_root,_,_=cellmap(layout_stage,layout_sheets[layout['sheet']]['part'])
            book['artifactLayoutLimitations']=[]
            for spec in layout['rows']:
                row=layout_root.find(f".//x:sheetData/x:row[@r='{spec['row']}']",N)
                assert row is not None,('Artifact layout row',spec['row'])
                if spec['row']==50 and spec['height']==0 and row.get('ht') is None and row.get('customHeight')=='1':
                    book['artifactLayoutLimitations'].append({'row':50,'kind':'Artifact exported rowHeight zero omits ht/hidden','artifactAttributes':dict(row.attrib),'explicitFinalAttributes':{'ht':'0','customHeight':'1','hidden':'1'}})
                else:assert float(row.get('ht','15'))==spec['height'],('Artifact layout height',spec['row'])
            book['mainRowLayout']={**layout,'planPath':str(args.layout_plan or layout.get('planPath')),'artifactSHA256':sha(layout['artifactPath'])}
        assert Path(book['backupPath']).is_file() and sha(book['backupPath'])==book['sourceSHA256'],(code,'root backup missing/mismatched')
        artifact=Path(args.artifact_dir)/(code+'-authored.xlsx') if args.artifact_dir else Path(book['artifactPath'])
        assert artifact.is_file(),(code,'Artifact authored/exported workbook required')
        stage,_=pack(artifact);stage_s={s['name']:s for s in sheets(stage)}
        original,infos=pack(book['backupPath']);d=original.copy();ss=sst(original)
        wb=xml(d['xl/workbook.xml']);rels=xml(d['xl/_rels/workbook.xml.rels']);ct=xml(d['[Content_Types].xml'])
        snodes=wb.find('x:sheets',N);oldcount=len(snodes);sid=max(int(s.get('sheetId')) for s in snodes)
        rid=max([int(a.get('Id')[3:]) for a in rels if re.fullmatch(r'rId\d+',a.get('Id',''))]+[0])
        number=max([int(re.search(r'sheet(\d+)\.xml$',p)[1]) for p in d if re.search(r'worksheets/sheet\d+\.xml$',p)]+[0])
        overrides={a.get('PartName').lstrip('/'):a.get('ContentType') for a in ct if E.QName(a).localname=='Override'}
        clones=[];counter=0;new_defined_titles=[];book['artifactMergeLimitations']=[]
        def clone(source):
            nonlocal counter
            if source in copied:return copied[source]
            counter+=1;stem,ext=posixpath.splitext(source);dest=stem+f'_svcopy{counter}'+ext
            assert dest not in d;copied[source]=dest;d[dest]=original[source]
            if source in overrides:E.SubElement(ct,'{'+CT+'}Override',PartName='/'+dest,ContentType=overrides[source])
            # VML Note shape IDs are local references; assign new IDs to avoid workbook collisions.
            if source.endswith('.vml'):
                vr=xml(d[dest]);notes=vr.xpath('//*[local-name()="ClientData"]')
                assert all(n.get('ObjectType')=='Note' for n in notes),(code,'unsupported VML control')
                ids=[x.get('id') for x in vr.xpath('//*[local-name()="shape"]')]
                for i,oldid in enumerate(ids):
                    newid=f'_x0000_s{100000+counter*100+i}'
                    for e in vr.iter():
                        for a,v in list(e.attrib.items()):
                            if v==oldid:e.set(a,newid)
                blocks=sorted({str((100000+counter*100+i)//1024) for i in range(len(ids))})
                for e in vr.xpath('//*[local-name()="idmap"]'):e.set('data',','.join(blocks))
                d[dest]=dump(vr)
            rp=relpart(source)
            if rp in original:
                rr=xml(original[rp])
                for a in rr:
                    kind=a.get('Type').rsplit('/',1)[-1];assert kind in SUPPORTED
                    if a.get('TargetMode')!='External':a.set('Target',relative(dest,clone(target(source,a.get('Target')))))
                d[relpart(dest)]=dump(rr)
            clones.append({'source':source,'clone':dest,'sourceSHA256':hashlib.sha256(original[source]).hexdigest().upper()})
            return dest
        for s in book['sheets']:
            assert s['name'] in stage_s and s['compareName'] in stage_s,(code,'Artifact worksheet missing')
            ar,ac,ass=cellmap(stage,stage_s[s['name']]['part']);cr,cc,css=cellmap(stage,stage_s[s['compareName']]['part'])
            sr,sc,_=cellmap(original,s['sourcePart'])
            for ch in s['changes']:assert value(ac[ch['cell']],ass)==ch['after'],(code,s['name'],ch['cell'],'Artifact accepted value')
            for addr,c in sc.items():
                f=c.find('x:f',N);cf=cc.get(addr)
                if f is not None:assert cf is not None and cf.findtext('x:f',namespaces=N)==f.text,(code,addr,'Artifact compare formula')
                else:
                    expected=value(c,ss);actual=value(cf,css) if cf is not None else ''
                    assert expected==actual,(code,addr,'Artifact compare original value')
            original_merges={m.get('ref') for m in sr.findall('x:mergeCells/x:mergeCell',N)}
            artifact_merges={m.get('ref') for m in cr.findall('x:mergeCells/x:mergeCell',N)}
            assert artifact_merges<=original_merges,(code,'Artifact unexpected compare merge')
            if original_merges!=artifact_merges:
                book['artifactMergeLimitations'].append({'sheet':s['name'],'kind':'Artifact copy omitted existing merge formatting','missing':sorted(original_merges-artifact_merges),'candidateUses':'exact original worksheet merge/layout bytes'})
            body=original[s['sourcePart']]
            for ch in s['changes']:
                c,after=accepted_cell(original,sc[ch['cell']],ss,code);assert after==ch['after']
                pattern=rb'<c\b[^>]*\br="'+ch['cell'].encode()+rb'"[^>]*(?:/>|>.*?</c>)'
                body,count=re.subn(pattern,lambda _:E.tostring(c),body,count=1,flags=re.S)
                assert count==1,(code,ch['cell'],'target cell byte patch')
            if layout and s['name']==layout['sheet']:body=apply_layout(body,layout['rows'])
            d[s['sourcePart']]=body
            number+=1;sid+=1;rid+=1;newpart=f'xl/worksheets/sheet{number}.xml';copied={}
            d[newpart]=original[s['sourcePart']]
            nr=xml(d[newpart]);updated=False
            for a in list(nr.attrib):
                if E.QName(a).localname=='uid':nr.set(a,'{'+str(uuid.uuid5(uuid.NAMESPACE_URL,code+s['compareName'])).upper()+'}');updated=True
            # A copied selected tab creates a workbook sheet group in Excel.
            # The comparison tab is visible but must start unselected; preserve
            # the existing workbook activeTab and current sheet selection.
            for view in nr.findall('x:sheetViews/x:sheetView',N):
                view.set('tabSelected','0');updated=True
            if updated:d[newpart]=dump(nr)
            rp=relpart(s['sourcePart'])
            if rp in original:
                rr=xml(original[rp])
                for a in rr:
                    if a.get('TargetMode')!='External':a.set('Target',relative(newpart,clone(target(s['sourcePart'],a.get('Target')))))
                d[relpart(newpart)]=dump(rr)
            E.SubElement(snodes,tag('sheet'),name=s['compareName'],sheetId=str(sid),attrib={'{'+RN+'}id':f'rId{rid}'})
            E.SubElement(rels,'{'+PK+'}Relationship',Id=f'rId{rid}',Type=RN+'/worksheet',Target=relative('xl/workbook.xml',newpart))
            E.SubElement(ct,'{'+CT+'}Override',PartName='/'+newpart,ContentType=overrides[s['sourcePart']])
            dn=wb.find('x:definedNames',N)
            if dn is not None:
                for a in list(dn):
                    if a.get('localSheetId')==str(s['sourceIndex']):
                        z=copy.deepcopy(a);z.set('localSheetId',str(len(snodes)-1));z.text=(z.text or '').replace("'"+s['name'].replace("'","''")+"'!","'"+s['compareName'].replace("'","''")+"'!")
                        dn.append(z)
                        new_defined_titles.append("'"+s['compareName'].replace("'","''")+"'!"+z.get('name').removeprefix('_xlnm.'))
            if 'xl/calcChain.xml' in d:
                chain=xml(d['xl/calcChain.xml']);effective=None;extra=[]
                for a in list(chain):
                    effective=a.get('i',effective)
                    if effective==s['sourceSheetId']:
                        z=copy.deepcopy(a);z.set('i',str(sid));extra.append(z)
                chain.extend(extra);d['xl/calcChain.xml']=dump(chain)
            s['comparisonPart']=newpart;s['comparisonSheetId']=str(sid)
        d['xl/workbook.xml']=dump(wb);d['xl/_rels/workbook.xml.rels']=dump(rels);d['[Content_Types].xml']=dump(ct)
        if 'docProps/app.xml' in d:
            app=xml(d['docProps/app.xml']);V={'v':'http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes'}
            groups=app.xpath('//*[local-name()="HeadingPairs"]/v:vector/v:variant',namespaces=V)
            assert len(groups)==4,(code,'unsupported app metadata categories')
            assert groups[0].findtext('v:lpstr',namespaces=V) in ['Worksheets','워크시트']
            assert groups[2].findtext('v:lpstr',namespaces=V) in ['Named Ranges','이름 지정된 범위']
            groups[1].find('v:i4',V).text=str(len(snodes))
            groups[3].find('v:i4',V).text=str(int(groups[3].findtext('v:i4',namespaces=V))+len(new_defined_titles))
            titles=app.xpath('//*[local-name()="TitlesOfParts"]/v:vector',namespaces=V)
            if titles:
                vec=titles[0]
                original_titles=[x.text for x in vec];assert original_titles[:oldcount]==[s['name'] for s in sheets(original)]
                for x in list(vec):vec.remove(x)
                for name in [s.get('name') for s in snodes]+original_titles[oldcount:]+new_defined_titles:E.SubElement(vec,'{'+V['v']+'}lpstr').text=name
                vec.set('size',str(len(vec)))
            d['docProps/app.xml']=dump(app)
        candidate=Path(book['candidatePath'])
        with zipfile.ZipFile(candidate,'w',zipfile.ZIP_DEFLATED) as z:
            for info in infos:z.writestr(info,d[info.filename])
            for p in d:
                if p not in original:z.writestr(p,d[p])
        book['candidateSHA256']=sha(candidate);book['artifactSHA256']=sha(artifact);book['clonedParts']=clones
        if previous_candidate is not None:
            changed=[p for p in d if d[p]!=previous_candidate.get(p)]
            mainpart=next(s['sourcePart'] for s in book['sheets'] if s['name']==layout['sheet'])
            assert set(d)==set(previous_candidate) and set(changed)<={mainpart},('unexpected layout package delta',changed)
            before_root,before_cells,bss=cellmap(previous_candidate,mainpart);after_root,after_cells,ass=cellmap(d,mainpart)
            assert set(before_cells)==set(after_cells)
            assert all(E.tostring(before_cells[a])==E.tostring(after_cells[a]) for a in before_cells),('layout cell/rich/formula delta')
            declared={s['row']:s for s in layout['rows']};actual=[]
            for ro,rn in zip(before_root.find('x:sheetData',N),after_root.find('x:sheetData',N)):
                rownum=int(ro.get('r'));expected=layout_attributes(ro.attrib,declared[rownum]) if rownum in declared else dict(ro.attrib)
                assert dict(rn.attrib)==expected,('layout row attribute delta',rownum)
                if dict(ro.attrib)!=dict(rn.attrib):actual.append({'row':rownum,'before':dict(ro.attrib),'after':dict(rn.attrib)})
            book['lastLayoutDelta']={'beforeCandidateSHA256':previous_candidate_sha,'afterCandidateSHA256':book['candidateSHA256'],'changedParts':changed,'actualRows':actual,'allCellsRichFormulasUnchanged':True,'otherPartsBytesUnchanged':True}
        assert sha(book['path'])==book['sourceSHA256']
    assert all(sha(next(b['candidatePath'] for b in plan['books'] if b['code']==code))==hashvalue for code,hashvalue in unselected.items()),'unselected candidate changed'
    plan['status']='Local candidates finalized; original-package preservation checks and native root QA required'
    save(planpath(args),plan);verify(args)

def verify(args):
    plan=load(planpath(args));results=[];loaded={}
    for b in plan['books']:
        assert sha(b['path'])==b['sourceSHA256'];assert sha(b['backupPath'])==b['sourceSHA256']
        original,_=pack(b['backupPath']);d,_=pack(b['candidatePath']);ss=sst(original);ms={s['name']:s for s in sheets(d)}
        bodyparts={s['sourcePart'] for s in b['sheets']};allowed=bodyparts|{'xl/workbook.xml','xl/_rels/workbook.xml.rels','[Content_Types].xml','docProps/app.xml','xl/calcChain.xml'}
        assert all(d[p]==v for p,v in original.items() if p not in allowed),(b['code'],'unrelated original part changed')
        for s in b['sheets']:
            sr,sc,_=cellmap(original,s['sourcePart']);nr,nc,nss=cellmap(d,s['sourcePart']);cr,cc,css=cellmap(d,ms[s['compareName']]['part'])
            changes={x['cell']:x for x in s['changes']}
            for addr,c in sc.items():
                if addr in changes:assert value(nc[addr],nss)==changes[addr]['after']
                else:assert E.tostring(nc[addr])==E.tostring(c),(b['code'],addr,'unchanged body cell')
                assert E.tostring(cc[addr])==E.tostring(c),(b['code'],addr,'compare original cell/rich/formula/cache changed')
                assert not MARK.search(value(nc[addr],nss)),(b['code'],addr,'body marker remains')
                co=container(nc[addr],nss);assert not green(co),(b['code'],addr,'review green remains')
                if co is not None:assert all(texts(r) for r in co.findall('x:r',N))
            # Main attributes/layout children stay untouched; only cell content differs.
            for a in sr:
                if E.QName(a).localname!='sheetData':assert E.tostring(a)==E.tostring(nr.find(a.tag)),(b['code'],'body layout',a.tag)
            layout=b.get('mainRowLayout');declared={x['row']:x for x in layout['rows']} if layout and s['name']==layout['sheet'] else {}
            for ro,rn in zip(sr.find('x:sheetData',N),nr.find('x:sheetData',N)):
                rownum=int(ro.get('r'));expected=layout_attributes(ro.attrib,declared[rownum]) if rownum in declared else dict(ro.attrib)
                assert expected==dict(rn.attrib),(b['code'],'body row attributes',rownum)
            assert len(sr.find('x:sheetData',N))==len(nr.find('x:sheetData',N))
            expected_compare=copy.deepcopy(sr)
            for view in expected_compare.findall('x:sheetViews/x:sheetView',N):view.set('tabSelected','0')
            for a in sr:
                expected=expected_compare.find(a.tag)
                assert E.tostring(expected)==E.tostring(cr.find(a.tag)),(b['code'],'compare worksheet layout except selection')
            assert dict(sr.attrib)==dict(nr.attrib),(b['code'],'original body worksheet attributes')
            assert all(v.get('tabSelected')=='0' for v in cr.findall('x:sheetViews/x:sheetView',N)),(b['code'],'comparison selected')
        original_wb=xml(original['xl/workbook.xml']);candidate_wb=xml(d['xl/workbook.xml'])
        assert E.tostring(original_wb.find('x:bookViews',N))==E.tostring(candidate_wb.find('x:bookViews',N)),(b['code'],'workbook active/selection view changed')
        for p,v in d.items():
            if p.endswith('.xml'):xml(v)
            if p.endswith('.rels'):
                base=posixpath.join(posixpath.dirname(posixpath.dirname(p)),posixpath.basename(p)[:-5]) if p!='_rels/.rels' else ''
                rels=xml(v);ids=[a.get('Id') for a in rels];assert len(ids)==len(set(ids))
                for a in rels:
                    if a.get('TargetMode')!='External':assert target(base,a.get('Target')) in d,(b['code'],p,a.get('Target'))
        ids=[s['id'] for s in sheets(d)];names=[s['name'] for s in sheets(d)];assert len(ids)==len(set(ids)) and len(names)==len(set(names))
        for item in b['clonedParts']:
            p=item['source'];dest=item['clone']
            if not p.endswith('.vml'):assert d[dest]==original[p],(b['code'],p,'cloned payload bytes')
        loaded[b['code']]=(d,ms)
        results.append({'code':b['code'],'candidateSHA256':sha(b['candidatePath']),'unchangedOriginalParts':len([p for p in original if p not in allowed]),'compareCellsOriginalRichExact':True,'projectionExact':True,'layoutPreserved':True,'explicitMainRowLayoutExceptions':b.get('mainRowLayout',{}).get('rows',[]),'relationsValid':True,'currentMainSelectionPreserved':True,'comparisonNotSelected':True,'workbookOriginalViewUnchanged':True,'nativeDisplay':'미실행/root 담당'})
    pairs=[]
    for ck,rk,cs,rs,start,end,last in [('SUVC','SUVR','기준서_유닛 검증','보고서_유닛 시험',30,1764,11),('SITC','SITR','기준서_통합 시험','보고서_통합시험',25,144,13),('SSTC','SSTR','기준서_시스템 시험','보고서_시스템 시험',34,285,10)]:
        ca,cm=loaded[ck];ra,rm=loaded[rk];_,cc,css=cellmap(ca,cm[cs]['part']);_,rc,rss=cellmap(ra,rm[rs]['part'])
        def col(a):
            n=0
            for x in re.match('[A-Z]+',a)[0]:n=n*26+ord(x)-64
            return n
        for a,c in cc.items():
            row=int(re.search(r'\d+',a)[0])
            if start<=row<=end and 2<=col(a)<=last:assert value(c,css)==value(rc[a],rss),(ck,rk,a,'final common text')
        pairs.append({'pair':[ck,rk],'commonFinalValues':'통과'})
    plan['verification']={'files':results,'pairs':pairs,'nativeDisplay':'미실행/root actual Excel QA required'};save(planpath(args),plan)
    print(json.dumps(plan['verification'],ensure_ascii=True))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('mode',choices=['prepare','finalize','verify']);p.add_argument('--manifest');p.add_argument('--plan');p.add_argument('--backup-dir');p.add_argument('--artifact-dir');p.add_argument('--code',choices=['SRS','SUVC','SUVR','SITC','SITR','SSTC','SSTR']);p.add_argument('--layout-plan');a=p.parse_args()
    {'prepare':prepare,'finalize':finalize,'verify':verify}[a.mode](a)
