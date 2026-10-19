"""Render Office-generated QA PDFs with the bundled PDFium runtime."""
import argparse
import json
import sys
from pathlib import Path
import pypdfium2 as pdfium
from PIL import Image, ImageDraw

sys.stdout.reconfigure(encoding='utf-8')

parser = argparse.ArgumentParser()
parser.add_argument('pdf')
parser.add_argument('--out', required=True)
parser.add_argument('--scale', type=float, default=1.5)
parser.add_argument('--contact', action='store_true')
args = parser.parse_args()
target = Path(args.out)
target.mkdir(parents=True, exist_ok=True)
document = pdfium.PdfDocument(args.pdf)
records, thumbs = [], []
for index in range(len(document)):
    page = document[index]
    bitmap = page.render(scale=args.scale)
    img = bitmap.to_pil().convert('RGB')
    path = target / f'page-{index+1:03d}.png'
    img.save(path)
    textpage = page.get_textpage()
    records.append({'page': index+1, 'png': str(path), 'size': list(img.size),
                    'text': textpage.get_text_range()})
    if args.contact:
        img.thumbnail((390, 552))
        thumbs.append(img.copy())
    textpage.close()
    bitmap.close()
    page.close()
if args.contact:
    for start in range(0, len(thumbs), 12):
        subset = thumbs[start:start+12]
        canvas = Image.new('RGB', (4*414, 3*584), '#d9d9d9')
        draw = ImageDraw.Draw(canvas)
        for pos, thumb in enumerate(subset):
            x, y = (pos % 4)*414+12, (pos//4)*584+24
            canvas.paste(thumb, (x,y))
            draw.text((x,y-18), f'Page {start+pos+1}', fill='black')
        canvas.save(target / f'contact-{start+1:03d}-{start+len(subset):03d}.png')
(target/'pages.json').write_text(json.dumps(records, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'pages':len(records), 'out':str(target)},ensure_ascii=False))
document.close()
