from pathlib import Path
from zipfile import ZipFile
from lxml import etree as E
import json,sys
sys.stdout.reconfigure(encoding='utf-8')
QA=Path(__file__).resolve().parent
source=next(x for x in json.loads((QA/'source-manifest.json').read_text(encoding='utf-8-sig'))['files'] if x['code']=='SAD')['path']
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main';N={'w':W}
with ZipFile(source) as z:tree=E.fromstring(z.read('word/document.xml'))
for p in tree.findall('.//w:p',N):
 if any(a.tag=='{'+W+'}tbl' for a in p.iterancestors()):continue
 texts=[t for t in p.findall('.//w:t',N) if not any(a.tag=='{'+W+'}del' for a in t.iterancestors())]
 text=''.join(t.text or '' for t in texts)
 if 'SI-ID-QCC-' not in text:continue
 print(json.dumps({'paraId':p.get('{http://schemas.microsoft.com/office/word/2010/wordml}paraId'),'text':text,
 'runs':[{'text':t.text,'parent':E.QName(t.getparent().getparent()).localname} for t in texts]},ensure_ascii=False))
