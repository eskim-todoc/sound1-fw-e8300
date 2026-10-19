"""Excel 읽기 전용 PDF를 렌더링하여 QA용 페이지/텍스트 근거를 남긴다."""
from __future__ import annotations
import json
import re
import subprocess
from pathlib import Path
from pypdf import PdfReader
from PIL import Image, ImageDraw
from review_evidence_collection import QA

POPPLER=Path('C:/Users/EUNSU/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/poppler/Library/bin/pdftoppm.exe')
checks=[]
plan=json.loads((QA/'patch-plan.json').read_text(encoding='utf-8'))
base=json.loads((QA/'baseline.json').read_text(encoding='utf-8'))
books={b['key']:b for b in base['books']}
def norm(text):
    return re.sub(r'\s+','',text).replace('−','-')
for pdf in QA.glob('*-native.pdf'):
    key=pdf.stem.removesuffix('-native')
    reader=PdfReader(pdf)
    texts=[page.extract_text() or '' for page in reader.pages]
    alltext=norm(''.join(texts))
    common=books[key]['common']
    first=common if key.endswith('-report') else (8 if key.startswith('unit') else 10 if key.startswith('integration') else 7)
    missing=[]
    for patch in plan:
        if patch['key']!=key or patch['sheet']!=books[key]['sheet']:continue
        match=re.match(r'([A-Z]+)(\d+)',patch['cell']);column=0
        for char in match[1]:column=column*26+ord(char)-64
        if column<first:continue
        tail=norm(str(patch['value']))[-55:]
        if tail and tail not in alltext:missing.append({'cell':patch['cell'],'tail':tail})
    target=QA/'native-pages'/key
    target.mkdir(parents=True,exist_ok=True)
    subprocess.run([str(POPPLER),'-r','110','-png',str(pdf),str(target/'page')],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    images=sorted(target.glob('page-*.png'))
    for start in range(0,len(images),4):
        canvas=Image.new('RGB',(1920,1432),'#cccccc')
        draw=ImageDraw.Draw(canvas)
        for n,path in enumerate(images[start:start+4]):
            img=Image.open(path).convert('RGB');img.thumbnail((960,690))
            x=(n%2)*960;y=(n//2)*716
            canvas.paste(img,(x,y+26));draw.text((x+8,y+6),f'{key} / {path.stem}',fill='black')
        canvas.save(target/f'contact-{start//4+1:02d}.png')
    (target/'page-text.json').write_text(json.dumps(texts,ensure_ascii=False,indent=2),encoding='utf-8')
    checks.append({'key':key,'pages':len(texts),'emptyPages':[i+1 for i,t in enumerate(texts) if not t.strip()], 'tailNotFound':missing,
                   'contacts':list(map(str,sorted(target.glob('contact-*.png'))))})
(QA/'rendered-pages.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps([{k:v for k,v in c.items() if k!='contacts'} for c in checks],ensure_ascii=False))
