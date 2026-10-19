from pathlib import Path
from lxml import etree as E
from PIL import ImageFont
import json,zipfile,math,re
from integrate import N,ref,sheetmap,vals,sha
H=Path(__file__).resolve().parent
P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'))
out=[];fontcache={}
for k in ['SSTC','SSTR']:
 b=P['books'][k]
 with zipfile.ZipFile(b['path']) as z:data={i.filename:z.read(i.filename) for i in z.infolist()}
 sn=next(s for s in sheetmap(data) if s.startswith(('기준서_','보고서_')));member=sheetmap(data)[sn]
 root=E.fromstring(data[member]);v=vals(data,member)
 styles=E.fromstring(data['xl/styles.xml']);fonts=styles.find('x:fonts',N);xfs=styles.find('x:cellXfs',N)
 cells={c.get('r'):c for c in root.findall('x:sheetData/x:row/x:c',N)}
 default=float(root.find('x:sheetFormatPr',N).get('defaultRowHeight','15'))
 rows={int(r.get('r')):float(r.get('ht',default)) for r in root.findall('x:sheetData/x:row',N)}
 merges={m.get('ref').split(':')[0]:m.get('ref') for m in root.findall('x:mergeCells/x:mergeCell',N)};widths={}
 for c in root.findall('x:cols/x:col',N):
  for n in range(int(c.get('min')),int(c.get('max'))+1):widths[n]=float(c.get('width','13'))
 for a,t in v.items():
  col,row=ref(a)
  if not (7<=col<=(10 if k=='SSTC' else 12) and 34<=row<=285) or not t.strip():continue
  c=cells[a];f=fonts[int(xfs[int(c.get('s','0'))].get('fontId','0'))]
  size=float(f.find('x:sz',N).get('val','11'))
  run_sizes=[float(e.get('val')) for e in c.findall('x:is/x:r/x:rPr/x:sz',N)]
  maxsize=max([size]+run_sizes)
  ff=fontcache.setdefault(maxsize,ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',round(maxsize*96/72)))
  c1,r1=ref(a);c2,r2=ref(merges.get(a,a).split(':')[-1])
  width=sum(widths.get(n,13) for n in range(c1,c2+1));px=max(30,width*7-8)
  height=12;wraps=0
  for line in t.splitlines():
   used=0;count=1
   for ch in line:
    cw=ff.getlength(ch)
    if used+cw>px:count+=1;used=0
    used+=cw
   height+=count*maxsize*1.3;wraps+=count
  available=sum(rows.get(r,default) for r in range(r1,r2+1))
  out.append({'key':k,'cell':a,'merge':merges.get(a,a),'font':size,'max_run_font':maxsize,'width':width,
   'lines':wraps,'need_estimate':round(height,1),'available':available,
   'row_heights':{str(r):rows.get(r,default) for r in range(r1,r2+1)},
   'has_current_comparison':'[+]' in t,'shortfall':height>available,
   'last_line':t.rstrip().splitlines()[-1],'text_length':len(t)})
(H/'system-height-all-final.json').write_text(json.dumps({'scope':'All system main body common G:J and result K:L cells. Font/wrap/height estimate; actual native pixels remain decisive.','cells':out,'source_hashes':{k:sha(Path(P['books'][k]['path'])) for k in ['SSTC','SSTR']}},ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps([r for r in out if r['shortfall']],ensure_ascii=True))
