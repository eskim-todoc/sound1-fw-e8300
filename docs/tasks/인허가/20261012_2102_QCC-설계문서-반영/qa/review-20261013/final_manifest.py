"""Bind saved document checks and native previews to the final delivered bytes."""
from pathlib import Path
from datetime import datetime, timezone, timedelta
import hashlib
import json
import shutil
import sys

sys.stdout.reconfigure(encoding='utf-8')
HERE = Path(__file__).resolve().parent
TASK = HERE.parents[1]

def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()

saved = read(HERE / 'saved-package-checks.json')
assert saved['failures'] == [], saved['failures']
assert len(saved['files']) == 12
assert len(saved['protected_sources']) == 23
assert all(item['unchanged'] for item in saved['protected_sources'])
# A package projection cannot prove Word's real table semantics or glyph
# visibility. Bind the final manifest to the independently checked pixels.
visual = read(HERE / 'qcc_requirements/word-visual-coverage.json')
assert visual['status'] == 'PASS', visual['status']
design = read(HERE / 'qcc_design/checks-29.json')
assert design['status'] == '통과', design['status']
test_review = read(HERE / 'qcc_tests/qa.json')
assert test_review['status'].startswith('통과')
assert test_review.get('final_native_display_pass') is True, 'Final Excel pixels are still pending'
native_word = read(HERE / 'final-word/requirements-native.json') + read(HERE / 'final-word/design-native.json')
sad_native = next(p for p in native_word if p['code'] == 'SAD')
sad_cells = read(HERE / 'final-word/SAD-native-accepted-cells.json')
assert sad_cells['pass'] is True and sad_cells['differences'] == []
assert (sad_cells['row_count'], sad_cells['column_count']) == (83, 3)
assert sha(Path(sad_native['nativeAcceptedXML'])) == sad_cells['native_XML_sha256'].upper(), 'SAD native accepted snapshot is stale'
word_settings = read(HERE / 'Word-user-confirmed-settings.json')
assert word_settings['status'] == 'PASS' and word_settings['userExplicitlySpecified'] is True
assert (word_settings['UserName'], word_settings['UserInitials']) == ('김은수', 'eunsu.kim')
native_excel = read(HERE / 'native-excel/SRS-full-native.json')
if isinstance(native_excel, dict):
    native_excel = [native_excel]
native_excel += read(HERE / 'native-excel/tests-native.json')
word_lookup = {item['code']: item for item in native_word}
excel_lookup = {}
for item in native_excel:
    excel_lookup.setdefault(item['code'].split('-')[0], []).append(item)
records = []
for item in saved['files']:
    path = Path(item['path'])
    current_hash = sha(path)
    assert current_hash == item['sha256'], (item['code'], 'Saved check is stale')
    if path.suffix == '.docx':
        assert visual['current_hashes'][item['code']].upper() == current_hash, (item['code'], 'Visual proof is stale')
    native = [word_lookup[item['code']]] if path.suffix == '.docx' else excel_lookup[item['code']]
    pages = []
    for preview in native:
        assert preview['sourceUnchanged']
        assert preview['sourceSHA256'].upper() == current_hash, (item['code'], 'Native preview is stale')
        pdf = Path(preview['pdf'])
        assert pdf.is_file()
        png_dir = pdf.with_suffix('')
        page_records = read(png_dir / 'pages.json')
        assert page_records
        assert [p['page'] for p in page_records] == list(range(1, len(page_records) + 1))
        assert all((png_dir / ('page-%03d.png' % p['page'])).is_file() for p in page_records)
        pages.append({'sheet': preview.get('sheet'), 'range': preview.get('range'),
                      'pdf': str(pdf), 'pdf_sha256': sha(pdf), 'pages': len(page_records),
                      'scope': preview['scope'], 'checked_at': preview['checkedAt']})
    records.append({**item, 'exists': True, 'native_previews': pages})

positions = read(HERE / 'current-positions.json')
assert {p['code'] for p in positions} == {p['code'] for p in records}
assert all(Path(p['path']).is_file() for p in positions)
now = datetime.now(timezone(timedelta(hours=9))).isoformat()
manifest = {
    'status': 'Documentation updated and checked; new product tests and formal approval remain pending',
    'checked_at': now,
    'scope': '12 existing E8300-QCC DHF copies; original E8300/QCC documents preserved',
    'requirement_ids': list(range(1, 82)) + [83],
    'active_requirement_count': 82,
    'removed_duplicate_requirement': 'RID82 folded into RID69 / SYS-RID-SP-FW-15; SYS test82 retained',
    'test_id_counts': {'unit': 130, 'integration': 24, 'system': 83},
    'new_QCC_conditions': {'unit': 38, 'integration': 14, 'system': 3},
    'prefix_policy': 'Existing E8300-QCC filenames and QCC/joint notices retained until user release',
    'protected_originals': saved['protected_sources'],
    'pairs': saved['pairs'],
    'current_location_count': sum(len(p['entries']) for p in positions),
    'files': records,
    'product_execution': 'No new product execution, fabricated measurements, or new approval/signature',
    'evidence': {
        'saved_packages': str(HERE / 'saved-package-checks.json'),
        'requirements': str(HERE / 'qcc_requirements/review-29.json'),
        'design': str(HERE / 'qcc_design/checks-29.json'),
        'tests': str(HERE / 'qcc_tests/qa.json'),
        'word_visual_coverage': str(HERE / 'qcc_requirements/word-visual-coverage.json'),
        'SAD_actual_native_table_acceptance': str(HERE / 'final-word/SAD-native-accepted-cells.json'),
        'Word_user_confirmed_settings': str(HERE / 'Word-user-confirmed-settings.json'),
        'current_positions': str(HERE / 'current-positions.json')
    }
}
target = TASK / 'evidence/final-output-manifest.json'
history = HERE / 'final-output-manifest-before-update.json'
if not history.exists():
    shutil.copy2(target, history)
target.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
(HERE / 'final-output-manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'files': len(records), 'native_word_pages': sum(len(read(Path(r['pdf']).with_suffix('') / 'pages.json')) for r in native_word),
                  'native_excel_pages': sum(len(read(Path(r['pdf']).with_suffix('') / 'pages.json')) for r in native_excel),
                  'current_locations': manifest['current_location_count'], 'target': str(target)}, ensure_ascii=False))
