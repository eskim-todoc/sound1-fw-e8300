from build_inputs import *
from openpyxl.utils.cell import range_boundaries,get_column_letter
jobs=json.loads((TASK/'workbook-plan.json').read_text(encoding='utf-8'))
def shifted(s,at,n):
    return re.sub(r'(\$?[A-Z]+)(\$?)(\d+)',lambda m:m[1]+m[2]+str(int(m[3])+n if int(m[3])>=at else int(m[3])),s)
for job in jobs:
    parts=readzip(job['source']);x=E.fromstring(parts['xl/worksheets/sheet2.xml']);ns={'s':S};data=x.find(f'{{{S}}}sheetData');extra=job['extra']
    if job['kind']=='SSTC':
        for row in reversed(list(data)):
            r=int(row.get('r'))
            if r>=93:
                row.set('r',str(r+extra))
                for c in row:
                    c.set('r',shifted(c.get('r'),93,extra))
                    f=c.find(f'{{{S}}}f')
                    if f is not None and f.text:f.text=shifted(f.text,93,extra)
        for tag in ('mergeCell','dimension','autoFilter'):
            for n in x.findall(f'.//{{{S}}}{tag}'):
                if n.get('ref'):n.set('ref',shifted(n.get('ref'),93,extra))
        start=93;template=next(r for r in data if r.get('r')=='92')
    else:start=80;template=next(r for r in data if r.get('r')=='79')
    for r in range(start,start+extra):
        row=deepcopy(template);row.set('r',str(r));row.set('ht','95');row.set('customHeight','1')
        for c in row:
            c.set('r',re.sub(r'\d+$',str(r),c.get('r')))
            for node in list(c):c.remove(node)
            c.attrib.pop('t',None)
        idx=next((i for i,old in enumerate(data) if int(old.get('r'))>r),len(data));data.insert(idx,row)
        if job['kind']=='SRS':
            m=x.find(f'{{{S}}}mergeCells');E.SubElement(m,f'{{{S}}}mergeCell',ref=f'C{r}:E{r}');m.set('count',str(len(m)))
    dim=x.find(f'{{{S}}}dimension');dim.set('ref',f'A1:{"I" if job["kind"]=="SRS" else "J"}{max(int(r.get("r")) for r in data)}')
    parts['xl/worksheets/sheet2.xml']=ser(x)
    wb=E.fromstring(parts['xl/workbook.xml'])
    for name in wb.findall(f'.//{{{S}}}definedName'):
        if job['kind']=='SSTC' and name.text and '기준서_시스템' in name.text:name.text=shifted(name.text,93,extra)
        elif job['kind']=='SRS' and name.text and '79' in name.text:name.text=name.text.replace('$79',f'${79+extra}')
    parts['xl/workbook.xml']=ser(wb)
    staged=STAGE/('base-'+job['name']);writezip(staged,parts);job['staged']=str(staged)
(TASK/'workbook-plan.json').write_text(json.dumps(jobs,ensure_ascii=False,indent=2),encoding='utf-8')
