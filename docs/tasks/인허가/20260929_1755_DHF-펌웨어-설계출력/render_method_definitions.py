from pathlib import Path
import json
import pypdfium2 as pdf
from PIL import Image,ImageDraw
from update_method_definitions import Q
for d in Q.iterdir():
 if not d.is_dir():continue
 for f in d.glob('*.pdf'):
  doc=pdf.PdfDocument(f)
  for i,p in enumerate(doc):
   p.render(scale=1.5).to_pil().save(d/f'{f.stem}-{i+1:02}.png');p.close()
  count=len(doc);doc.close()
  if f.stem in ('main','accepted'):
   for start in range(0,count,4):
    sheet=Image.new('RGB',(2200,1600),'#eee');draw=ImageDraw.Draw(sheet)
    for slot,i in enumerate(range(start,min(start+4,count))):
     im=Image.open(d/f'{f.stem}-{i+1:02}.png');im.thumbnail((1080,750))
     x=slot%2*1100;y=slot//2*800;sheet.paste(im,(x,y+30));draw.text((x+10,y+8),f'{d.name} {i+1}',fill='black')
    sheet.save(d/f'contact-{start//4+1:02}.png')
  print(d.name,f.name,count)
