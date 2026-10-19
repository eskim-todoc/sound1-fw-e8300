from pathlib import Path
import json
import pypdfium2 as pdfium
from integrate import sha
H=Path(__file__).resolve().parent
D=H/'final-system-guard-crops';D.mkdir(exist_ok=True)
ids=json.loads((H/'native-id-positions-final.json').read_text(encoding='utf-8'))
out=[]
for key in ['SSTC','SSTR']:
    item=next(x for x in ids if x['code']==key+'-main')
    doc=pdfium.PdfDocument(item['pdf'])
    for n in [52,75]:
        e=next(x for x in item['id_positions'] if x['id']==f'SW-FW-SYS-Test-ID-{n}')
        for pn in e['literal_text_pages']:
            pg=doc[pn-1];im=pg.render(scale=3).to_pil().convert('RGB');pg.close()
            path=D/f'{key}-SYS{n}-p{pn}.png'
            im.crop((int(285*3),int(48*3),int(815*3),int(545*3))).save(path)
            out.append({'key':key,'id':e['id'],'row':e['start_row'],'page':pn,'sourceSHA256':item['sourceSHA256'],'pdf_sha256':sha(Path(item['pdf'])),'crop':str(path),'sha256':sha(path)})
    doc.close()
(H/'final-system-guard-crops.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps([{'key':x['key'],'id':x['id'],'page':x['page']} for x in out]))
