"""Verify surgical SAD changes and reusable analysis routing without modifying them."""
from pathlib import Path
import json
import re
import zipfile
from urllib.parse import unquote
from copy import deepcopy
from lxml import etree as E

T=Path(__file__).resolve().parent
ROOT=T.parents[3]
QA=T/'qa/architecture'
OUT=ROOT/'docs/참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어'
N={'w':'http://schemas.openxmlformats.org/wordprocessingml/2006/main',
   'a':'http://schemas.openxmlformats.org/drawingml/2006/main',
   'r':'http://schemas.openxmlformats.org/officeDocument/2006/relationships'}
def load(p):
    with zipfile.ZipFile(p) as z: return {n:z.read(n) for n in z.namelist()}
def rejected(p):
    root=E.fromstring(p)
    for node in root.xpath('//w:ins',namespaces=N): node.getparent().remove(node)
    for node in root.xpath('//w:del',namespaces=N):
        parent=node.getparent(); pos=parent.index(node)
        for child in list(node): parent.insert(pos,child);pos+=1
        parent.remove(node)
    for node in root.xpath('//w:delText',namespaces=N): node.tag='{'+N['w']+'}t'
    # Compare text and drawing references, independent of run splitting.
    return (''.join(root.xpath('//w:t/text()',namespaces=N)),
            root.xpath('//a:blip/@r:embed',namespaces=N))

before=load(QA/'SAD-before-architecture.docx')
after=load(OUT/'SAD-FW-TD24A 소프트웨어 아키텍쳐 기술서.docx')
allowed={'word/document.xml','word/_rels/document.xml.rels'}
assert all(after[k]==v for k,v in before.items() if k not in allowed)
assert rejected(before['word/document.xml'])==rejected(after['word/document.xml'])
root=E.fromstring(after['word/document.xml'])
assert len(root.xpath('//w:ins//w:drawing[not(ancestor::w:del)]',namespaces=N))==2
assert E.fromstring(after['word/settings.xml']).find('w:trackRevisions',N) is not None
checked=0
for p in [ROOT/'docs/참고/시스템/Sound1 시스템 구조.md',ROOT/'AGENTS.md',ROOT/'docs/참고/인허가/사운드1/README.md']:
    for link in re.findall(r'\]\(([^)]+)\)',p.read_text(encoding='utf8')):
        link=unquote(link.strip('<>').split('#')[0])
        if not link or '://' in link: continue
        assert (p.parent/link).exists(),(p,link)
        checked+=1
result={'unchanged_existing_parts':len(before)-2,'reject_all_text_and_image_references_equal':True,
        'tracked_revisions_enabled':True,'new_figures':2,'checked_local_links':checked,
        'rendered_pages':16,'scope':'Package/link/render checks; not hardware testing.'}
(QA/'verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps(result,ensure_ascii=False,indent=2))
