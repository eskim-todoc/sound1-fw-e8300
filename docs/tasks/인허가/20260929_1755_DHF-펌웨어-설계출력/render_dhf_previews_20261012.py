"""Render actual Office exports without changing document content."""
from pathlib import Path
import json
import pypdfium2 as pdfium
from PIL import Image,ImageDraw,ImageFont
QA=Path(__file__).resolve().parent/'qa/dhf-materials-system-20261012'
font=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',18)
thumbs=[];manifest=[]
for pdf in sorted(QA.glob('*.pdf')):
    if '-before-' in pdf.name:continue
    doc=pdfium.PdfDocument(str(pdf))
    for i in range(len(doc)):
        page=doc[i];bmp=page.render(scale=1.6);im=bmp.to_pil().convert('RGB')
        box=im.convert('L').point(lambda v:255 if v<230 else 0).getbbox()
        if box:im=im.crop((max(0,box[0]-10),max(0,box[1]-10),min(im.width,box[2]+10),min(im.height,box[3]+10)))
        out=pdf.with_name(pdf.stem+f'-p{i+1}.png');im.save(out)
        manifest.append({'pdf':str(pdf),'page':i+1,'png':str(out)})
        im.thumbnail((930,670));tile=Image.new('RGB',(960,710),'white');ImageDraw.Draw(tile).text((10,5),pdf.stem+f' / {i+1}',font=font,fill='black');tile.paste(im,(15,35));thumbs.append(tile)
        bmp.close();page.close()
    doc.close()
for start in range(0,len(thumbs),6):
    im=Image.new('RGB',(1920,2130),'#EEEEEE')
    for k,tile in enumerate(thumbs[start:start+6]):im.paste(tile,((k%2)*960,(k//2)*710))
    im.save(QA/f'contact-{start//6+1:02}.png')
(QA/'renders.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf8')
print(len(manifest),'rendered pages;',len(thumbs)//6+1,'contact sheets')
