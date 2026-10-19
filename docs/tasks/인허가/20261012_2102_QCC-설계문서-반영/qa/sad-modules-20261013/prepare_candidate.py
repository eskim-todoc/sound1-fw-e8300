from pathlib import Path
from copy import deepcopy
from datetime import datetime, timezone
import hashlib, json, re, sys, zipfile
from lxml import etree as E

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parent / 'rid83-20261013' / 'word'))
from prepare_word_candidates import NS, q, visible, c14n, reject_new, history
from prepare_narrow_qa import accept_past_in_paragraph

SOURCE = Path(r'G:\공유 드라이브\20_프로젝트별\01_project sound1\08_DHF\03_설계 출력\SW\외부기\[E8300-QCC] SAD-FW-TD24A 소프트웨어 아키텍쳐 기술서.docx')
EXPECTED = '4e71a6b7d696c7e5e21804eff3182f4a9f2f5bb6991e4b441ee6cbf5874119b1'
DATE = datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace('+00:00', 'Z')
TARGETS = [('52838505', 1, 'toc'), ('1A93051C', 2, 'toc'), ('1537B30E', 3, 'toc'), ('14DCD630', 4, 'toc'),
           ('7EEA74BA', 1, 'heading'), ('748B004A', 2, 'heading'), ('03E77286', 3, 'heading'), ('134FFB4F', 4, 'heading')]

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def bh(data): return hashlib.sha256(data).hexdigest()
def raw_tables(data):
    stack, found = [], []
    for m in re.finditer(rb'<w:tbl(?:\s[^>]*|)>|</w:tbl>', data):
        if m.group().startswith(b'</'):
            start = stack.pop(); found.append((start, data[start:m.end()]))
        else: stack.append(m.start())
    assert not stack
    return [data for _, data in sorted(found)]

def write(entries, doc, path):
    data = E.tostring(doc, encoding='UTF-8', xml_declaration=True, standalone=True)
    with zipfile.ZipFile(path, 'w') as z:
        for info, old in entries: z.writestr(info, data if info.filename == 'word/document.xml' else old)
    return data

def main():
    assert sha(SOURCE) == EXPECTED, 'Current G changed'
    before_path = ROOT / 'SAD-before.docx'; before_path.write_bytes(SOURCE.read_bytes())
    with zipfile.ZipFile(before_path) as z:
        entries = [(deepcopy(i), z.read(i.filename)) for i in z.infolist()]
        old_xml = z.read('word/document.xml')
    before = E.fromstring(old_xml); current = deepcopy(before)
    next_id = max(int(v) for v in current.xpath('.//@w:id', namespaces=NS) if v.isdigit()) + 1
    ids, changes = [], []
    def rev(kind):
        nonlocal next_id
        n = E.Element(q(kind)); n.set(q('id'), str(next_id)); n.set(q('author'), '김은수'); n.set(q('date'), DATE)
        ids.append(str(next_id)); next_id += 1; return n
    for pid, number, kind in TARGETS:
        p = current.xpath('//w:p[@w14:paraId="' + pid + '"]', namespaces=NS)[0]
        prefix = f'SI-ID-QCC-{number:03d} '
        before_text = visible(p); assert before_text.count(prefix) == 1
        runs = p.xpath('.//w:r[w:t and not(ancestor::w:del)]', namespaces=NS)
        run = [r for r in runs if prefix in ''.join(r.xpath('w:t/text()', namespaces=NS))]
        assert len(run) == 1 and len(run[0].findall('w:t', NS)) == 1
        run = run[0]; replacement = deepcopy(run)
        text = replacement.find('w:t', NS); text.text = text.text.replace(prefix, '', 1)
        parent = run.getparent(); deletion = rev('del'); parent.replace(run, deletion); deletion.append(run)
        for t in run.iter(q('t')): t.tag = q('delText')
        insertion = rev('ins'); insertion.append(replacement)
        if parent.tag == q('ins'):
            # Keep the new heading inside the existing bookmark start/end span.
            p.insert(p.index(parent), insertion)
        else:
            assert parent.tag == q('p'); parent.insert(parent.index(deletion) + 1, insertion)
        after_text = visible(p); assert after_text == before_text.replace(prefix, '', 1)
        changes.append({'paraId': pid, 'kind': kind, 'before': before_text, 'after': after_text, 'deleted_prefix': prefix})
    rejected = reject_new(current, set(ids)); assert c14n(rejected) == c14n(before)
    assert history(current, set(ids)) == history(before)
    changed_pids = {pid for pid, _, _ in TARGETS}
    before_ps = before.xpath('//w:p', namespaces=NS); after_ps = current.xpath('//w:p', namespaces=NS)
    assert len(before_ps) == len(after_ps)
    for a, b in zip(before_ps, after_ps):
        pid = a.get('{http://schemas.microsoft.com/office/word/2010/wordml}paraId')
        if pid not in changed_pids: assert c14n(a) == c14n(b)
        assert c14n(a.find('w:pPr', NS)) == c14n(b.find('w:pPr', NS)) if a.find('w:pPr', NS) is not None else b.find('w:pPr', NS) is None
    for expression in ['//w:instrText', '//w:fldChar', '//w:bookmarkStart', '//w:bookmarkEnd']:
        assert [c14n(e) for e in before.xpath(expression, namespaces=NS)] == [c14n(e) for e in current.xpath(expression, namespaces=NS)]
    candidate = ROOT / 'SAD-candidate.docx'; new_xml = write(entries, current, candidate)
    old_tables, new_tables = raw_tables(old_xml), raw_tables(new_xml)
    assert old_tables == new_tables
    tables = []
    for ordinal in range(1, len(old_tables)+1):
        assert old_tables[ordinal-1] == new_tables[ordinal-1]
        tables.append({'stored_table_ordinal': ordinal, 'raw_XML_before_sha256': bh(old_tables[ordinal-1]),
                       'raw_XML_candidate_sha256': bh(new_tables[ordinal-1]), 'raw_XML_byte_identical': True})
    with zipfile.ZipFile(candidate) as z:
        for info, data in entries:
            if info.filename != 'word/document.xml': assert z.read(info.filename) == data
            if info.filename.endswith(('.xml', '.rels')): E.fromstring(z.read(info.filename))
    # One minimal main-story QA projection containing the eight original paragraphs.
    body = current.find('w:body', NS); qa = deepcopy(current); qb = qa.find('w:body', NS)
    section = deepcopy(body.find('w:sectPr', NS)); qa_before = deepcopy(qa)
    for b in list(qb): qb.remove(b)
    expected_accept, expected_reject = '', ''
    controls = []
    for pid, _, _ in TARGETS:
        path = '//w:p[@w14:paraId="' + pid + '"]'
        new_p = accept_past_in_paragraph(current.xpath(path, namespaces=NS)[0], set(ids))
        old_p = accept_past_in_paragraph(before.xpath(path, namespaces=NS)[0], set())
        assert c14n(reject_new(new_p, set(ids))) == c14n(old_p)
        expected_accept += visible(new_p); expected_reject += visible(old_p)
        qb.append(new_p); controls.append(old_p)
    if section is not None: qb.append(section)
    cb = qa_before.find('w:body', NS)
    for b in list(cb): cb.remove(b)
    for p in controls: cb.append(p)
    if section is not None: cb.append(deepcopy(section))
    qa_path = ROOT / 'SAD-new-only-qa.docx'; control_path = ROOT / 'SAD-before-control-qa.docx'
    write(entries, qa, qa_path); write(entries, qa_before, control_path)
    assert len(qa.find('w:body', NS).xpath('.//*[@w:author]', namespaces=NS)) == 16
    assert sha(SOURCE) == EXPECTED
    authored = {'code': 'SAD', 'source': str(SOURCE), 'candidate': str(candidate), 'before': str(before_path),
                'source_sha256': EXPECTED, 'candidate_sha256': sha(candidate), 'changed_paragraphs': changes,
                'new_revision_ids': ids, 'new_revision_count': 16, 'new_revision_author': '김은수', 'new_revision_date': DATE,
                'saved_tables': tables, 'checks': {'new_only_reject_document_c14n_exact': True,
                'existing_revision_text_author_date_preserved': True, 'only_8_prefix_deletions': True,
                'all_other_paragraphs_pPr_styles_images_fields_links_page_numbers_preserved': True,
                'all_other_ZIP_parts_byte_identical': True, 'all_tables_raw_XML_byte_identical': old_tables == new_tables,
                'stored_table_count': len(old_tables),
                'trace_module_unit_IDs_unchanged': True, 'G_source_unchanged': True},
                'native_status': '미실행: root serialized Word/PDF/PNG and narrow QA acceptance/rejection pending'}
    (ROOT / 'authored.json').write_text(json.dumps(authored, ensure_ascii=False, indent=2), encoding='utf-8')
    narrow = {'code': 'SAD', 'original_candidate': str(candidate), 'original_candidate_sha256': sha(candidate),
              'qa_new_only': str(qa_path), 'qa_new_only_sha256': sha(qa_path), 'qa_before_control': str(control_path),
              'qa_before_control_sha256': sha(control_path), 'expected_accept': expected_accept,
              'expected_reject': expected_reject, 'new_revision_ids': ids, 'new_revision_count': 16,
              'main_story_paragraph_count': 8, 'new_only_reject_xml_exact_before_control': True,
              'scope': 'Eight original heading/TOC paragraphs; only their past histories accepted. Native AcceptAll/RejectAll verifies remaining new revisions. Full candidate separately retains all history and exact XML new-only rejection; direct selective full-candidate COM rejection is not claimed.'}
    (ROOT / 'narrow-proof.json').write_text(json.dumps(narrow, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'candidate': str(candidate), 'source_sha256': EXPECTED, 'candidate_sha256': sha(candidate),
                      'changed_paragraph_count': 8, 'new_revision_count': 16, 'trace_tables_byte_identical': True}, ensure_ascii=True))

if __name__ == '__main__': main()
