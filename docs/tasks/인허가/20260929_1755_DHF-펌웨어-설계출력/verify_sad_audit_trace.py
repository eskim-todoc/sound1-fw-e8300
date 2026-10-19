"""저장된 문서의 유닛 연결/서식/원본 복원과 렌더 결과를 대조한다."""
import json
import re
from pathlib import Path

from lxml import etree as E
import pypdfium2 as pdfium
from PIL import Image

from patch_sad_audit_trace import QA, PREVIOUS, PREFIX, parts, current_rows
from inspect_sad_unit_trace import q, N, text, sha, blocks


def cell_text(cell):
    return '\n'.join(''.join((n.text or '') if n.tag == q('t') else '\n'
                            for n in p.iter() if n.tag in (q('t'), q('br')))
                     for p in cell.findall('w:p', N))


def semantic(path):
    return [('p', b['text']) if b['type'] == 'p' else ('table', b['rows'])
            for b in blocks(path) if b['type'] != 'p' or b['text'].strip()]


def main():
    old, new = parts(QA/'before.docx'), parts(QA/'candidate.docx')
    assert set(old) == set(new)
    assert [k for k in old if old[k] != new[k]] == ['word/document.xml']
    raw = E.fromstring(new['word/document.xml'])
    prior_raw = E.fromstring(old['word/document.xml'])
    # 원문의 삭제 이력을 잃지 않고 삽입 안의 수정은 기존 작성자의 변경으로 남긴다.
    assert [E.tostring(x) for x in raw.findall('.//w:del', N)] == [E.tostring(x) for x in prior_raw.findall('.//w:del', N)]
    assert semantic(QA/'candidate-rejected.docx') == semantic(PREVIOUS/'candidate-rejected.docx')
    before = E.fromstring(parts(PREVIOUS/'candidate-accepted.docx')['word/document.xml'])
    accepted = E.fromstring(parts(QA/'candidate-accepted.docx')['word/document.xml'])
    rows = current_rows(accepted)
    assert list(rows) == [f'SW-FW-RID-{n}' for n in range(1,82)]
    known = {u['id']: u for u in json.loads((PREVIOUS/'inputs.json').read_text(encoding='utf8'))['units']}
    plan = json.loads((PREVIOUS/'trace-map.json').read_text(encoding='utf8'))
    found_all = []
    expected = {50: ['SU-ID-CM3-001','SU-ID-CM3-046'],
                74: ['SU-ID-CM3-001','SU-ID-CM3-046'], 79: ['SU-ID-CM3-005']}
    for item in plan:
        row = rows[item['id']]
        cells = row.findall('w:tc', N)
        assert len(cells) == 3
        values = [cell_text(c) for c in cells]
        found = re.findall(r'SU-ID-[A-Z0-9]+-\d+', values[2])
        rid = int(item['id'].split('-')[-1])
        assert found == expected.get(rid,[u['id'] for u in item['units']]), item['id']
        for uid in found:
            unit = known[uid]
            assert uid+'\n'+unit['name'] in values[2], uid
            assert unit['module'].split(',')[0] in values[1], (item['id'], uid)
        found_all.extend(found)
    # 표 밖 내용은 QCC 입력 안내 세 문단만 바뀐다.
    changes = json.loads((QA/'changes.json').read_text(encoding='utf8'))
    allowed = {x['before']:x['after'].replace('\n','') for x in changes if 'id' not in x}
    def outside(root):
        return [text(p) for p in root.findall('w:body/w:p',N) if text(p)]
    assert [allowed.get(s,s) for s in outside(before)] == outside(accepted)
    p_qcc = [p for p in accepted.findall('.//w:p',N) if PREFIX in text(p)]
    assert len(p_qcc)==29
    for p in p_qcc:
        selected = False
        for r in p.findall('w:r',N):
            if PREFIX in text(r): selected=True
            if selected and (text(r) or r.find('w:br',N) is not None):
                assert r.find('w:rPr/w:color',N).get(q('val'))=='C00000'
                assert r.find('w:rPr/w:sz',N).get(q('val'))=='24'
                assert r.find('w:rPr/w:b',N) is not None
                assert r.find('w:rPr/w:u',N).get(q('val'))=='single'
    e_notes = [p for p in accepted.findall('.//w:p',N) if text(p).startswith('[E8300')]
    assert len(e_notes)==3
    for p in e_notes:
        for r in p.findall('w:r',N):
            if not text(r): continue
            assert r.find('w:rPr/w:color',N).get(q('val'))=='0070C0'
            assert r.find('w:rPr/w:b',N) is not None
    before_h = json.loads((PREVIOUS/'candidate-headings.json').read_text(encoding='utf-8-sig'))
    after_h = json.loads((QA/'candidate-headings.json').read_text(encoding='utf-8-sig'))
    assert [(h['text'],h['number'],h['style']) for h in before_h] == [(h['text'],h['number'],h['style']) for h in after_h]
    inputs = json.loads((QA/'sources.json').read_text(encoding='utf8'))
    assert all(sha(Path(p))==s for p,s in inputs.items())
    out = QA/'pages'
    out.mkdir(exist_ok=True)
    doc = pdfium.PdfDocument(QA/'candidate.pdf')
    different=[]; same=[]
    for i in range(len(doc)):
        page = doc[i]; bitmap=page.render(scale=1.5)
        dest=out/f'page-{i+1:02}.png'; bitmap.to_pil().save(dest)
        bitmap.close(); page.close()
        previous=PREVIOUS/'pages'/dest.name
        if previous.exists() and sha(previous)==sha(dest):same.append(i+1)
        else:different.append(i+1)
    pages=len(doc); doc.close()
    # 동등 페이지는 직전 검토 근거를 재사용하고 바뀐 페이지는 원본 크기로 확인한다.
    for start in range(0,len(different),4):
        imgs=[Image.open(out/f'page-{i:02}.png').convert('RGB') for i in different[start:start+4]]
        w,h=imgs[0].size
        sheet=Image.new('RGB',(w*2,h*2),'#cccccc')
        for k,im in enumerate(imgs):sheet.paste(im,((k%2)*w,(k//2)*h))
        sheet.save(out/f'review-{start//4+1:02}.png')
    result={'requirements':81,'unit_links':len(found_all),'unique_units':len(set(found_all)),
        'qcc_prefix_count':29,'e8300_blue_notes':3,'new_unit_ids':0,
        'all_rejected_same_baseline':True,'other_package_parts_unchanged':True,
        'untouched_body_and_heading_numbers_preserved':True,'sources_unchanged':True,
        'pages':pages,'identical_pages':same,'pages_to_review':different}
    (QA/'verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(result,ensure_ascii=False))


if __name__ == '__main__': main()
