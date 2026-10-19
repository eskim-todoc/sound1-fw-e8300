import json, hashlib, re
from pathlib import Path

HERE = Path(__file__).resolve().parent
TASK = HERE.parent.parent
native_path = TASK / 'evidence/qcc_tests_native_v2.json'
authored_path = TASK / 'evidence/qcc_tests_authored.json'
native = json.loads(native_path.read_text(encoding='utf-8'))
authored = json.loads(authored_path.read_text(encoding='utf-8'))
plans = json.loads((HERE / 'native-final-v2-plans.json').read_text(encoding='utf-8'))
exports = []
for plan in plans:
    data = json.loads(Path(plan['plan']).read_text(encoding='utf-8'))
    records = json.loads(Path(data['recordPath']).read_text(encoding='utf-8-sig'))
    exports.extend(records if isinstance(records, list) else [records])
assert len(exports) == 13
pdf_text_checks = []
for code in ['SITC', 'SITR']:
    pages = json.loads((TASK / 'qa/excel-native' / (code + '-final-v2') / 'pages.json').read_text(encoding='utf-8'))
    assert len(pages) == 7
    for index, page in enumerate(pages):
        expected = f'SW-FW-IntegrationTest-ID-{18 + index:03d}'
        ids = re.findall(r'SW-FW-IntegrationTest-ID-\d{3}', page['text'])
        assert ids == [expected], (code, index + 1, ids)
        pdf_text_checks.append({'code': code, 'page': index + 1, 'group_id': expected, 'full_id_on_single_group_page': True})
    for snippet in ['논리', '20byte', '21byte']:
        assert snippet in pages[0]['text'], (code, snippet)
for code in ['SUVC', 'SUVR']:
    pages = json.loads((TASK / 'qa/excel-native' / (code + '-final-v2') / 'pages.json').read_text(encoding='utf-8'))
    assert len(pages) == 2
    text = '\n'.join(p['text'] for p in pages)
    for snippet in ['SW-FW-UnitTest-QCC-ID-4', '21byte', '20byte', 'length', '[BT] CMD 0x34']:
        assert snippet in text, (code, snippet)
    pdf_text_checks.append({'code': code, 'group_id': 'SW-FW-UnitTest-QCC-ID-4', 'full_spi21_and_logical20_explanation_native_visible': True})
pages = json.loads((TASK / 'qa/excel-native/SSTR-final-v2/pages.json').read_text(encoding='utf-8'))
assert len(pages) == 1
for snippet in ['SW-FW-SYS-Test-ID-82', 'SW-FW-SYS-Test-ID-83', '미실행', '판정 대기']:
    assert snippet in pages[0]['text']
current_hashes = {f['key']: hashlib.sha256(Path(f['path']).read_bytes()).hexdigest() for f in authored['files']}
assert all(current_hashes[f['key']] == f['sha256_after'] for f in authored['files'])
native.update({
    'status': 'PASS',
    'exports': exports,
    'read_only_native_excel': {'separate_hidden_application_per_plan': True, 'open_read_only': True, 'close_save_changes': False, 'repair_mode': False, 'source_sha256_unchanged_since_targeted_patch': True},
    'source_sha256': current_hashes,
    'pdf_text_checks': pdf_text_checks,
    'visual_qa': {
        'inspected': ['SITC/SITR seven-group contact and full pages019/021/023', 'SUVC/SUVR1733:1735 two pages', 'SSTR282:286', 'all eight SSTR auxiliary range exports'],
        'group_labels_cross_page_clipping': 'Resolved: all seven integration groups each printed on one page.',
        'logical20_physical21': 'Both unit and integration criterion/report show complete text inside cells.',
        'system82_method': 'D at merged D282:D283; functional demonstration, no invented numeric tolerance.',
        'auxiliary_history': 'Preserved past observation scope plus current missing evidence and pending verdict; no new actual measurement or screenshot.',
        'notice_styles': 'Color and underline visible; +2pt checked structurally in verify.py.',
        'independent_review': 'qcc_design reviewing current label PNGs; result to be appended separately.'
    },
    'auxiliary_modified': {
        'sheet': '시험 보조자료', 'header_merge': 'A2:D2', 'header_height_pt': 90,
        'comparison_columns': 'B:D', 'modified_rows': [80,83,184,185,190,199,200,201,202,206,207,208,209,210,211,212,213,214,222,223,224,227,228,229],
        'comparison_height_pt': 340, 'method_column': 'Not present in historical auxiliary sheet; current test methods remain in main sheet.',
        'note': 'Auxiliary has repeated full IDs per row and no A-column vertical merge requiring range expansion.'
    }
})
native_path.write_text(json.dumps(native, ensure_ascii=False, indent=2), encoding='utf-8')
authored['verification']['native_render'] = {'status': 'PASS', 'evidence': str(native_path), 'pdf_exports': len(exports), 'source_unchanged': True, 'read_only_excel': True}
authored['aux_QA'] = native['auxiliary_modified']
authored_path.write_text(json.dumps(authored, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'status':'PASS','exports':len(exports),'source_sha256':current_hashes}, ensure_ascii=True))
