from pathlib import Path
import pypdfium2 as pdfium
H=Path(__file__).resolve().parent;R=H.parent/'native-excel'/'tests';D=H/'integration-native-details-final';D.mkdir(exist_ok=True)
for key in ['SITC','SITR']:
 doc=pdfium.PdfDocument(R/(key+'-main.pdf'))
 for pno in range(19,26):
  pg=doc[pno-1];im=pg.render(scale=3).to_pil().convert('RGB');pg.close()
  # Preserve native pixels of methods/prep/procedure/expected/results.
  im.crop((int(390*3),int(48*3),int(820*3),int(550*3))).save(D/f'{key}-{pno:03d}-right.png')
 doc.close()
