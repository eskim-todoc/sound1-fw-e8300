from pathlib import Path
import json,collections,pdfplumber,pypdfium2 as pdfium
from PIL import Image,ImageDraw
H=Path(__file__).resolve().parent;R=H.parent/'native-excel'/'tests'
out=[];D=H/'system-internal-boundary-final';D.mkdir(exist_ok=True)
for key in ['SSTC','SSTR']:
 doc=pdfplumber.open(R/(key+'-main.pdf'));pd=pdfium.PdfDocument(R/(key+'-main.pdf'));ims=[]
 for idx,p in enumerate(doc.pages):
  borders=[r for r in p.rects if r['height']<.7 and r['width']>20 and r['non_stroking_color'] in (0,0.0,(0,0,0),(0.,0.,0.)) and r['top']>70]
  near=[]
  for b in borders:
   y=b['top']; cs=[c for c in p.chars if b['x0']+.2<c['x0']<b['x1']-.2 and c['x0']>350 and c['top']<y-.2 and c['bottom']>y+.2 and c['text'].strip()]
   if cs:
    cs.sort(key=lambda c:c['x0']);near.append({'border':[b['x0'],y,b['x1']],'text':''.join(c['text'] for c in cs),'bbox':[min(c['x0'] for c in cs),min(c['top'] for c in cs),max(c['x1'] for c in cs),max(c['bottom'] for c in cs)]})
  if near:
   pg=pd[idx];im=pg.render(scale=4).to_pil().convert('RGB');pg.close()
   for n,b in enumerate(near):
    x,y,x1=b['border'];crop=im.crop((int(max(348,x-3)*4),int((y-35)*4),int(min(p.width,x1+3)*4),int((y+6)*4)))
    canvas=Image.new('RGB',(1850,190),'white');canvas.paste(crop,(0,25));ImageDraw.Draw(canvas).text((3,3),f'{key} page {idx+1} border x{x:.1f}..{x1:.1f} y{y:.1f}',fill='black')
    path=D/f'{key}-{idx+1:03d}-{n}.png';canvas.save(path);b['crop']=str(path);ims.append(canvas)
   out.append({'key':key,'page':idx+1,'candidates':near})
  p.close()
 for start in range(0,len(ims),6):
  canvas=Image.new('RGB',(1850,190*len(ims[start:start+6])),'#ddd')
  for j,im in enumerate(ims[start:start+6]):canvas.paste(im,(0,j*190))
  canvas.save(D/f'{key}-composite-{start+1:02d}.png')
 doc.close();pd.close()
(H/'system-internal-boundary-final.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps([{'key':x['key'],'page':x['page'],'columns':[r['border'][:1] for r in x['candidates']]} for x in out],ensure_ascii=True))
