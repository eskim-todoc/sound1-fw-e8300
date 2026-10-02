from pathlib import Path
import json,re
import openpyxl,pypdfium2 as pdf
Q=Path(__file__).parent/'feedback-srs-20260929';j=json.loads((Q/'plan.json').read_text(encoding='utf8'))
w=openpyxl.load_workbook(Q/'final.xlsx',data_only=True);s=w[j['sheet']];d=w[j['diffname']]
before=openpyxl.load_workbook(Q/'before.xlsx',data_only=True)
for row in before.worksheets[0]:
 for c in row:assert c.value==w.worksheets[0][c.coordinate].value
ids={}
for item in j['body']:
 r=item['row']
 for c,v in enumerate(item['values'],2):
  actual=s.cell(r,c)
  assert actual.value==v,('body',r,c,actual.value,v)
 if item['values'][5]:ids[item['values'][5]]=r
assert set(ids)=={f'SW-FW-RID-{n}' for n in range(1,82)}
assert s.cell(ids['SW-FW-RID-53'],8).value=='SYS-RID-FW-30'
assert s.cell(ids['SW-FW-RID-81'],8).value=='SYS-RID-FW-31'
for i,e in enumerate(j['entries'],6):
 assert (d.cell(i,1).value or '')==e['mark']
 for c,v in enumerate(e['values'],2):
  actual=d.cell(i,c)
  if isinstance(actual,openpyxl.cell.cell.MergedCell):continue
  assert actual.value==v,('diff',i,c,actual.value,v)
for kind in ['body','diff']:
 doc=pdf.PdfDocument(str(Q/(kind+'.pdf')))
 for i in range(len(doc)):
  doc[i].render(scale=1.25).to_pil().save(Q/(kind+str(i+1)+'.png'))
(Q/'verification.json').write_text(json.dumps({'cover_unchanged':True,'all_body_cells_verified':True,'IDs_1_to_80_preserved':True,'new_ID':81,'split':{'53':'SYS-RID-FW-30','81':'SYS-RID-FW-31'},'diff_verified':True},ensure_ascii=False,indent=2),encoding='utf8')
print('Verified 81 unique IDs, exact requested body, prefix assignments and comparison rows')
