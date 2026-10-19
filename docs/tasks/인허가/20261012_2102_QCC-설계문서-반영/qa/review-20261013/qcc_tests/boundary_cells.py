from pathlib import Path
import json,re,pdfplumber,collections,sys
H=Path(__file__).resolve().parent;suffix='-final' if '--final' in sys.argv else '';G=json.loads((H/('pdf-geometry-triage'+suffix+'.json')).read_text(encoding='utf-8'));V=json.loads((H/'current-values.json').read_text(encoding='utf-8'));P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'))
def norm(s):return re.sub(r'[^\w]','',s)
def lines(chars):
 rows=[]
 for c in sorted(chars,key=lambda c:(c['top'],c['x0'])):
  row=next((x for x in rows if abs(x[0]-c['top'])<.4),None)
  if row is None:row=[c['top'],[]];rows.append(row)
  row[1].append(c)
 return [(y,''.join(c['text'] for c in sorted(cs,key=lambda c:c['x0']))) for y,cs in sorted(rows)]
out=[]
for g in G:
 key=g['key'];sn=next(s for s in V[key] if s.startswith(('기준서_','보고서_')));values=V[key][sn].copy()
 values.update({a:c['display'] for a,c in P['books'][key]['comparison'].get(sn,{}).items()})
 doc=pdfplumber.open(g['pdf']);isunit=key.startswith('SU');idcol='D' if isunit else ('C' if key.startswith('SI') else 'B');pattern=r'SW-FW-UnitTest-[A-Za-z0-9-]+' if isunit else (r'SW-FW-IntegrationTest-ID-\d+' if key.startswith('SI') else r'SW-FW-SYS-Test-ID-\d+')
 group_rows={t.strip():int(a[len(idcol):]) for a,t in values.items() if re.match(idcol+r'\d+$',a) and re.search(pattern,t)}
 for cand in g['boundary_candidates']:
  page=doc.pages[cand['page']-1];black=lambda x:x in (0,0.0,(0,0,0),(0.0,0.0,0.0));xs=sorted(set(round(r['x0'],1) for r in page.rects if r['width']<1 and r['height']>20 and black(r.get('non_stroking_color'))))
  if len(xs)<8:continue
  def col(x):return chr(66+next((i for i in range(len(xs)-1) if xs[i]<=x<xs[i+1]),len(xs)-2))
  grouped=collections.defaultdict(list)
  for c in page.chars:
   if 0<c['top']<page.height+150:grouped[col((c['x0']+c['x1'])/2)].append(c)
  ids=[]
  for y,t in lines(grouped[idcol]):
   for m in re.findall(pattern,t):ids.append((y,m))
  last_id=max((a for a in ids if a[0]<cand['grid_bottom']+100),default=(0,None))
  tag_ids=[]
  if isunit:
   for y,t in lines(grouped['H']):
    for m in re.findall(r'SU-ID-(CFX|CM3|HEAR|BOOT)-(\d+)',t):
     guessed='SW-FW-UnitTest-'+m[0]+'-ID-'+str(int(m[1]))
     if y<cand['grid_bottom']+100 and guessed in group_rows:tag_ids.append((y,guessed))
   if tag_ids and max(tag_ids)[0]>last_id[0]:last_id=max(tag_ids)
  matches=[]
  for column,cs in grouped.items():
   for y,t in lines(cs):
    if abs(y-cand['grid_bottom'])>6:continue
    n=norm(t)
    if len(n)<8:continue
    candidates=[a for a,s in values.items() if re.match(column+r'\d+$',a) and n in norm(s)]
    if len(candidates)==1:matches.append({'column':column,'line':t,'cell':candidates[0]})
    elif candidates:matches.append({'column':column,'line':t,'ambiguous':candidates})
  out.append({'key':key,'page':cand['page'],'id':last_id[1],'group_row':group_rows.get(last_id[1]),'matches':matches})
  page.close()
 doc.close()
(H/('boundary-cell-map'+suffix+'.json')).write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(out,ensure_ascii=True))
