from pathlib import Path
from copy import deepcopy
import hashlib
import json
import zipfile
from lxml import etree as E
from prepare_word_candidates import NS, q, visible, c14n, reject_new

ROOT = Path(__file__).resolve().parent


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def accept_past_in_paragraph(paragraph, retained_ids):
    paragraph = deepcopy(paragraph)
    for change in list(paragraph.xpath('.//w:pPrChange | .//w:rPrChange', namespaces=NS)):
        if change.get(q('id')) not in retained_ids:
            change.getparent().remove(change)
    for deletion in list(paragraph.xpath('.//w:del', namespaces=NS)):
        if deletion.get(q('id')) not in retained_ids and deletion.getparent() is not None:
            assert deletion.getparent().tag not in [q('rPr'), q('trPr')], 'Target has paragraph/row deletion mark'
            deletion.getparent().remove(deletion)
    for insertion in list(paragraph.xpath('.//w:ins', namespaces=NS))[::-1]:
        if insertion.get(q('id')) in retained_ids or insertion.getparent() is None:
            continue
        parent = insertion.getparent()
        position = parent.index(insertion)
        for child in list(insertion):
            insertion.remove(child)
            parent.insert(position, child)
            position += 1
        parent.remove(insertion)
    return paragraph


def write_narrow(entries, doc, paragraph, path):
    tree = deepcopy(doc)
    body = tree.find('w:body', NS)
    section = deepcopy(body.find('w:sectPr', NS))
    for child in list(body):
        body.remove(child)
    body.append(deepcopy(paragraph))
    if section is not None:
        body.append(section)
    # Retain the original paragraph/style/section properties as a QA control.
    # Headers may retain their old history; the tested main-story range has only
    # this single paragraph. This document is never a shipping candidate.
    data = E.tostring(tree, encoding='UTF-8', xml_declaration=True, standalone=True)
    with zipfile.ZipFile(path, 'w') as z:
        for info, contents in entries:
            z.writestr(info, data if info.filename == 'word/document.xml' else contents)
    return tree


def main():
    records = json.loads((ROOT / 'authored.json').read_text(encoding='utf-8'))
    folder = ROOT / 'qa-narrow'
    folder.mkdir(exist_ok=True)
    proofs = []
    for record in records:
        candidate = Path(record['candidate'])
        baseline = Path(record['before_backup'])
        assert sha(candidate) == record['candidate_sha256']
        assert sha(baseline) == record['source_sha256']
        with zipfile.ZipFile(candidate) as z:
            entries = [(deepcopy(info), z.read(info.filename)) for info in z.infolist()]
            doc = E.fromstring(z.read('word/document.xml'))
        with zipfile.ZipFile(baseline) as z:
            before_doc = E.fromstring(z.read('word/document.xml'))
        find = '//w:p[@w14:paraId="' + record['paraId'] + '"]'
        after_p = accept_past_in_paragraph(doc.xpath(find, namespaces=NS)[0], set(record['new_revision_ids']))
        before_p = accept_past_in_paragraph(before_doc.xpath(find, namespaces=NS)[0], set())
        new_only_rejected = reject_new(after_p, set(record['new_revision_ids']))
        assert c14n(new_only_rejected) == c14n(before_p)
        assert visible(after_p) == record['after_current_paragraph']
        assert visible(before_p) == record['before_current_paragraph']
        revisions = after_p.xpath('.//*[@w:author]', namespaces=NS)
        assert {e.get(q('id')) for e in revisions} == set(record['new_revision_ids'])
        assert all(e.getparent().tag == q('p') for e in revisions)
        assert not before_p.xpath('.//*[@w:author]', namespaces=NS)
        qa = folder / (record['code'] + '-new-only.docx')
        control = folder / (record['code'] + '-before-control.docx')
        write_narrow(entries, doc, after_p, qa)
        write_narrow(entries, doc, before_p, control)
        proof = {'code': record['code'], 'candidate_unchanged': sha(candidate) == record['candidate_sha256'],
                 'original_candidate': str(candidate), 'original_candidate_sha256': record['candidate_sha256'],
                 'qa_new_only': str(qa), 'qa_new_only_sha256': sha(qa),
                 'qa_before_control': str(control), 'qa_before_control_sha256': sha(control),
                 'expected_accept': record['after_current_paragraph'], 'expected_reject': record['before_current_paragraph'],
                 'new_revision_ids': record['new_revision_ids'], 'new_revision_count': len(revisions),
                 'all_remaining_main_story_revisions_top_level': True, 'main_story_only_one_original_paragraph': True,
                 'new_only_reject_xml_exact_before_control': True, 'native_accept_reject': '미실행',
                 'scope': 'Minimal QA paragraph retaining original paragraph/section properties and saved styles; only its past text/format history accepted in XML. Headers are outside the tested main story. This is not direct selective native rejection on the full original candidate.'}
        proofs.append(proof)
    f = ROOT / 'narrow-qa-proof.json'
    f.write_text(json.dumps(proofs, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(proofs, ensure_ascii=True))


if __name__ == '__main__':
    main()
