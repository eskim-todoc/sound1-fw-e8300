from PIL import Image, ImageDraw, ImageChops
import pypdfium2 as pdf
import json
from update_method_definitions import Q
d=Q/'performance'
docs=[pdf.PdfDocument(d/(s+'-accepted.pdf')) for s in ('before','candidate')]
assert len(docs[0])==len(docs[1])
changed=[]
for i in range(len(docs[0])):
    ims=[]
    for s,doc in zip(('before','candidate'),docs):
        page=doc[i]; im=page.render(scale=1.5).to_pil().convert('RGB');page.close()
        im.save(d/f'{s}-accepted-{i+1:02}.png');ims.append(im)
    if ImageChops.difference(*ims).getbbox(): changed.append(i+1)
for start in range(0,len(changed),4):
    out=Image.new('RGB',(2200,1600),'#eee');draw=ImageDraw.Draw(out)
    for slot,n in enumerate(changed[start:start+4]):
        im=Image.open(d/f'candidate-accepted-{n:02}.png');im.thumbnail((1080,750))
        x=slot%2*1100;y=slot//2*800;out.paste(im,(x,y+30));draw.text((x+10,y+8),str(n),fill='black')
    out.save(d/f'changed-contact-{start//4+1:02}.png')
for doc in docs:doc.close()
(d/'page-checks.json').write_text(json.dumps({'pages':55,'changed_pages':changed,'unchanged_pages':[i for i in range(1,56) if i not in changed]},indent=2),encoding='utf8')
print(changed)
