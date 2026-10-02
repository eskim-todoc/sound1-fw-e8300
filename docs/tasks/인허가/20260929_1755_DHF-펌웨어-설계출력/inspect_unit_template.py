from pathlib import Path
from lxml import etree as E
import zipfile,json,hashlib
import pypdfium2 as pdfium
from PIL import Image,ImageDraw
T=Path(__file__).resolve().parent;QA=T/'qa/unit-reconciliation'
N={'w':'http://schemas.openxmlformats.org/wordprocessingml/2006/main','a':'http://schemas.openxmlformats.org/drawingml/2006/main'}
def text(p):return ''.join(p.xpath('.//w:t/text()',namespaces=N))
def run():
 p=QA/'SDDD-source.docx'
 with zipfile.ZipFile(p) as z:parts={n:z.read(n) for n in z.namelist()}
 r=E.fromstring(parts['word/document.xml']);ps=r.xpath('//w:body//w:p',namespaces=N)
 print('heads',[(i,text(x),x.xpath('./w:pPr/w:pStyle/@w:val',namespaces=N)) for i,x in enumerate(ps[:260]) if x.xpath('./w:pPr/w:pStyle[@w:val="10"]',namespaces=N)])
 print('pictures',[(i,x.xpath('.//a:blip/@*',namespaces=N)) for i,x in enumerate(ps[:250]) if x.xpath('.//a:blip',namespaces=N)])
 styles=E.fromstring(parts['word/styles.xml'])
 evidence={'sections':[E.tostring(s).decode() for s in r.xpath('//w:sectPr',namespaces=N)],
   'styles':{s.get('{'+N['w']+'}styleId'):E.tostring(s).decode() for s in styles if s.get('{'+N['w']+'}styleId') in ['10','2','3','4','5','a','Normal']},
   'parts':{n:dict(size=len(v),sha256=hashlib.sha256(v).hexdigest()) for n,v in parts.items()}}
 (QA/'template-evidence.json').write_text(json.dumps(evidence,ensure_ascii=False,indent=2),encoding='utf8')
 print('section',evidence['sections'])
 print('styles',evidence['styles'])
 for name in ['SDDD-source']:
  pdf=pdfium.PdfDocument(str(QA/(name+'.pdf')))
  out=QA/(name+'-pages');out.mkdir(exist_ok=True)
  ims=[]
  for i in range(len(pdf)):
   im=pdf[i].render(scale=1.5).to_pil();im.save(out/f'page-{i+1}.png')
   thumb=im.copy();thumb.thumbnail((280,400));ims.append(thumb)
  for start in range(0,len(ims),12):
   canvas=Image.new('RGB',(1120,1260),'#ddd');d=ImageDraw.Draw(canvas)
   for j,im in enumerate(ims[start:start+12]):
    x=(j%4)*280;y=(j//4)*420;canvas.paste(im,(x,y));d.text((x+5,y+400),str(start+j+1),fill='black')
   canvas.save(out/f'overview-{start+1}.png')
 print('reference pages rendered')
if __name__=='__main__':run()
