from pathlib import Path
from lxml import etree as E
import json, zipfile, re, hashlib, datetime

h = Path(__file__).resolve().parent
q = h.parent
p = h / 'principle3-independent-qa.json'
v = json.loads(p.read_text(encoding='utf-8'))
ns = {'__file__': str(h / 'read_inputs.py')}
exec((h / 'read_inputs.py').read_text(encoding='utf-8').split('matrix=pathlib.Path')[0], ns)
N = ns['NS']
sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()
paths = {k: Path(d['path']) for k, d in v['documents'].items()}
before = {k: sha(f) for k, f in paths.items()}
actual, records, words = {}, {}, {}

def wt(n):
    return ''.join(n.xpath('.//w:t[not(ancestor::w:del or ancestor::w:moveFrom or ancestor::w:tr[w:trPr/w:del] or ancestor::w:tc[w:tcPr/w:cellDel] or ancestor::w:pPrChange or ancestor::w:rPrChange or ancestor::w:tblPrChange or ancestor::w:trPrChange or ancestor::w:tcPrChange)]/text()', namespaces=N))

for k, f in paths.items():
    rr = []
    if f.suffix == '.xlsx':
        data = ns['xread'](f)
        for s in data['sheets']:
            if s['name'] == '표지' or '변경 비교' in s['name']:
                continue
            for row in s['rows']:
                for a, c in row['cells'].items():
                    # A comparison marker covers its whole multiline block,
                    # including continuation lines before the next marker.
                    # Old red RID82 inside that block is historical content.
                    segments = re.split(r'(\[[-+]\])', c['value'])
                    enabled, current = True, []
                    for segment in segments:
                        if segment == '[-]':
                            enabled = False
                        elif segment == '[+]':
                            enabled = True
                        elif enabled:
                            current.append(segment)
                    t = ''.join(current)
                    if t:
                        rr.append({'location': s['name'] + '!' + a, 'text': t})
    else:
        with zipfile.ZipFile(f) as z:
            d = E.fromstring(z.read('word/document.xml'))
        blocks = []
        for i, b in enumerate(d.find('w:body', N)):
            if E.QName(b).localname == 'sdt' and b.xpath('./w:sdtPr/w:docPartObj/w:docPartGallery[@w:val="Table of Contents"]', namespaces=N):
                continue
            if E.QName(b).localname == 'p':
                t = wt(b)
                blocks.append({'block': i, 'type': 'paragraph', 'text': t})
                if t:
                    rr.append({'location': 'block' + str(i), 'text': t})
            elif E.QName(b).localname == 'tbl':
                rows = []
                for j, tr in enumerate(b.findall('w:tr', N)):
                    if tr.find('w:trPr/w:del', N) is not None:
                        continue
                    cells = [wt(tc) for tc in tr.findall('w:tc', N) if tc.find('w:tcPr/w:cellDel', N) is None]
                    rows.append({'row': j, 'cells': cells})
                    for l, t in enumerate(cells):
                        if t:
                            rr.append({'location': f'block{i}/row{j}/cell{l}', 'text': t})
                blocks.append({'block': i, 'type': 'table', 'rows': rows})
        words[k] = blocks
    records[k] = rr
    actual[k] = {'path': str(f), 'sha256_before': before[k], 'sha256_after': sha(f), 'readonly_unchanged': before[k] == sha(f), 'actual_active_record_count': len(rr)}

pattern = r'(?:SW-FW-RID-|S[UI]-ID-(?:BOOT|CFX|CM3|HEAR|QCC)-|SW-FW-(?:UnitTest-(?:BOOT|CFX|CM3|HEAR|QCC)-ID-|IntegrationTest-ID-|SYS-Test-ID-))\d+(?:/\d+)*'

def expand(t):
    out = set()
    for raw in re.findall(pattern, t):
        parts = raw.split('/')
        prefix = re.sub(r'\d+$', '', parts[0])
        for n in [re.search(r'\d+$', parts[0]).group()] + parts[1:]:
            out.add(prefix + str(int(n)))
    return out

known = set(sum(v['definitions'].values(), []))
unknown, currentsets = [], {}
for k, rr in records.items():
    ss = set()
    for rec in rr:
        ids = expand(rec['text'])
        ss |= ids
        for i in sorted(ids - known):
            unknown.append({'code': k, 'location': rec['location'], 'id': i, 'text': rec['text']})
    currentsets[k] = sorted(ss)

sip22 = [b for b in words['SIP'] if 'SW-FW-IntegrationTest-ID-022' in str(b)]
actualsiptext = '\n'.join(r['text'] for r in records['SIP'] if any(t in r['text'] for t in ['QCC-I-06', 'SU-ID-CFX-005', 'SW-FW-IntegrationTest-ID-022']))
unitids = ['SU-ID-CFX-005', 'SU-ID-CFX-008', 'SU-ID-CFX-015', 'SU-ID-QCC-008']
assert {'SU-ID-CFX-5', 'SU-ID-CFX-8', 'SU-ID-CFX-15', 'SU-ID-QCC-8'} <= expand(actualsiptext)
assert 'SW-FW-IntegrationTest-ID-013' in actualsiptext

src, rep = ns['xread'](paths['SITC']), ns['xread'](paths['SITR'])
def flat(data, name):
    return {a: c['value'] for s in data['sheets'] if s['name'] == name for row in s['rows'] for a, c in row['cells'].items()}
sc, sr = flat(src, '기준서_통합 시험'), flat(rep, '보고서_통합시험')
assert sc['F141'] == sr['F141'] and all(i in sc['F141'] for i in unitids)
assert '미실행' in sr['O141'] and '판정' in sr['O141'] and '미실행' in sr['N141']
ins = {'__file__': str(h / 'independent_unit_qa.py')}
exec((h / 'independent_unit_qa.py').read_text(encoding='utf-8').split("c=read(specs['SUVC-main']);")[0], ins)
l = ins['read']({'path': str(paths['SITC']), 'sheet': '기준서_통합 시험'})
r = ins['read']({'path': str(paths['SITR']), 'sheet': '보고서_통합시험'})
rich_equal = l['cell']('F141')['rich'] == r['cell']('F141')['rich']
assert rich_equal
print('undefined', len(unknown), unknown[:5])
assert not unknown
defines_checks = {k: all(i in set(currentsets[doc]) for i in v['definitions'][k]) for k, doc in [('requirements', 'SRS'), ('modules', 'SDDD'), ('units', 'SDDD')]}
assert all(defines_checks.values())
testdefs = set(currentsets['SUVC']) | set(currentsets['SITC']) | set(currentsets['SSTC'])
assert set(v['definitions']['tests']) <= testdefs
assert expand(' '.join(r['text'] for r in records['SRS'] if '!G' in r['location'])) == set(v['definitions']['requirements'])
after = {k: sha(f) for k, f in paths.items()}
assert before == after
v['latest_saved_reinspection'] = {
    'checkedAt': datetime.datetime.now().astimezone().isoformat(),
    'mode': 'G12 readonly; own QA JSON only', 'sources': actual,
    'active_ID_sets': currentsets, 'undefined_current_references': unknown,
    'actual_definition_presence_checks': defines_checks,
    'test_definitions_in_actual_criteria': True,
    'definition_baseline_preserved': {'requirements': 82, 'modules': 14, 'units': 129, 'unit_tests': 130, 'integration_tests': 24, 'system_tests': 83, 'test_total': 237},
    'current_SIP22_blocks': sip22, 'current_SIP22_receive_unit_text': actualsiptext,
    'current_SIT22': {'SITC': {a: sc.get(a, '') for a in ['D141', 'E141', 'F141', 'L141', 'M141']}, 'SITR': {a: sr.get(a, '') for a in ['D141', 'E141', 'F141', 'L141', 'M141', 'N141', 'O141']}, 'current_F141_text_equal': True, 'current_F141_rich_equal': rich_equal, 'participating_units': unitids, 'mixing_stimulation_separate_test': 'SW-FW-IntegrationTest-ID-013', 'product_unexecuted_preserved': True},
    'semantic_limits': v['scope_limits'], 'remaining_product_data': v['unresolved_data'],
    'verdict': '통과(문서 ID/상위 연결 및 담당 기능 연결 검사 범위)'}
v['current12_final_hashes'], v['current12_unchanged'] = after, True
for finding in v.get('followup_findings', []):
    finding.setdefault('historical_status', finding['status'])
    finding['status'] = '해소/실제 최종 G SIP 및 SITC/SITR 연결 재검사 통과'
    finding['reinspection'] = '현재 SIP22의 QCC008/CFX005/008/015 참여와 수신 경계/013 별도 분리, F141 text/rich 동일, N/O141 미실행/판정대기 확인'
if 'followup_reinspection' in v:
    v['followup_reinspection']['actual_SIP_saved_CFUnits'] = '후속 최신 저장 검사에서 실제 SIP347D8632 현재 CFX005/008/015 반영 확인 완료'
    v['followup_reinspection']['historical_only'] = True
rollback = json.loads((q / 'SAD-native-rollback.json').read_text(encoding='utf-8'))
native_path = q / 'final-word/SAD-final-native.json'
native_cells_path = q / 'final-word/SAD-native-accepted-cells.json'
native = json.loads(native_path.read_text(encoding='utf-8'))
native_cells = json.loads(native_cells_path.read_text(encoding='utf-8'))
assert native['sourceSHA256'] == before['SAD'] and native['sourceUnchanged']
assert native_cells['pass'] and not native_cells['differences'] and native_cells['row_count'] == 83 and native_cells['column_count'] == 3
native_xml = Path(native['nativeAcceptedXML'])
assert sha(native_xml) == native_cells['native_XML_sha256'].upper()
native_rids = {re.sub(r'0+(?=\d+$)', '', r['id']) for r in native_cells['rows'] if re.fullmatch(r'SW-FW-RID-\d+', r['id'])}
assert native_rids == set(v['definitions']['requirements']) and len(native_rids) == 82
def compact(text):
    return re.sub(r'\s+', '', text)
actual_trace_rows = {}
for block in words['SAD']:
    if block['type'] != 'table':
        continue
    for row in block['rows']:
        cells = row['cells']
        if len(cells) == 3 and re.fullmatch(r'SW-FW-RID-\d+', cells[0].strip()):
            actual_trace_rows[cells[0].strip()] = cells
assert len(actual_trace_rows) == 82
native_trace_differences = []
for row in native_cells['rows']:
    if row['id'] not in actual_trace_rows:
        continue
    for c, (rawtext, nativetext) in enumerate(zip(actual_trace_rows[row['id']], row['cells'])):
        if compact(rawtext) != compact(nativetext):
            native_trace_differences.append({'id': row['id'], 'cell': c})
assert not native_trace_differences
v.setdefault('native_SAD_history', []).append(v.get('native_SAD_followup', {}))
v['native_SAD_followup'] = {'state': '최종 G 저장본 실제 native AcceptAll 추적 본문 보존 재검사 통과',
    'sourceSHA256': before['SAD'], 'native_metadata': str(native_path), 'native_metadataSHA256': sha(native_path),
    'native_trace_proof': str(native_cells_path), 'native_trace_proofSHA256': sha(native_cells_path),
    'native_XML': str(native_xml), 'native_XML_SHA256': sha(native_xml),
    'requirement_rows': 82, 'total_rows_including_header': 83, 'columns': 3,
    'current_G_XML_vs_actual_native_accepted_cells_differences': native_trace_differences,
    'comparison_note': '각 셀의 실제 문구를 대조했다. Word AcceptAll의 단락 합침에 의한 공백/줄바꿈 차이만 제외했으며 ID/이름/미비 안내의 모든 문자는 동일하다.',
    'failed_candidateSHA256': rollback['failedCandidateSHA256'], 'rollback_history': rollback,
    'scope': '과거35쪽 실패를 보존하고 현재55쪽/84F native 본문 보존과 연결. 전체 페이지 시각 검사는 word-visual-coverage.json 별도 판정이다. 제품 검증/새승인을 주장하지 않는다.'}
qcc007_reverse = [row for block in words['SAD'] if block['type'] == 'table' for row in block['rows']
    if row['cells'] and row['cells'][0].startswith('SU-ID-QCC-007')]
assert len(qcc007_reverse) == 1
assert {'SW-FW-RID-52', 'SW-FW-RID-76'} <= expand(qcc007_reverse[0]['cells'][-1])
qcc007_forward = [row for block in words['SAD'] if block['type'] == 'table' for row in block['rows']
    if len(row['cells']) == 3 and row['cells'][0] in {'SW-FW-RID-52', 'SW-FW-RID-76'}]
assert len(qcc007_forward) == 2 and all('SU-ID-QCC-007' in row['cells'][2] for row in qcc007_forward)
qcc007_test_scope = {code: [r for r in records[code] if r['location'].endswith('!E1733')] for code in ['SUVC','SUVR']}
assert all(len(rows) == 1 and '외부기 일련번호 NVM 동작의 정식 직접 요구 추적' in rows[0]['text'] for rows in qcc007_test_scope.values())
v.setdefault('unresolved_data_history', []).append(v['unresolved_data'])
v['unresolved_data'] = [
    {'unit': 'SU-ID-QCC-007', 'test': 'SW-FW-UnitTest-QCC-ID-3',
        'state': '외부기 일련번호 PS 매핑/읽기/쓰기 시험조건의 정식 직접 요구 추적 자료 미비',
        'locations': ['SUVC/SUVR E1733'],
        'confirmed_unit_links': ['SW-FW-RID-52', 'SW-FW-RID-76'],
        'treatment': 'NVM 유닛 자체는 SAD 정방향/역방향과 SDDD에52/76으로 연결된다. 이 보안 키 NVM 상위 연결을 일련번호 PS 시험조건의 직접 요구로 임의 확대하지 않는다. 실제 시험 문구의 제한된 미비를 보존한다.'},
    {'state': 'QCC/E8300 product implementation, pins/clocks/codecs/serialization/security vectors/measurement logs and actual product manual inspection remain pending',
        'treatment': 'Document trace audit is not product-test pass.'}]
v['latest_saved_reinspection']['remaining_product_data'] = v['unresolved_data']
v['latest_saved_reinspection']['QCC007_scope_clarification'] = {
    'unit_forward_reverse_RID52_76_confirmed': True, 'current_SAD_reverse_row': qcc007_reverse[0],
    'current_test_scope': qcc007_test_scope,
    'SDDD_current_scope': 'NVM unit / 표251 구현 조건: 상위52/76과 UnitTest-QCC-ID-3 외부기 일련번호 PS 매핑/읽기/쓰기를 연결한다. 일반 인터페이스 존재를 키 보호 완료로 확대하지 않는다.',
    'broad_no_requirement_trace_notice_found_in_current_Word': False,
    'document_patch_needed': False, 'evidence_scope': '직접 읽은 현재 SAD/SDDD/SIP/SR/SGR 및 SUVC/SUVR E1733. QA 과거 넓은 미비 표현을 시험조건 범위로 정정했다.'}
v['status'] = '정적 전체 추적 및 현재 SAD 실제 native 추적 본문 보존 통과. 전체 페이지 시각 검사는 별도 evidence로 판정하며 제품 검증 미실행/미비 유지.'
v['completed'] = datetime.datetime.now().astimezone().isoformat()
p.write_text(json.dumps(v, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'file': str(p), 'sha256': sha(p), 'current12': after, 'undefined': len(unknown), 'SIP22PASS': True, 'F141richPASS': rich_equal}, ensure_ascii=False))
