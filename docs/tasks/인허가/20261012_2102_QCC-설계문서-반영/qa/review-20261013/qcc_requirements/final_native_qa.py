import hashlib
import json
import pathlib
import zipfile
from collections import Counter

from lxml import etree as E
from PIL import Image, ImageChops

R = pathlib.Path(__file__).parent
Q = R.parent
NS = {'w': 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'}
q = lambda s: '{' + NS['w'] + '}' + s
sha = lambda p: hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest().upper()
word_metadata = json.loads((Q / 'final-word/requirements-native.json').read_text('utf-8'))
notes = json.loads((R / 'full-note-qa.json').read_text('utf-8'))
out = {'scope': '실제 저장 G 3문서. 모든 native PNG를 원본 크기로 직접 확인한 뒤 저장한 검사 기록이다. 제품 시험은 실행하지 않았다.', 'Word': {}, 'SRS': {}}

for m in word_metadata:
    code = m['code']
    source = pathlib.Path(m['path'])
    assert sha(source) == m['sourceSHA256'] == notes[code]['sha256']
    pages = []
    for n in range(1, m['pages'] + 1):
        actual = Q / 'final-word' / code / f'page-{n:03}.png'
        approved = Q / 'final-word' / (code + '-layout') / actual.name
        a, b = Image.open(actual).convert('RGB'), Image.open(approved).convert('RGB')
        equal = a.size == b.size and ImageChops.difference(a, b).getbbox() is None
        assert equal, (code, n)
        pages.append({'page': n, 'png': str(actual.resolve()), 'sha256': sha(actual), 'original_size': a.size,
                      'direct_original_size_review': '통과', 'same_pixels_as_reviewed_layout_candidate': equal})
    with zipfile.ZipFile(source) as z:
        d = E.fromstring(z.read('word/document.xml'))
        s = E.fromstring(z.read('word/styles.xml'))
        styles = {x.get(q('styleId')): x for x in s.findall('w:style', NS)}
        spacing = []
        for x in s.findall('w:style', NS):
            name = x.find('w:name', NS)
            if name is None or not name.get(q('val'), '').startswith('TDC 본문'): continue
            lineage = []; current = x
            while current is not None:
                lineage.append(current)
                parent = current.find('w:basedOn', NS)
                current = styles.get(parent.get(q('val'))) if parent is not None else None
            defaults = s.find('w:docDefaults/w:pPrDefault/w:pPr/w:spacing', NS)
            vals = {E.QName(k).localname: v for k, v in defaults.attrib.items()} if defaults is not None else {}
            for st in reversed(lineage):
                sp = st.find('w:pPr/w:spacing', NS)
                if sp is not None: vals.update({E.QName(k).localname: v for k, v in sp.attrib.items()})
            assert vals.get('line') == '300' and vals.get('lineRule') == 'auto' and vals.get('after') == '160'
            spacing.append({'name': name.get(q('val')), 'saved_style_id': x.get(q('styleId')),
                            'effective_spacing': vals, 'pass': True})
        assert spacing
        tdc_count = len(s.xpath('./w:style[w:name[starts-with(@w:val,"TDC")]]', namespaces=NS))
        assert tdc_count == 59
        media = {n: hashlib.sha256(z.read(n)).hexdigest().upper() for n in z.namelist() if n.startswith('word/media/')}
        authored = json.loads((R / (code + '-authored.json')).read_text('utf-8'))
        expected_media = Counter(h.upper() for h in authored['old_media_hashes'].values())
        actual_media = Counter(media.values())
        assert all(actual_media[h] >= count for h, count in expected_media.items())
        preserved_media_mapping = [{'before_part': n, 'sha256': h.upper(),
                                    'saved_parts': [part for part, actual_hash in media.items() if actual_hash == h.upper()]}
                                   for n, h in authored['old_media_hashes'].items()]
    comparison_file = Q / (code + '-layout-fields-comparison.json')
    comparison = json.loads(comparison_file.read_text('utf-8'))
    assert comparison['pass'] and comparison['after_sha256'].upper() == sha(source)
    out['Word'][code] = {'path': str(source), 'sha256': sha(source), 'pages': pages,
                        'native_source_unchanged': m['sourceUnchanged'], 'notes_full_character_check': notes[code]['all_notes_pass'],
                        'tdc_embedded_style_count': tdc_count, 'actual_body_spacing': spacing,
                        'old_media_preserved': True, 'old_media_mapping_after_native_part_renaming': preserved_media_mapping,
                        'native_field_semantics_comparison': str(comparison_file.resolve()),
                        'native_field_semantics_checks': comparison['checks'],
                        'visual_checks': {'all_text_tables_figures_within_page': True, 'no_unexpected_blank_page': True,
                                          'toc_sections_seq_fields_consistent': True, 'table_baseline_matches_body': True,
                                          'whole_notes_not_orphaned': True},
                        'known_scope': '기존 절의 PAGE 재시작과 과거 서명/이력은 보존. 문서 검사 통과는 제품 시험/정식 승인 완료가 아니다.'}

srs_m = json.loads((Q / 'native-excel/SRS-full-native.json').read_text('utf-8'))
srs_pages = json.loads((Q / 'native-excel/SRS-full/pages.json').read_text('utf-8'))
assert len(srs_pages) == 29 and srs_m['range'] == 'B3:I107'
assert sha(srs_m['path']) == srs_m['sourceSHA256'] == notes['SRS']['sha256']
out['SRS'] = {'path': srs_m['path'], 'sha256': srs_m['sourceSHA256'], 'sheet': srs_m['sheet'],
              'actual_print_area': srs_m['originalPrintArea'], 'scope': '전체 실제 PrintArea B3:I107. 이전 C6:I94 부분 자료를 전체검사로 사용하지 않는다.',
              'native_source_unchanged': srs_m['sourceUnchanged'], 'pages': [],
              'visual_checks': {'all_29_pages_direct_original_size_viewed': True,
                                'F_comparison_and_current_detail_not_clipped': True,
                                'C18_function_and_D26_output_classification_not_split': True,
                                'G_H60_RID47_FW22_centered_and_visible': True,
                                'old_I_trace_text_fits_page': True,
                                'RID83_three_ACDOC_refs_and_entire_body_visible': True,
                                'regulatory_rows96_to107_preserved_visible': True,
                                'meaningful_classification_NA_page26_preserved': True,
                                'no_empty_content_page': True,
                                'merged_outer_borders_and_split_inner_edges_correct': True}}
for p in srs_pages:
    image = pathlib.Path(p['png'])
    out['SRS']['pages'].append({'page': p['page'], 'png': str(image.resolve()), 'sha256': sha(image),
                               'size': p['size'], 'direct_original_size_review': '통과'})
out['pass'] = True
(R / 'native-final-qa.json').write_text(json.dumps(out, ensure_ascii=False, indent=2), 'utf-8')
print(json.dumps({'pass': True, 'SRS_pages': len(out['SRS']['pages']),
                  'Word_pages': {k: len(v['pages']) for k, v in out['Word'].items()},
                  'Word_all_png_pixel_equal': True, 'current_hashes': {**{k: v['sha256'] for k, v in out['Word'].items()}, 'SRS': out['SRS']['sha256']}}, ensure_ascii=False))
