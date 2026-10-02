"""Separate unchanged reviewed pages from pages requiring renewed visual QA."""
from pathlib import Path
import json
from PIL import Image, ImageChops, ImageDraw

T=Path(__file__).resolve().parent
Q=T/'qa/design-readability/design-intent-and-placeholders'
B=T/'qa/design-readability/flow12-and-module-explanation'
base=json.loads((B/'visual-check.json').read_text(encoding='utf8'))
patch=json.loads((Q/'patch.json').read_text(encoding='utf8'))
assert base['candidate_sha256']==patch['baseline']
count=json.loads((Q/'checks.json').read_text(encoding='utf8'))['pages']
same=[];changed=[]
for n in range(1,count+1):
    fn=f'page-{n:03}.png'
    a=Image.open(Q/'pages'/fn).convert('RGB')
    b=Image.open(B/'pages'/fn).convert('RGB')
    (same if a.size==b.size and ImageChops.difference(a,b).getbbox() is None else changed).append(n)
folder=Q/'changed-boards';folder.mkdir(exist_ok=True)
for k in range(0,len(changed),4):
    batch=changed[k:k+4]
    ims=[Image.open(Q/'pages'/f'page-{n:03}.png').convert('RGB') for n in batch]
    w=max(im.width for im in ims);h=max(im.height for im in ims)
    board=Image.new('RGB',(2*w,2*(h+24)),'#ddd');d=ImageDraw.Draw(board)
    for j,(n,im) in enumerate(zip(batch,ims)):
        x=(j%2)*w;y=(j//2)*(h+24)
        d.text((x+6,y+4),f'Page {n}',fill='black');board.paste(im,(x,y+24))
    board.save(folder/f'board-{k//4+1:02}.png')
result={'baseline_review':str(B/'visual-check.json'),'pixel_identical_pages':same,
        'pages_requiring_review':changed,'changed_boards':(len(changed)+3)//4}
(Q/'page-comparison.json').write_text(json.dumps(result,indent=2),encoding='utf8')
print(json.dumps(result))
