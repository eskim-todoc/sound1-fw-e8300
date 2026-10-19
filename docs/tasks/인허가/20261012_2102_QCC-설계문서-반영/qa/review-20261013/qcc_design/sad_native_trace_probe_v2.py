"""Native WordOpenXML extraction / v2 probe preparation. Never uses COM."""
from pathlib import Path
from collections import Counter
from copy import deepcopy
import argparse,json,sys
from lxml import etree as E
import sad_native_trace_probe as A

A.OUT=A.ROOT/'native-trace-probe-v2'
A.AUTHOR='QCCNativeParagraphProbeV2_20261013'
PKG='http://schemas.microsoft.com/office/2006/xmlPackage'

def extract(xmlpath, output, rows_required):
    raw=E.fromstring(Path(xmlpath).read_bytes())
    if raw.tag==A.Q('document'):
        doc=raw
    else:
        found=raw.xpath('.//pkg:part[@pkg:name="/word/document.xml"]/pkg:xmlData/w:document',namespaces={**A.N,'pkg':PKG})
        assert len(found)==1,len(found)
        doc=found[0]
    probe_nodes=doc.xpath('.//*[@w:author=$author]',namespaces=A.N,author=A.AUTHOR)
    def semantic(x):
        if x is None:return None
        if x.tag in (A.Q('pPrChange'),A.Q('rPrChange'),A.Q('ins'),A.Q('del')):return None
        children=[v for c in x if (v:=semantic(c)) is not None]
        if x.tag==A.Q('rPr') and not children and not x.attrib:return None
        return (x.tag,tuple(sorted(x.attrib.items())),(x.text or '').strip(),tuple(children))
    formats=[semantic(p.find('w:pPr',A.N)) for p in doc.iter(A.Q('p'))]
    styles=raw.xpath('.//pkg:part[@pkg:name="/word/styles.xml"]/pkg:xmlData/w:styles',namespaces={**A.N,'pkg':PKG})
    report={'native_XML':str(xmlpath),'native_XML_sha256':A.digest(xmlpath),'scope':'Document.Content.WordOpenXML emitted by actual native Word; accepted snapshots are obtained only after actual AcceptAllRevisions',
            'probe_revision_count':len(doc.xpath('.//*[@w:author=$author]',namespaces=A.N,author=A.AUTHOR)),
            'probe_revision_types':dict(Counter(E.QName(r).localname for r in probe_nodes)),
            'paragraph_count':len(formats),
            'paragraph_format_sha256':A.sha(json.dumps(formats,ensure_ascii=False).encode()),
            'style_semantic_sha256':A.sha(json.dumps(semantic(styles[0]),ensure_ascii=False).encode()) if styles else None,
            'historical_revision_text_signatures':A.revision_text(doc,ignore_probe=True),
            'historical_revision_inventory':A.revision_inventory(doc,ignore_probe=True)}
    if rows_required:
        _,rows=A.current_table(doc)
        expect=json.loads((A.OUT/'expected-cells.json').read_text(encoding='utf8'))
        differences=[]
        for index,(actual,expected) in enumerate(zip(rows,expect['rows'])):
            for col,(a,b) in enumerate(zip(actual['cells'],expected['cells']),1):
                if A.compact(a)!=A.compact(b):differences.append({'row':index,'column':col,'expected':b,'actual':a})
        report.update({'rows':rows,'row_count':len(rows),'column_count':3,'pass':not differences,'differences':differences})
    Path(output).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'report':str(output),'rows':report.get('row_count'),'probe_revisions':report['probe_revision_count'],'pass':report.get('pass')},ensure_ascii=True))
    if rows_required and not report['pass']:raise SystemExit(1)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('mode',choices=['prepare','inspect','xml-cells','xml-history']);p.add_argument('path',nargs='?');p.add_argument('output',nargs='?');a=p.parse_args()
    if a.mode=='prepare':A.prepare()
    elif a.mode=='inspect':A.inspect_native(a.path)
    else:extract(a.path,a.output,a.mode=='xml-cells')
