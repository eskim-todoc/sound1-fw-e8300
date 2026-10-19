from pathlib import Path
from PIL import Image,ImageDraw
import pypdfium2 as pdfium
QA=Path(__file__).resolve().parent/'qa/sad-qcc-cleanup-20261016'
files=sorted((QA/'pages').glob('*.png'))
for start in range(0,len(files),9):
 sheet=Image.new('RGB',(1125,1650),'#ddd');d=ImageDraw.Draw(sheet)
 for n,p in enumerate(files[start:start+9]):
  im=Image.open(p);im.thumbnail((365,520))
  x=(n%3)*375;y=(n//3)*550
  sheet.paste(im,(x,y+24));d.text((x+5,y+5),p.stem,fill='black')
 sheet.save(QA/f'contact-{start+1:02}.png')
pdf=pdfium.PdfDocument(QA/'candidate.pdf')
for i,p in enumerate(pdf):
 txt=p.get_textpage().get_text_range()
 if any(s in txt for s in ['QCC 소프트웨어','QCC5181','지원 소프트웨어','[QCC','SU-ID-QCC','DMIC2']):
  print(i+1,txt[:100].replace('\r\n',' '))
assert '소프트웨어 유닛과 상위 요구사항 연결' not in (QA/'candidate-word.txt').read_text(encoding='utf8')
