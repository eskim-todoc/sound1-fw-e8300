from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
from lxml import etree as E
import json,copy
QA=Path(__file__).resolve().parent/'qa/sad-qcc-cleanup-20261016'
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main'
q=lambda s:'{'+W+'}'+s
meta=json.loads((QA/'changes.json').read_text(encoding='utf8'))
with ZipFile(QA/'candidate.docx')as z:parts={n:z.read(n) for n in z.namelist()}
root=E.fromstring(parts['word/document.xml'])
def group(e):
 if e.tag==q('rPrChange'):return 'color'
 if e.getparent().tag==q('trPr'):return 'table'
 p=next((a for a in e.iterancestors() if a.tag==q('p')),None)
 if p is not None and any('소프트웨어 유닛과 상위 요구사항 연결' in ''.join(a.itertext()) for a in [p]):return 'caption'
 return 'text'
for selected in ['color','text','table','caption']:
 r=copy.deepcopy(root)
 for e in reversed(list(r.iter())):
  if not e.get(q('id'),'').isdigit() or int(e.get(q('id')))<meta['new_revision_start']:continue
  if group(e)==selected:continue
  par=e.getparent()
  if par is None:continue
  if e.tag==q('ins'):par.remove(e)
  elif e.tag==q('del'):
   if par.tag in [q('trPr'),q('rPr')]:par.remove(e)
   else:
    at=par.index(e)
    for c in list(e):
     for t in c.iter(q('delText')):t.tag=q('t')
     e.remove(c);par.insert(at,c);at+=1
    par.remove(e)
  elif e.tag==q('rPrChange'):par.getparent().replace(par,copy.deepcopy(e.find(q('rPr'))))
 with ZipFile(QA/f'diag-{selected}.docx','w',ZIP_DEFLATED)as z:
  for n,b in parts.items():z.writestr(n,E.tostring(r,encoding='UTF-8',xml_declaration=True,standalone=True) if n=='word/document.xml' else b)
