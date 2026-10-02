"""Render the native Word/Excel PDFs; keep QA images outside deliverables."""
from pathlib import Path
import sys,json
import pypdfium2 as pdfium
from pypdf import PdfReader
from PIL import Image,ImageDraw

T=Path(__file__).resolve().parent;QA=T/'qa/unit-reconciliation'
names=sys.argv[1:] or ['SDDD-candidate','SUVC-body','SUVC-sheet-1','SUVC-sheet-3','SUVC-sheet-4','SUVC-sheet-5','SUVC-sheet-6','SITC-sheet-2','SITC-sheet-3']
for name in names:
    path=QA/(name+'.pdf');doc=pdfium.PdfDocument(path);out=QA/(name+'-pages');out.mkdir(exist_ok=True)
    pages=[]
    for i,page in enumerate(doc):
        page.render(scale=1.5).to_pil().save(out/f'page-{i+1}.png')
        tp=page.get_textpage();text=tp.get_text_range();pages.append({'page':i+1,'text':text});tp.close();page.close()
    (out/'pages.json').write_text(json.dumps(pages,ensure_ascii=False,indent=2),encoding='utf8')
    for start in range(0,len(doc),12):
        count=min(12,len(doc)-start)
        board=Image.new('RGB',(1000,360*((count+3)//4)),'#dddddd')
        draw=ImageDraw.Draw(board)
        for offset in range(count):
            tile=Image.open(out/f'page-{start+offset+1}.png').convert('RGB')
            tile.thumbnail((242,335))
            x=(offset%4)*250;y=(offset//4)*360
            board.paste(tile,(x+(250-tile.width)//2,y))
            draw.text((x+8,y+338),str(start+offset+1),fill='black')
        board.save(out/f'contact-{start//12+1}.png')
    print(name,len(doc));doc.close()
