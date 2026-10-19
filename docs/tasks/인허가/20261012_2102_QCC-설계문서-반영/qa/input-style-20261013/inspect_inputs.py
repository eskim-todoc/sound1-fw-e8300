from pathlib import Path
from zipfile import ZipFile
from lxml import etree as E
import json,hashlib,shutil
Q=Path(__file__).resolve().parent;T=Q.parent.parent
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main';N={'w':W};q=lambda n:'{'+W+'}'+n
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def active(x):return not x.xpath('ancestor::w:del|ancestor::w:moveFrom|ancestor::w:pPrChange|ancestor::w:rPrChange|ancestor::w:tr[w:trPr/w:del]',namespaces=N)
def text(x):return ''.join(x.xpath('.//w:t[not(ancestor::w:del or ancestor::w:moveFrom)]/text()',namespaces=N))
m=json.loads((T/'evidence/final-output-manifest.json').read_text(encoding='utf-8'))
snapshot=[];docs=[]
for f in m['files']:
 p=Path(f['path']);snapshot.append({'code':f['code'],'path':str(p),'sha256':sha(p),'previous_record_sha256':f['sha256']})
 if f['code'] not in ['SR','SGR']:continue
 backup=Q/(f['code']+'-before.docx');shutil.copy2(p,backup)
 with ZipFile(p) as z:
  doc=E.fromstring(z.read('word/document.xml'));styles=E.fromstring(z.read('word/styles.xml'))
  (Q/(f['code']+'-numbering.xml')).write_bytes(z.read('word/numbering.xml'))
 data={'code':f['code'],'source':str(p),'source_sha256':sha(p),'backup':str(backup),'output':str(p.with_stem(p.stem+'_완료')),'paragraphs':[], 'styles':[]}
 for i,el in enumerate(doc.xpath('//w:body//w:p',namespaces=N)):
  if not active(el):continue
  pp=el.find(q('pPr'));data['paragraphs'].append({'i':i,'paraId':el.get('{http://schemas.microsoft.com/office/word/2010/wordml}paraId'),'text':text(el),'pPr':E.tostring(pp,encoding='unicode') if pp is not None else '', 'style':el.xpath('./w:pPr/w:pStyle/@w:val',namespaces=N),'table':bool(el.xpath('ancestor::w:tc',namespaces=N)),'drawings':len(el.xpath('.//w:drawing[not(ancestor::w:del)]',namespaces=N))})
 for s in styles.findall(q('style')):
  name=s.find(q('name'))
  if name is not None and name.get(q('val')).startswith('TDC'):data['styles'].append(E.tostring(s,encoding='unicode'))
 (Q/(f['code']+'-inspection.json')).write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
 docs.append({k:v for k,v in data.items() if k not in ['paragraphs','styles']})
 print(f['code'],len(data['paragraphs']),'paragraphs',len(data['styles']),'TDC styles')
 (Q/(f['code']+'-outline.txt')).write_text('\n'.join(f"{p['i']} {p['paraId']} {','.join(p['style'])} {'TABLE' if p['table'] else ''} {p['text']}" for p in data['paragraphs']),encoding='utf-8')
(Q/'baseline.json').write_text(json.dumps({'files':snapshot,'docs':docs},ensure_ascii=False,indent=2),encoding='utf-8')
