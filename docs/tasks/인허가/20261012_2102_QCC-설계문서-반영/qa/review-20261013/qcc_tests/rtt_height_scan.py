from pathlib import Path
from lxml import etree as E
from PIL import ImageFont
import json,zipfile,re,math,sys
from integrate import sheetmap,vals,N,ref
H=Path(__file__).resolve().parent;P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'));out=[];allrows=[];fontcache={};suffix='-final' if '--final' in sys.argv else ''
for k in ['SUVC','SUVR']:
 b=P['books'][k]
 with zipfile.ZipFile(b['path']) as z:data={i.filename:z.read(i.filename) for i in z.infolist()}
 sn=next(s for s in sheetmap(data) if s.startswith(('기준서_','보고서_')));member=sheetmap(data)[sn];root=E.fromstring(data[member]);v=vals(data,member)
 styles=E.fromstring(data['xl/styles.xml']);fonts=styles.find('x:fonts',N);xfs=styles.find('x:cellXfs',N);cells={c.get('r'):c for c in root.findall('x:sheetData/x:row/x:c',N)}
 default=float(root.find('x:sheetFormatPr',N).get('defaultRowHeight','15'));rows={int(r.get('r')):float(r.get('ht',default)) for r in root.findall('x:sheetData/x:row',N)}
 merges={m.get('ref').split(':')[0]:m.get('ref') for m in root.findall('x:mergeCells/x:mergeCell',N)};widths={}
 for c in root.findall('x:cols/x:col',N):
  for n in range(int(c.get('min')),int(c.get('max'))+1):widths[n]=float(c.get('width','13'))
 for a,t in v.items():
  if not re.match(r'[KL]\d+$',a) or not re.search(r'\[UT-[A-Z0-9-]+\]',t):continue
  c=cells[a];f=fonts[int(xfs[int(c.get('s','0'))].get('fontId','0'))];size=float(f.find('x:sz',N).get('val','11'))
  ff=fontcache.setdefault(size,ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',round(size*96/72)))
  c1,r1=ref(a);c2,r2=ref(merges.get(a,a).split(':')[-1]);width=sum(widths.get(n,13) for n in range(c1,c2+1));px=max(30,width*7-8);height=8;wraps=0
  for line in t.splitlines():
   used=0;count=1
   for ch in line:
    cw=ff.getlength(ch)
    if used+cw>px:count+=1;used=0
    used+=cw
   height+=count*size*1.24;wraps+=count
  available=sum(rows.get(r,default) for r in range(r1,r2+1))
  record={'key':k,'cell':a,'merge':merges.get(a,a),'font':size,'width':width,'lines':wraps,'need_estimate':round(height,1),'available':available,'row_heights':{str(r):rows.get(r,default) for r in range(r1,r2+1)},'rtt_labels':sorted(set(re.findall(r'\[UT-[A-Z0-9-]+\]',t))),'height_shortfall':height>available*1.15}
  allrows.append(record)
  if record['height_shortfall']:out.append(record)
(H/('rtt-height-scan'+suffix+'.json')).write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(out,ensure_ascii=True))
(H/('rtt-height-all'+suffix+'.json')).write_text(json.dumps({'cell_count':len(allrows),'labels':sorted(set(t for r in allrows for t in r['rtt_labels'])),'scope':'All native worksheet expected/result cells containing UT RTT strings; font/wrap/width/merged heights only. Native pixel visibility must be checked separately.','cells':allrows},ensure_ascii=False,indent=2),encoding='utf-8')
