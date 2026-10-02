from pathlib import Path
import json,sys
import pypdfium2 as pdf
from PIL import Image,ImageDraw
Q=Path(__file__).resolve().parent/'qa/suvc-practical-criteria'
if '--record' in sys.argv:
    checks=json.loads((Q/'checks.json').read_text('utf-8'))
    result={'candidate_sha256':checks['candidate_sha256'],'main_pages':14,'review':'전체 14쪽 배치 개요와 장비/비교/수치/HEAR/CM3/BOOT의 상세 출력 검토. 셀 입력/결과는 구조 검사로 전수 대조.','native_rich_text_verified':True,'row_autofit_checked':True,'font_size':11,'unit_criteria_completed':115,'runtime_tests_executed':False}
    (Q/'visual-check.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print('Visual review recorded for',checks['candidate_sha256'])
    sys.exit(0)
pages={}
for f in Q.glob('*.pdf'):
    d=pdf.PdfDocument(f);pages[f.name]=len(d)
    for i,p in enumerate(d):
        p.render(scale=1.5).to_pil().save(Q/(f.stem+f'-{i+1:02d}.png'))
        p.close()
    d.close()
for group in range(2):
    sheet=Image.new('RGB',(1686,4*625),'#eee');draw=ImageDraw.Draw(sheet)
    for slot,i in enumerate(range(group*8,min(group*8+8,pages['main-full.pdf']))):
        im=Image.open(Q/f'main-full-{i+1:02d}.png');im.thumbnail((830,580))
        x=(slot%2)*843;y=(slot//2)*625
        sheet.paste(im,(x,y+30));draw.text((x+10,y+8),f'Page {i+1}',fill='black')
    sheet.save(Q/f'contact-{group+1}.png')
(Q/'render.json').write_text(json.dumps(pages,ensure_ascii=False,indent=2),encoding='utf-8')
print(pages)
