"""Apply evidence-based owner notices, without changing functional requirements."""
from pathlib import Path
from copy import deepcopy
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib
import json
import re
import shutil
import sys
from lxml import etree as E
from review_srs_materials import QA, S, NS, workbook

q = lambda name: '{' + S + '}' + name
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()

# Each note has its own evidence and owner. Participation in an interface alone
# does not make the E8300 side incomplete.
DECISIONS = {
    15: ('qcc', 'I2S 오디오 공급을 담당하는 QCC 내부 모듈/유닛의 설계 연결을 추가한다.', 'SAD 표 1 RID-15 / 시스템 시험 15, K78:K81'),
    44: ('qcc', '리모콘의 무선 명령/응답을 중계하는 QCC 내부 모듈/유닛의 설계 연결을 추가한다.', 'SAD 표 1 RID-44 / 시스템 시험 44, K168:K177'),
    45: ('qcc', '매핑의 무선 명령/응답을 중계하는 QCC 내부 모듈/유닛의 설계 연결을 추가한다.', 'SAD 표 1 RID-45 / 시스템 시험 45, K178:K183'),
    47: ('qcc', '무조작 판단, 연결 종료 시간과 해제 동작의 QCC 설계/검증 자료를 입력한다.', 'SAD 표 1 RID-47 / 시스템 시험 47, K190 / UC-03'),
    48: ('qcc', '페어링과 CRA 인증의 QCC 설계 및 조건별 검증 자료를 입력한다.', 'SAD 표 1 RID-48 / 시스템 시험 48, K191:K193 / IA-01, IA-03'),
    49: ('qcc', '무선 데이터의 암호화/무결성 보호 방식과 페어링 전 차단의 QCC 자료를 입력한다.', 'SAD 표 1 RID-49 / 시스템 시험 49, K194 / SI-01'),
    50: ('e8300', '리모콘 연결 해제 이벤트의 자동 기록 경로와 기존 시험 기록의 적용 조건을 대조한다.', 'SAD 표 1 CM3-001/046 / 시스템 시험 50, K196 / UC-04'),
    51: ('e8300', '설정 파일 손상 시 무결성 검사와 감사기록 자동 생성 경로를 함께 확인한다.', 'SAD 표 1 CM3-042/043 / UC-04, SI-11'),
    52: ('qcc', '리모콘/매핑의 인증키 구분, 키 관리와 인증 전후 허용 동작의 QCC 자료를 입력한다.', 'SAD 표 1 RID-52 / 시스템 시험 52, K199:K202 / IA-04, UC-01'),
    53: ('both', 'QCC의 인증/암호화 전송과 E8300의 OTA 수신/저장/재기동 보호 자료를 연결한다. 제품 회수/교체 절차와 OTA의 적용 범위도 확인한다.', 'SAD 배포/변경관리 절차 / SI-08, SI-09'),
    54: ('e8300', '일반 구획과 VOLATILE 구획의 CRC 검사 범위, 짧은 헤더/누락 구획의 오류 처리 근거를 확인한다.', 'SAD 표 1 BOOT-010~013 / SI-04, SI-09, SI-11'),
    59: ('e8300', 'OTA 입력 길이 검사, 잘못된 종류/슬롯의 오류 응답 이후 처리 중단 근거를 확인한다.', 'SAD 표 1 CM3-045/046/047/049/051 / SI-05'),
    60: ('qcc', 'CRC가 적용된 무선 통신의 QCC 내부 모듈/유닛 설계 연결을 추가한다.', 'SAD 표 1 RID-60 / 시스템 시험 60, K249:K259 / SI-01'),
    69: ('qcc', 'Bluetooth Classic 오디오 공급을 담당하는 QCC 내부 모듈/유닛의 설계 연결을 추가한다.', 'SAD 표 1 RID-69 / 시스템 시험 69, K184:K185'),
    70: ('qcc', 'BLE 명령/응답을 중계하는 QCC 내부 모듈/유닛의 설계 연결을 추가한다.', 'SAD 표 1 RID-70 / 시스템 시험 70, K186:K187'),
    71: ('qcc', '인증 상태별 응답과 인증정보 미노출을 확인한 QCC 자료를 입력한다.', 'SAD 표 1 RID-71 / 시스템 시험 71, K210:K211 / IA-06'),
    72: ('qcc', '인증 실패 횟수, 연결 제한 시간과 해제 조건의 QCC 설계/검증 자료를 입력한다.', 'SAD 표 1 RID-72 / 시스템 시험 72, K212:K214 / IA-07'),
    73: ('e8300', '감사기록 저장 중 f_sync/f_close 실패 시 오류 반환/표시 경로와 기존 시험 기록을 대조한다.', 'SAD 표 1 CM3-016 / 시스템 시험 73, K216 / UC-05'),
    76: ('qcc', '인증 전후 송수신/OTA 전송의 보호와 생산 시 인증키 암호화 저장의 QCC 자료를 입력한다.', 'SAD 표 1 RID-76 / 시스템 시험 76, K222:K224 / SI-08, DC-01'),
    78: ('both', 'QCC 통신/인증키 보호 방식과 E8300 AES 저장/부트 보호 방식의 사용 목적 및 키 관리 자료를 함께 입력한다.', 'SAD 표 1 CM3-042/043, BOOT-013/014 / DC-03, SI-09'),
    79: ('e8300', '등록 내부기 정보 일치/정상 연결 시에만 로그 조회를 허용하는 조건과 출하/로컬 RTT 접근 제한을 확인한다.', 'SAD 표 1 CM3-005/030/031 / 시스템 시험 79, K230:K233 / TRE-01, RA-05'),
    81: ('e8300', '양산 펌웨어의 시험/RTT/서비스 기능 설정과 디버그 접근 제한 자료를 연결한다.', 'SAD 품질관리 절차 / SI-10, RA-05'),
}
PREFIXES = {'qcc': '[QCC 자료 입력]', 'e8300': '[E8300 보완]', 'both': '[QCC/E8300 자료 입력]'}
COLORS = {'qcc': 'C00000', 'e8300': '0070C0', 'both': '7030A0'}


def dump(name, data):
    (QA / name).write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf8')


def package(path):
    with ZipFile(path) as z:
        return {n: z.read(n) for n in z.namelist()}


def cell(root, addr):
    return root.find(f'.//s:c[@r="{addr}"]', NS)


def text(c, shared):
    if c.get('t') == 's':
        return shared[int(c.findtext(q('v')))]
    if c.get('t') == 'inlineStr':
        return ''.join(c.xpath('.//s:t/text()', namespaces=NS))
    return c.findtext(q('v'))


def strings(pkg):
    if 'xl/sharedStrings.xml' not in pkg:
        return []
    return [''.join(x.xpath('.//s:t/text()', namespaces=NS))
            for x in E.fromstring(pkg['xl/sharedStrings.xml'])]


def prepare():
    evidence = json.loads((QA / 'evidence.json').read_text(encoding='utf8'))
    srs = evidence['workbooks']['SRS']['소프트웨어 입력 요구사항 사양서']['cells']
    rid_rows = {int(v.rsplit('-', 1)[1]): int(a[1:]) for a, v in srs.items()
                if a.startswith('G') and re.fullmatch(r'SW-FW-RID-\d+', v)}
    assert set(rid_rows) == set(range(1, 82))
    arch = {int(row[0].rsplit('-', 1)[1]): row for row in evidence['architecture_tables'][6]['rows']
            if re.fullmatch(r'SW-FW-RID-\d+', row[0])}
    assert set(arch) == set(rid_rows)
    for kind, name in [('SSTC', '기준서_시스템 시험'), ('SSTR', '보고서_시스템 시험')]:
        ids = {int(v.strip().rsplit('-', 1)[1]) for a, v in evidence['workbooks'][kind][name]['cells'].items()
               if a.startswith('E') and re.fullmatch(r'SW-FW-RID-\d+', v.strip())}
        assert ids == set(rid_rows), (kind, ids)
    pkg = package(QA / 'srs-before.xlsx')
    tree = E.fromstring(pkg['xl/worksheets/sheet2.xml'])
    styles = E.fromstring(pkg['xl/styles.xml'])
    fonts, xfs = styles.find(q('fonts')), styles.find(q('cellXfs'))
    edits = []
    for rid in sorted(DECISIONS, key=rid_rows.get):
        mode, explanation, basis = DECISIONS[rid]
        addr = 'F' + str(rid_rows[rid])
        before = srs[addr]
        # Remove earlier status/owner labels only, never the functional wording.
        requirement = before.split('\n\n[QCC 자료 입력]', 1)[0]
        requirement = re.sub(r'^\[(?:QCC 단독|E8300/QCC [^\]]+)\]\s*', '', requirement)
        note = PREFIXES[mode] + ' ' + explanation
        conditional = ('QCC를 통한 무선 로그 전송을 제공하는 경우에는 권한과 전송 보호 자료도 확인한다.'
                       if rid == 79 else '')
        after = requirement + '\n\n' + note + ('\n' + conditional if conditional else '')
        c = cell(tree, addr)
        font = fonts[int(xfs[int(c.get('s', '0'))].get('fontId', '0'))]
        size = float(font.find(q('sz')).get('val'))
        assert size == 10
        edits.append({'id': rid, 'cell': addr, 'row': rid_rows[rid], 'before': before,
                      'requirement': requirement, 'mode': mode, 'note': note,
                      'conditional': conditional, 'after': after, 'evidence': basis,
                      'base_size': size, 'notice_size': size if mode == 'e8300' else size + 2,
                      'font_name': font.find(q('name')).get('val'), 'color': COLORS[mode],
                      'base_bold': font.find(q('b')) is not None and font.find(q('b')).get('val', '1') != '0',
                      'height': float(c.getparent().get('ht', '15'))})
    source_files = ['src/2__cm3/Gen1_5/FS/ci_event_log.c',
                    'src/2__cm3/Gen1_5/ui/tdc_ui_command.c',
                    'src/2__cm3/Cortex-M3-src/BleCommunication/remoteControl.c',
                    'src/2__cm3/Cortex-M3-src/BleCommunication/ble_communication.c',
                    'src/2__cm3/Cortex-M3-src/BleCommunication/ci_ble_control_ota.c',
                    'src/0__bootloader/source/bootloader.c']
    from review_srs_materials import ROOT
    plan = {'target': evidence['paths']['SRS'], 'sha256': evidence['hashes']['SRS'],
            'sheet': '소프트웨어 입력 요구사항 사양서', 'edits': edits,
            'protected': evidence['protected'],
            'source_hashes': {str(ROOT / p): sha(ROOT / p) for p in source_files},
            'reviewed_requirements': 81, 'removed_role_prefixes': 12,
            'counts': {mode: sum(x['mode'] == mode for x in edits) for mode in PREFIXES}}
    dump('plan.json', plan)
    print(json.dumps({k: plan[k] for k in ['reviewed_requirements', 'removed_role_prefixes', 'counts']}, ensure_ascii=False))
    print('\n'.join(f"RID-{x['id']} {x['cell']} {x['mode']} / {x['note']}" for x in edits))


def merge():
    plan = json.loads((QA / 'plan.json').read_text(encoding='utf8'))
    pkg, authored = package(QA / 'srs-before.xlsx'), package(QA / 'artifact.xlsx')
    root = E.fromstring(pkg['xl/worksheets/sheet2.xml'])
    ar = E.fromstring(authored['xl/worksheets/sheet2.xml'])
    ast = strings(authored)
    measured = {}
    if (QA / 'native-check.json').exists():
        measured = {x['cell']: x for x in json.loads((QA / 'native-check.json').read_text(encoding='utf-8-sig'))}
    for edit in plan['edits']:
        c = cell(root, edit['cell'])
        assert text(cell(ar, edit['cell']), ast) == edit['after']
        c.set('t', 'inlineStr')
        for child in list(c):
            c.remove(child)
        inline = E.SubElement(c, q('is'))
        for value, is_notice in [(edit['requirement'] + '\n\n', False), (edit['note'], True),
                                  ('\n' + edit['conditional'], False)]:
            if not value or value == '\n':
                continue
            run = E.SubElement(inline, q('r'))
            if is_notice:
                props = E.SubElement(run, q('rPr'))
                E.SubElement(props, q('rFont'), {'val': edit['font_name']})
                E.SubElement(props, q('sz'), {'val': str(edit['notice_size'])})
                E.SubElement(props, q('color'), {'rgb': 'FF' + edit['color']})
                if edit['mode'] != 'e8300':
                    E.SubElement(props, q('u'), {'val': 'single'})
                if edit['mode'] == 'e8300' or edit['base_bold']:
                    E.SubElement(props, q('b'))
            E.SubElement(run, q('t'), {'{http://www.w3.org/XML/1998/namespace}space': 'preserve'}).text = value
        if edit['cell'] in measured:
            row = c.getparent()
            needed = measured[edit['cell']]['needed'] + 4
            if needed > float(row.get('ht', '15')):
                assert needed <= 409.5
                row.set('ht', str(needed)); row.set('customHeight', '1')
    pkg['xl/worksheets/sheet2.xml'] = E.tostring(root, encoding='UTF-8', xml_declaration=True, standalone=True)
    with ZipFile(QA / 'candidate.xlsx', 'w', ZIP_DEFLATED) as z:
        for name, data in pkg.items():
            z.writestr(name, data)
    print('22 notices merged; original styles and all other package parts unchanged.')


def verify(publish=False):
    plan = json.loads((QA / 'plan.json').read_text(encoding='utf8'))
    a, b = package(QA / 'srs-before.xlsx'), package(QA / 'candidate.xlsx')
    assert set(a) == set(b)
    changed = [n for n in a if a[n] != b[n]]
    assert changed == ['xl/worksheets/sheet2.xml'], changed
    ra, rb = E.fromstring(a[changed[0]]), E.fromstring(b[changed[0]])
    sa, sb = strings(a), strings(b)
    for edit in plan['edits']:
        ca, cb = cell(ra, edit['cell']), cell(rb, edit['cell'])
        assert ca.get('s') == cb.get('s')
        assert text(ca, sa) == edit['before'] and text(cb, sb) == edit['after']
        assert not re.match(r'^\[(?:QCC 단독|E8300/QCC )', edit['requirement'])
        rs = cb.findall('s:is/s:r', NS)
        assert rs[0].find(q('t')).text == edit['requirement'] + '\n\n'
        assert rs[1].find(q('t')).text == edit['note']
        props = rs[1].find(q('rPr'))
        assert props.find(q('color')).get('rgb') == 'FF' + edit['color']
        assert float(props.find(q('sz')).get('val')) == edit['notice_size']
        if edit['mode'] == 'e8300':
            assert props.find(q('b')) is not None and props.find(q('u')) is None
        else:
            assert props.find(q('u')).get('val') == 'single'
            assert (props.find(q('b')) is not None) == edit['base_bold']
        cb.getparent().replace(cb, deepcopy(ca))
    height_changes = []
    for xa, xb in zip(ra.findall('.//s:row', NS), rb.findall('.//s:row', NS)):
        if xa.attrib != xb.attrib:
            assert int(xa.get('r')) in {x['row'] for x in plan['edits']}
            height_changes.append([xa.get('r'), xa.get('ht'), xb.get('ht')])
            xb.attrib.clear(); xb.attrib.update(xa.attrib)
    assert E.tostring(ra) == E.tostring(rb), 'Other worksheet contents differ'
    for path, digest in {**plan['protected'], **plan['source_hashes']}.items():
        assert sha(Path(path)) == digest, 'Read-only file changed: ' + path
    result = {'reviewed_requirements': 81, 'counts': plan['counts'],
              'removed_role_prefixes': 12, 'edited_cells': [x['cell'] for x in plan['edits']],
              'functional_text_IDs_formulas_caches_styles_merges_preserved': True,
              'protected_document_files': len(plan['protected']), 'source_files': len(plan['source_hashes']),
              'height_changes': height_changes, 'changed_parts': changed}
    if publish:
        native = json.loads((QA / 'native-check.json').read_text(encoding='utf-8-sig'))
        assert len(native) == len(plan['edits']) and all(x['needed'] <= x['current'] + 1 for x in native)
        assert json.loads((QA / 'visual-review.json').read_text(encoding='utf8'))['issues'] == []
        target = Path(plan['target'])
        assert sha(target) == plan['sha256'], 'External SRS edit'
        pending = target.with_name(target.name + '.pending')
        assert not pending.exists()
        shutil.copyfile(QA / 'candidate.xlsx', pending)
        assert sha(target) == plan['sha256']
        pending.replace(target)
        result.update({'sha256': sha(target), 'target': str(target)})
        assert sha(target) == sha(QA / 'candidate.xlsx')
    dump('publication.json' if publish else 'checks.json', result)
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    if '--merge' in sys.argv:
        merge()
    elif '--verify' in sys.argv:
        verify()
    elif '--publish' in sys.argv:
        verify(True)
    else:
        prepare()
