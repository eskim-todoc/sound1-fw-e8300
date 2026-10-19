from pathlib import Path
import hashlib,json
import pypdfium2 as pdfium
from PIL import ImageChops
QA=Path(__file__).resolve().parent
manifest=json.loads((QA/'source-manifest.json').read_text(encoding='utf-8-sig'))
old=next(x for x in manifest['files'] if x['code']=='SAD')
native=json.loads((QA/'native-checks.json').read_text(encoding='utf-8-sig'))
after=next(x for x in native if x['mode']=='after')
before=pdfium.PdfDocument(QA/'SAD-before-native.pdf')
current=pdfium.PdfDocument(QA/'SAD-after-native.pdf')
prior=pdfium.PdfDocument(old['native_previews'][0]['pdf'])
assert len(before)==len(current)==len(prior)
dest=QA/'views';dest.mkdir(exist_ok=True)
changed=[];same=[];baseline_diff=[];text=[]
for n in range(len(current)):
    bp=before[n].render(scale=1.5).to_pil().convert('RGB')
    cp=current[n].render(scale=1.5).to_pil().convert('RGB')
    pp=prior[n].render(scale=1.5).to_pil().convert('RGB')
    if bp.size!=pp.size or ImageChops.difference(bp,pp).getbbox() is not None: baseline_diff.append(n+1)
    equal=bp.size==cp.size and ImageChops.difference(bp,cp).getbbox() is None
    (same if equal else changed).append(n+1)
    cp.save(dest/f'page-{n+1:03}.png')
    text.append(current[n].get_textpage().get_text_range())
record={'candidate_sha256':after['sha256'],'pages':len(current),'same_pages':same,'changed_pages':changed,
    'before_matches_previously_validated_render':not baseline_diff,'baseline_different_pages':baseline_diff,
    'full_native_render':str(QA/'SAD-after-native.pdf'),'all_page_images':str(dest),
    'toc_and_body_module_titles':[{ 'page':i+1,'matching_lines':[t for t in page.splitlines() if any(name in t for name in ['시스템 관리 모듈','HW 인터페이스 모듈','블루투스 모듈','보안 모듈'])]} for i,page in enumerate(text) if any(name in page for name in ['시스템 관리 모듈','HW 인터페이스 모듈','블루투스 모듈','보안 모듈'])]}
(QA/'render-checks.json').write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:record[k] for k in ['pages','same_pages','changed_pages','baseline_different_pages']}))
