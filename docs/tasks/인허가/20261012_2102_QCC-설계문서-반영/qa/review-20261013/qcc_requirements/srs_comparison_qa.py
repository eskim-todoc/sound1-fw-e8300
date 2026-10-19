import hashlib, json, pathlib, re, runpy, zipfile
from lxml import etree as E

R = pathlib.Path(__file__).parent
ctx = runpy.run_path(str(R / 'srs_qa.py'))
d, ns, cells, value = [ctx[k] for k in ['doc', 'ns', 'cells', 'value']]
target = ctx['target']
q = lambda k: '{' + ns['x'] + '}' + k
with zipfile.ZipFile(target) as z:
    ss = E.fromstring(z.read('xl/sharedStrings.xml')) if 'xl/sharedStrings.xml' in z.namelist() else []
    styles = E.fromstring(z.read('xl/styles.xml'))
fonts, xfs = styles.find(q('fonts')), styles.find(q('cellXfs'))
second = {'F' + str(b['rows'][1]) for b in ctx['A']['blocks'] if b['rows'][0] != b['rows'][1]}
results = []
for ref, cell in cells.items():
    text = value(ref)
    if '[-]' not in text and '[+]' not in text and ref not in second:
        continue
    item = cell.find(q('is')) if cell.get('t') == 'inlineStr' else ss[int(cell.find(q('v')).text)] if cell.get('t') == 's' else None
    if item is None:
        continue
    basefont = fonts[int(xfs[int(cell.get('s', '0'))].get('fontId', '0'))]
    chars, segments = '', []
    for run in item.findall(q('r')) or [item]:
        t = ''.join(run.xpath('./x:t/text()', namespaces=ns))
        pr = run.find(q('rPr'))
        pr = basefont if pr is None else pr
        co = pr.find(q('color'))
        color = co.get('rgb', '').removeprefix('FF') if co is not None else ''
        segments.append((len(chars), len(chars) + len(t), color))
        chars += t
    boundaries = [(0, 'new' if ref in second else 'old')]
    boundaries.extend((m.start(), 'new' if m[0] == '[+]' else 'old') for m in re.finditer(r'\[[-+]\]', chars))
    boundaries.append((len(chars), 'end'))
    failures = []
    for (lo, state), (hi, _) in zip(boundaries, boundaries[1:]):
        for a, b, color in segments:
            sub = chars[max(lo, a):min(hi, b)] if max(lo, a) < min(hi, b) else ''
            if not sub.strip():
                continue
            expected = 'C00000' if state == 'old' else '008000'
            # Missing-information descriptions have their mandated current color.
            if state == 'new':
                for prefix, note_color in {'[QCC 자료 입력]': 'C00000', '[QCC/E8300 자료 입력]': '7030A0', '[E8300 보완]': '0070C0'}.items():
                    if sub.lstrip().startswith(prefix):
                        expected = note_color
                        break
            if color != expected:
                failures.append({'text': sub[:100], 'color': color, 'expected': expected})
    results.append({'cell': ref, 'order': 'old_then_new' if '[-]' in chars and '[+]' in chars else 'retired_old_only' if '[-]' in chars else 'new_continuation',
                    'old_before_new': '[+]' not in chars or '[-]' not in chars or chars.index('[-]') < chars.index('[+]'),
                    'color_pass': not failures, 'failures': failures})

matrix = []
actual = json.loads((R / 'srs-qa.json').read_text('utf-8'))
for req, saved in zip(ctx['P']['requirements'], actual['requirements']):
    if not req['matrix_match']:
        continue
    rest = saved['current_F'][len(req['base_text']):]
    matrix.append({'rid': req['rid'], 'matrix_rows': saved['matrix_actual_rows'],
                   'literal_base_match': saved['current_F'].startswith(req['base_text']),
                   'next_block_begins_on_new_line': not rest or rest.startswith('\n'),
                   'details_separator': '상세: ' in rest if rest.lstrip().startswith('상세:') else None})
protected = []
for source in json.loads((R.parent / 'review-baseline.json').read_text('utf-8')):
    if source['role'] != 'protected-source':
        continue
    p = pathlib.Path(source['path'])
    protected.append({'path': str(p), 'sha256': hashlib.sha256(p.read_bytes()).hexdigest().upper(),
                      'unchanged': hashlib.sha256(p.read_bytes()).hexdigest().upper() == source['sha256']})
out = {'target': str(target), 'sha256': hashlib.sha256(target.read_bytes()).hexdigest().upper(),
       'comparison_cells': results, 'all_markers_order_color_pass': all(x['old_before_new'] and x['color_pass'] for x in results),
       'matrix_literal_blocks': matrix, 'all_matrix_blocks_pass': all(x['literal_base_match'] and x['next_block_begins_on_new_line'] for x in matrix),
       'protected_sources': protected, 'protected_source_count': len(protected), 'all_protected_unchanged': all(x['unchanged'] for x in protected),
       'row_height_limit': all(float(x.get('ht', '0')) <= 409.5 for x in d.xpath('//x:row', namespaces=ns))}
assert out['all_markers_order_color_pass'] and out['all_matrix_blocks_pass'] and out['all_protected_unchanged'] and out['row_height_limit']
(R / 'srs-comparison-qa.json').write_text(json.dumps(out, ensure_ascii=False, indent=2), 'utf-8')
print(json.dumps({k: v for k, v in out.items() if k not in ['comparison_cells', 'matrix_literal_blocks', 'protected_sources']}, ensure_ascii=False))
