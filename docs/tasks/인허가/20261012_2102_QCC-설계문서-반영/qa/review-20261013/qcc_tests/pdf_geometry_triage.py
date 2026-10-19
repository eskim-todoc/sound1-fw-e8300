from pathlib import Path
import json,pdfplumber,collections,sys
H=Path(__file__).resolve().parent;R=H.parent/'native-excel'/'tests';out=[]
for key in ['SUVC','SUVR','SITC','SITR','SSTC','SSTR']:
 p=R/(key+'-main.pdf');doc=pdfplumber.open(p);pages=[]
 for idx,page in enumerate(doc.pages):
  ys=[]
  for line in page.lines:
   if abs(line['x0']-line['x1'])<.2 and line['height']>20:ys.extend([line['top'],line['bottom']])
  # Native Excel writes borders as filled narrow black rectangles, not strokes.
  for rect in page.rects:
   color=rect.get('non_stroking_color')
   black=color in (0,0.0,(0,0,0),(0.0,0.0,0.0))
   if black and rect['width']<1 and rect['height']>20:ys.extend([rect['top'],rect['bottom']])
  if not ys:continue
  bottom=max(ys);top=min(ys);near=[]
  spans=collections.defaultdict(list)
  for c in page.chars:
   if c['top']<bottom+.5 and c['bottom']>bottom-.5 and c['text'].strip():spans[(round(c['top'],1),str(c['non_stroking_color']))].append(c)
  for cs in spans.values():
   cs.sort(key=lambda c:c['x0']);near.append({'text':''.join(c['text'] for c in cs),'bbox':[min(c['x0'] for c in cs),min(c['top'] for c in cs),max(c['x1'] for c in cs),max(c['bottom'] for c in cs)],'size':cs[0]['size'],'below':round(max(c['bottom'] for c in cs)-bottom,2)})
  if near:pages.append({'page':idx+1,'grid_bottom':bottom,'near_bottom':near})
  page.close()
 out.append({'key':key,'pdf':str(p),'pages_total':len(doc.pages),'boundary_candidates':pages});doc.close()
(H/('pdf-geometry-triage-final.json' if '--final' in sys.argv else 'pdf-geometry-triage.json')).write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps([{'key':x['key'],'total':x['pages_total'],'candidate_pages':[p['page'] for p in x['boundary_candidates']],'examples':x['boundary_candidates'][:2]} for x in out],ensure_ascii=True))
