from pathlib import Path
import json,re,pypdfium2 as pdfium
from lxml import etree as E
from integrate import N,sheetmap,vals
import zipfile
H=Path(__file__).resolve().parent;P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'))
R=H.parent/'native-excel'/'tests';out=[]
norm=lambda x:re.sub(r'\s+','',x)
for key in ['SSTC','SSTR']:
 with zipfile.ZipFile(P['books'][key]['path']) as z:data={i.filename:z.read(i.filename) for i in z.infolist()}
 member=sheetmap(data)[next(s for s in sheetmap(data) if s.startswith(('기준서_','보고서_')))]
 v=vals(data,member)
 doc=pdfium.PdfDocument(R/(key+'-main.pdf'));pages=[]
 for p in doc:
  tp=p.get_textpage();pages.append(norm(tp.get_text_range()));tp.close();p.close()
 doc.close()
 for a,t in v.items():
  if not re.match(r'[GIJKLM]\d+$',a) or not 34<=int(re.findall(r'\d+',a)[0])<=285 or not t.strip():continue
  tail=norm(t.rstrip().splitlines()[-1])[-35:]
  if len(tail)<12:continue
  if not any(tail in p for p in pages):out.append({'key':key,'cell':a,'tail':tail,'estimate':next((x for x in json.loads((H/'system-height-all-final.json').read_text(encoding='utf-8'))['cells'] if x['key']==key and x['cell']==a),None)})
(H/'system-native-tails-missing.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps([{'key':x['key'],'cell':x['cell'],'estimate':x['estimate']['need_estimate'] if x['estimate'] else None,'available':x['estimate']['available'] if x['estimate'] else None} for x in out],ensure_ascii=True))
