"""Render the actual Excel PDF previews for bounded visual review."""
from pathlib import Path
import json
import pypdfium2 as pdfium
from PIL import Image, ImageDraw, ImageFont

QA = Path(__file__).resolve().parent / 'qa/system-upper-method-alignment-20261012'
font = ImageFont.truetype('C:/Windows/Fonts/malgun.ttf', 19)
manifest = []
thumbs = []
for pdf in sorted(QA.glob('*.pdf')):
    doc = pdfium.PdfDocument(str(pdf))
    for index in range(len(doc)):
        page = doc[index]
        bitmap = page.render(scale=1.7)
        source = bitmap.to_pil().convert('RGB')
        # Crop only blank paper margins. No workbook content is drawn or changed.
        pixels = source.convert('L')
        mask = pixels.point(lambda v: 255 if v < 230 else 0)
        box = mask.getbbox()
        if box:
            box = (max(0, box[0]-12), max(0, box[1]-12),
                   min(source.width, box[2]+12), min(source.height, box[3]+12))
            source = source.crop(box)
        output = pdf.with_name(pdf.stem + f'-p{index+1}.png')
        source.save(output)
        manifest.append({'pdf': str(pdf), 'page': index+1, 'png': str(output),
                         'width': source.width, 'height': source.height})
        thumb = source.copy()
        thumb.thumbnail((940, 680))
        tile = Image.new('RGB', (980, 730), 'white')
        ImageDraw.Draw(tile).text((14, 8), pdf.stem+f' / page {index+1}', font=font, fill='black')
        tile.paste(thumb, (20, 42))
        thumbs.append(tile)
        bitmap.close()
        page.close()
    doc.close()
for start in range(0, len(thumbs), 6):
    sheet = Image.new('RGB', (1960, 2190), '#eeeeee')
    for offset, tile in enumerate(thumbs[start:start+6]):
        sheet.paste(tile, ((offset%2)*980, (offset//2)*730))
    sheet.save(QA / f'contact-{start//6+1:02}.png')
(QA / 'renders.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf8')
print('Rendered pages:', len(manifest), '; contact sheets:', (len(thumbs)+5)//6)
