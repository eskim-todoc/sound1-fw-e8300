from pathlib import Path
import json,re,hashlib,sys
import pypdfium2 as pdfium
from PIL import Image,ImageDraw
H=Path(__file__).resolve().parent
suffix='-final' if '--final' in sys.argv else ''
G=json.loads((H/('pdf-geometry-triage'+suffix+'.json')).read_text(encoding='utf-8'))
V=json.loads((H/'current-values.json').read_text(encoding='utf-8'))
out=[];dest=H/('boundary-review'+suffix);dest.mkdir(exist_ok=True)
for entry in G:
 key=entry['key'];doc=pdfium.PdfDocument(entry['pdf']);sn=next(s for s in V[key] if s.startswith(('기준서_','보고서_')))
 idcol='D' if key.startswith('SU') else 'C' if key.startswith('SI') else 'B'
 patterns=r'SW-FW-UnitTest-[A-Za-z0-9-]+' if key.startswith('SU') else (r'SW-FW-IntegrationTest-ID-\d+' if key.startswith('SI') else r'SW-FW-SYS-Test-ID-\d+')
 rows={text.strip():int(addr[len(idcol):]) for addr,text in V[key][sn].items() if re.match(idcol+r'\d+$',addr) and re.search(patterns,text)}
 images=[]
 for candidate in entry['boundary_candidates']:
  number=candidate['page'];page=doc[number-1];tp=page.get_textpage();text=tp.get_text_range();ids=re.findall(patterns,text);tp.close()
  group=ids[-1] if ids else None
  rec={'key':key,'page':number,'candidate_group':group,'candidate_start_row':rows.get(group),'near_bottom':candidate['near_bottom']};out.append(rec)
  im=page.render(scale=3).to_pil().convert('RGB');bottom=candidate['grid_bottom'];left=min(x['bbox'][0] for x in candidate['near_bottom'])
  # Read-only native pixel crop. No content/format is synthesized.
  crop=im.crop((max(0,int((left-5)*3)),int((bottom-60)*3),int(page.get_width()*3),int((bottom+15)*3)))
  canvas=Image.new('RGB',(1800,255),'white');canvas.paste(crop,(0,25));ImageDraw.Draw(canvas).text((2,3),f'{key} page {number} / {group} / row {rows.get(group)}',fill='black')
  p=dest/f'{key}-{number:03d}.png';canvas.save(p);rec['crop']=str(p);images.append(canvas);page.close()
 for start in range(0,len(images),6):
  canvas=Image.new('RGB',(1800,255*len(images[start:start+6])),'#dddddd')
  for i,im in enumerate(images[start:start+6]):canvas.paste(im,(0,i*255))
  canvas.save(dest/f'{key}-candidates-{start+1:02d}.png')
 doc.close()
(H/('boundary-candidates'+suffix+'.json')).write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps([{'key':x['key'],'page':x['page'],'id':x['candidate_group'],'row':x['candidate_start_row']} for x in out],ensure_ascii=True))
