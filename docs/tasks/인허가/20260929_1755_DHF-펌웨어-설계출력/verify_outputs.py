from build_architecture import *
from collections import Counter
plans=json.loads((T/'test_plan.json').read_text(encoding='utf8'))
report={}
inputs=ROOT/'docs/참고/인허가/사운드1/DHF/02_설계 입력/SW/펌웨어'
srs=openpyxl.load_workbook(next(inputs.glob('*SRS*.xlsx')),data_only=True)['소프트웨어 입력 요구사항 사양서']
upper={}
for row in srs:
 if not isinstance(row[6].value,str) or not re.fullmatch(r'SW-FW-RID-\d+',row[6].value):continue
 cell=row[7];value=cell.value
 if value is None:
    for merged in srs.merged_cells.ranges:
        if cell.coordinate in merged:value=srs.cell(merged.min_row,merged.min_col).value;break
 upper[row[6].value]=set(re.findall(r'SYS-RID-FW-\d+',str(value)))
detail=E.fromstring(zipfile.ZipFile(next(STAGE.glob('SDDD*.docx'))).read('word/document.xml'))
si_ids=set()
for p in detail.xpath('//w:body/w:p',namespaces=N):
 if txt(p).startswith('SI-ID-'):si_ids.update(re.findall(r'SI-ID-\w+-\d+',txt(p)))
su_ids=set(json.loads((T/'detail_ids.json').read_text(encoding='utf8')))
for p in plans:
 f=OUT/p['name'];w=openpyxl.load_workbook(f,data_only=False)
 s=w[p['sheet']];d=w[p['sheet']+'_변경 비교']
 got=[[s.cell(p['start']+i,c+1).value for c in range(p['cols'])] for i in range(len(p['physical']))]
 def norm(v):
    if isinstance(v,str):return v.strip() or None
    return v
 assert [[norm(x) for x in r] for r in got]==[[norm(x) for x in r] for r in p['physical']],p['kind']
 for i,e in enumerate(p['diffphysical']):
    row=[d.cell(p['start']+i,c+1).value for c in range(p['cols'])]
    assert list(map(norm,row))==list(map(norm,[e['mark']]+e['values'][1:])),(p['kind'],i)
 if p['kind']=='SUVC':
    ids={r[6] for r in got if r[6]};assert ids==set(json.loads((T/'detail_ids.json').read_text(encoding='utf8')))
 if p['kind']=='SSTC':
    ids={r[4] for r in got};assert ids=={f'SW-FW-RID-{i}' for i in range(1,82)}
    sysids=set(re.findall(r'SYS-RID-FW-\d+',' '.join(str(r[5]) for r in got)))
    assert sysids=={f'SYS-RID-FW-{i}' for i in range(1,51)},sysids
    for row in got:assert set(re.findall(r'SYS-RID-FW-\d+',str(row[5])))==upper[row[4]],row[4]
 if p['kind']=='SITC':
    for row in got:
        assert set(re.findall(r'SI-ID-\w+-\d+',str(row[3])))<=si_ids,row[3]
        assert set(re.findall(r'SU-ID-\w+-\d+',str(row[5])))<=su_ids,row[5]
 assert len(w.worksheets[0]._images)==0, 'Copied signature remains'
 report[p['kind']]={'tests':len(p['rows']),'physical_rows':len(got),'comparison_rows':len(p['diffphysical']),'saved_values_match':True,'height_limit_rows':[i for i in range(p['start'],p['start']+len(got)) if (s.row_dimensions[i].height or 0)>=409]}
for item in INV:assert hashlib.sha256(Path(item['source']).read_bytes()).hexdigest()==item['sha256']
for f in OUT.glob('*.docx'):
 if f.name.startswith('~'):continue
 with zipfile.ZipFile(f) as z:
    root=E.fromstring(z.read('word/document.xml'));settings=E.fromstring(z.read('word/settings.xml'))
    revision_count=len(root.xpath('//w:ins|//w:del',namespaces=N))
    assert revision_count>0
    assert settings.find(q('trackRevisions')) is not None
    report[f.name]={'revisions':revision_count,'tracking_enabled':True}
report['source_unchanged']=True
(T/'verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps(report,ensure_ascii=False,indent=2))
