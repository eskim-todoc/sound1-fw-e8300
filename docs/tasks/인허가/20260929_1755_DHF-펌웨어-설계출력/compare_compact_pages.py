"""Identify exact raster changes after the reviewed orphan-paragraph repair."""
from compact_unit_style import *
import pypdfium2 as pdfium
from PIL import ImageChops

before=pdfium.PdfDocument(str(Q/'before-orphan-fix.pdf'))
after=pdfium.PdfDocument(str(Q/'candidate.pdf'))
assert len(before)==len(after)
changed=[]
for i in range(len(after)):
    a=before[i].render(scale=96/72).to_pil().convert('RGB')
    b=after[i].render(scale=96/72).to_pil().convert('RGB')
    if a.size!=b.size or ImageChops.difference(a,b).getbbox():changed.append(i+1)
jsonout(Q/'orphan-fix-page-diff.json',{'pages':len(after),'pixel_identical_pages':len(after)-len(changed),'changed_pages':changed})
print(json.dumps({'pages':len(after),'changed_pages':changed}))
