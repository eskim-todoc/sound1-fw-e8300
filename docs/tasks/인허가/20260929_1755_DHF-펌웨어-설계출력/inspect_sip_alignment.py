"""Read active Word text and retain paragraph indices for bounded edits."""
from pathlib import Path
import hashlib
import json
import zipfile
from lxml import etree as E

TASK = Path(__file__).resolve().parent
OUT = TASK.parents[2] / '참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어'
W = '{http://schemas.openxmlformats.org/wordprocessingml/2006/main}'
NS = {'w': W[1:-1]}

def text(p):
    return ''.join(p.xpath('.//w:t[not(ancestor::w:del)]/text()', namespaces=NS))

def inspect(kind):
    path = next(OUT.glob(kind + '-FW-TD24A *.docx'))
    with zipfile.ZipFile(path) as z:
        root = E.fromstring(z.read('word/document.xml'))
        styles = E.fromstring(z.read('word/styles.xml'))
        names = {s.get(W+'styleId'): ''.join(s.xpath('./w:name/@w:val',namespaces=NS)) for s in styles}
    paragraphs = root.xpath('//w:body//w:p[not(ancestor::w:del)]', namespaces=NS)
    records=[]
    for i,p in enumerate(paragraphs):
        st = p.find('./'+W+'pPr/'+W+'pStyle')
        sid = st.get(W+'val') if st is not None else 'Normal'
        value = text(p)
        deleted_mark = p.find('./'+W+'pPr/'+W+'rPr/'+W+'del') is not None
        if value or p.xpath('.//w:drawing[not(ancestor::w:del)]',namespaces=NS):
            records.append({'index':i,'style':names.get(sid,sid),'text':value,'deleted_mark':deleted_mark,
                            'inserted_text': all(t.xpath('ancestor::w:ins',namespaces=NS) for t in p.xpath('.//w:t[not(ancestor::w:del)]',namespaces=NS))})
    return {'kind':kind,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
            'records':records,'ins':len(root.findall('.//'+W+'ins')),'del':len(root.findall('.//'+W+'del'))}

if __name__=='__main__':
    import sys
    for kind in sys.argv[1:] or ['SIP']:
        result=inspect(kind)
        print(json.dumps({k:v for k,v in result.items() if k!='records'},ensure_ascii=False))
        for r in result['records']:
            if kind=='SIP' or '제목' in r['style']:
                print(f"{r['index']:04d} [{r['style']}] {'JOIN ' if r['deleted_mark'] else ''}{r['text']}")
