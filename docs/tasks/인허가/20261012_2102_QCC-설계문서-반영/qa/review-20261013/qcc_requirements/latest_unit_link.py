from pathlib import Path
import json, hashlib, datetime
from PIL import Image

h = Path(__file__).resolve().parent
q = h.parent
ns = {'__file__': str(h / 'independent_unit_qa.py')}
exec((h / 'independent_unit_qa.py').read_text(encoding='utf-8').split("c=read(specs['SUVC-main']);")[0], ns)
c, r = ns['read'](ns['specs']['SUVC-main']), ns['read'](ns['specs']['SUVR-main'])
p = h / 'independent-unit-native-qa.json'
out = json.loads(p.read_text(encoding='utf-8'))
diff, qdiff = [], []
for row in range(30, 1765):
    for col in 'BCDEFGHIJK':
        a = col + str(row)
        for k in ['text', 'rich', 'formula']:
            if c['cell'](a)[k] != r['cell'](a)[k]:
                diff.append({'cell': a, 'part': k})
for g in out['groups']:
    for s in g['states']:
        n = s['row']
        for key, val in [('expected', ns['active'](c['values'].get('K' + str(n), ''))), ('result', ns['active'](r['values'].get('L' + str(n), ''))), ('verdict', ns['active'](r['values'].get('M' + str(n), '')))]:
            if s[key] != val:
                qdiff.append({'row': n, 'part': key})
changes = {'SUVC-main': {4: 4, 102: 123, 103: 124, 109: 132, 136: 164}, 'SUVR-main': {4: 8, 82: 104, 87: 113, 108: 139}}
functions = {('SUVC-main', 4): 'CFX003 1/2-DMIC 설정', ('SUVC-main', 123): 'HEAR002 vMag192..239', ('SUVC-main', 124): 'HEAR002 vMag240..255와 경계조건', ('SUVC-main', 132): 'CM3-007 C1/C2 입력/RTT 경계', ('SUVC-main', 164): 'CM3-023 음량 독립성', ('SUVR-main', 8): 'CFX007 DMIC7 버퍼 전체RTT', ('SUVR-main', 104): 'HEAR002 vMag216..255와 경계조건', ('SUVR-main', 113): 'CM3-007 C2/C3 전체RTT', ('SUVR-main', 139): 'CM3-023 음량 독립성 전체RTT'}
latest, counts, meta = {}, {}, {}
for key in ['SUVC-main', 'SUVR-main']:
    folder = q / 'native-excel/tests' / key
    ps = json.loads((folder / 'pages.json').read_text(encoding='utf-8'))
    counts[key], latest[key] = len(ps), {}
    for item in ps:
        n = item['page']
        f = folder / f'page-{n:03}.png'
        with Image.open(f) as im:
            pix = hashlib.sha256(im.convert('RGB').tobytes()).hexdigest().upper()
            size = list(im.size)
        meta[(key, n)] = {'png': str(f), 'sha256': ns['sha'](f), 'pixelSHA256': pix, 'size': size}
        latest[key].setdefault(pix, []).append(n)
mapping = []
for old in out['pages']:
    key, n = old['code'], old['page']
    matches = latest[key].get(old['pixelSHA256'], [])
    if matches:
        assert len(matches) == 1
        new = matches[0]
    else:
        new = changes[key][n]
    mapping.append({'code': key, 'old_page': n, 'current_page': new, 'old_pngSHA256': old['sha256'], 'old_pixelSHA256': old['pixelSHA256'], **meta[(key, new)], 'exact_pixel_equal': bool(matches), 'verification': '기존 개별full검토 + 최신 전체픽셀동일' if matches else '최신 개별full PNG 원본크기 직접 재검토. 과거 픽셀 동일 주장 없음.', 'function': functions.get((key, new), old['range']), 'verdict': '통과', 'text_or_border_clipping': '없음'})
assert len(mapping) == 44 and sum(x['exact_pixel_equal'] for x in mapping) == 35
assert not diff and not qdiff
current = {
    'checkedAt': datetime.datetime.now().astimezone().isoformat(),
    'native_metadata': str(q / 'native-excel/tests-native.json'),
    'sources': [{'code': k, 'path': x['path'], 'sha256_before': x['before'], 'sha256_after': x['after'], 'readonly_unchanged': x['before'] == x['after'], 'matches_current_native': x['before'] == ns['specs'][k]['sourceSHA256']} for k, x in [('SUVC-main', c), ('SUVR-main', r)]],
    'actual_page_counts': counts, 'reviewed_previous_scope': 44,
    'pixel_same_reused_count': 35, 'latest_direct_changed_page_count': 9,
    'page_mapping': mapping,
    'additional_current_direct_page': {'code': 'SUVC-main', 'page': 133, **meta[('SUVC-main', 133)], 'function': 'CM3-007 C3 payload 및 C1/C2/C3 전체 RTT 비교 꼬리', 'direct_full_original_size': True, 'verdict': '통과'},
    'current_common': {'range': 'B30:K1764', 'cells': 17350, 'value_rich_formula_differences': diff, 'QCC38_expected_observation_verdict_vs_previous_differences': qdiff, 'current_QCC38_common_styles_equal': all(c['cell'](col + str(n))['style'] == r['cell'](col + str(n))['style'] for n in range(1727, 1765) for col in 'BCDEFGHIJK'), 'current_QCC38_heights_equal': all(c['rows'][n].get('ht') == r['rows'][n].get('ht') for n in range(1727, 1765))},
    'scope_limit': '현재 SUVC305/SUVR260쪽 중 이전44곳 대응과 추가1쪽만 독립full검사. 전체565쪽 glyph검사 또는 제품시험 통과로 확대하지 않는다.',
    'status': '통과(독립 표시/동일 조건 대응 범위); QCC 제품미실행/판정대기 유지'}
assert counts == {'SUVC-main': 305, 'SUVR-main': 260}
assert all(s['readonly_unchanged'] and s['matches_current_native'] for s in current['sources'])
out.setdefault('render_history', []).append(out['latest_layout_render_link'])
out['latest_layout_render_link'] = current
out['status'] = '통과: 최신 97ABF9/C2AA80 저장 해시와 44곳 대응 + 추가1쪽 독립표시검사. 제품시험 미실행/판정대기.'
p.write_text(json.dumps(out, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'file': str(p), 'sha256': ns['sha'](p), 'sources': current['sources'], 'matched35': True, 'direct_changed9plus1': True, 'common_differences': diff, 'Q38_differences': qdiff}, ensure_ascii=False))
