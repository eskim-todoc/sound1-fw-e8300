from pathlib import Path
from zipfile import ZipFile
from lxml import etree as E
import json
from style_patch import NS,q,_visible
base=Path(__file__).parent
for code,indices in [('SR',[174,175,185,186,187,188,197,217,218,219,220,221,222,223,225,244,246]),('SGR',[130,131,132,133,134,135,136,137])]:
 with ZipFile(base/(code+'-candidate.docx')) as z: d=E.fromstring(z.read('word/document.xml'));s=E.fromstring(z.read('word/styles.xml'))
 ps=d.xpath('//w:body//w:p',namespaces=NS)
 for i in indices:
  p=ps[i]
  def simple(e):
   if e is None:return None
   e=E.fromstring(E.tostring(e));E.cleanup_namespaces(e);return E.tostring(e,encoding='unicode')
  print(code,i,json.dumps({'text':_visible(p)[:100],'pPr':simple(p.find('w:pPr',NS)),'tabs':len(p.xpath('.//w:tab',namespaces=NS)),'breaks':p.xpath('.//w:br/@w:type',namespaces=NS),'sect':len(p.xpath('./w:pPr/w:sectPr',namespaces=NS))},ensure_ascii=False))
 print(code,'sect',*[simple(x) for x in d.xpath('//w:sectPr',namespaces=NS)])
