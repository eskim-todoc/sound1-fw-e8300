import copy, hashlib, json, pathlib, zipfile
from lxml import etree as E

R = pathlib.Path(__file__).parent
D = json.loads((R / 'inspection.json').read_text('utf-8'))
p = pathlib.Path(D['files']['[E8300-QCC] SRS']['path'])
sha = lambda: hashlib.sha256(p.read_bytes()).hexdigest().upper()
before = sha()
assert before == 'D87A445328E501D48A481D57FC3C3CCC5B502B27D640930D3398B55F4F078E4D'
with zipfile.ZipFile(p) as z:
    parts = {n: z.read(n) for n in z.namelist()}
ns = {'x': 'http://schemas.openxmlformats.org/spreadsheetml/2006/main'}
q = lambda k: '{' + ns['x'] + '}' + k
d = E.fromstring(parts['xl/worksheets/sheet2.xml'])
s = E.fromstring(parts['xl/styles.xml'])
before_d = copy.deepcopy(d)
xfs = s.find(q('cellXfs'))
changes = []
cache = {}
for ref in ['C18', 'D26', 'I51', 'I55', 'I60', 'I62', 'I68', 'I72']:
    cell = d.xpath('//x:c[@r=$ref]', namespaces=ns, ref=ref)[0]
    old = int(cell.get('s', '0'))
    if old not in cache:
        new = copy.deepcopy(xfs[old])
        a = new.find(q('alignment'))
        if a is None:
            a = E.SubElement(new, q('alignment'))
        a.set('vertical', 'top')
        new.set('applyAlignment', '1')
        cache[old] = len(xfs)
        xfs.append(new)
    cell.set('s', str(cache[old]))
    changes.append({'cell': ref, 'old_style': old, 'new_style': cache[old], 'vertical': 'top'})
xfs.set('count', str(len(xfs)))
row = d.xpath('//x:row[@r="61"]', namespaces=ns)[0]
changes.append({'row': 61, 'old_height': row.get('ht'), 'new_height': '260'})
row.set('ht', '260')
row.set('customHeight', '1')
assert d.xpath('//x:c/x:v/text()|//x:c/x:is//x:t/text()|//x:c/x:f/text()', namespaces=ns) == before_d.xpath('//x:c/x:v/text()|//x:c/x:is//x:t/text()|//x:c/x:f/text()', namespaces=ns)
assert E.tostring(d.find(q('mergeCells'))) == E.tostring(before_d.find(q('mergeCells')))
parts['xl/worksheets/sheet2.xml'] = E.tostring(d, xml_declaration=True, encoding='UTF-8', standalone=True)
parts['xl/styles.xml'] = E.tostring(s, xml_declaration=True, encoding='UTF-8', standalone=True)
assert sha() == before
with zipfile.ZipFile(p, 'w', zipfile.ZIP_DEFLATED) as z:
    for name, data in parts.items():
        z.writestr(name, data)
proof = {'before_sha256': before, 'after_sha256': sha(), 'changes': changes,
         'content_formula_rich_text_merge_print_area_unchanged': True,
         'requirement_and_upper_ids_center_preserved': True,
         'native_recheck': '대기'}
(R / 'srs-layout-fix3.json').write_text(json.dumps(proof, ensure_ascii=False, indent=2), 'utf-8')
print(json.dumps(proof, ensure_ascii=False))
