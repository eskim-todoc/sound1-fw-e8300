from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import re
import zipfile
from lxml import etree as ET

TASK = Path(__file__).parent
EVIDENCE = TASK / 'evidence/qcc_cross_review_design.json'
NS = {'s': 'http://schemas.openxmlformats.org/spreadsheetml/2006/main', 'r': 'http://schemas.openxmlformats.org/officeDocument/2006/relationships', 'w': 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'}
W = '{' + NS['w'] + '}'

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()

def read_sheet(path, name):
    with zipfile.ZipFile(path) as z:
        shared = []
        if 'xl/sharedStrings.xml' in z.namelist():
            shared = [''.join(s.itertext()) for s in ET.fromstring(z.read('xl/sharedStrings.xml'))]
        wb = ET.fromstring(z.read('xl/workbook.xml'))
        rel = {x.get('Id'): x.get('Target') for x in ET.fromstring(z.read('xl/_rels/workbook.xml.rels'))}
        sheet = next(x for x in wb.find('s:sheets', NS) if x.get('name') == name)
        target = rel[sheet.get('{' + NS['r'] + '}id')]
        member = target.lstrip('/') if target.startswith('/') else 'xl/' + target
        root = ET.fromstring(z.read(member))
        values = {}
        for cell in root.findall('.//s:sheetData/s:row/s:c', NS):
            kind = cell.get('t')
            v = cell.find('s:v', NS)
            raw = v.text if v is not None and v.text else ''
            if kind == 's': value = shared[int(raw)]
            elif kind == 'inlineStr': value = ''.join(cell.xpath('.//s:t/text()', namespaces=NS))
            else: value = raw
            values[cell.get('r')] = value
        merges = [x.get('ref') for x in root.findall('s:mergeCells/s:mergeCell', NS)]
        breaks = [dict(x.attrib) for x in root.findall('s:rowBreaks/s:brk', NS)]
        return values, merges, breaks

def current_text(node):
    texts = []
    for t in node.xpath('.//w:t', namespaces=NS):
        if t.xpath('ancestor::w:del', namespaces=NS): continue
        if any(a.find('w:trPr/w:del', NS) is not None for a in t.iterancestors(W + 'tr')): continue
        texts.append(t.text or '')
    return ''.join(texts)

data = json.loads(EVIDENCE.read_text('utf-8'))
data['review_date'] = datetime.now(timezone.utc).isoformat()
srs = data['srs']
sv, sm, sb = read_sheet(srs['path'], '소프트웨어 입력 요구사항 사양서')
for req in srs['requirements']:
    req['cells'] = {col: sv.get(col + str(req['row']), '') for col in req['cells']}
srs.setdefault('observed_hash_history', []).append(srs['sha256'])
srs['sha256'] = sha(srs['path'])
ids = [int(re.fullmatch('SW-FW-RID-([0-9]+)', v).group(1)) for ref, v in sv.items() if ref.startswith('G') and re.fullmatch('SW-FW-RID-([0-9]+)', v)]
srs['independent_id_check'] = {'count': len(ids), 'unique': len(set(ids)), 'complete_1_to_83': sorted(ids) == list(range(1, 84))}
reqs = {x['id']: x for x in srs['requirements']}
srs['new_82_83_trace_check'] = {'82_upper': reqs[82]['cells']['H'], '82_origin': reqs[82]['cells']['I'], '82_transmit_direction': '마이크→Classic 상대' in reqs[82]['cells']['F'] or ('마이크' in reqs[82]['cells']['F'] and '전송' in reqs[82]['cells']['F']), '83_upper_pending_explicit': '[E8300 보완]' in reqs[83]['cells']['H'], '83_origin': reqs[83]['cells']['I']}

col_limits = {'SUVC': 'K', 'SUVR': 'K', 'SITC': 'M', 'SITR': 'M', 'SSTC': 'J', 'SSTR': 'J'}
sheets = {}
for test in data['tests']:
    values, merges, breaks = read_sheet(test['path'], test['sheet'])
    test.setdefault('observed_hash_history', []).append(test['sha256'])
    test['sha256'] = sha(test['path'])
    for row in test['rows']:
        row['cells'] = {col: values.get(col + str(row['row']), '') for col in row['cells']}
    test['manual_row_breaks_current'] = breaks
    sheets[test['key']] = (values, merges, test)

common = []
for criterion, report in [('SUVC', 'SUVR'), ('SITC', 'SITR'), ('SSTC', 'SSTR')]:
    cv, cm, ct = sheets[criterion]
    rv, rm, rt = sheets[report]
    limit = col_limits[criterion]
    def included(ref):
        m = re.match('([A-Z]+)([0-9]+)', ref)
        return len(m.group(1)) == 1 and 'B' <= m.group(1) <= limit and int(m.group(2)) >= (28 if criterion == 'SUVC' else 28 if criterion == 'SITC' else 30)
    differences = [ref for ref in sorted(set(cv) | set(rv)) if included(ref) and cv.get(ref, '') != rv.get(ref, '')]
    def shared_merge(m):
        start, end = m.split(':')
        return included(start) and included(end)
    merge_diff = sorted(set(m for m in cm if shared_merge(m)) ^ set(m for m in rm if shared_merge(m)))
    common.append({'criterion': criterion, 'report': report, 'body_common_columns': 'B:' + limit, 'value_differences': differences, 'merge_differences': merge_diff, 'new_results_pending': all('미실행' in r['cells'].get({'SUVR': 'L', 'SITR': 'N', 'SSTR': 'K'}[report], '') and '대기' in r['cells'].get({'SUVR': 'M', 'SITR': 'O', 'SSTR': 'L'}[report], '') for r in rt['rows'] if any(v.strip() for v in r['cells'].values()))})
data['independent_common_column_checks'] = common

base = Path('G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF/03_설계 출력/SW/외부기')
word = []
for code in ['SAD', 'SDDD', 'SIP']:
    path = next(base.glob('[[]E8300-QCC] ' + code + '*.docx'))
    with zipfile.ZipFile(path) as z: root = ET.fromstring(z.read('word/document.xml'))
    text = current_text(root)
    units = sorted(set(re.findall('SU-ID-QCC-[0-9]{3}', text)))
    modules = sorted(set(re.findall('SI-ID-QCC-[0-9]{3}', text)))
    record = {'code': code, 'path': str(path), 'sha256': sha(path), 'QCC_unit_ids': units, 'QCC_module_ids': modules, 'QCC_14_units_complete': units == [f'SU-ID-QCC-{i:03d}' for i in range(1, 15)], 'QCC_4_modules_complete': modules == [f'SI-ID-QCC-{i:03d}' for i in range(1, 5)], 'legacy_owner_prefix_current_count': sum(text.count(x) for x in ['[QCC 단독]', '[E8300/QCC 각각 확인]', '[자료 추가 위치]'])}
    if code == 'SAD':
        trace_tables = []
        for i, table in enumerate(root.xpath('//w:body//w:tbl', namespaces=NS), 1):
            rows = []
            for row in table.findall(W + 'tr'):
                if row.find('w:trPr/w:del', NS) is not None: continue
                cells = row.findall(W + 'tc')
                if cells and re.fullmatch('SW-FW-RID-[0-9]+', current_text(cells[0]).strip()): rows.append(current_text(cells[0]).strip())
            if len(rows) >= 81: trace_tables.append({'table_1based': i, 'ids': rows, 'unique_count': len(set(rows)), 'complete_1_to_83': set(rows) == {f'SW-FW-RID-{j}' for j in range(1, 84)}})
        record['forward_requirement_trace'] = trace_tables
    if code == 'SDDD':
        record['unit002_test_support'] = [{'table_1based': i, 'text': current_text(table)} for i, table in enumerate(root.xpath('//w:body//w:tbl', namespaces=NS), 1) if '원문 SW-FW-DS-ID-002 / 시스템 상태 유닛' in current_text(table)]
    if code == 'SIP':
        linked_tests = set()
        for fragment in re.findall('SW-FW-IntegrationTest-ID-([0-9]{3}(?:[/~][0-9]{3})*)', text):
            if '~' in fragment:
                start, end = map(int, fragment.split('~'))
                linked_tests.update(range(start, end + 1))
            else: linked_tests.update(map(int, fragment.split('/')))
        record['integration_018_to_024_complete'] = set(range(18, 25)).issubset(linked_tests)
        record['expanded_integration_test_ids'] = sorted(linked_tests)
    word.append(record)
data['word_cross_trace_current'] = word
data['assessment'] = {
    'SRS_82_83': 'RID82의 외부기 마이크 Classic 송신은 제공 Classic 수신/I2S 처리와 방향을 구분하고 QCC 송신 설계 미비 유지. RID83은 사용자 설명서 검토이며 실행 유닛을 임의 할당하지 않고 상위 추적 E8300 미비 유지.',
    'security': 'CRA 시험패턴 중계와 실제 인증 계산 분리. 설치 shared_service_key/clinician_master_key 및 생성 clinic_service_key 구분. SRS 앱별 알고리즘과 상세설계 SHA256 키 생성/AESCMAC 인증의 역할 상충 유지. 30초 인증/180초 무송수신/5회 실패60초 제한 각각 분리.',
    'provided_design_scope': '제공 4모듈14유닛과 원문ID를 연결했으며 소스/측정/승인으로 확대하지 않음. QCC PIO 극성/4충전상태→binary/UUID/GAIA/보안NVM/OTA/boot gaps 유지.',
    'test_inputs': '15유닛 그룹38조건/7통합 그룹14조건/2시스템 요구3조건의 입력·전바이트·상태·기대값을 실제 동일 기준행에서 검토. 새 보고행은 전부 미실행/판정대기.'
}
data['findings'] = [
    {'id': 'DESIGN-TRACE-001', 'topic': 'SDDD unit002 지원 시험 연결 누락', 'status': 'resolved', 'resolution': '현재 table251 row4 cell2에 시험ID4는 시스템 초기화/SPI 연계 지원, 주시험ID5와 구분하여 김은수 tracked sibling insert. 별도 authored evidence의 post_office_targeted_patches 참조.'},
    {'id': 'SEMANTIC-SCOPE-001', 'topic': 'PIO/Power/BT 유닛의 RID46/67/68/23/28/34 완주 범위 과장 가능성', 'status': 'owner_fixed_pending_independent_refresh', 'location': 'SUVC/SUVR E1727/E1729/E1736/E1738/E1740', 'resolution': '시험 담당자가 지원 입력/상태와 E8300 RF신원/자극/알림 별도 검증 범위를 명시.'},
    {'id': 'SEMANTIC-FRAME-001', 'topic': '논리20byte와 SPI21byte 변환 구분', 'status': 'owner_notified', 'location': 'SUVC/SUVR K1735; SITC/SITR M131/M132', 'requested': '송신20byte를 논리 메시지20byte로 명확화하고 실제 SPI21byte/길이필드 변환은 확정자료/실측 확인 대상으로 유지.'},
    {'id': 'SEMANTIC-SCOPE-002', 'topic': '통합019 PIO/연결 상태 지원과 RF신원/자극 완주 구분', 'status': 'owner_notified', 'location': 'SITC/SITR F134', 'requested': 'RID46/67/68는 연계지원 범위이며 E8300 등록 RF신원/시동종료 자극무출력 검증은 별도 경로임을 표시.'},
    {'id': 'FALSE-POSITIVE-001', 'topic': 'SAD 표7의 stale 접두어/128bits 충분히안전 주장', 'status': 'withdrawn_deleted_rows', 'resolution': '7개 해당행은 trPr/w:del 과거 삭제행이며 현재문구가 아니다. 현재문서 수정 대상 제외.'}
]
data['status'] = '최종 저장본 교차 대조 및 의미 QA 완료. 담당자 수정/재 native 검증 항목의 상태는 findings와 qa/cross-design-native.json에 유지.'
EVIDENCE.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'srs_hash': srs['sha256'], 'srs_ids': srs['independent_id_check'], 'common_columns': common, 'words': [{k: v for k, v in r.items() if k not in ['unit002_test_support', 'forward_requirement_trace']} for r in word]}, ensure_ascii=True))
