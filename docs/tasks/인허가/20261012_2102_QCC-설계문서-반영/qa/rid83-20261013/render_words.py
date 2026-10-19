from pathlib import Path
import json
import pypdfium2 as pdfium
from PIL import ImageChops
H=Path(__file__).resolve().parent
native=json.loads((H/'word/native-checks.json').read_text(encoding='utf-8-sig'));sources={f['code']:f for f in json.loads((H/'source-manifest.json').read_text(encoding='utf-8'))['files']};out=[]
for f in native:
 code=f['code'];d=pdfium.PdfDocument(f['pdf']);old=pdfium.PdfDocument(sources[code]['native_previews'][0]['pdf']);dest=H/'word/views'/code;dest.mkdir(parents=True,exist_ok=True);changed=[];same=[]
 for i in range(len(d)):
  pic=d[i].render(scale=1.5).to_pil().convert('RGB');file=dest/f'page-{i+1:03}.png';pic.save(file)
  prior=old[i].render(scale=1.5).to_pil().convert('RGB') if i<len(old) else None
  equal=prior is not None and prior.size==pic.size and ImageChops.difference(pic,prior).getbbox() is None
  (same if equal else changed).append(i+1)
 out.append({'code':code,'sourceSHA256':f['sha256'],'pdf':f['pdf'],'pages':len(d),'baselinePages':len(old),'pixelIdenticalPages':same,'changedPages':changed,'imageDir':str(dest)})
(H/'word/render-checks.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(out))
