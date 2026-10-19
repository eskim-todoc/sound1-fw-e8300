from pathlib import Path
import json,hashlib
H=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
maprows=json.loads((H/'boundary-cell-map-final.json').read_text(encoding='utf-8'))
normal={'SUVR':[7,28,60,66,93,122]}
current=json.loads((H/'qa.json').read_text(encoding='utf-8'))
out={'status':'실패','scope':'Actual native PDF pixel review, document display only. No new product execution or observation.',
     'saved_source_sha256':{f['key']:f['sha256'] for f in current['files']},
     'native_pdf_archive':[],'reviewed_boundary_pages':[],
     'repair_candidate':'group-boundary-candidates.json',
     'whole_RTT_static_support':'72 formal RTT groups preserved; 1095 expected/observed cells scanned for actual merged height/font/line/width. String existence and geometry do not prove visible glyphs.',
     'CFX31_C1_repaired_native':{'SUVC_page':39,'SUVR_page':34,'directly_visible':'A[0:63] all printed rows and PASS N=64 F=0. Criterion/report expected and historical RTT adjacent; historical observation only.'},
     'limitations':'This record fixes the failed round to its hashes. New candidate display must be reviewed separately. Normal long-row continuation is preserved.'}
for p in sorted((H/'group-boundary-before-native').glob('*.pdf')):
    out['native_pdf_archive'].append({'path':str(p),'sha256':sha(p)})
for r in maprows:
    p=H/'boundary-review-final'/f"{r['key']}-{r['page']:03d}.png"
    if not p.exists():continue
    out['reviewed_boundary_pages'].append({**r,'pixel_path':str(p),'pixel_sha256':sha(p),
      'inspection':'normal long RTT/array continuation; not a short-paragraph repair' if r['page'] in normal.get(r['key'],[]) else 'actual short paragraph/label/page edge inspected; fixed start-boundary candidate required'})
for key in ['SUVC','SUVR']:
    p=H/f'{key}-CFX31-C1-native-visible.png'
    out['CFX31_C1_repaired_native'][key]={'path':str(p),'sha256':sha(p)}
(H/'native-boundary-round1-review.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'status':out['status'],'PDFs':len(out['native_pdf_archive']),'pixel_pages':len(out['reviewed_boundary_pages']),'normal_array_pages':sum(len(x) for x in normal.values())}))
