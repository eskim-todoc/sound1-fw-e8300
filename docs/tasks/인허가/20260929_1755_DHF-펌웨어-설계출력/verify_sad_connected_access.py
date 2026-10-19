"""Verify the single trace row and render its affected pages before publication."""
import hashlib
import json
import re
import shutil
import sys
import zipfile
from pathlib import Path
import pypdfium2 as pdfium
from lxml import etree as E
from patch_sad_connected_access import QA,NOTE,TRANSFER
from inspect_audit_units import FILES,TASK
from inspect_sad_unit_trace import N,q,text,sha
from patch_sad_audit_trace import parts,current_rows
from verify_audit_documents import stable_doc
from publish_audit_documents import page_caches

def main():
    base=json.loads((QA/'baseline.json').read_text(encoding='utf8'))
    assert sha(FILES['sad'])==base['sha256']
    assert all(sha(Path(p))==h for p,h in base['protected'].items())
    before=parts(QA/'before.docx');after=parts(QA/'candidate.docx')
    assert [k for k in before if before[k]!=after[k]]==['word/document.xml']
    r0=E.fromstring(before['word/document.xml']);r1=E.fromstring(after['word/document.xml'])
    rows0,rows1=current_rows(r0),current_rows(r1)
    assert list(rows0)==list(rows1) and len(rows1)==81
    assert all(E.tostring(rows0[k])==E.tostring(rows1[k]) for k in rows0 if k!='SW-FW-RID-79')
    row=rows1['SW-FW-RID-79'];tx=text(row)
    assert NOTE in tx and TRANSFER in tx
    assert set(re.findall(r'SU-ID-\w+-\d+',tx))=={'SU-ID-CM3-005','SU-ID-CM3-030','SU-ID-CM3-031'}
    assert E.fromstring(after['word/settings.xml']).find(q('trackRevisions')) is not None
    assert sum('[QCC 자료 입력]' in text(p) for p in r1.findall('.//w:p',N))==29
    for run in row.xpath('.//w:r[w:t[contains(.,"[E8300")]]',namespaces=N):
        pr=run.find(q('rPr'));assert pr.find(q('b')) is not None
        assert pr.find(q('color')).get(q('val'))=='0070C0'
    assert stable_doc(QA/'candidate-rejected.docx')==stable_doc(TASK/'qa/audit-units-four-docs-20261012/sad-rejected.docx')
    # Remove just the edited row and verify every other document node is unchanged.
    rows0['SW-FW-RID-79'].getparent().remove(rows0['SW-FW-RID-79'])
    row.getparent().remove(row)
    assert E.tostring(r0)==E.tostring(r1)
    previous=pdfium.PdfDocument(str(TASK/'qa/audit-units-four-docs-20261012/sad.pdf'))
    rendered=pdfium.PdfDocument(str(QA/'candidate.pdf'))
    old={}
    for i,page in enumerate(previous):
        im=page.render(scale=1.5).to_pil().convert('RGB')
        old[hashlib.sha256(im.crop((0,195,im.width,im.height-100)).tobytes()).hexdigest()]=i+1
    changed=[];same=[]
    output=QA/'pages';output.mkdir(exist_ok=True)
    for i,page in enumerate(rendered):
        im=page.render(scale=1.5).to_pil().convert('RGB');im.save(output/f'page-{i+1:03d}.png')
        digest=hashlib.sha256(im.crop((0,195,im.width,im.height-100)).tobytes()).hexdigest()
        (same if digest in old else changed).append(i+1)
    result={'requirements':81,'edited_rows':['SW-FW-RID-79'],'qcc_markers':29,
            'units':['CM3-005','CM3-030','CM3-031'],'design_condition_only':True,
            'pages':len(rendered),'changed_pages':changed,'pixel_identical_bodies':same,
            'rejection_matches':True,'protected_unchanged':len(base['protected'])}
    (QA/'verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    if '--publish' in sys.argv:
        visual=json.loads((QA/'visual-review.json').read_text(encoding='utf8'))
        assert visual['reviewed_pages']==changed and not visual['issues']
        package=parts(QA/'candidate.docx')
        result['page_cache_changes']=page_caches(package,parts(QA/'candidate-accepted.docx'),len(rendered))
        staged=QA/'publish.docx'
        with zipfile.ZipFile(staged,'w',zipfile.ZIP_DEFLATED) as z:
            for name,data in package.items():z.writestr(name,data)
        assert sha(FILES['sad'])==base['sha256']
        temp=FILES['sad'].with_name(FILES['sad'].name+'.pending');assert not temp.exists()
        shutil.copyfile(staged,temp);temp.replace(FILES['sad'])
        assert sha(FILES['sad'])==sha(staged)
        assert all(sha(Path(p))==h for p,h in base['protected'].items())
        result['output']=str(FILES['sad']);result['sha256']=sha(FILES['sad'])
        (QA/'published-verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(result,ensure_ascii=False,indent=2))

if __name__=='__main__':main()
