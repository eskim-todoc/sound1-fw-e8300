from pathlib import Path
import sys,copy,json,zipfile,re
from lxml import etree as E
H=Path(__file__).resolve().parent
sys.path.insert(0,str(H.parent/'sv-clean-20261013'))
from prepare_clean import pack,sheets,cellmap,value,container,texts,xml,dump,N,tag,load,save,sha
def fontprops(d,c,co):
    if co is not None:
        for rp in co.findall('x:r/x:rPr',N):
            col=rp.find('x:color',N)
            if col is None or col.get('rgb') in ['FF000000','000000']:return copy.deepcopy(rp)
    st=xml(d['xl/styles.xml']);xf=st.find('x:cellXfs',N)[int(c.get('s','0'))];font=st.find('x:fonts',N)[int(xf.get('fontId','0'))]
    rp=E.Element(tag('rPr'))
    for ch in font:
        ch=copy.deepcopy(ch)
        if E.QName(ch).localname=='name':ch.tag=tag('rFont')
        rp.append(ch)
    return rp
def addrun(co,line,rp,color=None,notice=False):
    if not line:return
    rr=E.SubElement(co,tag('r'));pr=copy.deepcopy(rp)
    if color:
        for child in list(pr):
            if E.QName(child).localname in ['color','u']:pr.remove(child)
        E.SubElement(pr,tag('color'),rgb=color)
    if notice:
        sz=pr.find('x:sz',N);sz.set('val',str(float(sz.get('val'))+2));E.SubElement(pr,tag('u'))
    rr.append(pr);tt=E.SubElement(rr,tag('t'));tt.set('{http://www.w3.org/XML/1998/namespace}space','preserve');tt.text=line
plan=load(H/'plan.json');records=[]
for b in plan['books']:
    assert sha(b['path'])==b['sourceSHA256']
    d,infos=pack(b['path']);auth,_=pack(H/'authored'/f"{b['code']}.xlsx")
    auth_tabs={s['name']:s for s in sheets(auth)}
    changedparts=[];diffcells=[]
    for which,g in [(w,g) for g in b['groups'] for w in ['main','compare']]:
        s=g[which];r,cm,ss=cellmap(d,s['part']);_,ac,ass=cellmap(auth,auth_tabs[s['name']]['part'])
        for e in g['edits']:
            addr=e['cell'];expected=e['after'] if which=='main' else e['compareAfter']
            got=value(ac[addr],ass) if addr in ac else ''
            assert got==expected,(b['code'],which,addr,got,expected)
            c=cm[addr];oldco=container(c,ss);oldtext=value(c,ss);base=fontprops(d,c,oldco)
            for child in list(c):
                if E.QName(child).localname in ['v','is']:c.remove(child)
            if not expected:
                c.attrib.pop('t',None)
            else:
                c.set('t','inlineStr');co=E.SubElement(c,tag('is'))
                if which=='compare':
                    # Preserve all earlier rich text byte-equivalent children before the new dated diff.
                    if oldco is not None:
                        for child in oldco:co.append(copy.deepcopy(child))
                    # Plain inline t cannot coexist with rich r children. Promote without changing its text.
                    for t in list(co.findall('x:t',N)):
                        co.remove(t);run=E.Element(tag('r'));run.append(t);co.insert(0,run)
                    for line in e['append'].splitlines(keepends=True):
                        if not line:continue
                        color='FFFF0000' if line.startswith('[-]') else 'FF008000' if line.startswith('[+]') else 'FF000000'
                        addrun(co,line,base,color)
                else:
                    for line in expected.splitlines(keepends=True):
                        m=re.search(r'\[(E8300 보완|QCC 자료 입력|QCC/E8300 자료 입력)\]',line)
                        if m:
                            addrun(co,line[:m.start()],base)
                            addrun(co,line[m.start():],base,{'E8300 보완':'FF0070C0','QCC 자료 입력':'FFC00000','QCC/E8300 자료 입력':'FF7030A0'}[m[1]],True)
                        else:addrun(co,line,base)
                assert texts(co)==expected
            diffcells.append({'sheet':s['name'],'cell':addr,'before':oldtext,'after':expected})
        if which=='compare':
            for idx,height in g['compareHeights'].items():
                row=r.find(f"x:sheetData/x:row[@r='{idx}']",N);row.set('ht',str(height));row.set('customHeight','1')
            for dest,src in g.get('compareStyleCopies',{}).items():
                if dest not in cm:
                    parent=r.find(f"x:sheetData/x:row[@r='{re.search(r'\d+',dest)[0]}']",N)
                    cm[dest]=E.SubElement(parent,tag('c'),r=dest)
                assert value(cm[dest],ss)==''
                cm[dest].set('s',cm[src].get('s'))
            for address in g.get('compareMerges',[]):
                merge=r.find('x:mergeCells',N)
                assert merge is not None
                E.SubElement(merge,tag('mergeCell'),ref=address);merge.set('count',str(len(merge)))
        d[s['part']]=dump(r);changedparts.append(s['part'])
    out=H/'candidates'/f"{b['code']}.xlsx";out.parent.mkdir(exist_ok=True)
    with zipfile.ZipFile(out,'w') as z:
        for info in infos:z.writestr(info,d[info.filename])
    saved,_=pack(out);original,_=pack(b['path'])
    actualparts=[p for p in saved if saved[p]!=original[p]]
    assert set(actualparts)==set(changedparts)
    # Verify every untouched cell, row, merge, drawing relationship, print setting, formula and metadata.
    for which,g in [(w,g) for g in b['groups'] for w in ['main','compare']]:
        s=g[which];before,bc,bs=cellmap(original,s['part']);after,fc,fs=cellmap(saved,s['part'])
        expected_addresses={e['cell'] for e in g['edits']}
        if which=='compare':expected_addresses.update(g.get('compareStyleCopies',{}))
        added=set(fc)-set(bc)
        assert added<=set(g.get('compareStyleCopies',{})) if which=='compare' else not added
        assert not(set(bc)-set(fc))
        for addr in bc:
            if addr not in expected_addresses:assert E.tostring(bc[addr])==E.tostring(fc[addr]),(b['code'],which,addr)
        for e in g['edits']:
            assert value(fc[e['cell']],fs)==(e['after'] if which=='main' else e['compareAfter'])
            if which=='main':assert '[+]' not in value(fc[e['cell']],fs) and '[-]' not in value(fc[e['cell']],fs)
        for rr in [before,after]:
            for e in g['edits']:
                cell=rr.find(f".//x:c[@r='{e['cell']}']",N);cell.getparent().remove(cell)
            if which=='compare':
                for idx,height in g['compareHeights'].items():rr.find(f"x:sheetData/x:row[@r='{idx}']",N).set('ht',str(height))
                for dest,src in g.get('compareStyleCopies',{}).items():
                    cell=rr.find(f".//x:c[@r='{dest}']",N)
                    if dest not in bc:
                        if cell is not None:cell.getparent().remove(cell)
                    else:cell.set('s',bc[src].get('s'))
                merge=rr.find('x:mergeCells',N)
                for m in list(merge):
                    if m.get('ref') in g.get('compareMerges',[]):merge.remove(m)
                merge.set('count',str(len(merge)))
        assert E.tostring(before)==E.tostring(after),(b['code'],which,'outside-scope change')
    records.append({'code':b['code'],'path':str(out),'sha256':sha(out),'changedParts':actualparts,'changedCells':diffcells,'allUntouchedPartsAndCellsPreserved':True})
save(H/'package-checks.json',records)
print(json.dumps([{'code':r['code'],'sha256':r['sha256'],'changedCells':len(r['changedCells'])} for r in records]))
