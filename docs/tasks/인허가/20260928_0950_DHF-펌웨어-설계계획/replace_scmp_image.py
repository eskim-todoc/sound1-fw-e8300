"""Replace the branch diagram with a tracked image replacement."""
import sys
from copy import deepcopy
from datetime import datetime, timezone
from build_plans import *

asset=Path(sys.argv[1])
path=next(DEST.glob('SCMP*.docx'))
with ZipFile(path) as z:package={n:z.read(n) for n in z.namelist()}
root=xml(package['word/document.xml'])
blips=root.xpath('//*[@r:embed="rId12"]',namespaces=NS)
assert len(blips)==1
oldrun=blips[0].xpath('ancestor::w:r',namespaces=NS)[0]
assert not oldrun.xpath('ancestor::w:ins|ancestor::w:del',namespaces=NS)
newrun=deepcopy(oldrun)
newrun.xpath('//*[@r:embed="rId12"]',namespaces=NS)[0].set('{'+R+'}embed','rIdQCCDiagram')
docids=[int(v) for v in root.xpath('//*[local-name()="docPr"]/@id')]
for prop in newrun.xpath('.//*[local-name()="docPr"]'):
    prop.set('id',str(max(docids+[0])+1));prop.set('name','QCC 형상관리 분기 구성')
ids=[int(v) for v in root.xpath('//@w:id',namespaces=NS) if v.isdigit()]
base=max(ids+[0])+1
now=datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
parent=oldrun.getparent();pos=parent.index(oldrun);parent.remove(oldrun)
for offset,(kind,run) in enumerate([('del',oldrun),('ins',newrun)]):
    revision=E.Element('{'+W+'}'+kind)
    for k,v in [('id',str(base+offset)),('author','김은수'),('date',now)]:revision.set('{'+W+'}'+k,v)
    revision.append(run);parent.insert(pos+offset,revision)
note=' 그림은 분기 구성의 예시이며, Sound1에서는 NRF 대신 QCC5181을 사용한다.'
found=0
for p in root.xpath('//w:p',namespaces=NS):
    full=''.join(p.xpath('.//w:t/text()',namespaces=NS))
    if note not in full:continue
    assert full.endswith(note)
    end=len(full)-len(note);offset=0
    for node in p.xpath('.//w:t',namespaces=NS):
        value=node.text or ''
        if offset+len(value)>end:
            assert node.xpath('ancestor::w:ins',namespaces=NS)
            node.text=value[:max(0,end-offset)]
        offset+=len(value)
    for ins in p.xpath('.//w:ins',namespaces=NS):
        if not ''.join(ins.xpath('.//w:t/text()',namespaces=NS)):ins.getparent().remove(ins)
    found+=1
assert found==1
rels=xml(package['word/_rels/document.xml.rels'])
rel=E.SubElement(rels,'{http://schemas.openxmlformats.org/package/2006/relationships}Relationship')
rel.set('Id','rIdQCCDiagram');rel.set('Type',R+'/image');rel.set('Target','media/qcc_branch_diagram.png')
package['word/media/qcc_branch_diagram.png']=asset.read_bytes()
package['word/document.xml']=serialize(root)
package['word/_rels/document.xml.rels']=serialize(rels)
temp=path.with_suffix('.docx.tmp')
with ZipFile(temp,'w',ZIP_DEFLATED) as z:
    for n,d in package.items():z.writestr(n,d)
temp.replace(path)
print(path)
print('Tracked image replacement; original retained only in deletion revision; note withdrawn.')
