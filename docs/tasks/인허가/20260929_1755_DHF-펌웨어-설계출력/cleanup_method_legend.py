"""방법 안내 문구만 교체한다. 값 작성은 Artifact Tool, 원래 패키지 보존은 OOXML로 처리한다."""
from pathlib import Path
from copy import deepcopy
import hashlib,json,shutil,sys,zipfile
from lxml import etree as E
import openpyxl
from inspect_methods import T,D,ROOT,paths
from update_method_definitions import LEGEND
Q=T/'qa/method-legend-cleanup'
NS={'s':'http://schemas.openxmlformats.org/spreadsheetml/2006/main'}
NOTE='시험 방법은 위의 I/A/D/T 정의를 따른다. RTT 결과가 누락되면 판정하지 않고 다시 확인한다.'
def tag(n):return '{'+NS['s']+'}'+n
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def parts(p):
    with zipfile.ZipFile(p) as z:return {n:z.read(n) for n in z.namelist()}
def sheetparts(data):
    r=E.fromstring(data['xl/workbook.xml']);links={x.get('Id'):x.get('Target') for x in E.fromstring(data['xl/_rels/workbook.xml.rels'])}
    out={}
    for x in r.find(tag('sheets')):
        target=links[x.get('{http://schemas.openxmlformats.org/officeDocument/2006/relationships}id')]
        out[x.get('name')]=target.lstrip('/') if target.startswith('/') else 'xl/'+target
    return out
def value(cell,data):
    if cell.get('t')=='s':
        strings=E.fromstring(data['xl/sharedStrings.xml']);return ''.join(strings[int(cell.findtext(tag('v')))].xpath('.//s:t/text()',namespaces=NS))
    if cell.get('t')=='str':return cell.findtext(tag('v')) or ''
    return ''.join(cell.xpath('.//s:t/text()',namespaces=NS))
def prepare():
    Q.mkdir(exist_ok=True)
    targets=set(paths.values())
    protected={str(p):sha(p) for p in D.rglob('*') if p.is_file() and not p.name.startswith('~$') and p not in targets}
    protected.update({str(p):sha(p) for p in (ROOT/'src').rglob('*') if p.is_file() and p.suffix.lower() in ('.c','.h','.hct')})
    plan={'legend':LEGEND,'unitNote':NOTE,'protected':protected,'books':[]}
    for key,path in paths.items():
        d=Q/key;d.mkdir(exist_ok=True);shutil.copy2(path,d/'before.xlsx')
        w=openpyxl.load_workbook(path,rich_text=True);edits={}
        for s in w.worksheets[1:3]:
            assert 'Inspection' in str(s['C12'].value)
            edits[s.title]={'C12':LEGEND}
            if key=='unit':edits[s.title]['D23']=NOTE
        plan['books'].append({'key':key,'path':str(path),'sha':sha(path),'edits':edits,'cols':{'input':10,'system':10,'unit':11,'integration':13}[key]})
    (Q/'plan.json').write_text(json.dumps(plan,ensure_ascii=False,indent=2),encoding='utf8')
    print([(b['key'],b['edits']) for b in plan['books']])
def apply():
    p=json.loads((Q/'plan.json').read_text(encoding='utf8'))
    for b in p['books']:
        d=Q/b['key'];data=parts(d/'before.xlsx');authored=parts(d/'authored.xlsx');am=sheetparts(authored)
        new=value(E.fromstring(authored[am['정의']]).find('.//'+tag('c')),authored)
        note=value(E.fromstring(authored[am['유닛 안내']]).find('.//'+tag('c')),authored)
        assert new==LEGEND and note==NOTE
        sm=sheetparts(data)
        for sheet,edits in b['edits'].items():
            r=E.fromstring(data[sm[sheet]]);cells={c.get('r'):c for c in r.iter(tag('c'))}
            for addr,s in edits.items():
                c=cells[addr]
                for x in list(c):
                    if x.tag in (tag('v'),tag('is'),tag('f')):c.remove(x)
                c.set('t','inlineStr');ins=E.SubElement(c,tag('is'));tt=E.SubElement(ins,tag('t'));tt.text=new if addr=='C12' else note
                tt.set('{http://www.w3.org/XML/1998/namespace}space','preserve')
            data[sm[sheet]]=E.tostring(r,encoding='UTF-8',xml_declaration=True,standalone=True)
        with zipfile.ZipFile(d/'candidate.xlsx','w',zipfile.ZIP_DEFLATED) as z:
            for name,raw in data.items():z.writestr(name,raw)
    verify()
def verify(publish=False):
    p=json.loads((Q/'plan.json').read_text(encoding='utf8'));result=[]
    for b in p['books']:
        d=Q/b['key'];a=parts(d/'before.xlsx');z=parts(d/'candidate.xlsx');sm=sheetparts(a)
        assert set(a)==set(z)
        changed={sm[s] for s in b['edits']}
        for name in a:
            if name not in changed:assert a[name]==z[name],(b['key'],name)
        for sheet,edits in b['edits'].items():
            before=E.fromstring(a[sm[sheet]]);after=E.fromstring(z[sm[sheet]])
            ac={c.get('r'):c for c in before.iter(tag('c'))};zc={c.get('r'):c for c in after.iter(tag('c'))}
            for addr,s in edits.items():
                assert value(zc[addr],z)==s
                parent=zc[addr].getparent();parent.replace(zc[addr],deepcopy(ac[addr]))
            assert E.tostring(before)==E.tostring(after),(b['key'],sheet,'Unrequested content/style')
        w=openpyxl.load_workbook(d/'candidate.xlsx',rich_text=True)
        assert not any('한 글자' in str(c.value) for s in w for row in s for c in row)
        assert all('(' not in str(w[s]['C12'].value) and '예:' not in str(w[s]['C12'].value) for s in b['edits'])
        result.append({'key':b['key'],'sha':sha(d/'candidate.xlsx'),'edited_cells':sum(map(len,b['edits'].values())),'all_other_parts_preserved':True})
    assert all(sha(Path(path))==h for path,h in p['protected'].items()),'Excluded document/source changed'
    (Q/'checks.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    print(result)
    if publish:
        reviewed=json.loads((Q/'visual-reviewed.json').read_text(encoding='utf8'))
        for b,r in zip(p['books'],result):
            assert reviewed[b['key']]==r['sha']
            assert sha(Path(b['path']))==b['sha'],'External edit'
            with Path(b['path']).open('r+b'):pass
        for b in p['books']:
            shutil.copyfile(Q/b['key']/'candidate.xlsx',b['path'])
            assert sha(Path(b['path']))==sha(Q/b['key']/'candidate.xlsx')
        (Q/'publication.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
if __name__=='__main__':{'prepare':prepare,'apply':apply,'verify':verify,'publish':lambda:verify(True)}[sys.argv[1]]()
