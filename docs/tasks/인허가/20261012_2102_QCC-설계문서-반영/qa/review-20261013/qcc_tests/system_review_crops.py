from pathlib import Path
import json,pypdfium2 as pdfium,pdfplumber
from PIL import Image,ImageDraw
H=Path(__file__).resolve().parent;R=H.parent/'native-excel'/'tests';D=H/'system-additional-crops';D.mkdir(exist_ok=True)
ids=json.loads((H/'native-id-positions-final.json').read_text(encoding='utf-8'))
groups=[1,3,4,21,23,29,33,34,36,38,41,46,49,51,55,60,61,63,67,68,74]
records=[]
for key in ['SSTC','SSTR']:
 item=next(x for x in ids if x['code']==key+'-main');pd=pdfium.PdfDocument(item['pdf']);pl=pdfplumber.open(item['pdf']);ims=[]
 for num in groups:
  entry=next(x for x in item['id_positions'] if x['id']==f'SW-FW-SYS-Test-ID-{num}')
  for pno in entry['literal_text_pages']:
   p=pl.pages[pno-1];ys=[r['bottom'] for r in p.rects if r['width']<1 and r['height']>20 and r['non_stroking_color'] in (0,0.,(0,0,0),(0.,0.,0.))]
   bottom=max(ys) if ys else 500
   pg=pd[pno-1];im=pg.render(scale=4).to_pil().convert('RGB');pg.close()
   # Crops preserve native pixels; this is display QA, not test imagery.
   left=529 if key=='SSTC' else 427;right=809 if key=='SSTC' else 777
   crop=im.crop((int(left*4),int(max(89,bottom-80)*4),int(right*4),int(min(p.height,bottom+8)*4)))
   canvas=Image.new('RGB',(1420,390),'white');canvas.paste(crop,(0,28));ImageDraw.Draw(canvas).text((3,3),f'{key} SYS{num} page {pno} row{entry["start_row"]} bottom{bottom:.1f}',fill='black')
   path=D/f'{key}-SYS{num}-p{pno}.png';canvas.save(path);ims.append(canvas);records.append({'key':key,'id':entry['id'],'page':pno,'row':entry['start_row'],'path':str(path)})
 for start in range(0,len(ims),5):
  canvas=Image.new('RGB',(1420,390*len(ims[start:start+5])),'#ddd')
  for j,im in enumerate(ims[start:start+5]):canvas.paste(im,(0,j*390))
  canvas.save(D/f'{key}-composite-{start+1:02d}.png')
 pd.close();pl.close()
(H/'system-additional-crops.json').write_text(json.dumps(records,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'crops':len(records),'directory':str(D)}))
