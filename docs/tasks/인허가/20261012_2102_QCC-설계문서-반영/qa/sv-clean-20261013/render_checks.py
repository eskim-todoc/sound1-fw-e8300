from pathlib import Path
import json,re,sys
import pypdfium2 as pdfium
from PIL import Image,ImageDraw
import prepare_clean as p

sys.stdout.reconfigure(encoding='utf-8')
h=Path(__file__).resolve().parent
checks=p.load(h/'native-checks.json')
records=[]
previous=p.load(h/'render-checks.json')['records'] if (h/'render-checks.json').exists() else []
for book in checks:
 for preview in book['previews']:
  path=Path(preview['pdf'])
  cached=next((r for r in previous if r['pdf']==str(path) and r['pdfSHA256']==p.sha(path) and r['sourceSHA256']==book['sha256']),None)
  if cached:records.append(cached);continue
  pdf=pdfium.PdfDocument(path)
  texts=[]
  for i in range(len(pdf)):
   page=pdf[i];tp=page.get_textpage();texts.append(tp.get_text_range());tp.close();page.close()
  compare=preview['sheet'].endswith('_비교')
  if not compare:assert not re.search(r'\[\s*[-+]\s*\]','\n'.join(texts)),(book['code'],preview['sheet'],'marker in native body PDF')
  indices=set(range(len(pdf))) if compare or book['code']=='SRS' else {0,1,len(pdf)//2,max(0,len(pdf)-2),len(pdf)-1}
  if not compare and book['code'] in ('SSTR','SSTC'):
   for i,t in enumerate(texts):
    if re.search(r'(?:SYS-Test-69|SYS-Test-82|SYS-Test-83)',t):indices.add(i)
  out=h/'views'/(path.stem+'-'+book['sha256'][:8]);out.mkdir(parents=True,exist_ok=True)
  images=[]
  for i in sorted(indices):
   page=pdf[i];bitmap=page.render(scale=1.5);im=bitmap.to_pil().convert('RGB');dest=out/f'page-{i+1:03d}.png';im.save(dest)
   im.thumbnail((620,438));images.append((i+1,im.copy(),str(dest)))
   bitmap.close();page.close()
  contacts=[]
  for start in range(0,len(images),6):
   group=images[start:start+6];canvas=Image.new('RGB',(1280,3*470),'#dddddd');draw=ImageDraw.Draw(canvas)
   for j,(number,im,dest) in enumerate(group):
    x=(j%2)*640+10;y=(j//2)*470+25;canvas.paste(im,(x,y));draw.text((x,y-18),f'{path.stem} page {number}',fill='black')
   dest=out/f'contact-{start+1:03d}.png';canvas.save(dest);contacts.append(str(dest))
  rec={'code':book['code'],'sheet':preview['sheet'],'range':preview['range'],'pdf':str(path),'pdfSHA256':p.sha(path),'sourceSHA256':book['sha256'],'pages':len(pdf),'comparison':compare,'mainMarkerFree':None if compare else True,'text':[{'page':i+1,'text':t} for i,t in enumerate(texts)],'renderedPages':[{'page':i,'png':dest} for i,_,dest in images],'contacts':contacts}
  records.append(rec);pdf.close()
p.save(h/'render-checks.json',{'records':records,'mainPages':sum(r['pages'] for r in records if not r['comparison']),'renderedPages':sum(len(r['renderedPages']) for r in records),'allMainMarkerFree':True,'scope':'All native main PDFs text scanned; SRS and representative comparison PDF pages fully rendered; other main sheets sampled at beginning/middle/end and system call cases. Unchanged geometry and retained rich runs are checked against the previously rendered source packages.'})
print(json.dumps({'previews':len(records),'mainPages':sum(r['pages'] for r in records if not r['comparison']),'renderedPages':sum(len(r['renderedPages']) for r in records),'allMainMarkerFree':True}))
