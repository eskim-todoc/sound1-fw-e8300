"""Keep native print setup and existing rich text lost by artifact-tool round trips.

The artifact tool authors the changed values. Only those exported values are
transplanted into the untouched package. Unsupported run-level QCC/E8300 emphasis
is applied locally; every unrelated cell and package part is retained byte-for-byte.
"""
from copy import deepcopy
import json
import zipfile
from lxml import etree as E
from inspect_audit_units import QA

S='http://schemas.openxmlformats.org/spreadsheetml/2006/main'
N={'s':S}
q=lambda v:'{'+S+'}'+v

def unpack(path):
    with zipfile.ZipFile(path) as z:return {n:z.read(n) for n in z.namelist()}

def value(cell,strings):
    if cell.get('t')=='s':return ''.join(strings[int(cell.find(q('v')).text)].itertext())
    if cell.get('t')=='inlineStr':return ''.join(cell.find(q('is')).itertext())
    v=cell.find(q('v'));return v.text if v is not None else ''

def base_rpr(styles,cell):
    xf=styles.find(q('cellXfs'))[int(cell.get('s','0'))]
    font=styles.find(q('fonts'))[int(xf.get('fontId','0'))]
    pr=E.Element(q('rPr'))
    for child in font:
        node=deepcopy(child)
        if node.tag==q('name'):node.tag=q('rFont')
        pr.append(node)
    return pr

def prop(pr,name,val=None):
    for old in pr.findall(q(name)):pr.remove(old)
    E.SubElement(pr,q(name),{} if val is None else {'val':val})

def mark(pr,mode):
    prop(pr,'b')
    for old in pr.findall(q('color')):pr.remove(old)
    E.SubElement(pr,q('color'),{'rgb':'FFC00000' if mode=='qcc' else 'FF0070C0'})
    if mode=='qcc':prop(pr,'sz','12');prop(pr,'u','single');prop(pr,'rFont','맑은 고딕')

def text_runs(cell,content,styles,mode=None):
    pr=base_rpr(styles,cell)
    for c in list(cell):cell.remove(c)
    cell.set('t','inlineStr');entry=E.SubElement(cell,q('is'))
    segments=[]
    for line in content.splitlines(keepends=True):
        marked=mode or ('e8300' if '[E8300 ' in line or '보완 필요' in line else None)
        if segments and segments[-1][1]==marked:segments[-1]=(segments[-1][0]+line,marked)
        else:segments.append((line,marked))
    for chunk,marked in segments:
        r=E.SubElement(entry,q('r'));rp=deepcopy(pr)
        if marked:mark(rp,marked)
        r.append(rp)
        E.SubElement(r,q('t'),{'{http://www.w3.org/XML/1998/namespace}space':'preserve'}).text=chunk

def main():
    plan=json.loads((QA/'sheet-edits.json').read_text(encoding='utf8'))
    report={}
    for kind in ('suvc','suvr'):
        original=unpack(QA/(kind+'-before.xlsx'))
        authored=unpack(QA/(kind+'-artifact.xlsx'))
        root=E.fromstring(original['xl/worksheets/sheet2.xml'])
        src=E.fromstring(authored['xl/worksheets/sheet2.xml'])
        ss=E.fromstring(authored['xl/sharedStrings.xml']) if 'xl/sharedStrings.xml' in authored else []
        styles=E.fromstring(original['xl/styles.xml'])
        edits={**plan['common'],**(plan['report'] if kind=='suvr' else {})}
        for addr,want in edits.items():
            cell=root.find(f'.//s:c[@r="{addr}"]',N)
            source=src.find(f'.//s:c[@r="{addr}"]',N)
            assert cell is not None and source is not None,addr
            actual=value(source,ss)
            assert actual==want,(addr,actual,want)
            text_runs(cell,actual,styles,'qcc' if addr in plan['qccCells'] else None)
        for row,height in plan['heights'].items():
            r=root.find(f'.//s:row[@r="{row}"]',N)
            r.set('ht',str(height));r.set('customHeight','1')
        original['xl/worksheets/sheet2.xml']=E.tostring(root,encoding='UTF-8',xml_declaration=True,standalone=True)
        with zipfile.ZipFile(QA/(kind+'.xlsx'),'w',zipfile.ZIP_DEFLATED) as z:
            for name,data in original.items():z.writestr(name,data)
        report[kind]={'changed_cells':list(edits),'missing_roundtrip_parts':sorted(set(original)-set(authored)),
                      'preserved_features':['rich text outside target cells','print setup and printer settings','sheet properties','metadata','all other sheets/parts'],
                      'supplemental_OOXML':['run-level underline/red 12pt/bold','run-level blue bold','unchanged package preservation']}
    (QA/'xlsx-preservation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(report,ensure_ascii=False,indent=2))

if __name__=='__main__':main()
