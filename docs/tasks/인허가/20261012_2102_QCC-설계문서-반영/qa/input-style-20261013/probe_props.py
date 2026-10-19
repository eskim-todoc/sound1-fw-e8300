import json
from pathlib import Path
from zipfile import ZipFile
from lxml import etree as E
q=Path(__file__).resolve().parent;N={'w':'http://schemas.openxmlformats.org/wordprocessingml/2006/main'}
for code in ['SR','SGR']:
 with ZipFile(q/(code+'-before.docx')) as z:doc=E.fromstring(z.read('word/document.xml'))
 pp=doc.xpath('//w:body//w:p',namespaces=N)
 idx=[175,197,199,200,201,228,229,230,251,254,255,256,275] if code=='SR' else [143,155,179,188,282]
 for i in idx:
  p=pp[i];pr=p.find('w:pPr',N)
  print(code,i,json.dumps({'pPr':{E.QName(x).localname:{E.QName(k).localname:v for k,v in x.attrib.items()} for x in pr if E.QName(x).localname not in ['rPr','pPrChange']},'numPr':E.tostring(pr.find('w:numPr',N),encoding='unicode').split('</w:numPr>')[0][-400:] if pr.find('w:numPr',N) is not None else None,'markdel':bool(pr.xpath('./w:rPr/w:del',namespaces=N)),'breaks':p.xpath('.//w:br/@w:type',namespaces=N)},ensure_ascii=False))
