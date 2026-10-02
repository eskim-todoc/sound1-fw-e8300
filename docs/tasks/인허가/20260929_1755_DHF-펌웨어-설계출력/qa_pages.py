from pathlib import Path
import pypdfium2 as pdfium
from PIL import Image,ImageDraw
from build_architecture import T,INV,N,E,zipfile

for sub in ['source','final','']:
 for pdf in (T/'qa'/sub).glob('*.pdf'):
    if 'markup' in pdf.name:continue
    doc=pdfium.PdfDocument(pdf);out=pdf.parent/pdf.stem;out.mkdir(exist_ok=True)
    thumbs=[]
    for i,p in enumerate(doc):
        image=p.render(scale=1.5).to_pil();image.save(out/f'page-{i+1}.png')
        image.thumbnail((260,380));canvas=Image.new('RGB',(280,410),'#eeeeee');canvas.paste(image,((280-image.width)//2,20));ImageDraw.Draw(canvas).text((10,390),str(i+1),fill='black');thumbs.append(canvas)
    for start in range(0,len(thumbs),16):
        sheet=Image.new('RGB',(1120,410*((min(16,len(thumbs)-start)+3)//4)),'white')
        for i,thumb in enumerate(thumbs[start:start+16]):sheet.paste(thumb,((i%4)*280,(i//4)*410))
        sheet.save(out/f'contact-{start//16+1}.png')
    print(pdf.name,len(doc))
for item in INV:
 if not item['name'].endswith('.docx'):continue
 with zipfile.ZipFile(item['source']) as z:
    root=E.fromstring(z.read('word/document.xml'))
    print(item['name'], 'drawings',len(root.xpath('.//a:blip',namespaces=N)), 'vml',len(root.xpath('//*[local-name()="imagedata"]')))
