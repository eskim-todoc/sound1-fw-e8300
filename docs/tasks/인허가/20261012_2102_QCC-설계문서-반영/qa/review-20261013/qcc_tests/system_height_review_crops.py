from pathlib import Path
import json,pypdfium2 as pdfium
from integrate import sha
H=Path(__file__).resolve().parent;D=H/'system-height-native-final-crops';D.mkdir(exist_ok=True)
ids=json.loads((H/'native-id-positions-final.json').read_text(encoding='utf-8'));groups=[27,28,29,33,34,36,38,46,49,51,55,61,67,68,74,82,83]
out=[]
for key in ['SSTC','SSTR']:
 item=next(x for x in ids if x['code']==key+'-main');doc=pdfium.PdfDocument(item['pdf'])
 for n in groups:
  e=next(x for x in item['id_positions'] if x['id']==f'SW-FW-SYS-Test-ID-{n}');pno=e['literal_text_pages'][0]
  pg=doc[pno-1];im=pg.render(scale=3).to_pil().convert('RGB');pg.close()
  # Read-only native crops of actual sheet/body pixels, never test captures.
  left=348 if key=='SSTC' else 286
  end=438 if n in [74,82,83] else 360
  path=D/f'{key}-SYS{n}-p{pno}-current.png'
  im.crop((int(left*3),int(48*3),int(815*3),int(end*3))).save(path)
  out.append({'key':key,'id':e['id'],'start_row':e['start_row'],'page':pno,'sourceSHA256':item['sourceSHA256'],'pdf_sha256':sha(Path(item['pdf'])),'crop':str(path),'crop_sha256':sha(path)})
 doc.close()
(H/'system-height-native-final-crops.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps([{'key':x['key'],'id':x['id'],'page':x['page']} for x in out],ensure_ascii=True))
