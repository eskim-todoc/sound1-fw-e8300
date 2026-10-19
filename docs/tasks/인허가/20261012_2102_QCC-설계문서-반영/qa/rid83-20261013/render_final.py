from pathlib import Path
import json,re
import pypdfium2 as pdfium
from PIL import Image,ImageDraw
H=Path(__file__).resolve().parent
records=json.loads((H/'native-checks.json').read_text(encoding='utf-8-sig'));out=[]
for r in records:
 for p in r['previews']:
  pdf=Path(p['pdf']);doc=pdfium.PdfDocument(pdf);dest=H/'views'/pdf.stem;dest.mkdir(parents=True,exist_ok=True);images=[];alltext=''
  for i in range(len(doc)):
   page=doc[i];text=page.get_textpage().get_text_range();alltext+=text+'\n'
   if p['scope']!='Full main print area' or 'RID-83' in text or 'Test-ID-83' in text:
    png=dest/f'page-{i+1:03}.png';page.render(scale=1.5).to_pil().save(png);images.append(str(png))
  (dest/'text.txt').write_text(alltext,encoding='utf-8')
  if '_비교' not in p['sheet']:
   assert '[-]' not in alltext and '[+]' not in alltext
   assert 'ACDOC' not in alltext
  out.append({'code':r['code'],'sheet':p['sheet'],'pdf':str(pdf),'pages':len(doc),'images':images,'sourceSHA256':r['sha256']})
(H/'render-checks.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps([{k:v for k,v in r.items() if k in ['code','sheet','pages','images']} for r in out]))
