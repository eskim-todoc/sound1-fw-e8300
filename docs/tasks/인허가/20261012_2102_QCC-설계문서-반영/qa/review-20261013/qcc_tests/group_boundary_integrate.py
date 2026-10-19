from pathlib import Path
from lxml import etree as E
import json, zipfile, copy
from integrate import N, NS, sha, sheetmap, vals, tag, ref
H=Path(__file__).resolve().parent
plan=json.loads((H/'group-boundary-plan.json').read_text(encoding='utf-8'))
packages={}; evidence=[]
for k,b in plan.items():
    path=Path(b['path']); assert sha(path)==b['sha256_before'],(k,'concurrent change')
    with zipfile.ZipFile(path) as z:
        infos=z.infolist(); original={i.filename:z.read(i.filename) for i in infos}
    data=original.copy(); root=E.fromstring(data[b['member']]); styles=E.fromstring(data['xl/styles.xml'])
    xfs=styles.find('x:cellXfs',N); aligns=[]
    with zipfile.ZipFile(H/(k+'-group-boundary-authored.xlsx')) as z:
        stage={i.filename:z.read(i.filename) for i in z.infolist()}
    sr=E.fromstring(stage[sheetmap(stage)[b['sheet']]])
    sxfs=E.fromstring(stage['xl/styles.xml']).find('x:cellXfs',N)
    for a in b['top_alignment_cells']:
        sc=sr.find(f'.//x:c[@r="{a}"]',N)
        sal=sxfs[int(sc.get('s','0'))].find('x:alignment',N)
        assert sal.get('vertical')=='top' and sal.get('horizontal')=='left',(k,a,'artifact alignment')
        c=root.find(f'.//x:c[@r="{a}"]',N); old=int(c.get('s','0')); xf=copy.deepcopy(xfs[old])
        al=xf.find('x:alignment',N)
        if al is None:al=E.SubElement(xf,tag('alignment'))
        al.set('horizontal','left'); al.set('vertical','top'); xf.set('applyAlignment','1')
        index=next((i for i,x in enumerate(xfs) if E.tostring(x)==E.tostring(xf)),None)
        if index is None:index=len(xfs);xfs.append(xf)
        c.set('s',str(index));aligns.append({'cell':a,'before_style':old,'after_style':index,'alignment':'left/top','font_size_color_border_unchanged':True})
    br=root.find('x:rowBreaks',N)
    if br is None:
        br=E.Element(tag('rowBreaks'))
        after={'colBreaks','customProperties','cellWatches','ignoredErrors','smartTags','drawing','legacyDrawing','legacyDrawingHF','picture','oleObjects','controls','webPublishItems','tableParts','extLst'}
        root.insert(next((i for i,x in enumerate(root) if E.QName(x).localname in after),len(root)),br)
    previous=[dict(x.attrib) for x in br]; ids={int(x.get('id')) for x in br}
    for row in b['page_break_before_rows']:
        if row-1 not in ids:E.SubElement(br,tag('brk'),id=str(row-1),min='0',max='16383',man='1')
    ordered=sorted(br,key=lambda x:int(x.get('id')))
    for x in list(br):br.remove(x)
    for x in ordered:br.append(x)
    br.set('count',str(len(ordered))); br.set('manualBreakCount',str(sum(x.get('man')=='1' for x in ordered)))
    data[b['member']]=E.tostring(root,xml_declaration=True,encoding='UTF-8',standalone=True)
    if aligns:
        xfs.set('count',str(len(xfs)));data['xl/styles.xml']=E.tostring(styles,xml_declaration=True,encoding='UTF-8',standalone=True)
    oldroot=E.fromstring(original[b['member']])
    assert vals(data,b['member'])==vals(original,b['member']),(k,'all cell values')
    for child in ['sheetFormatPr','cols','mergeCells','pageMargins','pageSetup','printOptions']:
        before=oldroot.find('x:'+child,N);after=root.find('x:'+child,N)
        assert (E.tostring(before) if before is not None else None)==(E.tostring(after) if after is not None else None),(k,child)
    for oldrow,newrow in zip(oldroot.findall('x:sheetData/x:row',N),root.findall('x:sheetData/x:row',N)):
        assert oldrow.attrib==newrow.attrib,(k,'row height/order')
        for oc,nc in zip(oldrow, newrow):
            if oc.get('r') in b['top_alignment_cells']:
                assert {a:v for a,v in oc.attrib.items() if a!='s'}=={a:v for a,v in nc.attrib.items() if a!='s'}
                assert [E.tostring(x,method='c14n',exclusive=True) for x in oc]==[E.tostring(x,method='c14n',exclusive=True) for x in nc],(k,oc.get('r'),'body retained')
            else:assert E.tostring(oc)==E.tostring(nc),(k,oc.get('r'),'body/rich/results')
    assert all(original[n]==data[n] for n in data if n not in [b['member'],'xl/styles.xml'])
    assert not root.xpath('//x:r[not(x:t/text())]',namespaces=N)
    candidate=Path(b['candidate']);candidate.parent.mkdir(exist_ok=True)
    with zipfile.ZipFile(candidate,'w') as z:
        for i in infos:z.writestr(i,data[i.filename])
    packages[k]=(data,root)
    evidence.append({'key':k,'path':b['path'],'candidate':str(candidate),'sha256_before':b['sha256_before'],'sha256_candidate':sha(candidate),
       'changed_parts':[n for n in data if data[n]!=original[n]],'new_break_before_rows':b['new_break_before_rows'],
       'all_breaks_after':[dict(x.attrib) for x in br],'alignment_changes':aligns,
       'all_values_rich_runs_results_formulas_rows_heights_merges_widths_printarea_and_other_parts_unchanged':True,
       'new_product_execution_or_pass':False,'native_visibility':'미실행: candidate root native Open/full export/pixel review required'})
pairs=[]
for ck,rk,last,start,end in [('SUVC','SUVR',11,30,1764),('SITC','SITR',13,25,144),('SSTC','SSTR',10,34,285)]:
    cv=vals(packages[ck][0],plan[ck]['member']);rv=vals(packages[rk][0],plan[rk]['member'])
    crich={c.get('r'):c.find('x:is',N) for c in packages[ck][1].findall('x:sheetData/x:row/x:c',N)}
    rrich={c.get('r'):c.find('x:is',N) for c in packages[rk][1].findall('x:sheetData/x:row/x:c',N)}
    for a,v in cv.items():
        c,r=ref(a)
        if 2<=c<=last and start<=r<=end:
            assert v==rv.get(a,''),(ck,a,'common values')
            ci=crich.get(a);ri=rrich.get(a)
            cr=E.tostring(ci) if ci is not None and ci.find('x:r',N) is not None else None
            rr=E.tostring(ri) if ri is not None and ri.find('x:r',N) is not None else None
            assert cr==rr,(ck,a,'common rich')
    assert [dict(x.attrib) for x in packages[ck][1].find('x:rowBreaks',N)]==[dict(x.attrib) for x in packages[rk][1].find('x:rowBreaks',N)]
    cm={x.get('ref') for x in packages[ck][1].findall('x:mergeCells/x:mergeCell',N) if ref(x.get('ref').split(':')[0])[0]>=2 and ref(x.get('ref').split(':')[-1])[0]<=last and ref(x.get('ref').split(':')[0])[1]>=start}
    rm={x.get('ref') for x in packages[rk][1].findall('x:mergeCells/x:mergeCell',N) if ref(x.get('ref').split(':')[0])[0]>=2 and ref(x.get('ref').split(':')[-1])[0]<=last and ref(x.get('ref').split(':')[0])[1]>=start}
    assert cm==rm,(ck,'common merges')
    pairs.append({'pair':[ck,rk],'common_range':f'B{start}:{chr(64+last)}{end}','common_values_rich_merges_paired_breaks':'통과','id_count':len(plan[ck]['all_group_ids'])})
(H/'group-boundary-candidates.json').write_text(json.dumps({'files':evidence,'pairs':pairs,'status':'Candidate package/common checks passed; actual native visibility pending'},ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'files':[{k:v for k,v in r.items() if k in ['key','candidate','sha256_before','sha256_candidate','alignment_changes']} for r in evidence],'pairs':pairs},ensure_ascii=True))
