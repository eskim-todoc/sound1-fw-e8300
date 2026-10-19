"""추적성 표 내용, 변경 추적 복원 및 범위 밖 본문 보존을 독립 검사한다."""
import json
import re
import zipfile
from pathlib import Path
from lxml import etree as E
import pypdfium2 as pdfium
from inspect_sad_unit_trace import QA, blocks, text, q, N, sha


def xml(path, part='word/document.xml'):
    with zipfile.ZipFile(path) as z:
        return E.fromstring(z.read(part))


def trace(doc_blocks):
    return next(b for b in doc_blocks if b['type'] == 'table' and
                any('SW-FW-RID-81' in cell for row in b['rows'] for cell in row))


def outside(doc_blocks):
    table = trace(doc_blocks)
    return [('p', b['text']) if b['type'] == 'p' else ('table', b['rows'])
            for b in doc_blocks if b is not table and
            (b['type'] != 'p' or (b['text'].strip() and
             not b['text'].startswith(('소프트웨어 요구사항을 담당 모듈과 상세 설계 유닛에 연결한다.',
                                      '표 1. 소프트웨어 요구사항과 상세 설계 유닛 연결'))))]


def package_parts(path, prefix):
    with zipfile.ZipFile(path) as z:
        return {name: z.read(name) for name in z.namelist() if name.startswith(prefix)}


def semantic(path):
    # Word가 정규화하는 rsid/개체 ID 대신 거절 후의 본문/표 내용을 비교한다.
    return [('p', b['text']) if b['type'] == 'p' else ('table', b['rows'])
            for b in blocks(path) if b['type'] != 'p' or b['text'].strip()]


def verify():
    inputs = json.loads((QA / 'inputs.json').read_text(encoding='utf8'))
    plan = json.loads((QA / 'trace-map.json').read_text(encoding='utf8'))
    before = blocks(QA / 'before-accepted.docx')
    after = blocks(QA / 'candidate-accepted.docx')
    table = trace(after)
    native_table = next(t for t in xml(QA / 'candidate-accepted.docx').findall('w:body/w:tbl', N) if 'SW-FW-RID-81' in text(t))
    def cell_text(cell):
        return '\n'.join(''.join(n.text or '' if n.tag == q('t') else '\n' for n in p.iter() if n.tag in [q('t'), q('br')]) for p in cell.findall('w:p', N))
    table = {'rows': [[cell_text(c) for c in r.findall('w:tc', N)] for r in native_table.findall('w:tr', N)]}
    assert len(table['rows']) == 82
    assert all(len(row) == 3 for row in table['rows'])
    ids = [row[0] for row in table['rows'][1:]]
    assert ids == [f'SW-FW-RID-{n}' for n in range(1, 82)]
    known = {u['id']: u for u in inputs['units']}
    count = 0
    for expected, actual in zip(plan, table['rows'][1:]):
        assert actual[0] == expected['id']
        module_text = actual[1].replace('\n', ' ')
        for module in expected['modules']:
            assert module.replace(', ', ' ', 1) in module_text, (actual[0], module)
        found = re.findall(r'SU-ID-[A-Z0-9]+-\d+', actual[2])
        assert found == [u['id'] for u in expected['units']], actual[0]
        for u in expected['units']:
            assert u['name'] == known[u['id']]['name']
            assert u['id'] + '\n' + u['name'] in actual[2], u
        for note in expected['notes']:
            assert note in actual[2]
        count += len(found)
    assert outside(before) == outside(after), '추적성 표 외의 본문 변경'
    assert semantic(QA / 'before-rejected.docx') == semantic(QA / 'candidate-rejected.docx'), '변경 거절 시 원본 불일치'
    assert package_parts(QA / 'before.docx', 'word/media/') == package_parts(QA / 'candidate.docx', 'word/media/')
    for prefix in ['word/header', 'word/footer']:
        bparts = package_parts(QA / 'before.docx', prefix)
        aparts = package_parts(QA / 'candidate.docx', prefix)
        assert set(bparts) == set(aparts)
        for part in bparts:
            if part.endswith('.xml'):
                assert text(E.fromstring(bparts[part])) == text(E.fromstring(aparts[part]))
    before_heads = json.loads((QA / 'before-headings.json').read_text(encoding='utf-8-sig'))
    after_heads = json.loads((QA / 'candidate-headings.json').read_text(encoding='utf-8-sig'))
    assert [(h['text'], h['number'], h['style']) for h in before_heads] == [(h['text'], h['number'], h['style']) for h in after_heads]
    original_styles = xml(QA / 'before.docx', 'word/styles.xml')
    candidate_styles = xml(QA / 'candidate.docx', 'word/styles.xml')
    def style_spec(root):
        return {s.get(q('styleId')): E.tostring(s, method='c14n') for s in root.findall('w:style', N)
                if s.find('w:name', N) is not None and s.find('w:name', N).get(q('val')).startswith('TDC ')}
    # 별도 데이터에 보존 검토 결과를 남기며 스타일 속성은 의미 비교한다.
    def strip_ids(root):
        for node in root.iter():
            for key in list(node.attrib):
                if 'rsid' in key.lower(): del node.attrib[key]
    strip_ids(original_styles); strip_ids(candidate_styles)
    assert style_spec(original_styles) == style_spec(candidate_styles), '공통 TDC 스타일 변경'
    settings = xml(QA / 'candidate.docx', 'word/settings.xml')
    assert settings.find('w:trackRevisions', N) is not None
    raw = xml(QA / 'candidate.docx')
    revisions = raw.xpath('//w:ins|//w:del', namespaces=N)
    assert revisions
    accepted = xml(QA / 'candidate-accepted.docx')
    t = next(t for t in accepted.findall('w:body/w:tbl', N) if 'SW-FW-RID-81' in text(t))
    assert t.find('w:tr/w:trPr/w:tblHeader', N) is not None
    assert all(r.find('w:trPr/w:cantSplit', N) is not None for r in t.findall('w:tr', N))
    pending_runs = [r for r in t.findall('.//w:r', N) if any(s in text(r) for s in ['추가 예정', '연결 필요'])]
    assert pending_runs
    assert all(r.find('w:rPr/w:color', N).get(q('val')) == 'C00000' and r.find('w:rPr/w:b', N) is not None for r in pending_runs)
    assert all(sha(Path(p)) == h for p, h in inputs['sources'].items()), '검토 중 입력 파일 변경'
    result = {'requirements': len(ids), 'unit_links': count, 'unique_units': len({u['id'] for row in plan for u in row['units']}),
              'outside_body_unchanged': True, 'all_revisions_reject_to_same_baseline': True,
              'images_headers_footers_preserved': True, 'tdc_styles_and_heading_numbers_preserved': True,
              'tracked_changes_enabled': True, 'qcc_and_design_pending_red_bold': True,
              'source_hashes_unchanged': True}
    (QA / 'verification.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf8')
    print(json.dumps(result, ensure_ascii=False))


def render():
    folder = QA / 'pages'
    folder.mkdir(exist_ok=True)
    previous = {p.name: sha(p) for p in folder.glob('page-*.png')}
    pdf = pdfium.PdfDocument(QA / 'candidate.pdf')
    for i in range(len(pdf)):
        page = pdf[i]
        bitmap = page.render(scale=1.5)
        bitmap.to_pil().save(folder / f'page-{i + 1:02}.png')
        bitmap.close(); page.close()
    print('rendered pages=' + str(len(pdf)))
    changed = [p.name for p in folder.glob('page-*.png') if previous.get(p.name) != sha(p)]
    print('changed page images=' + str(changed))
    (QA / 'last-render.json').write_text(json.dumps({'pages': len(pdf), 'changed_images': changed}, indent=2), encoding='utf8')
    pdf.close()


if __name__ == '__main__':
    verify()
    render()
