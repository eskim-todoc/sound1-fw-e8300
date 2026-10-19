from pathlib import Path
import json,sys
import pypdfium2 as pdfium
H=Path(__file__).resolve().parent
m=json.loads((H/'source-manifest.json').read_text(encoding='utf-8'))
out=[]
for f in m['files']:
    if f['code'] not in ['SRS','SSTC','SSTR']:continue
    p=f['native_previews'][0]['pdf']; doc=pdfium.PdfDocument(p)
    for i in range(len(doc)):
        page=doc[i];t=page.get_textpage().get_text_range()
        if 'RID-83' in t or 'Test-ID-83' in t:
            dest=H/f"before-{f['code']}-{i+1}.png";page.render(scale=1.2).to_pil().save(dest)
            out.append({'code':f['code'],'pdf':p,'page':i+1,'png':str(dest),'text':t})
(H/'before-render.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps([{k:v for k,v in x.items() if k!='text'} for x in out]))
