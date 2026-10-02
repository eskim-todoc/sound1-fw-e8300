"""SDDD의 앞부분 아키텍처 그림 5개만 교체한다."""
from copy import deepcopy
import hashlib
import json
import zipfile
from lxml import etree as E
from PIL import Image
from patch_sad_architecture import N, q, xml, OUT, T

def main():
    path=OUT/'SDDD-FW-TD24A 소프트웨어 상세 설계서.docx'
    qa=T/'qa/sddd-architecture';qa.mkdir(parents=True,exist_ok=True)
    backup=qa/'before.docx';assert not backup.exists()
    raw=path.read_bytes();backup.write_bytes(raw)
    with zipfile.ZipFile(path) as z:parts={n:z.read(n) for n in z.namelist()}
    original=dict(parts);root=E.fromstring(parts['word/document.xml']);before=deepcopy(root)
    rels=E.fromstring(parts['word/_rels/document.xml.rels'])
    drawings=root.xpath('//w:body//w:drawing[not(ancestor::w:del)]',namespaces=N)
    names=['시스템 블록도.png','SDDD 시스템 제어 연계.png','SDDD 신호처리 연계.png','SDDD 내부기 실행 연계.png','SDDD 블루투스 연계.png']
    for i,(drawing,name) in enumerate(zip(drawings,names),1):
        assert drawing.xpath('ancestor::w:ins',namespaces=N)
        picture=OUT/'이미지 참조'/name
        rid=f'rIdSdddArchitecture{i}';media=f'media/sddd_architecture_{i}.png'
        assert not any(r.get('Id')==rid for r in rels)
        E.SubElement(rels,'{http://schemas.openxmlformats.org/package/2006/relationships}Relationship',Id=rid,Type=N['r']+'/image',Target=media)
        parts['word/'+media]=picture.read_bytes()
        for blip in drawing.xpath('.//a:blip',namespaces=N):blip.set('{'+N['r']+'}embed',rid)
        extent=drawing.find('.//{'+N['wp']+'}extent');width=int(extent.get('cx'))
        with Image.open(picture) as im:height=round(width*im.height/im.width)
        extent.set('cy',str(height))
        for ext in drawing.xpath('.//a:xfrm/a:ext',namespaces=N):ext.set('cx',str(width));ext.set('cy',str(height))
        for prop in drawing.xpath('.//wp:docPr',namespaces=N):prop.set('descr',name[:-4])
    comparison=deepcopy(root)
    for dst,src in zip(comparison.xpath('//w:body//w:drawing[not(ancestor::w:del)]',namespaces=N)[:5],before.xpath('//w:body//w:drawing[not(ancestor::w:del)]',namespaces=N)[:5]):dst.getparent().replace(dst,deepcopy(src))
    assert E.tostring(comparison)==E.tostring(before)
    parts['word/document.xml']=xml(root);parts['word/_rels/document.xml.rels']=xml(rels)
    assert all(parts[n]==v for n,v in original.items() if n not in ('word/document.xml','word/_rels/document.xml.rels'))
    candidate=qa/'candidate.docx'
    with zipfile.ZipFile(candidate,'w',zipfile.ZIP_DEFLATED) as z:
        for n,v in parts.items():z.writestr(n,v)
    assert path.read_bytes()==raw
    path.write_bytes(candidate.read_bytes())
    (qa/'checks.json').write_text(json.dumps({'changed_figures':names,'remaining_drawings_unchanged':len(drawings)-5,'text_and_tables_unchanged':True,'tracking_preserved':True,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()},ensure_ascii=False,indent=2),encoding='utf8')
    print('Updated 5 architecture drawings; other content preserved.')
if __name__=='__main__':main()
