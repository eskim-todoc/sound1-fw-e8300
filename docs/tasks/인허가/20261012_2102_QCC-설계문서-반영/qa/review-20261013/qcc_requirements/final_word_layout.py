import copy, hashlib, json, pathlib, zipfile
from lxml import etree as E

W = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'
NS = {'w': W}
q = lambda k: '{' + W + '}' + k
ROOT = pathlib.Path(__file__).parent
REVIEW = ROOT.parent
sha = lambda p: hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest().upper()

for item in json.loads((REVIEW / 'final-word/requirements-native.json').read_text('utf-8')):
    target = pathlib.Path(item['path'])
    start = sha(target)
    assert start == item['sourceSHA256']
    with zipfile.ZipFile(target) as z:
        parts = {n: z.read(n) for n in z.namelist()}
    doc = E.fromstring(parts['word/document.xml'])
    before = copy.deepcopy(doc)
    styles = E.fromstring(parts['word/styles.xml'])
    names = {s.get(q('styleId')): s.find(q('name')).get(q('val')) for s in styles.findall(q('style')) if s.find(q('name')) is not None}
    changes = []
    for table in doc.xpath('//w:body/w:tbl', namespaces=NS):
        pr = table.find(q('tblPr'))
        sid = pr.find(q('tblStyle'))
        if sid is None:
            continue
        name = names.get(sid.get(q('val')), '')
        if not name.startswith('TDC 표 '):
            continue
        level = int(name.rsplit(' ', 1)[1])
        ind = pr.find(q('tblInd'))
        old = ind.get(q('w'))
        new = str(round((level - 1) * .35 * 567))
        # Preserve the original tracked table properties from the current edit.
        assert pr.find(q('tblPrChange')) is not None
        ind.set(q('w'), new)
        changes.append({'kind': 'table-left-border', 'style': name, 'old_twips': old, 'new_twips': new,
                        'text': ''.join(table.xpath('.//w:t[not(ancestor::w:del)]/text()', namespaces=NS))[:60]})
    if item['code'] == 'SGR':
        ps = doc.xpath('//w:body/w:p[not(ancestor::w:del)]', namespaces=NS)
        matched = [p for p in ps if ''.join(p.xpath('.//w:t[not(ancestor::w:del)]/text()', namespaces=NS)).strip() == '표준 및 규제']
        assert len(matched) == 1
        pr = matched[0].find(q('pPr'))
        assert pr.find(q('pPrChange')) is not None
        if pr.find(q('keepNext')) is None:
            insertion = list(pr).index(pr.find(q('pPrChange')))
            pr.insert(insertion, E.Element(q('keepNext')))
        changes.append({'kind': 'label-with-table', 'text': '표준 및 규제', 'keepNext': True})
    # The scoped style patch must preserve all text, revision records and images.
    assert doc.xpath('//w:t/text()|//w:delText/text()', namespaces=NS) == before.xpath('//w:t/text()|//w:delText/text()', namespaces=NS)
    def revisions(tree):
        return [(E.QName(e).localname, e.get(q('id')), e.get(q('author')), e.get(q('date'))) for e in tree.xpath('//*[@w:author]', namespaces=NS)]
    assert revisions(doc) == revisions(before)
    parts['word/document.xml'] = E.tostring(doc, xml_declaration=True, encoding='UTF-8', standalone=True)
    candidate = ROOT / (item['code'] + '-final-layout-candidate.docx')
    with zipfile.ZipFile(candidate, 'w', zipfile.ZIP_DEFLATED) as z:
        for n, b in parts.items():
            z.writestr(n, b)
    proof = {'target': str(target), 'before_sha256': start, 'candidate': str(candidate.resolve()),
             'candidate_sha256': sha(candidate), 'changes': changes, 'text_unchanged': True,
             'revision_records_unchanged': True, 'prior_tracked_properties_preserved': True,
             'images_relationships_styles_unchanged': True, 'native_review': '대기'}
    (ROOT / (item['code'] + '-final-layout.json')).write_text(json.dumps(proof, ensure_ascii=False, indent=2), 'utf-8')
    print(json.dumps(proof, ensure_ascii=False))
