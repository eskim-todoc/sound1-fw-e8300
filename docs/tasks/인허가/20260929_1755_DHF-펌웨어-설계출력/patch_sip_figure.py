"""SIP 그림만 SAD의 확인된 공통 구조도로 교체한다."""
from copy import deepcopy
from pathlib import Path
import hashlib
import json
import zipfile
from lxml import etree as E
from PIL import Image
from patch_sad_architecture import N, q, xml, OUT, T


def main():
    path = OUT / 'SIP-FW-TD24A 소프트웨어 통합 계획서.docx'
    qa = T / 'qa/sip-architecture'
    qa.mkdir(parents=True, exist_ok=True)
    backup = qa / 'before.docx'
    assert not backup.exists(), '기준 사본을 덮어쓰지 않는다.'
    raw = path.read_bytes()
    backup.write_bytes(raw)
    with zipfile.ZipFile(path) as z:
        parts = {n: z.read(n) for n in z.namelist()}
    original = dict(parts)
    root = E.fromstring(parts['word/document.xml'])
    before = deepcopy(root)
    drawings = root.xpath('//w:drawing[not(ancestor::w:del)]', namespaces=N)
    assert len(drawings) == 1
    drawing = drawings[0]
    assert drawing.xpath('ancestor::w:ins', namespaces=N)
    picture = OUT / '이미지 참조/소프트웨어 구조도.png'
    data = picture.read_bytes()
    # 로컬 그림이 현재 SAD에 들어 있는 이미지와 정확히 같은지 확인한다.
    with zipfile.ZipFile(OUT / 'SAD-FW-TD24A 소프트웨어 아키텍쳐 기술서.docx') as z:
        sad = E.fromstring(z.read('word/document.xml'))
        rels = E.fromstring(z.read('word/_rels/document.xml.rels'))
        links = {x.get('Id'): x.get('Target') for x in rels}
        ids = sad.xpath('//w:drawing[not(ancestor::w:del)]//a:blip/@r:embed', namespaces=N)
        assert any(z.read('word/' + links[rid]) == data for rid in ids)
    rels = E.fromstring(parts['word/_rels/document.xml.rels'])
    rid = 'rIdSound1SipArchitecture'
    media = 'media/sound1_sip_architecture.png'
    assert not any(x.get('Id') == rid for x in rels)
    E.SubElement(rels, '{http://schemas.openxmlformats.org/package/2006/relationships}Relationship',
                 Id=rid, Type=N['r'] + '/image', Target=media)
    parts['word/' + media] = data
    for blip in drawing.xpath('.//a:blip', namespaces=N):
        blip.set('{' + N['r'] + '}embed', rid)
    extent = drawing.find('.//{' + N['wp'] + '}extent')
    width = int(extent.get('cx'))
    with Image.open(picture) as im:
        height = round(width * im.height / im.width)
    extent.set('cy', str(height))
    for ext in drawing.xpath('.//a:xfrm/a:ext', namespaces=N):
        ext.set('cx', str(width)); ext.set('cy', str(height))
    for prop in drawing.xpath('.//wp:docPr', namespaces=N):
        prop.set('descr', 'SAD와 동일한 Sound1 코어별 실행 위치 및 공통 기능 모듈 구조도')
    # 그림 외 문서 XML이 그대로인지 확인한다.
    comparison = deepcopy(root)
    target = comparison.xpath('//w:drawing[not(ancestor::w:del)]', namespaces=N)[0]
    target.getparent().replace(target, deepcopy(before.xpath('//w:drawing[not(ancestor::w:del)]', namespaces=N)[0]))
    assert E.tostring(comparison) == E.tostring(before)
    parts['word/document.xml'] = xml(root)
    parts['word/_rels/document.xml.rels'] = xml(rels)
    assert all(parts[n] == value for n, value in original.items()
               if n not in ('word/document.xml', 'word/_rels/document.xml.rels'))
    assert E.fromstring(parts['word/settings.xml']).find(q('trackRevisions')) is not None
    candidate = qa / 'candidate.docx'
    with zipfile.ZipFile(candidate, 'w', zipfile.ZIP_DEFLATED) as z:
        for name, value in parts.items():
            z.writestr(name, value)
    assert path.read_bytes() == raw, '편집 중 사용자 변경 감지'
    path.write_bytes(candidate.read_bytes())
    result = {'SAD_image_identical': True, 'only_visible_drawing_changed': True,
              'original_deleted_images_preserved': True, 'tracking_enabled': True,
              'before_sha256': hashlib.sha256(raw).hexdigest(),
              'after_sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
    (qa / 'checks.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
