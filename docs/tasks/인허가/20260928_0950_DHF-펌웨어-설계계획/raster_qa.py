"""Rasterize Word-exported PDFs without changing the DOCX packages."""
import sys
from pathlib import Path
import pypdfium2 as pdfium
from PIL import Image, ImageDraw

folder = Path(sys.argv[1])
for pdf in sorted(folder.glob('*.pdf')):
    out = folder / pdf.stem.split('-')[0]
    out.mkdir(exist_ok=True)
    with pdfium.PdfDocument(pdf) as doc:
        thumbs=[]
        for i,page in enumerate(doc):
            im=page.render(scale=1.5).to_pil().convert('RGB')
            im.save(out/f'page-{i+1}.png')
            im.thumbnail((600,849))
            thumbs.append(im)
        for start in range(0,len(thumbs),4):
            sheet=Image.new('RGB',(1220,1760),'#dddddd')
            draw=ImageDraw.Draw(sheet)
            for j,im in enumerate(thumbs[start:start+4]):
                x=(j%2)*610;y=(j//2)*880
                draw.text((x+10,y+3),f'{pdf.stem.split("-")[0]} page {start+j+1}',fill='black')
                sheet.paste(im,(x+5,y+25))
            sheet.save(out/f'contact-{start//4+1}.png')
        print(f'{pdf.stem.split("-")[0]}: {len(doc)} pages',flush=True)
