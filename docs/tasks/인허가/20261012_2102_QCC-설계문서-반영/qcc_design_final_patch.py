"""Targeted post-Office corrections; preserve all other authors' edits and media."""
from pathlib import Path
from copy import deepcopy
from datetime import datetime, timezone
import hashlib, json, zipfile
from lxml import etree as E

TASK=Path(__file__).parent
BASE=Path('G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF/03_설계 출력/SW/외부기')
NS={'w':'http://schemas.openxmlformats.org/wordprocessingml/2006/main','a':'http://schemas.openxmlformats.org/drawingml/2006/main','r':'http://schemas.openxmlformats.org/officeDocument/2006/relationships','wp':'http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing','pic':'http://schemas.openxmlformats.org/drawingml/2006/picture'}
W='{'+NS['w']+'}'
DATE=datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace('+00:00','Z')
def sha(b):return hashlib.sha256(b).hexdigest().upper()
def xml(r):return E.tostring(r,encoding='UTF-8',xml_declaration=True,standalone=True)

records=[]
for code,old_rid,new_name in [('SAD','rId13','SAD-SIP-QCC-software.png'),('SDDD','rId21','SDDD-QCC-bluetooth.png'),('SIP','rId11','SAD-SIP-QCC-software.png')]:
    path=next(BASE.glob('[[]E8300-QCC] '+code+'*.docx'))
    original=next(BASE.glob('[[]E8300] '+code+'*.docx'))
    original_hash=sha(original.read_bytes())
    before_bytes=path.read_bytes()
    with zipfile.ZipFile(path) as z:entries=[(deepcopy(i),z.read(i.filename))for i in z.infolist()]
    parts={i.filename:b for i,b in entries}
    before=E.fromstring(parts['word/document.xml']);root=deepcopy(before)
    rels=E.fromstring(parts['word/_rels/document.xml.rels'])
    revision_no=max(int(v)for v in root.xpath('//@w:id',namespaces=NS)if v.isdigit())+1
    new_revision_ids=[]
    def revision(kind):
        global revision_no
        e=E.Element(W+kind,{W+'id':str(revision_no),W+'author':'김은수',W+'date':DATE})
        new_revision_ids.append(str(revision_no));revision_no+=1
        return e
    drawings=root.xpath('//w:drawing[not(ancestor::w:del)][.//a:blip[@r:embed="'+old_rid+'"]]',namespaces=NS)
    assert len(drawings)==1,(code,len(drawings))
    old_drawing=drawings[0];old_run=old_drawing.getparent()
    assert old_run.tag==W+'r'
    assert [E.QName(e).localname for e in old_run if e.tag!=W+'rPr']==['drawing']
    old_parent=old_run.getparent();assert old_parent.tag==W+'ins'
    paragraph=old_parent.getparent();assert paragraph.tag==W+'p'
    old_target=next(e.get('Target')for e in rels if e.get('Id')==old_rid)
    old_media='word/'+old_target
    new_rid='rIdQccDesignFigure'+code
    new_media='word/media/qcc_design_'+code.lower()+'.png'
    assert not any(e.get('Id')==new_rid for e in rels)
    assert new_media not in parts
    new_image=(TASK/'qa/qcc_design/figures'/new_name).read_bytes()
    new_run=deepcopy(old_run)
    for blip in new_run.xpath('.//a:blip',namespaces=NS):blip.set('{'+NS['r']+'}embed',new_rid)
    max_docpr=max(int(v)for v in root.xpath('//wp:docPr/@id',namespaces=NS))
    for prop in new_run.xpath('.//wp:docPr',namespaces=NS):
        prop.set('id',str(max_docpr+1));prop.set('name','QCC 제공설계 반영 '+code+' 구조도')
        prop.set('descr','코드 원본에서 새로 생성한 QCC 제공설계 4모듈/14유닛과 E8300 연계 구조도. 구현과 측정은 미확인.')
    old_extent=[dict(e.attrib)for e in old_run.xpath('.//wp:extent|.//a:xfrm/a:ext',namespaces=NS)]
    assert old_extent==[dict(e.attrib)for e in new_run.xpath('.//wp:extent|.//a:xfrm/a:ext',namespaces=NS)]
    old_index=old_parent.index(old_run);delete=revision('del');old_parent.remove(old_run);delete.append(old_run);old_parent.insert(old_index,delete)
    insert=revision('ins');insert.append(new_run);paragraph.insert(paragraph.index(old_parent)+1,insert)
    assert not insert.xpath('ancestor::w:ins|ancestor::w:del',namespaces=NS)
    E.SubElement(rels,'{http://schemas.openxmlformats.org/package/2006/relationships}Relationship',Id=new_rid,Type=NS['r']+'/image',Target=new_media.removeprefix('word/'))
    corrections=[]
    if code=='SAD':
        sdt=root.find(W+'body')[30]
        assert sdt.tag==W+'sdt'
        tail=sdt.xpath('.//w:p',namespaces=NS)[-1]
        assert not tail.xpath('.//w:t',namespaces=NS)
        assert tail.xpath('.//w:fldChar[@w:fldCharType="end"]',namespaces=NS)
        ppr=tail.find(W+'pPr');assert ppr is not None and ppr.find(W+'pPrChange')is None
        old_ppr=deepcopy(ppr)
        spacing=ppr.find(W+'spacing');assert spacing is not None and spacing.get(W+'line')=='240'
        spacing.set(W+'line','20');spacing.set(W+'lineRule','exact')
        change=revision('pPrChange');change.append(old_ppr);ppr.append(change)
        corrections.append({'locator':'TOC sdt final field-end paragraph (paraId34E32455)','type':'w:pPrChange','before':'line240/auto (12pt)','after':'line20/exact (1pt)','readonly_native_experiment':'48→47pages; TOC.Update retains47; no manual pagebreak deleted'})
        matches=[]
        for p in root.xpath('//w:p',namespaces=NS):
            if ''.join(p.xpath('.//w:t[not(ancestor::w:del)]/text()',namespaces=NS))=='SDP-FW-TD20B':matches.append(p)
        assert len(matches)==1
        refp=matches[0];t=refp.xpath('.//w:t[text()="D20B"]',namespaces=NS);assert len(t)==1
        run=t[0].getparent();assert run.getparent() is refp
        newrun=deepcopy(run);newrun.find(W+'t').text='D24A'
        index=refp.index(run);d=revision('del');refp.remove(run)
        for textnode in run.findall(W+'t'):textnode.tag=W+'delText'
        d.append(run);refp.insert(index,d)
        ins=revision('ins');ins.append(newrun);refp.insert(index+1,ins)
        corrections.append({'locator':'1.4.1 프로젝트 관련문서 / SDP 참조','before':'SDP-FW-TD20B','after':'SDP-FW-TD24A','reference_verified_by_root':'01_설계 계획/SW/외부기/[E8300] SDP-FW-TD24A 소프트웨어 개발 계획서.docx'})
    # Reject this patch only, with inherited revision nodes intact.
    rejected=deepcopy(root)
    for rid in reversed(new_revision_ids):
        nodes=rejected.xpath('//*[@w:id="'+rid+'"]',namespaces=NS);assert len(nodes)==1
        node=nodes[0];parent=node.getparent();kind=E.QName(node).localname
        if kind=='ins':parent.remove(node)
        elif kind=='del':
            index=parent.index(node)
            for child in list(node):
                node.remove(child)
                for dt in child.iter(W+'delText'):dt.tag=W+'t'
                parent.insert(index,child);index+=1
            parent.remove(node)
        elif kind=='pPrChange':
            old=deepcopy(node.find(W+'pPr'));parent.getparent().replace(parent,old)
        else:raise AssertionError(kind)
    assert E.tostring(rejected,method='c14n')==E.tostring(before,method='c14n'),code+' reject does not restore'
    backup=TASK/'qa/qcc_design'/f'{code}-before-final-targeted.docx';assert not backup.exists()
    backup.write_bytes(before_bytes)
    candidate=TASK/'qa/qcc_design'/f'{code}-final-targeted.docx'
    changes={'word/document.xml':xml(root),'word/_rels/document.xml.rels':xml(rels)}
    with zipfile.ZipFile(candidate,'w')as z:
        for info,data in entries:z.writestr(info,changes.get(info.filename,data))
        z.writestr(new_media,new_image)
    with zipfile.ZipFile(candidate)as z:
        assert all(z.read(i.filename)==b for i,b in entries if i.filename not in changes)
        assert z.read(old_media)==parts[old_media]
    assert path.read_bytes()==before_bytes,'Concurrent edit detected: '+code
    path.write_bytes(candidate.read_bytes())
    assert sha(original.read_bytes())==original_hash
    records.append({'date':DATE,'code':code,'path':str(path),'sha256_before':sha(before_bytes),'sha256_after':sha(path.read_bytes()),'revision_ids':new_revision_ids,'author':'김은수','figure':{'old_rid':old_rid,'old_media':old_media,'old_sha256':sha(parts[old_media]),'new_rid':new_rid,'new_media':new_media,'new_sha256':sha(new_image),'old_media_and_relationship_preserved':True,'new_image_generated_from_code_not_bitmap_edit':True,'new_code':'qcc_design_figures.py','source_code':'docs/tasks/인허가/20260929_1755_DHF-펌웨어-설계출력/draw_architecture.py; draw_detail_architecture.py','drawing_extent_unchanged':True,'tracked_old_drawing_delete_in_inherited_ins':True,'tracked_new_drawing_insert_direct_p':True,'meaning':'Supplied QCC4modules14units, implementation/measurement unconfirmed; preserve E8300 allocation and chip/core boundaries'},'other_targeted_corrections':corrections,'reject_patch_c14n_equals_before':True,'other_package_bytes_preserved':True,'original_unchanged':True,'original_sha256':original_hash,'native_qa':'root rerender pending'})
evidence_path=TASK/'evidence/qcc_design_authored.json'
evidence=json.loads(evidence_path.read_text('utf-8'));evidence.setdefault('post_office_targeted_patches',[]).extend(records)
evidence_path.write_text(json.dumps(evidence,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(records,ensure_ascii=True))
