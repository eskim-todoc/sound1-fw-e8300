"""Read current/source SAD and extract figures for architecture review."""
from pathlib import Path
import json, zipfile
from lxml import etree as E

T = Path(__file__).resolve().parent
ROOT = T.parents[3]
OUT = ROOT / 'docs/참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어'
QA = T / 'qa/architecture'
QA.mkdir(parents=True, exist_ok=True)
N = {'w':'http://schemas.openxmlformats.org/wordprocessingml/2006/main',
     'a':'http://schemas.openxmlformats.org/drawingml/2006/main',
     'r':'http://schemas.openxmlformats.org/officeDocument/2006/relationships'}
inv = json.loads((T/'inventory.json').read_text(encoding='utf8'))
source = Path(next(v['source'] for v in inv if v['name'].startswith('SAD-')))
for label, path in [('source',source),('current',OUT/'SAD-FW-TD24A 소프트웨어 아키텍쳐 기술서.docx')]:
    with zipfile.ZipFile(path) as z:
        root=E.fromstring(z.read('word/document.xml'))
        rels={r.get('Id'):r.get('Target') for r in E.fromstring(z.read('word/_rels/document.xml.rels'))}
        print(label, 'revisions',len(root.xpath('//w:ins|//w:del',namespaces=N)))
        for i,p in enumerate(root.xpath('//w:body//w:p',namespaces=N)):
            for rid in p.xpath('.//a:blip/@r:embed',namespaces=N):
                target=rels[rid];dest=QA/f'{label}-{i}-{Path(target).name}'
                dest.write_bytes(z.read('word/'+target))
                print(i,rid,str(dest), 'deleted',bool(p.xpath('.//w:del',namespaces=N)))
        if label=='current':
            for i,p in enumerate(root.xpath('//w:body//w:p',namespaces=N)):
                if 175 <= i <= 280:
                    print(i,''.join(p.xpath('.//w:t[not(ancestor::w:del)]/text()',namespaces=N)))
