from pathlib import Path
from lxml import etree as E
from copy import deepcopy
import json,zipfile,math,os
from integrate import sheetmap,vals,N,sha,ref,required_height,rich
H=Path(__file__).resolve().parent;F=json.loads((H/'followup-plan.json').read_text(encoding='utf-8'))
def package(p):
 with zipfile.ZipFile(p) as z:return z.infolist(),{i.filename:z.read(i.filename) for i in z.infolist()}
packages={k:package(p['path']) for k,p in F.items()};records=[];splits=[];save={}
for k,p in F.items():
 infos,data=packages[k];beforedata=data.copy();before=sha(p['path']);assert before==p['hash'],(k,'concurrent change')
 sn=p['sheet'];sm=sheetmap(data);member=sm[sn];root=E.fromstring(data[member]);existing=vals(data,member)
 rows={int(x.get('r')):x for x in root.find('x:sheetData',N)};cells={c.get('r'):c for c in root.findall('.//x:sheetData/x:row/x:c',N)}
 styles=E.fromstring(data['xl/styles.xml']);fonts=styles.find('x:fonts',N);xfs=styles.find('x:cellXfs',N)
 sourcekey={'SUVR':'SUVC','SSTR':'SSTC'}.get(k,k);source=packages[sourcekey][1];sourceRoot=E.fromstring(source[sheetmap(source)[F[sourcekey]['sheet']]])
 sourceCells={c.get('r'):c for c in sourceRoot.findall('.//x:sheetData/x:row/x:c',N)};sourceStyles=E.fromstring(source['xl/styles.xml']);sourceFonts=sourceStyles.find('x:fonts',N);sourceXfs=sourceStyles.find('x:cellXfs',N)
 with zipfile.ZipFile(H/(k+'-followup-authored.xlsx')) as z:stage={i.filename:z.read(i.filename) for i in z.infolist()}
 sv=vals(stage,sheetmap(stage)[sn]);widths={}
 for col in root.findall('x:cols/x:col',N):
  for i in range(int(col.get('min')),int(col.get('max'))+1):widths[i]=float(col.get('width','13'))
 merges=[m.get('ref') for m in root.findall('x:mergeCells/x:mergeCell',N)];heights={}
 for a,comp in p['changes'].items():
  assert existing[a]==comp['before'] and sv[a]==comp['display'],(k,a,'stage/input mismatch')
  c=cells[a];common=not (k=='SSTR' and a.startswith('L'))
  font=sourceFonts[int(sourceXfs[int(sourceCells[a].get('s','0'))].get('fontId','0'))] if common else fonts[int(xfs[int(c.get('s','0'))].get('fontId','0'))]
  rich(c,comp,font)
  cn,rn=ref(a);m=next((m for m in merges if m.split(':')[0]==a),None);ec,er=ref(m.split(':')[-1]) if m else (cn,rn)
  need=required_height(comp['display'],sum(widths.get(i,13) for i in range(cn,ec+1)));avail=sum(float(rows[r].get('ht','15')) for r in range(rn,er+1))
  if need>avail:
   if need>409.5*(er-rn+1):splits.append({'key':k,'cell':a,'need':round(need,1),'rows':er-rn+1})
   else:
    per=math.ceil(need/(er-rn+1))
    for r in range(rn,er+1):heights[r]=max(heights.get(r,float(rows[r].get('ht','15'))),per)
 for r,h in heights.items():rows[r].set('ht',str(min(h,409.5)));rows[r].set('customHeight','1')
 data[member]=E.tostring(root,xml_declaration=True,encoding='utf-8',standalone=True);save[k]=(infos,data)
 records.append({'key':k,'path':p['path'],'before':before,'member':member,'changed_cells':list(p['changes']),'height_changes':heights,'criterion_input_observation_scope':'Original inputs, expectation vectors, full RTT and historic observations preserved. Methods clarified; incomplete current verdict remains pending.'})
import sys
write='--write' in sys.argv
assert not splits or not write,splits
if write:
 for k,(infos,data) in save.items():
  p=Path(F[k]['path']);tmp=H/(k+'-followup-ready.xlsx')
  with zipfile.ZipFile(tmp,'w') as z:
   for i in infos:z.writestr(i,data[i.filename])
  with zipfile.ZipFile(tmp) as z:assert z.testzip() is None
  with p.open('wb') as f:f.write(tmp.read_bytes())
  next(r for r in records if r['key']==k)['after']=sha(p)
(H/('followup-integrated.json' if write else 'followup-preflight.json')).write_text(json.dumps({'write':write,'files':records,'split_candidates':splits},ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'write':write,'split_candidates':splits,'height_changes':{r['key']:r['height_changes'] for r in records}},ensure_ascii=True))
