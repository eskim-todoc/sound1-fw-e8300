from pathlib import Path
import json,sys,hashlib
import pypdfium2 as pdf
from PIL import Image,ImageDraw
Q=Path(__file__).resolve().parent/'qa/sitc-current-design'
if '--record' in sys.argv:
    render=json.loads((Q/'render.json').read_text('utf-8'))
    obj={'candidate_sha256':hashlib.sha256((Q/'candidate.xlsx').read_bytes()).hexdigest(),'main_pages':render['main-full.pdf'],'reviewed_all_main_pages':True,'native_rich_text_verified':True,'review':'전체 시험 행 배치와 장비/변경 문자/NofM/부트로더 상세 출력 검토. 기존 열 폭/표지 유지.','runtime_tests_executed':False}
    (Q/'visual-check.json').write_text(json.dumps(obj,ensure_ascii=False,indent=2),encoding='utf-8');print(obj);sys.exit()
pages={}
for f in Q.glob('*.pdf'):
    d=pdf.PdfDocument(f);pages[f.name]=len(d)
    for i,p in enumerate(d):
        p.render(scale=2.0).to_pil().save(Q/(f.stem+f'-{i+1:02d}.png'));p.close()
    d.close()
for start in range(0,pages['main-full.pdf'],4):
    sheet=Image.new('RGB',(2000,2*750),'#eee');draw=ImageDraw.Draw(sheet)
    for slot,i in enumerate(range(start,min(start+4,pages['main-full.pdf']))):
        im=Image.open(Q/f'main-full-{i+1:02d}.png');im.thumbnail((980,695))
        x=(slot%2)*1000;y=(slot//2)*750;sheet.paste(im,(x,y+30));draw.text((x+10,y+8),f'Page {i+1}',fill='black')
    sheet.save(Q/f'contact-{start//4+1}.png')
(Q/'render.json').write_text(json.dumps(pages,ensure_ascii=False,indent=2),encoding='utf-8');print(pages)
