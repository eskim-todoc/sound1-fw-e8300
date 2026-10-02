"""Render every QA PDF page using the bundled PDFium runtime."""
from pathlib import Path
from PIL import Image, ImageDraw
import pypdfium2 as pdfium
from inspect_sip_alignment import TASK
import argparse

parser=argparse.ArgumentParser()
parser.add_argument('--qa',default='qa/sip-sddd-alignment')
args=parser.parse_args()
QA=TASK/args.qa
for kind in ('before','candidate'):
    pdf=pdfium.PdfDocument(QA/(kind+'.pdf'))
    folder=QA/(kind+'-pages');folder.mkdir(exist_ok=True)
    pages=[]
    for i,p in enumerate(pdf):
        im=p.render(scale=1.3333333).to_pil().convert('RGB')
        im.save(folder/f'page-{i+1}.png')
        canvas=Image.new('RGB',(im.width,im.height+30),'#eeeeee')
        canvas.paste(im,(0,30));ImageDraw.Draw(canvas).text((12,8),f'{kind} / {i+1}',fill='black')
        pages.append(canvas)
    for start in range(0,len(pages),2):
        pair=pages[start:start+2]
        board=Image.new('RGB',(sum(p.width for p in pair),max(p.height for p in pair)),'white')
        x=0
        for im in pair:board.paste(im,(x,0));x+=im.width
        board.save(folder/f'pair-{start//2+1}.png')
    print(kind,len(pages),'pages')
