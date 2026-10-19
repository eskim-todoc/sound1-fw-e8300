"""Remove stale template shared-formula nodes retained by XLSX import/export.

All replacement text is authored through Artifact Tool. This repair only removes
old formula definitions from report cells that are specified as literal values.
It preserves the authored shared-string value and every other package member.
"""
from pathlib import Path
import zipfile
from lxml import etree as E

P=Path(__file__).parent/'qa/suvr/candidate.xlsx'
NS={'s':'http://schemas.openxmlformats.org/spreadsheetml/2006/main'}
with zipfile.ZipFile(P) as z:
    parts={i.filename:z.read(i.filename) for i in z.infolist()}
root=E.fromstring(parts['xl/worksheets/sheet2.xml'])
removed=[]
for c in root.findall('.//s:c',NS):
    a=c.get('r');row=int(''.join(x for x in a if x.isdigit()))
    if 34<=row<=223 and a not in ['M153','M154']:
        f=c.find('s:f',NS)
        if f is not None:
            # Literal cells already contain the newly authored string index.
            if row<=148:assert c.get('t') in ['s','str'], (a,E.tostring(c))
            c.remove(f);removed.append(a)
parts['xl/worksheets/sheet2.xml']=E.tostring(root,xml_declaration=True,encoding='UTF-8',standalone=True)
temp=P.with_suffix('.repaired.xlsx')
with zipfile.ZipFile(temp,'w',zipfile.ZIP_DEFLATED) as z:
    for name,data in parts.items():z.writestr(name,data)
temp.replace(P)
print('Removed stale shared formulas:',len(removed))
