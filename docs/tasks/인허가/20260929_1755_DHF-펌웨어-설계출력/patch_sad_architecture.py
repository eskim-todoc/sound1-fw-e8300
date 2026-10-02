"""Narrow package edit: retain original redlines and replace pending figure insertions."""
from pathlib import Path
from copy import deepcopy
from datetime import datetime, timezone
import hashlib
import json
import shutil
import zipfile
from lxml import etree as E
from PIL import Image

T=Path(__file__).resolve().parent
ROOT=T.parents[3]
OUT=ROOT/'docs/참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어'
PATH=OUT/'SAD-FW-TD24A 소프트웨어 아키텍쳐 기술서.docx'
QA=T/'qa/architecture'
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main'
N={'w':W,'a':'http://schemas.openxmlformats.org/drawingml/2006/main',
   'r':'http://schemas.openxmlformats.org/officeDocument/2006/relationships',
   'wp':'http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing'}
def q(n): return '{'+W+'}'+n
def xml(r): return E.tostring(r,encoding='UTF-8',xml_declaration=True,standalone=True)
def visible(p): return ''.join(p.xpath('.//w:t[not(ancestor::w:del)]/text()',namespaces=N))

def main():
    backup=QA/'SAD-before-architecture.docx'
    if backup.exists():
        raise RuntimeError('Baseline already exists; do not overwrite or rerun blindly.')
    shutil.copy2(PATH,backup)
    with zipfile.ZipFile(PATH) as z: parts={n:z.read(n) for n in z.namelist()}
    original=dict(parts)
    root=E.fromstring(parts['word/document.xml'])
    rels=E.fromstring(parts['word/_rels/document.xml.rels'])
    ps=root.xpath('//w:body//w:p',namespaces=N)
    changes=[]
    # The current pending picture insertions remain pending. Their old source-picture
    # deletions remain untouched, so Reject All still restores the original image.
    candidates=root.xpath('//w:ins//w:drawing[not(ancestor::w:del)]',namespaces=N)
    assert len(candidates)==2, len(candidates)
    for drawing,name in zip(candidates,['시스템 블록도.png','소프트웨어 구조도.png']):
        rid='rIdSound1Architecture'+str(len(changes)+1)
        media='media/sound1_architecture_'+str(len(changes)+1)+'.png'
        pic=OUT/'이미지 참조'/name
        E.SubElement(rels,'{http://schemas.openxmlformats.org/package/2006/relationships}Relationship',
                     Id=rid,Type=N['r']+'/image',Target=media)
        parts['word/'+media]=pic.read_bytes()
        for blip in drawing.xpath('.//a:blip',namespaces=N): blip.set('{'+N['r']+'}embed',rid)
        extent=drawing.find('.//{'+N['wp']+'}extent')
        width=int(extent.get('cx'))
        with Image.open(pic) as im: height=round(width*im.height/im.width)
        extent.set('cy',str(height))
        for e in drawing.xpath('.//a:xfrm/a:ext',namespaces=N): e.set('cx',str(width));e.set('cy',str(height))
        for prop in drawing.xpath('.//wp:docPr',namespaces=N): prop.set('descr',name[:-4])
        changes.append({'figure':name,'width_emu':width,'height_emu':height})
    replacements={
        '각 MCU 별로 소프트웨어가 개별적으로 실행되게된다. 따라서 각 MCU별로 아키텍처를 설계한다.':
        '세로 영역은 실행 코어를, 가로 영역은 각 코어가 나누어 수행하는 기능 모듈을 나타낸다. 각 코어의 유닛은 공유 메모리, 함수체인 완료 이벤트 및 통신 인터페이스를 통해 연계된다.',
        'CFX에서 공유한 매핑 데이터를 사용하여 C/T 전류크기를 C/T 자극 레벨로 변환하여 CFX에 공유하는 유닛으로 구성되어 있다.':
        'CM3에서 읽은 매핑 데이터의 펄스폭과 C/T 전류크기를 사용하여 DAC 범위, C/T 자극 레벨 및 전송 가능한 자극 수를 계산하고 CFX에 공유하는 유닛으로 구성되어 있다.',
        'QCC를 통해 수신한 갱신 데이터를 처리하고, 이미지 확인 후 다음 기동에 적용한다.':
        'CM3 응용 프로그램이 QCC를 통해 수신한 갱신 데이터를 저장한다. 부트로더는 다음 기동 시 선택된 이미지를 검사하고 적재한다.'}
    ids=[int(x) for x in root.xpath('//@w:id',namespaces=N) if x.isdigit()]
    next_id=max(ids,default=0)+1
    stamp=datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
    for old,new in replacements.items():
        hits=[p for p in ps if visible(p)==old]
        assert len(hits)==1,(old,len(hits))
        p=hits[0]
        existing=p.findall(q('ins'))
        if existing and not p.findall(q('r')):
            # Entire paragraph is already an insertion relative to the source.
            # Keep the insertion identity and replace only its proposed text.
            node=existing[0]
            proto=node.find('.//'+q('rPr'))
            for ch in list(node): node.remove(ch)
            run=E.SubElement(node,q('r'))
            if proto is not None: run.append(deepcopy(proto))
            E.SubElement(run,q('t')).text=new
        else:
            # These source paragraphs are unchanged in the pending document.
            assert not p.xpath('./w:ins|./w:del',namespaces=N),old
            runs=p.findall(q('r'))
            proto=deepcopy(runs[0].find(q('rPr'))) if runs and runs[0].find(q('rPr')) is not None else None
            for ch in list(p):
                if ch.tag!=q('pPr'): p.remove(ch)
            for tag,value in [('del',old),('ins',new)]:
                node=E.SubElement(p,q(tag),{q('id'):str(next_id),q('author'):'김은수',q('date'):stamp})
                next_id+=1
                run=E.SubElement(node,q('r'))
                if proto is not None: run.append(deepcopy(proto))
                E.SubElement(run,q('delText' if tag=='del' else 't')).text=value
        changes.append({'before':old,'after':new})
    parts['word/document.xml']=xml(root)
    parts['word/_rels/document.xml.rels']=xml(rels)
    # Preserve every other existing package part byte-for-byte.
    assert all(parts[k]==v for k,v in original.items() if k not in ['word/document.xml','word/_rels/document.xml.rels'])
    temp=QA/'SAD-candidate.docx'
    with zipfile.ZipFile(temp,'w',zipfile.ZIP_DEFLATED) as z:
        for name,data in parts.items(): z.writestr(name,data)
    shutil.copy2(temp,PATH)
    result={'changes':changes,'before_sha256':hashlib.sha256(backup.read_bytes()).hexdigest(),
            'after_sha256':hashlib.sha256(PATH.read_bytes()).hexdigest(),
            'preserved_existing_package_parts':len(original)-2,
            'revision_count':len(root.xpath('//w:ins|//w:del',namespaces=N))}
    (QA/'patch-result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(result,ensure_ascii=False,indent=2))

if __name__=='__main__': main()
