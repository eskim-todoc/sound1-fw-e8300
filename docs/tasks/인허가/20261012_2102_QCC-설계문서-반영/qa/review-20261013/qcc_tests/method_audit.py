from pathlib import Path
import json,re,openpyxl
H=Path(__file__).resolve().parent;P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'));C=json.loads((H/'current-values.json').read_text(encoding='utf-8'))
allout=[]
for key,mc,ec in [('SUVC','F','K'),('SITC','I','M'),('SSTC','D','J')]:
 w=openpyxl.load_workbook(P['books'][key]['path']);s=w[P['books'][key]['sheet']];cur=C[key][s.title];out=[]
 for a,v in cur.items():
  if not re.fullmatch(mc+r'\d+',a) or v.strip()!='D':continue
  row=int(a[1:]);merge=next((m for m in s.merged_cells.ranges if m.start_cell.coordinate==a),None);end=merge.max_row if merge else row
  expected='\n'.join(cur.get(ec+str(r),'') for r in range(row,end+1))
  if any(x in expected for x in ['RTT','전체 예상','ACK=','응답=[','응답 5바이트','예상 Cnew','최소 증가 시간']):out.append({'cell':a,'end':end,'expected':expected})
 allout.append({'key':key,'candidates':out});print(json.dumps({'key':key,'candidates':[{'cell':x['cell'],'end':x['end'],'RTT':len(re.findall('RTT',x['expected']))} for x in out]},ensure_ascii=True));w.close()

(H/'method-audit.json').write_text(json.dumps(allout,ensure_ascii=False,indent=2),encoding='utf-8')

