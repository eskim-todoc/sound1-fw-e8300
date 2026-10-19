"""Raster and compare the independent Word previews; never edits source documents."""
from pathlib import Path
import json, re, hashlib
import pypdfium2 as pdfium
from PIL import Image,ImageDraw,ImageChops

QA=Path(__file__).resolve().parent/'qa/sddd-qcc-cleanup-20261016'

def main():
    docs={x:pdfium.PdfDocument(QA/(x+'.pdf')) for x in ['before','candidate','reject-new']}
    assert len(set(len(d) for d in docs.values()))==1
    pngdir=QA/'pages';pngdir.mkdir(exist_ok=True)
    changed=[];rows=[]
    for i in range(len(docs['candidate'])):
        row={'page':i+1}
        imgs={}
        for name,doc in docs.items():
            page=doc[i];tp=page.get_textpage();txt=tp.get_text_range()
            row[name+'_text_len']=len(txt)
            row[name+'_text']=txt
            im=page.render(scale=1.5).to_pil().convert('RGB')
            imgs[name]=im
            if name=='candidate':im.save(pngdir/f'page-{i+1:03}.png')
            tp.close();page.close()
        row['reject_pixels_identical']=ImageChops.difference(imgs['before'],imgs['reject-new']).getbbox() is None
        row['changed_pixels']=ImageChops.difference(imgs['before'],imgs['candidate']).getbbox()
        if row['changed_pixels']:changed.append(i+1)
        rows.append(row)
    # Contact sheets preserve the page ordering for the whole-document layout pass.
    sheets=QA/'sheets';sheets.mkdir(exist_ok=True)
    for first in range(1,len(rows)+1,12):
        canvas=Image.new('RGB',(1500,1740),'#d0d0d0');draw=ImageDraw.Draw(canvas)
        for j,n in enumerate(range(first,min(first+12,len(rows)+1))):
            im=Image.open(pngdir/f'page-{n:03}.png');im.thumbnail((370,550))
            x=(j%4)*375;y=(j//4)*580
            canvas.paste(im,(x,y+24));draw.text((x+8,y+5),f'Page {n}',fill='black')
        canvas.save(sheets/f'pages-{first:03}-{min(first+11,len(rows)):03}.png')
    b=json.loads((QA/'before-word-text.json').read_text(encoding='utf-8-sig'))
    r=json.loads((QA/'reject-new-word-text.json').read_text(encoding='utf-8-sig'))
    c=json.loads((QA/'candidate-word-text.json').read_text(encoding='utf-8-sig'))
    report={'pages':len(rows),'changed_pages':changed,'reject_word_text_identical':b==r,
            'reject_all_pages_pixel_identical':all(x['reject_pixels_identical'] for x in rows),
            'prefix_counts_in_word':{'qcc':c.count('[QCC 자료 입력]'),'joint':c.count('[QCC/E8300 자료 입력]')},
            'new_mixed_paragraphs_in_word':[s['after'].replace('\n','\r') in c for s in json.loads((QA/'changes.json').read_text(encoding='utf-8'))['mixed_notice_edits']],
            'per_page':rows}
    (QA/'pdf-qa.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k!='per_page'},ensure_ascii=False,indent=2))
    for d in docs.values():d.close()

if __name__=='__main__':main()
