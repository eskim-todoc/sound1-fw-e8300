"""Transfer authored cells into original OOXML to retain drawings/styles/print setup."""
from build_inputs import *
import math
from openpyxl.utils import get_column_letter,column_index_from_string
jobs=json.loads((TASK/'workbook-plan.json').read_text(encoding='utf-8'))
for job in jobs:
    parts=readzip(job['staged']);edited=openpyxl.load_workbook(STAGE/('artifact-'+job['name']),data_only=False)
    cache=openpyxl.load_workbook(STAGE/('artifact-'+job['name']),data_only=True)
    original=openpyxl.load_workbook(job['staged'])
    styles=E.fromstring(parts['xl/styles.xml']);xfs=styles.find(f'{{{S}}}cellXfs');wrapped={}
    for si,sh in enumerate(edited,1):
        name=f'xl/worksheets/sheet{si}.xml';x=E.fromstring(parts[name]);data=x.find(f'{{{S}}}sheetData')
        for addr,value in job['edits'].get(sh.title,{}).items():
            c=sh[addr]
            assert c.value==value or (c.value is None and value in (None,'')),(job['kind'],addr,c.value,value)
            rn=c.row;row=next((r for r in data if int(r.get('r'))==rn),None)
            if row is None:row=E.SubElement(data,f'{{{S}}}row',r=str(rn))
            cell=next((v for v in row if v.get('r')==addr),None)
            if cell is None:
                cell=E.Element(f'{{{S}}}c',r=addr)
                # Existing template cell style or nearest preceding body-row style.
                ref=original[sh.title].cell(min(rn,79 if job['kind']=='SRS' else 92),c.column)
                cell.set('s',str(ref.style_id))
                ix=next((i for i,v in enumerate(row) if column_index_from_string(re.sub(r'\d','',v.get('r')))>c.column),len(row));row.insert(ix,cell)
            for child in list(cell):cell.remove(child)
            cell.attrib.pop('t',None)
            if si==2 and isinstance(value,str) and not value.startswith('='):
                style=int(cell.get('s','0'))
                if style not in wrapped:
                    xf=deepcopy(xfs[style]);al=xf.find(f'{{{S}}}alignment')
                    if al is None:al=E.SubElement(xf,f'{{{S}}}alignment')
                    al.set('wrapText','1');xf.set('applyAlignment','1');wrapped[style]=len(xfs);xfs.append(xf)
                cell.set('s',str(wrapped[style]))
            if value is None:continue
            if isinstance(value,str) and value.startswith('='):
                E.SubElement(cell,f'{{{S}}}f').text=value[1:]
                cv=cache[sh.title][addr].value
                if isinstance(cv,str):cell.set('t','str')
                if cv is not None:E.SubElement(cell,f'{{{S}}}v').text=str(cv)
            elif isinstance(value,str):
                cell.set('t','inlineStr');t=E.SubElement(E.SubElement(cell,f'{{{S}}}is'),f'{{{S}}}t');t.text=value;t.set('{http://www.w3.org/XML/1998/namespace}space','preserve')
            else:E.SubElement(cell,f'{{{S}}}v').text=str(value)
        if si==2:
            # Only enough height to show changed text; column widths and print scale stay original.
            for row in data:
                rn=int(row.get('r'))
                if not (6<=rn<=91 if job['kind']=='SRS' else 25<=rn<=104):continue
                high=float(row.get('ht','17'))
                for cell in row:
                    addr=cell.get('r');v=sh[addr].value
                    if not isinstance(v,str) or v.startswith('='):continue
                    col=re.sub(r'\d','',addr);width=original[sh.title].column_dimensions[col].width
                    if col=='C' and job['kind']=='SRS':width+=17
                    # Korean glyphs take roughly two Latin character widths.
                    size=original[sh.title][addr].font.sz or 11
                    lines=sum(max(1,math.ceil(sum(2 if ord(ch)>127 else 1 for ch in line)/max(4,width-1))) for line in v.split('\n'))
                    high=max(high,lines*(size+3)+8)
                row.set('ht',str(min(400,high)));row.set('customHeight','1')
        parts[name]=ser(x)
    xfs.set('count',str(len(xfs)));parts['xl/styles.xml']=ser(styles)
    # Source author/approver signatures are drawings, not cell values.
    for name in list(parts):
        if re.match(r'xl/drawings/drawing\d+\.xml$',name):
            dr=E.fromstring(parts[name]);dn={'x':'http://schemas.openxmlformats.org/drawingml/2006/spreadsheetDrawing'}
            for anchor in list(dr):
                col=anchor.find('x:from/x:col',dn);row=anchor.find('x:from/x:row',dn)
                if col is not None and row is not None and col.text=='9' and row.text in ('11','15'):dr.remove(anchor)
            parts[name]=ser(dr)
    # No obsolete external workbook dependency remains after explicit RID mapping.
    wx=E.fromstring(parts['xl/workbook.xml'])
    ext=wx.find(f'{{{S}}}externalReferences')
    if ext is not None:wx.remove(ext)
    calc=wx.find(f'{{{S}}}calcPr')
    if calc is not None:calc.set('fullCalcOnLoad','1')
    parts['xl/workbook.xml']=ser(wx)
    # Remove only no-longer-used external-link package parts.
    relname='xl/_rels/workbook.xml.rels';rels=E.fromstring(parts[relname])
    for r in list(rels):
        if r.get('Type','').endswith(('/externalLink','/calcChain')):rels.remove(r)
    parts[relname]=ser(rels)
    for n in list(parts):
        if n.startswith('xl/externalLinks/') or n=='xl/calcChain.xml':del parts[n]
    ct=E.fromstring(parts['[Content_Types].xml'])
    for n in list(ct):
        if n.get('PartName','').startswith('/xl/externalLinks/') or n.get('PartName')=='/xl/calcChain.xml':ct.remove(n)
    parts['[Content_Types].xml']=ser(ct)
    writezip(OUT/job['name'],parts)
    print('Preserved original package:',job['name'])
