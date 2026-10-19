"""현재 유닛 기준/보고서의 자료 안내 문구를 좁은 범위로 정리한다.

openpyxl은 읽기/비교에만 사용한다. 값 작성은 Artifact Tool에 맡긴다.
실제 동작의 금지 조건이나 미확인 판정을 삭제하는 도구가 아니다.
"""
from __future__ import annotations
import collections
import copy
import difflib
import json
import re
import shutil
import sys
import zipfile
from pathlib import Path
from openpyxl import load_workbook
from openpyxl.utils import get_column_letter
import update_report_results as prev
from merge_unit_conditions import segments, plain, norm, character_styles

TASK=Path(__file__).resolve().parent
QA=TASK/'qa/evidence-positive-wording'
SOURCE=TASK/'qa/unit-merged-conditions/baseline.json'
NEG=re.compile(r'요구하지|필요[가 ]*없|필요하지|불필요|첨부하지|제출하지|수집하지|촬영하지|찍지')

REPLACEMENTS = [
    ('RTT 화면은 필요할 때만 캡처하며 스코프 사진을 요구하지 않는다.', ''),
    ('전체 .log를 보관하며 정수 시험에 스코프 사진은 필요 없다.', '전체 .log를 보관한다.'),
    ('사진은 요구하지 않으며 RAM 결과를 LED 색상/점멸 시간 증거로 사용하지 않는다.',
     '확인 범위: LED 요청/선택값의 RAM 기록.'),
    ('UART 바이트 파형 측정은 기본 판정에 요구하지 않는다.', ''),
    ('ISD_CHECK 변경을 이 함수의 결과로 요구하지 않습니다.', ''),
    ('QCC_CTRL 변경을 이 함수의 결과로 요구하지 않습니다.', ''),
    ('청취는 입력 조건과 소리를 글로 기록하며 사진을 요구하지 않는다.',
     '청취 입력 조건과 확인한 소리를 글로 기록한다.'),
    ('청취만 확인할 때는 사진 불필요.', ''),
]

def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))

def cleaned(v):
    old = norm(v)
    new = old
    for a, b in REPLACEMENTS:
        new = new.replace(a, b)
    if new != old:
        new = '\n'.join(line.rstrip() for line in new.split('\n')).strip()
    return new

def current_sign(sheet, row):
    cell = sheet._cells.get((row,1))
    v = plain(cell.value) if cell else ''
    if v:
        return v
    for m in sheet.merged_cells:
        if m.min_col == 1 and m.min_row <= row <= m.max_row:
            anchor = sheet._cells.get((m.min_row,1))
            return plain(anchor.value) if anchor else ''
    return ''

def xmlshape(node):
    return (node.tag, sorted(node.attrib.items()), node.text or '',
            [xmlshape(ch) for ch in node])

def native_text_props(package,cell,style,shared):
    """기존 글자의 전체 폰트 속성을 유지한다. 일반 문자열은 셀 폰트를 명시한다."""
    xf=style.find('s:cellXfs',prev.N)[int(cell.get('s','0'))]
    font=style.find('s:fonts',prev.N)[int(xf.get('fontId','0'))]
    base=prev.E.Element('{'+prev.NS+'}rPr')
    for prop in font:
        child=copy.deepcopy(prop)
        if child.tag=='{'+prev.NS+'}name': child.tag='{'+prev.NS+'}rFont'
        base.append(child)
    if cell.get('t')=='s':
        source=shared[int(cell.find('s:v',prev.N).text)]
    else:
        source=cell.find('s:is',prev.N)
    old,props='',[]
    runs=source.findall('s:r',prev.N) if source is not None else []
    if runs:
        for run in runs:
            text=norm(run.find('s:t',prev.N).text)
            rp=run.find('s:rPr',prev.N)
            if rp is None: rp=prev.E.Element('{'+prev.NS+'}rPr')
            old+=text;props.extend([rp]*len(text))
    else:
        t=source.find('s:t',prev.N) if source is not None else None
        old=norm(t.text if t is not None else prev.xmltext(package,cell))
        props=[base]*len(old)
    return old,props,base

def set_preserved_rich(package,cell,p,style,shared,source_root):
    old,props='',[]
    base=None
    for addr in p.get('sourceCells',[p['cell']]):
        source_cell=source_root[addr]
        text,fonts,default=native_text_props(package,source_cell,style,shared)
        if old:
            old+='\n';props.append(props[-1] if props else default)
        old+=text;props.extend(fonts)
        if base is None: base=default
    assert old==p.get('sourceText',p['before']), (p['cell'],'source rich text')
    value=p['value']
    target=[None]*len(value)
    expected=[(col,bold) for text,col,bold in p['segments'] for _ in text]
    for tag,i,j,k,l in difflib.SequenceMatcher(None,old,value,autojunk=False).get_opcodes():
        if tag=='equal':
            target[k:l]=props[i:j]
        else:
            for at in range(k,l):
                rp=copy.deepcopy(props[0] if props else base)
                for child in list(rp):
                    if child.tag in ['{'+prev.NS+'}color','{'+prev.NS+'}b']: rp.remove(child)
                col,bold=expected[at]
                prev.E.SubElement(rp,'{'+prev.NS+'}color',rgb='FF'+col)
                prev.E.SubElement(rp,'{'+prev.NS+'}b',val='1' if bold else '0')
                target[at]=rp
    for child in list(cell): cell.remove(child)
    cell.set('t','inlineStr')
    ins=prev.E.SubElement(cell,'{'+prev.NS+'}is')
    grouped=[]
    for ch,rp in zip(value,target):
        key=prev.serialize(rp)
        if grouped and grouped[-1][2]==key: grouped[-1][0]+=ch
        else: grouped.append([ch,rp,key])
    for text,rp,_ in grouped:
        run=prev.E.SubElement(ins,'{'+prev.NS+'}r')
        run.append(copy.deepcopy(rp))
        t=prev.E.SubElement(run,'{'+prev.NS+'}t')
        t.set(prev.XMLSPACE,'preserve');t.text=text

def continuation_groups(s,col,sign=False):
    result=[]
    r=1
    while r<=s.max_row:
        text=norm(s.cell(r,col).value).strip('\n')
        title=text.split('\n')[0]
        if not title or '(계속)' in title or (sign and current_sign(s,r)!='[+]'):
            r+=1;continue
        head=next((m for m in s.merged_cells if m.min_col==col and m.max_col==col and m.min_row==r),None)
        stop=head.max_row if head else r
        original_stop=stop
        while stop<s.max_row:
            following=norm(s.cell(stop+1,col).value).strip('\n').split('\n')[0]
            if following!=title+' (계속)' or (sign and current_sign(s,stop+1)!='[+]'):break
            stop+=1
        if stop>original_stop:result.append(list(range(r,stop+1)))
        r=stop+1
    return result

def padded(v):
    text=norm(v).strip('\n')
    return '\n'+text+'\n' if text else ''

def expand_padding():
    """추가 요청: 전체 표시 셀의 빈 줄과 같은 RTT 조건의 계속 셀 병합."""
    base=read(QA/'baseline.json')
    original_plan=read(QA/'wording-only-plan.json') if (QA/'wording-only-plan.json').exists() else read(QA/'plan.json')
    if not (QA/'wording-only-plan.json').exists():prev.save_json(QA/'wording-only-plan.json',original_plan)
    edits={(p['key'],p['sheet'],p['cell']):p for p in original_plan['changes']}
    plan=dict(changes=[],shades=[],merges=[],unmerges=[],heights=[],wraps=[],groups=[])
    for b in base['books']:
        assert prev.digest(b['path'])==b['sha256'],('external edit',b['key'])
        w=load_workbook(b['backup'],rich_text=True)
        combined={};cleared=set()
        targets=[(w[b['sheet']],11,False)]
        if b['key']=='criterion':targets.append((w['기준서_유닛 검증_변경 비교'],11,True))
        else:targets.append((w['RTT 결과 대조'],2,False))
        for s,col,sign in targets:
            for rows in continuation_groups(s,col,sign):
                cols=([11] if b['key']=='criterion' else [11,12]) if col==11 else list(range(1,s.max_column+1))
                plan['groups'].append(dict(key=b['key'],sheet=s.title,rows=rows,title=norm(s.cell(rows[0],col).value).split('\n')[0]))
                for ci in cols:
                    cells=[s.cell(r,ci) for r in rows]
                    values=[norm(c.value) for c in cells]
                    if not any(values):continue
                    if col==11 and ci==11:
                        lines=values[0].split('\n')
                        for v in values[1:]:
                            if not v:continue
                            tail=v.split('\n')[1:]
                            lines.extend(x for x in tail if x.strip() not in ['입력/조건:','예상 RTT 전문:'])
                        value='\n'.join(lines)
                    elif col==2 and ci==2:
                        value=values[0]
                    elif col==2 and ci not in [4,5]:
                        value='\n'.join(dict.fromkeys(x for x in values if x))
                    else:
                        value='\n'.join(x for x in values if x)
                    assert '(계속)' not in value
                    addr=cells[0].coordinate
                    combined[(s.title,addr)]=dict(value=value,sourceCells=[c.coordinate for c in cells if c.value is not None],
                                                sourceText='\n'.join(v for c,v in zip(cells,values) if c.value is not None))
                    ref=f'{get_column_letter(ci)}{rows[0]}:{get_column_letter(ci)}{rows[-1]}'
                    for m in s.merged_cells:
                        if m.min_row<=rows[-1] and m.max_row>=rows[0] and m.min_col<=ci<=m.max_col:
                            assert m.min_col==m.max_col==ci and rows[0]<=m.min_row<=m.max_row<=rows[-1],(s.title,ref,'overlap')
                            plan['unmerges'].append(dict(key=b['key'],sheet=s.title,range=str(m)))
                    plan['merges'].append(dict(key=b['key'],sheet=s.title,range=ref))
                    for c in cells[1:]:cleared.add((s.title,c.coordinate))
        for s in w:
            dimensions={}
            for c in list(s._cells.values()):
                if c.value is None:continue
                pos=(s.title,c.coordinate)
                before=norm(c.value)
                if pos in cleared:
                    item=dict(key=b['key'],sheet=s.title,cell=c.coordinate,before=before,value='',segments=[],reason='계속 내용은 위 병합 셀에 포함')
                else:
                    group=combined.get(pos)
                    old=edits.get((b['key'],s.title,c.coordinate))
                    core=group['value'] if group else old['value'] if old else before
                    value=padded(core)
                    sources=[s[a] for a in group['sourceCells']] if group else [c]
                    item=dict(key=b['key'],sheet=s.title,cell=c.coordinate,before=before,
                              value=value,segments=segments(sources,value),reason='앞뒤 빈 줄 한 줄/같은 계속 조건 병합')
                    if group:item.update({k:v for k,v in group.items() if k!='value'})
                    height=float(s.row_dimensions[c.row].height or s.sheet_format.defaultRowHeight or 15)
                    font_size=float(c.font.sz or 11)
                    dimensions[c.row]=max(dimensions.get(c.row,0),min(409.5,height+3*font_size))
                if item['value']!=before:plan['changes'].append(item)
            if dimensions:
                plan['wraps'].append(dict(key=b['key'],sheet=s.title,range=f'A1:{get_column_letter(s.max_column)}{s.max_row}'))
                plan['heights'].extend(dict(key=b['key'],sheet=s.title,row=r,height=h) for r,h in sorted(dimensions.items()))
        b['views']=['K58:K59','K81:K83','J161:J168','K191:K192'] if b['key']=='criterion' else ['K58:L59','K81:L83','J161:J168','K191:L192']
        w.close()
    prev.save_json(QA/'baseline.json',base)
    prev.save_json(QA/'plan.json',plan)
    print(json.dumps(dict(changes=len(plan['changes']),merges=len(plan['merges']),groups=len(plan['groups']),
        heights=len(plan['heights']),by_book=dict(collections.Counter(x['key'] for x in plan['changes']))),ensure_ascii=True))

def prepare():
    QA.mkdir(parents=True, exist_ok=True)
    assert not (QA/'baseline.json').exists(), 'baseline already exists'
    previous = read(SOURCE)
    books = []
    plan = dict(changes=[], shades=[], merges=[], heights=[])
    for source in previous['books']:
        b = dict(source)
        b['sha256'] = prev.digest(b['path'])
        b['backup'] = str(QA/(b['key']+'-before.xlsx'))
        shutil.copyfile(b['path'], b['backup'])
        assert prev.digest(b['backup']) == b['sha256']
        w = load_workbook(b['backup'], rich_text=True)
        s = w[b['sheet']]
        b['views'] = [next((str(m) for m in s.merged_cells if addr in m), addr)
                      for addr in ['J54','J161','J901','J1666']]
        for s in w:
            diff = '변경 비교' in s.title
            for c in list(s._cells.values()):
                old = norm(c.value)
                if not old or (diff and current_sign(s, c.row) != '[+]'):
                    continue
                new = cleaned(old)
                if new == old:
                    continue
                assert re.findall(r'-?\d+(?:\.\d+)?', old) == re.findall(r'-?\d+(?:\.\d+)?', new), c.coordinate
                plan['changes'].append(dict(key=b['key'],sheet=s.title,cell=c.coordinate,
                    before=old,value=new,segments=segments([c],new),
                    reason='필요한 확인/기록 안내만 남김'))
        w.close()
        books.append(b)
    targets = {b['path'] for b in books}
    protected = {p:prev.digest(p) for p in previous['protected']
                 if p not in targets and Path(p).is_file()}
    prev.save_json(QA/'baseline.json',dict(books=books,protected=protected))
    prev.save_json(QA/'plan.json',plan)
    print(json.dumps(dict(changes=len(plan['changes']),
        by_book=dict(collections.Counter(p['key'] for p in plan['changes'])),
        by_sheet=dict(collections.Counter(p['sheet'] for p in plan['changes']))),ensure_ascii=True))

def build():
    """Artifact Tool 작성값을 확인하고 원본 셀의 값만 교체한다."""
    base, plan = read(QA/'baseline.json'), read(QA/'plan.json')
    for b in base['books']:
        assert prev.digest(b['path']) == b['sha256'], ('external edit', b['key'])
        package = prev.parts(b['backup'])
        authored = prev.parts(QA/(b['key']+'-authored.xlsx'))
        astrings=[''.join(si.itertext()) for si in prev.parse(authored['xl/sharedStrings.xml'])] if 'xl/sharedStrings.xml' in authored else []
        roots, approved, source, destcells = {}, {}, {}, {}
        style=prev.parse(package['xl/styles.xml'])
        shared=prev.parse(package['xl/sharedStrings.xml']) if 'xl/sharedStrings.xml' in package else None
        for p in (p for p in plan['changes'] if p['key'] == b['key']):
            sn = p['sheet']
            if sn not in roots:
                roots[sn] = prev.parse(package[prev.sheet_path(package,sn)])
                src=prev.parse(package[prev.sheet_path(package,sn)])
                source[sn]={c.get('r'):c for c in src.findall('.//s:c',prev.N)}
                destcells[sn]={c.get('r'):c for c in roots[sn].findall('.//s:c',prev.N)}
                ar=prev.parse(authored[prev.sheet_path(authored,sn)])
                approved[sn]={c.get('r'):c for c in ar.findall('.//s:c',prev.N)}
            cell=destcells[sn].get(p['cell'])
            ac=approved[sn].get(p['cell'])
            assert cell is not None
            av=astrings[int(ac.find('s:v',prev.N).text)] if ac is not None and ac.get('t')=='s' else prev.xmltext(authored,ac)
            assert norm(av) == p['value'], (b['key'],sn,p['cell'])
            if p['value']:
                set_preserved_rich(package,cell,p,style,shared,source[sn])
            else:
                for child in list(cell):cell.remove(child)
                cell.attrib.pop('t',None)
        for m in (m for m in plan.get('unmerges',[]) if m['key']==b['key']):
            mc=roots[m['sheet']].find('s:mergeCells',prev.N)
            for child in list(mc):
                if child.get('ref')==m['range']:mc.remove(child)
        for m in (m for m in plan['merges'] if m['key']==b['key']):
            root=roots[m['sheet']]
            mc=root.find('s:mergeCells',prev.N)
            prev.E.SubElement(mc,'{'+prev.NS+'}mergeCell',ref=m['range'])
            mc.set('count',str(len(mc)))
        styles_map={}
        xfs=style.find('s:cellXfs',prev.N)
        for sn,root in roots.items():
            for c in root.findall('.//s:c',prev.N):
                if c.get('t')!='inlineStr':continue
                oldid=int(c.get('s','0'))
                if oldid not in styles_map:
                    xf=copy.deepcopy(xfs[oldid])
                    align=xf.find('s:alignment',prev.N)
                    if align is None:align=prev.E.SubElement(xf,'{'+prev.NS+'}alignment')
                    align.set('wrapText','1');xf.set('applyAlignment','1')
                    styles_map[oldid]=len(xfs);xfs.append(xf)
                c.set('s',str(styles_map[oldid]))
        xfs.set('count',str(len(xfs)))
        package['xl/styles.xml']=prev.serialize(style)
        rowmaps={sn:{int(row.get('r')):row for row in root.findall('s:sheetData/s:row',prev.N)} for sn,root in roots.items()}
        for h in (h for h in plan['heights'] if h['key']==b['key']):
            row=rowmaps[h['sheet']][h['row']]
            row.set('ht',str(h['height']));row.set('customHeight','1')
        for sn,root in roots.items():
            package[prev.sheet_path(package,sn)] = prev.serialize(root)
        with zipfile.ZipFile(QA/(b['key']+'-candidate.xlsx'),'w',zipfile.ZIP_DEFLATED) as z:
            for name,data in package.items():
                z.writestr(name,data)
        print('candidate',b['key'])

def verify_wording_only():
    base,plan = read(QA/'baseline.json'),read(QA/'plan.json')
    summaries,remaining,newbooks = [],[],{}
    for b in base['books']:
        approved = {(p['sheet'],p['cell']):p for p in plan['changes'] if p['key']==b['key']}
        old = load_workbook(b['backup'],rich_text=True)
        new = load_workbook(QA/(b['key']+'-candidate.xlsx'),rich_text=True)
        assert old.sheetnames == new.sheetnames
        for s in old:
            t = new[s.title]
            assert (s.max_row,s.max_column)==(t.max_row,t.max_column)
            assert {str(m) for m in s.merged_cells}=={str(m) for m in t.merged_cells}
            assert len(s._images)==len(t._images)
            assert set(s._cells)==set(t._cells)
            for pos,c in s._cells.items():
                d=t._cells[pos]
                p=approved.get((s.title,c.coordinate))
                assert norm(d.value)==(p['value'] if p else norm(c.value)), (b['key'],s.title,c.coordinate)
                assert c._style==d._style, (b['key'],s.title,c.coordinate,'style')
                if p:
                    _,props=character_styles([d])
                    expected=[(col,bold) for text,col,bold in p['segments'] for _ in text]
                    assert props==expected, (b['key'],s.title,c.coordinate,'text styles')
                if NEG.search(norm(d.value)) and '변경 비교' not in t.title:
                    remaining.append(dict(key=b['key'],sheet=t.title,cell=d.coordinate,text=norm(d.value)))
        op,np=prev.parts(b['backup']),prev.parts(QA/(b['key']+'-candidate.xlsx'))
        allowed={prev.sheet_path(op,sn) for sn,_ in approved}
        assert set(op)==set(np)
        for name in op:
            if name not in allowed:
                assert op[name]==np[name], (b['key'],name,'native part changed')
        for sn in {sn for sn,_ in approved}:
            ox=prev.parse(op[prev.sheet_path(op,sn)])
            nx=prev.parse(np[prev.sheet_path(np,sn)])
            for sheet,addr in approved:
                if sheet!=sn: continue
                for root in [ox,nx]:
                    c=root.find(f".//s:c[@r='{addr}']",prev.N)
                    c.attrib.pop('t',None)
                    for child in list(c): c.remove(child)
            assert xmlshape(ox)==xmlshape(nx), (b['key'],sn,'unapproved XML change')
        main=new[b['sheet']]
        count=sum(1 for c in main._cells.values()
                  if c.column==4 and re.search(r'^SW-FW-UnitTest-',norm(c.value)))
        assert count==115, (b['key'],count)
        assert 'OD1_N에 연결하지 않는다' in norm(main['J161'].value)
        assert 'RAM 기록' in norm(main['J901'].value)
        assert '불필요한 변경 요청' in norm(main['K824'].value)
        assert all(a not in norm(c.value) for t in new for c in t._cells.values()
                   if c.value is not None and ('변경 비교' not in t.title or current_sign(t,c.row)=='[+]')
                   for a,_ in REPLACEMENTS)
        summaries.append(dict(key=b['key'],changed_cells=len(approved),tests=count,
                              images=sum(len(s._images) for s in new),rows=main.max_row))
        old.close()
        newbooks[b['key']]=new
    cs=newbooks['criterion'][base['books'][0]['sheet']]
    rs=newbooks['unit'][base['books'][1]['sheet']]
    for r in range(30,cs.max_row+1):
        for col in range(1,12):
            assert norm(cs.cell(r,col).value)==norm(rs.cell(r,col).value), ('common',r,col)
    for p,h in base['protected'].items():
        assert prev.digest(p)==h, ('protected file',p)
    assert len(remaining)==2 and all(p['cell']=='K824' for p in remaining), remaining
    checks=dict(status='PASS',books=summaries,common_columns='A:K identical',
        unchanged_native_parts=True,protected_files=len(base['protected']),
        preserved_functional_requirements=remaining)
    prev.save_json(QA/'verification.json',checks)
    for w in newbooks.values(): w.close()
    print(json.dumps(checks,ensure_ascii=True))

def logs(w):
    return collections.Counter(line.strip() for s in w for c in s._cells.values()
        for line in norm(c.value).split('\n') if line.strip().startswith('[UT-'))

def verify():
    base,plan=read(QA/'baseline.json'),read(QA/'plan.json')
    checks=[];newbooks={}
    for b in base['books']:
        approved={(p['sheet'],p['cell']):p for p in plan['changes'] if p['key']==b['key']}
        old=load_workbook(b['backup'],rich_text=True)
        new=load_workbook(QA/(b['key']+'-candidate.xlsx'),rich_text=True)
        assert old.sheetnames==new.sheetnames
        assert logs(old)==logs(new),(b['key'],'RTT text or values lost')
        for s in old:
            t=new[s.title]
            merges={m['range'] for m in plan['merges'] if m['key']==b['key'] and m['sheet']==s.title}
            unmerged={m['range'] for m in plan.get('unmerges',[]) if m['key']==b['key'] and m['sheet']==s.title}
            assert {str(m) for m in t.merged_cells}==({str(m) for m in s.merged_cells}-unmerged)|merges
            assert s.max_row==t.max_row and len(s._images)==len(t._images)
            for pos,c in s._cells.items():
                d=t._cells.get(pos)
                p=approved.get((s.title,c.coordinate))
                assert norm(d.value if d else None)==(p['value'] if p else norm(c.value)),(b['key'],s.title,c.coordinate)
                if p and p['value']:
                    assert p['value'].startswith('\n') and p['value'].endswith('\n')
                    assert not p['value'].startswith('\n\n') and not p['value'].endswith('\n\n')
                if isinstance(c.value,int):
                    assert int(norm(d.value).strip())==c.value,(b['key'],s.title,c.coordinate,'numeric value')
            for c in t._cells.values():
                v=norm(c.value)
                if not v:continue
                assert v.startswith('\n') and v.endswith('\n'),(b['key'],s.title,c.coordinate,'missing padding')
                if '변경 비교' not in s.title or current_sign(s,c.row).strip()=='[+]':
                    assert all(a not in v for a,_ in REPLACEMENTS),(s.title,c.coordinate,'negative evidence wording')
                assert '(계속)' not in v,(s.title,c.coordinate,'continuation left')
        # 값과 개행, 승인 병합/행 높이, 줄 바꿈 이외의 패키지 내용은 보존한다.
        op,np=prev.parts(b['backup']),prev.parts(QA/(b['key']+'-candidate.xlsx'))
        assert set(op)==set(np)
        allowed={'xl/styles.xml'}|{prev.sheet_path(op,sn) for sn,_ in approved}
        for name in op:
            if name not in allowed:assert op[name]==np[name],(b['key'],name)
        os,ns=prev.parse(op['xl/styles.xml']),prev.parse(np['xl/styles.xml'])
        oldx,newx=os.find('s:cellXfs',prev.N),ns.find('s:cellXfs',prev.N)
        assert all(xmlshape(a)==xmlshape(z) for a,z in zip(oldx,newx))
        ns.remove(newx);os.remove(oldx)
        assert xmlshape(os)==xmlshape(ns),(b['key'],'font/fill/number format changed')
        for sn in {sn for sn,_ in approved}:
            ox,nx=prev.parse(op[prev.sheet_path(op,sn)]),prev.parse(np[prev.sheet_path(np,sn)])
            oc={c.get('r'):c for c in ox.findall('.//s:c',prev.N)}
            nc={c.get('r'):c for c in nx.findall('.//s:c',prev.N)}
            for addr,c in nc.items():
                oldc=oc.get(addr)
                assert oldc is not None,(sn,addr,'new cell')
                oldid,newid=int(oldc.get('s','0')),int(c.get('s','0'))
                if oldid!=newid:
                    a,z=copy.deepcopy(oldx[oldid]),copy.deepcopy(newx[newid])
                    for xf in [a,z]:
                        xf.attrib.pop('applyAlignment',None)
                        align=xf.find('s:alignment',prev.N)
                        if align is not None:
                            align.attrib.pop('wrapText',None)
                            if not len(align) and not align.attrib:xf.remove(align)
                    assert xmlshape(a)==xmlshape(z),(sn,addr,'non-wrap style change')
                    if oldc.get('s') is None:c.attrib.pop('s',None)
                    else:c.set('s',oldc.get('s'))
                if (sn,addr) in approved:
                    for node in [oldc,c]:
                        node.attrib.pop('t',None)
                        for child in list(node):node.remove(child)
            added={m['range'] for m in plan['merges'] if m['key']==b['key'] and m['sheet']==sn}
            mc=nx.find('s:mergeCells',prev.N)
            if mc is not None:
                for m in list(mc):
                    if m.get('ref') in added:mc.remove(m)
                mc.set('count',str(len(mc)))
            oldmc=ox.find('s:mergeCells',prev.N)
            removed={m['range'] for m in plan.get('unmerges',[]) if m['key']==b['key'] and m['sheet']==sn}
            if oldmc is not None:
                for m in list(oldmc):
                    if m.get('ref') in removed:oldmc.remove(m)
                oldmc.set('count',str(len(oldmc)))
            hr={h['row'] for h in plan['heights'] if h['key']==b['key'] and h['sheet']==sn}
            for root in [ox,nx]:
                for row in root.findall('s:sheetData/s:row',prev.N):
                    if int(row.get('r')) in hr:
                        row.attrib.pop('ht',None);row.attrib.pop('customHeight',None)
            assert xmlshape(ox)==xmlshape(nx),(b['key'],sn,'unapproved native change')
        main=new[b['sheet']]
        count=sum(1 for c in main._cells.values() if c.column==4 and norm(c.value).strip().startswith('SW-FW-UnitTest-'))
        assert count==115,(b['key'],count)
        assert 'OD1_N에 연결하지 않는다' in norm(main['J161'].value)
        assert '불필요한 변경 요청' in norm(main['K824'].value)
        checks.append(dict(key=b['key'],cells=len(approved),tests=count,rows=main.max_row,
            images=sum(len(s._images) for s in new),merges=sum(m['key']==b['key'] for m in plan['merges'])))
        old.close();newbooks[b['key']]=new
    cs=newbooks['criterion'][base['books'][0]['sheet']];rs=newbooks['unit'][base['books'][1]['sheet']]
    for r in range(30,cs.max_row+1):
        for col in range(1,12):
            assert norm(cs.cell(r,col).value)==norm(rs.cell(r,col).value),('common',r,col)
    assert {str(m) for m in cs.merged_cells}=={str(m) for m in rs.merged_cells if m.max_col<=11}
    for p,h in base['protected'].items():assert prev.digest(p)==h,('protected',p)
    result=dict(status='PASS',books=checks,common_columns='A:K identical',all_populated_cells_padded=True,
        all_RTT_lines_preserved=True,styles_preserved_except_wrap_and_height=True,protected_files=len(base['protected']))
    prev.save_json(QA/'verification.json',result)
    for w in newbooks.values():w.close()
    print(json.dumps(result,ensure_ascii=True))

def publish():
    assert read(QA/'verification.json')['status']=='PASS'
    assert all(x['savedValuesAndFonts']=='PASS' for x in read(QA/'native-checks.json'))
    base=read(QA/'baseline.json')
    handles=[]
    try:
        for b in base['books']:
            assert prev.digest(b['path'])==b['sha256'], ('external edit',b['key'])
            handles.append((b,open(b['path'],'r+b')))
        for b,h in handles:
            data=(QA/(b['key']+'-candidate.xlsx')).read_bytes()
            h.seek(0);h.write(data);h.truncate();h.flush()
    finally:
        for _,h in handles: h.close()
    saved=[]
    for b in base['books']:
        h=prev.digest(b['path'])
        assert h==prev.digest(QA/(b['key']+'-candidate.xlsx'))
        saved.append(dict(key=b['key'],path=b['path'],sha256=h))
    prev.save_json(QA/'publication.json',dict(status='PASS',books=saved))
    print('published',len(saved))

def saved_checks():
    base=read(QA/'baseline.json');result=[]
    for b in base['books']:
        assert prev.digest(b['path'])==prev.digest(QA/(b['key']+'-candidate.xlsx'))
        w=load_workbook(b['path'],rich_text=True);s=w[b['sheet']]
        assert 'QCC' in norm(s['C19'].value)+norm(s['D19'].value)
        assert norm(s['D1703'].value).strip().endswith('BOOT-ID-20')
        if b['key']=='unit':
            assert 'L1252:L1254' in {str(m) for m in s.merged_cells}
            assert 'L1271:L1272' in {str(m) for m in s.merged_cells}
        assert sum('(계속)' in norm(c.value) for t in w for c in t._cells.values())==0
        result.append(dict(key=b['key'],qcc='C19:D19 / D1703 unchanged',sha256=prev.digest(b['path'])))
        w.close()
    for p,h in base['protected'].items():assert prev.digest(p)==h,('protected',p)
    prev.save_json(QA/'saved-checks.json',dict(status='PASS',books=result))
    print('saved checks PASS')

def audit():
    found=collections.defaultdict(list)
    for b in read(SOURCE)['books']:
        w=load_workbook(b['path'],rich_text=True)
        for s in w:
            if '변경 비교' in s.title:
                continue
            for c in s._cells.values():
                v=norm(c.value)
                if not NEG.search(v):continue
                # 문장 단위로 확인하되 소수/코드의 마침표를 문장 끝으로 읽지 않는다.
                for sentence in re.split(r'(?<=\.)\s+|\n',v):
                    if NEG.search(sentence):
                        found[sentence].append(f"{b['key']}/{s.title}/{c.coordinate}")
        w.close()
    for sentence,places in found.items():
        print(json.dumps(dict(sentence=sentence,locations=places),ensure_ascii=False))
    print('unique',len(found))

def inspect_padding():
    report=[]
    for b in read(QA/'baseline.json')['books']:
        w=load_workbook(b['backup'],rich_text=True)
        types=collections.Counter((c.data_type,type(c.value).__name__) for s in w
                                  for c in s._cells.values() if c.value is not None)
        print(b['key'],str(dict(types)))
        found=[]
        for s in w:
            for c in list(s._cells.values()):
                if '(계속)' not in norm(c.value): continue
                found.append(dict(sheet=s.title,cell=c.coordinate,
                    value=norm(c.value),above=norm(s.cell(c.row-1,c.column).value),
                    merges=[str(m) for m in s.merged_cells if c.coordinate in m]))
        item=dict(key=b['key'],types={str(k):v for k,v in types.items()},
                  continuations=found,by_sheet=dict(collections.Counter(x['sheet'] for x in found)),
                  max_height=max(d.height or 0 for s in w for d in s.row_dimensions.values()))
        report.append(item)
        print(json.dumps({k:v for k,v in item.items() if k!='continuations'},ensure_ascii=True))
        w.close()
    prev.save_json(QA/'padding-inspection.json',report)

def inspect_groups():
    b=read(QA/'baseline.json')['books'][1]
    w=load_workbook(b['backup'],rich_text=True)
    for sn,rows,cols in [('보고서_유닛 시험',[58,59,81,82,83,681,682,683],['D','K','L','M']),
                         ('RTT 결과 대조',[3,5,6,10,11,12],list('ABCDEF'))]:
        s=w[sn]
        for r in rows:
            print(json.dumps(dict(sheet=sn,row=r,values={c:norm(s[f'{c}{r}'].value).split('\n')[:4]
                for c in cols}),ensure_ascii=True))
    w.close()

def inspect_left():
    for b in read(QA/'baseline.json')['books']:
        w=load_workbook(QA/(b['key']+'-candidate.xlsx'),rich_text=True)
        for s in w:
            hits=[c for c in s._cells.values() if '(계속)' in norm(c.value)]
            print(json.dumps(dict(key=b['key'],sheet=s.title,count=len(hits)),ensure_ascii=True))
            for c in hits[:3]:
                print(json.dumps(dict(cell=c.coordinate,lines=[x for x in norm(c.value).split('\n') if '(계속)' in x],
                    above=[dict(row=r,value=norm(s.cell(r,c.column).value).split('\n')[:2],
                                merges=[str(m) for m in s.merged_cells if s.cell(r,c.column).coordinate in m])
                           for r in range(max(1,c.row-3),c.row+1)]),ensure_ascii=True))
        w.close()

if __name__=='__main__':
    dict(audit=audit,prepare=prepare,build=build,verify=verify,publish=publish,
         inspect_padding=inspect_padding,inspect_groups=inspect_groups,expand=expand_padding,
         inspect_left=inspect_left,saved_checks=saved_checks)[sys.argv[1]]()
