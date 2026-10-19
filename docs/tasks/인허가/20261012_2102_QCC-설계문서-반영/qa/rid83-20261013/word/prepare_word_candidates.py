from pathlib import Path
from copy import deepcopy
from datetime import datetime, timezone
import hashlib
import json
import zipfile
from lxml import etree as E

ROOT = Path(__file__).resolve().parent
G = Path(r'G:\공유 드라이브\20_프로젝트별\01_project sound1\08_DHF')
W = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'
W14 = 'http://schemas.microsoft.com/office/word/2010/wordml'
NS = {'w': W, 'w14': W14}
q = lambda name: '{' + W + '}' + name
STAMP = datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace('+00:00', 'Z')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def visible(element):
    if element.tag in [q('del'), q('moveFrom'), q('pPrChange'), q('rPrChange')]:
        return ''
    if element.tag == q('tr') and element.find('w:trPr/w:del', NS) is not None:
        return ''
    if element.tag == q('t'):
        return element.text or ''
    if element.tag in [q('br'), q('cr')]:
        return '\n'
    if element.tag == q('tab'):
        return '\t'
    content = ''.join(visible(child) for child in element)
    return content + ('\n' if element.tag == q('p') else '')


def c14n(element):
    return E.tostring(element, method='c14n')


def reject_new(element, new_ids):
    result = deepcopy(element)
    for revision in list(result.xpath('.//w:ins | .//w:del', namespaces=NS))[::-1]:
        if revision.get(q('id')) not in new_ids:
            continue
        parent = revision.getparent()
        offset = parent.index(revision)
        if revision.tag == q('del'):
            for child in list(revision):
                for text in child.iter(q('delText')):
                    text.tag = q('t')
                revision.remove(child)
                parent.insert(offset, child)
                offset += 1
        parent.remove(revision)
    return result


def history(element, excluded=()):
    records = {}
    for revision in element.xpath('.//*[@w:author]', namespaces=NS):
        rid = revision.get(q('id'))
        if rid in excluded:
            continue
        text = ''.join(revision.xpath('.//w:t/text() | .//w:delText/text()', namespaces=NS))
        records[rid] = {'type': E.QName(revision).localname, 'author': revision.get(q('author')),
                        'date': revision.get(q('date')), 'text': text}
    return records


def make(code, stage, filename, baseline_hash, para_id, old, new):
    source = G / stage / 'SW' / '외부기' / filename
    assert sha(source) == baseline_hash, 'Current G baseline changed: ' + code
    snapshot = ROOT / 'backups' / filename
    snapshot.parent.mkdir(exist_ok=True)
    snapshot.write_bytes(source.read_bytes())
    assert sha(snapshot) == baseline_hash
    with zipfile.ZipFile(snapshot) as z:
        entries = [(deepcopy(item), z.read(item.filename)) for item in z.infolist()]
    parts = {item.filename: data for item, data in entries}
    original = E.fromstring(parts['word/document.xml'])
    current = deepcopy(original)
    settings = E.fromstring(parts['word/settings.xml'])
    tracked = settings.find('w:trackRevisions', NS)
    assert tracked is not None and tracked.get(q('val'), 'true') not in ['0', 'false', 'off']
    paragraph = current.xpath('//w:p[@w14:paraId="' + para_id + '"]', namespaces=NS)
    assert len(paragraph) == 1
    paragraph = paragraph[0]
    assert visible(paragraph).count(old) == 1
    next_id = max(int(item) for item in current.xpath('.//@w:id', namespaces=NS) if item.isdigit()) + 1
    new_ids = []

    def revision(kind):
        nonlocal next_id
        rev = E.Element(q(kind))
        rev.set(q('id'), str(next_id))
        rev.set(q('author'), '김은수')
        rev.set(q('date'), STAMP)
        new_ids.append(str(next_id))
        next_id += 1
        return rev

    if code == 'SR':
        # One saved run holds the three sentences. Retain that exact run inside a
        # new deletion so rejecting only this edit restores its original XML.
        runs = paragraph.xpath('.//w:r[w:t and not(ancestor::w:del)]', namespaces=NS)
        found = [run for run in runs if old in ''.join(run.xpath('w:t/text()', namespaces=NS))]
        assert len(found) == 1 and len(found[0].findall('w:t', NS)) == 1
        run = found[0]
        parent = run.getparent()
        assert parent.tag == q('ins')
        replacement = deepcopy(run)
        replacement.find('w:t', NS).text = replacement.find('w:t', NS).text.replace(old, new)
        deletion = revision('del')
        parent.replace(run, deletion)
        deletion.append(run)
        for text in run.iter(q('t')):
            text.tag = q('delText')
        insertion = revision('ins')
        insertion.append(replacement)
        paragraph.insert(paragraph.index(parent) + 1, insertion)
        replacement_unit = 'Saved single run; accepted text changes only the RID83 sentence'
    else:
        # Delete the upper-ID sentence and its own following line break. The
        # retained product-document text and blue notice runs are untouched.
        insertions = paragraph.findall('w:ins', NS)
        found = [ins for ins in insertions if visible(ins) == old + '\n']
        assert len(found) == 1
        parent = found[0]
        assert all(child.tag == q('r') for child in parent)
        deletion = revision('del')
        for child in list(parent):
            parent.remove(child)
            deletion.append(child)
            for text in child.iter(q('t')):
                text.tag = q('delText')
        parent.append(deletion)
        replacement_unit = 'Only upper-ID sentence and its attached line break'

    rejected = reject_new(current, set(new_ids))
    exact_reject = c14n(rejected) == c14n(original)
    assert exact_reject, 'New-only reject does not exactly restore ' + code
    before_text = visible(original)
    after_text = visible(current)
    delete_text = old if code == 'SR' else old + '\n'
    expected_text = before_text.replace(delete_text, new, 1)
    assert after_text == expected_text
    assert 'ACDOC' not in after_text
    assert history(current, set(new_ids)) == history(original)
    new_revision_nodes = [node for node in current.xpath('.//*[@w:author]', namespaces=NS)
                          if node.get(q('id')) in new_ids]
    assert len(new_revision_nodes) == len(new_ids)
    assert all(node.get(q('author')) == '김은수' for node in new_revision_nodes)
    assert not any(node.tag == q('ins') and any(a.tag in [q('ins'), q('del')] for a in node.iterancestors())
                   for node in new_revision_nodes)
    assert len(original.xpath('//w:p', namespaces=NS)) == len(current.xpath('//w:p', namespaces=NS))
    assert len(original.xpath('//w:tr', namespaces=NS)) == len(current.xpath('//w:tr', namespaces=NS))
    assert [c14n(node) for node in original.xpath('//w:pPr', namespaces=NS)] == [c14n(node) for node in current.xpath('//w:pPr', namespaces=NS)]
    candidate = ROOT / 'candidates' / filename
    candidate.parent.mkdir(exist_ok=True)
    xml = E.tostring(current, encoding='UTF-8', xml_declaration=True, standalone=True)
    with zipfile.ZipFile(candidate, 'w') as z:
        for info, data in entries:
            z.writestr(info, xml if info.filename == 'word/document.xml' else data)
    with zipfile.ZipFile(candidate) as z:
        assert len(z.infolist()) == len(entries)
        for info, data in entries:
            if info.filename != 'word/document.xml':
                assert z.read(info.filename) == data
            if info.filename.endswith(('.xml', '.rels')):
                E.fromstring(z.read(info.filename))
    (ROOT / (code + '-before.xml')).write_bytes(E.tostring(original, encoding='UTF-8', xml_declaration=True))
    (ROOT / (code + '-new-rejected.xml')).write_bytes(E.tostring(rejected, encoding='UTF-8', xml_declaration=True))
    (ROOT / (code + '-candidate-current.txt')).write_text(after_text, encoding='utf-8')
    proof = {'code': code, 'source': str(source), 'before_backup': str(snapshot), 'candidate': str(candidate),
             'source_sha256': baseline_hash, 'candidate_sha256': sha(candidate), 'paraId': para_id,
             'before_current_paragraph': visible(original.xpath('//w:p[@w14:paraId="' + para_id + '"]', namespaces=NS)[0]),
             'after_current_paragraph': visible(paragraph), 'old': old, 'new': new,
             'new_revision_ids': new_ids, 'new_revision_types': [E.QName(n).localname for n in new_revision_nodes],
             'new_revision_author': '김은수', 'new_revision_date': STAMP, 'replacement_unit': replacement_unit,
             'checks': {'new_only_reject_c14n_exact_before': exact_reject, 'only_planned_current_text_difference': True,
                        'active_acdoc_count': after_text.count('ACDOC'), 'existing_revision_text_author_date_preserved': True,
                        'all_other_package_parts_byte_identical': True, 'all_styles_numbering_images_fields_relationships_preserved': True,
                        'paragraph_properties_byte_semantically_unchanged': True, 'paragraph_table_row_counts_unchanged': True,
                        'new_insertions_not_nested': True, 'G_source_unchanged': sha(source) == baseline_hash},
             'retained_notice': 'Existing [E8300 보완] product manual content / distribution target / review result notice unchanged',
             'native_open_accept_reject_render': '미실행: root serialized native validation pending',
             'publication': 'Local candidate only; G unchanged', 'status': '통과',
             'status_scope': 'Saved package, active XML and exact new-only rejection verification only'}
    (ROOT / (code + '-proof.json')).write_text(json.dumps(proof, ensure_ascii=False, indent=2), encoding='utf-8')
    return proof


def main():
    sr_old = '사용 설명서 요구 SW-FW-RID-83은 SYS-RID-ACDOC-17, SYS-RID-ACDOC-18 및 SYS-RID-ACDOC-20의 경고/안전사항, 안전한 작동법 및 동작방법에 연결한다.'
    sr_new = '사용 설명서 요구 SW-FW-RID-83은 경고/안전사항, 안전한 작동법 및 동작방법을 정의한 자체 소프트웨어 요구사항이다.'
    sad_old = '상위 요구사항: SYS-RID-ACDOC-17 / SYS-RID-ACDOC-18 / SYS-RID-ACDOC-20.'
    results = [
        make('SR', '02_설계 입력', '[E8300-QCC] SR-FW-TD24A 소프트웨어 요구사항.docx',
             '975c9f5389f20fc02cc483265d038eb85f950f50abdda4d733b5695978656895', '545E39EF', sr_old, sr_new),
        make('SAD', '03_설계 출력', '[E8300-QCC] SAD-FW-TD24A 소프트웨어 아키텍쳐 기술서.docx',
             '84f592a7f7988a78eb73fa718268cc22ddf20a5bac796358b971aba0d23ccc68', '32A0D308', sad_old, '')]
    before_cells = ROOT.parents[1] / 'review-20261013' / 'final-word' / 'SAD-native-accepted-cells.json'
    cells = json.loads(before_cells.read_text(encoding='utf-8-sig'))
    assert cells['pass'] and cells['row_count'] == 83 and cells['column_count'] == 3
    expected_rows = deepcopy(cells['rows'])
    row = [r for r in expected_rows if r['id'] == 'SW-FW-RID-83']
    assert len(row) == 1 and row[0]['cells'][2].count(sad_old + '\n') == 1
    row[0]['cells'][2] = row[0]['cells'][2].replace(sad_old + '\n', '', 1)
    expected = {'baseline_native_proof': str(before_cells), 'source_sha256': results[1]['source_sha256'],
                'candidate_sha256': results[1]['candidate_sha256'], 'row_count': 83, 'column_count': 3,
                'rows': expected_rows, 'change': 'RID83 third cell removes only the ACDOC upper-ID sentence and line break',
                'actual_native_status': '미실행: root compares actual accepted Word cells'}
    (ROOT / 'SAD-expected-native-cells.json').write_text(json.dumps(expected, ensure_ascii=False, indent=2), encoding='utf-8')
    (ROOT / 'authored.json').write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(results, ensure_ascii=True))


if __name__ == '__main__':
    main()
