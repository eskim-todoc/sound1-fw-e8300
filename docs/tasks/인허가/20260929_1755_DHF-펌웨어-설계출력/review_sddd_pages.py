from pathlib import Path
import pypdfium2 as pdfium
from PIL import Image, ImageDraw

root = Path(__file__).parent / 'qa' / 'sddd-architecture'
pdf = pdfium.PdfDocument(root / 'SDDD-final-view.pdf')
thumbs = []
for i in range(len(pdf)):
    page = pdf[i]
    im = page.render(scale=1).to_pil()
    im.save(root / f'page-{i+1}.png')
    im.thumbnail((245, 350))
    tile = Image.new('RGB', (260, 375), '#eeeeee')
    tile.paste(im, ((260-im.width)//2, 0))
    ImageDraw.Draw(tile).text((8, 355), str(i+1), fill='black')
    thumbs.append(tile)
for start in range(0, len(thumbs), 24):
    canvas = Image.new('RGB', (1560, 1500), 'white')
    for j, im in enumerate(thumbs[start:start+24]):
        canvas.paste(im, (j%6*260, j//6*375))
    canvas.save(root / f'contact-{start//24+1}.png')
print(len(pdf))
