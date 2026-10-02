"""Refresh only generated TOC page values; retain the original TOC revision history."""
from pathlib import Path
import zipfile, json, argparse
from lxml import etree as E
T=Path(__file__).resolve().parent
parser=argparse.ArgumentParser()
parser.add_argument('--qa', default='qa/design-readability/common-style')
parser.add_argument('--retire-obsolete',action='append',default=[])
args=parser.parse_args()
QA=T/args.qa
N={'w':'http://schemas.openxmlformats.org/wordprocessingml/2006/main'}
def load(p):
    with zipfile.ZipFile(p) as z:return {n:z.read(n) for n in z.namelist()}
parts=load(QA/'candidate.docx')
r=E.fromstring(parts['word/document.xml'])
a=E.fromstring(load(QA/'candidate-accepted.docx')['word/document.xml'])
def entries(root):
    result={}
    for p in root.xpath('./w:body/w:sdt//w:p',namespaces=N):
        if p.xpath('./w:pPr/w:rPr/w:del',namespaces=N):continue
        nodes=p.xpath('.//w:t[not(ancestor::w:del)]',namespaces=N)
        if len(nodes)<2 or not (nodes[-1].text or '').isdigit():continue
        key=''.join((n.text or '') for n in nodes[:-1]).replace(' ','')
        result[key]=nodes[-1]
    return result
old=entries(r);new=entries(a)
obsolete=old.keys()-new.keys()
assert obsolete==set(args.retire_obsolete),('Unexpected obsolete TOC entries',obsolete)
if obsolete:
    w=N['w'];q=lambda t:'{'+w+'}'+t
    original=E.fromstring(load(QA/'before.docx')['word/document.xml'])
    original_revisions=set(original.xpath('.//w:ins/@w:id|.//w:del/@w:id|.//w:pPrChange/@w:id|.//w:rPrChange/@w:id',namespaces=N))
    for key in obsolete:
        p=old[key]
        while p.tag!=q('p'):p=p.getparent()
        # Generated field caches are refreshed by Word, not authored normative
        # text. Remove only the obsolete cache paragraph without nesting tracked
        # deletions into PAGEREF field runs (Word cannot open that representation).
        assert not original_revisions & set(p.xpath('.//@w:id',namespaces=N))
        p.getparent().remove(p)
    old=entries(r)
assert old.keys()==new.keys(),(old.keys()-new.keys(),new.keys()-old.keys())
changes=[]
for key,node in old.items():
    if node.text!=new[key].text:
        changes.append({'entry':key,'before':node.text,'after':new[key].text})
        node.text=new[key].text
parts['word/document.xml']=E.tostring(r,xml_declaration=True,encoding='UTF-8',standalone=True)
with zipfile.ZipFile(QA/'candidate.docx','w',zipfile.ZIP_DEFLATED) as z:
    for n,data in parts.items():z.writestr(n,data)
(QA/'toc-cache.json').write_text(json.dumps(changes,ensure_ascii=False,indent=2),encoding='utf8')
print('TOC entries checked:',len(old),'page caches refreshed:',len(changes))
