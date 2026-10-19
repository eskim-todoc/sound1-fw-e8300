from pathlib import Path
import sys,json
import pypdfium2 as pdfium
Q=Path(__file__).resolve().parent;mode=sys.argv[1] if len(sys.argv)>1 else 'after';label=sys.argv[2] if len(sys.argv)>2 else mode
records=[]
for code in ['SR','SGR']:
 d=pdfium.PdfDocument(str(Q/(code+'-'+mode+'.pdf')));out=Q/('views-'+label)/code;out.mkdir(parents=True,exist_ok=True)
 pages=[]
 for i in range(len(d)):
  p=d[i];p.render(scale=1.5).to_pil().convert('RGB').save(out/f'page-{i+1:03d}.png')
  text=p.get_textpage().get_text_range();pages.append({'page':i+1,'text_chars':len(text),'head':text[:200],'tail':text[-200:]})
 records.append({'code':code,'pages':len(d),'folder':str(out),'page_info':pages})
 print(code,mode,len(d),'pages rendered')
(Q/('render-'+label+'.json')).write_text(json.dumps(records,ensure_ascii=False,indent=2),encoding='utf-8')
