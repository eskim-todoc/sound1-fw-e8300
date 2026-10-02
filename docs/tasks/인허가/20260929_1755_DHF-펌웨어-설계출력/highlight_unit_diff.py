"""Use rich OOXML runs for the unsupported per-character comparison feature."""
from pathlib import Path
import zipfile,json,difflib
from lxml import etree as E

T=Path(__file__).resolve().parent;QA=T/'qa/unit-reconciliation'
plan=json.loads((QA/'tables-plan.json').read_text(encoding='utf8'))['suvc']
path=QA/'SUVC-candidate.xlsx'
S='http://schemas.openxmlformats.org/spreadsheetml/2006/main';q=lambda n:'{'+S+'}'+n
N={'s':S,'r':'http://schemas.openxmlformats.org/officeDocument/2006/relationships'}
with zipfile.ZipFile(path) as z:parts={n:z.read(n) for n in z.namelist()}
wb=E.fromstring(parts['xl/workbook.xml']);rels=E.fromstring(parts['xl/_rels/workbook.xml.rels'])
rid=wb.xpath('//s:sheet[@name="기준서_유닛 검증_변경 비교"]/@r:id',namespaces=N)[0]
target=next(x.get('Target') for x in rels if x.get('Id')==rid)
entry=target.lstrip('/') if target.startswith('/') else 'xl/'+target
sheet=E.fromstring(parts[entry]);cells={c.get('r'):c for c in sheet.iter(q('c'))}
flags={}
for ni,pi in plan['pairs']:
    for col in range(1,11):
        old=str(plan['diff'][ni][col] or '');new=str(plan['diff'][pi][col] or '')
        neg=flags.setdefault((ni,col),[True]*len(old));pos=flags.setdefault((pi,col),[True]*len(new))
        for m in difflib.SequenceMatcher(None,old,new,autojunk=False).get_matching_blocks():
            neg[m.a:m.a+m.size]=[False]*m.size;pos[m.b:m.b+m.size]=[False]*m.size
changed=0
for logical,row in enumerate(plan['diff']):
    first,count=plan['diffpositions'][logical]
    for col in range(11):
        whole=str(row[col] or '');mask=flags.get((logical,col),[True]*len(whole));offset=0
        for k in range(count):
            text=str(plan['diffphysical'][first+k][col] or '')
            if not text:continue
            addr=f'{chr(65+col)}{30+first+k}';cell=cells[addr]
            start=whole.find(text,offset)
            cm=mask[start:start+len(text)] if start>=0 else [True]*len(text)
            if start>=0:offset=start+len(text)
            for child in list(cell):cell.remove(child)
            cell.set('t','inlineStr');inline=E.SubElement(cell,q('is'))
            begin=0
            while begin<len(text):
                stop=begin+1
                while stop<len(text) and cm[stop]==cm[begin]:stop+=1
                run=E.SubElement(inline,q('r'));rp=E.SubElement(run,q('rPr'))
                E.SubElement(rp,q('rFont'),val='맑은 고딕');E.SubElement(rp,q('sz'),val='11')
                E.SubElement(rp,q('b'),val='1' if cm[begin] else '0')
                color=('FF9C2020' if row[0]=='[-]' else 'FF14632A') if cm[begin] else 'FF666666'
                E.SubElement(rp,q('color'),rgb=color)
                t=E.SubElement(run,q('t'));t.set('{http://www.w3.org/XML/1998/namespace}space','preserve');t.text=text[begin:stop]
                changed+=sum(cm[begin:stop]);begin=stop
parts[entry]=E.tostring(sheet,xml_declaration=True,encoding='UTF-8',standalone=True)
with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED) as z:
    for n,data in parts.items():z.writestr(n,data)
print('Character emphasis applied:',changed)
