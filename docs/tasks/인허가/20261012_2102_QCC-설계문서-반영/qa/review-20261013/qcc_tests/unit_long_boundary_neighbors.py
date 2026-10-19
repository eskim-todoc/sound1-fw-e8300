from pathlib import Path
import pypdfium2 as pdfium,json
from PIL import Image,ImageDraw
H=Path(__file__).resolve().parent;R=H.parent/'native-excel'/'tests';D=H/'unit-long-boundary-neighbors';D.mkdir(exist_ok=True)
doc=pdfium.PdfDocument(R/'SUVR-main.pdf')
entry=next(x for x in json.loads((H/'pdf-geometry-triage-final.json').read_text(encoding='utf-8')) if x['key']=='SUVR')
for number in [13,84,133,150]:
 ims=[]
 for pno in [number,number+1]:
  p=doc[pno-1];im=p.render(scale=5).to_pil().convert('RGB')
  # Full native last expected/observed columns at bottom and next page top.
  bottom=next(x['grid_bottom'] for x in entry['boundary_candidates'] if x['page']==number)
  y0=bottom-75 if pno==number else 55;y1=bottom+8 if pno==number else 138
  crop=im.crop((int(555*5),int(y0*5),int(810*5),int(y1*5)))
  can=Image.new('RGB',(1300,530),'white');can.paste(crop,(0,30));ImageDraw.Draw(can).text((3,3),f'SUVR native p{pno}',fill='black');ims.append(can);p.close()
 can=Image.new('RGB',(1300,1060),'white');can.paste(ims[0],(0,0));can.paste(ims[1],(0,530));can.save(D/f'SUVR-{number}-{number+1}.png')
doc.close()
