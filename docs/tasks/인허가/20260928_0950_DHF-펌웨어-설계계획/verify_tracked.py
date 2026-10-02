import re, json
from copy import deepcopy
from zipfile import ZipFile
from build_plans import DEST, NS, W, xml, TASK

def accepted_text(data):
    x=xml(data)
    for n in x.xpath('//w:del|//w:moveFrom',namespaces=NS): n.getparent().remove(n)
    return re.sub(r'\s+','', ''.join(x.xpath('//w:t/text()',namespaces=NS)))

results=[]
for p in sorted(DEST.glob('*_변경추적.docx')):
    revised=DEST/p.name.replace('_변경추적','')
    with ZipFile(p) as z,ZipFile(revised) as final:
        assert z.testzip() is None
        setting=xml(z.read('word/settings.xml'))
        assert setting.find(f'{{{W}}}trackRevisions') is not None
        ins=deleted=0
        for name in z.namelist():
            if re.match(r'word/(document|header\d+|footer\d+)\.xml$',name):
                x=xml(z.read(name));ins+=len(x.xpath('//w:ins',namespaces=NS));deleted+=len(x.xpath('//w:del',namespaces=NS))
        a=accepted_text(z.read('word/document.xml'))
        b=accepted_text(final.read('word/document.xml'))
        assert a==b,(p.name,'accepted body text mismatch')
        accepted=xml(z.read('word/document.xml'))
        for n in accepted.xpath('//w:del|//w:moveFrom',namespaces=NS):
            n.getparent().remove(n)
        clean=xml(final.read('word/document.xml'))
        assert len(accepted.xpath('//w:drawing|//w:pict',namespaces=NS))==len(clean.xpath('//w:drawing|//w:pict',namespaces=NS)),(p.name,'accepted image count mismatch')
        assert ins>0 and deleted>0
        result={'file':p.name,'insertions':ins,'deletions':deleted,'accepted_body_equals_current':True}
        results.append(result);print(result)
assert len(results)==6
(TASK/'tracked_verification.json').write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf-8')
