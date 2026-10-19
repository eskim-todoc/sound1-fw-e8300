"""현행 상세 설계 식별자만 사용하여 요구사항 추적성 표를 구성한다.

원본 Word는 변경하지 않는다. 실제 변경 추적은 별도 Word 편집 단계가 담당한다.
대응은 구현/검증 완료 판정이 아니라 상세 설계의 연결 위치이다.
"""
import json
import re
import zipfile
from collections import Counter
from lxml import etree as E
from inspect_sad_unit_trace import QA, N, q

DATA = json.loads((QA / 'inputs.json').read_text(encoding='utf8'))
UNITS = {u['id']: u for u in DATA['units']}

# 코어 뒤 숫자는 현재 상세 설계서의 유닛 번호이다. 범위 확장이 아니라
# 해당 요구사항의 직접 처리와 전달에 필요한 설계 유닛을 선택한다.
LINKS = {
    1: 'CM3:27,29 CFX:33,39', 2: 'CM3:27,29 CFX:33,39',
    3: 'CM3:41 CFX:36,40', 4: 'CM3:41 CFX:36',
    5: 'CM3:28,29 CFX:29,30,32', 6: 'CM3:27,37,41',
    7: 'CM3:28,29 CFX:32', 8: 'CM3:23 CFX:11,22,23,24,25 HEAR:1',
    9: 'CFX:2,26,28 HEAR:2', 10: 'CFX:12,27,28 CM3:20',
    11: 'CM3:27 CFX:33,34,35,36,37',
    12: 'CFX:1,7,21,26,38 HEAR:1,2',
    13: 'CM3:6,22,42,43 CFX:11,12', 14: 'CM3:23 CFX:11,30',
    15: 'CFX:3,5,7,8,15,16,17,18,21', 16: 'CFX:3,5,7,15',
    17: 'CFX:4,36,38,39', 18: 'CM3:20,27,28,29,41 CFX:11,29,31,32',
    19: 'CM3:7,9,10,15', 20: 'CM3:5,13,14', 21: 'CM3:11,13,14,45',
    22: 'CM3:11,13,14', 23: 'CM3:11,44 CFX:32',
    24: 'CM3:6,22,46 CFX:11,12', 25: 'CM3:23,46 CFX:11,23',
    26: 'CM3:23,46 CFX:11,30', 27: 'CM3:13,14,24,46',
    28: 'CM3:24,44,46', 29: 'CM3:11,46', 30: 'CM3:22,46',
    31: 'CM3:23,46', 32: 'CM3:23,46', 33: 'CM3:24,46', 34: 'CM3:24,46',
    35: 'CM3:16,21,49', 36: 'CM3:17,30,49 CFX:40',
    37: 'CM3:30,32,49 CFX:38,39', 38: 'CM3:16,21,49',
    39: 'CM3:17,26,49', 40: 'CM3:17,19,26,49',
    41: 'CM3:25,34,36', 42: 'CM3:2,10,25', 43: 'CM3:25,35,38,39,40',
    44: 'CM3:1,26,45,46', 45: 'CM3:1,26,45,48,49',
    46: 'CM3:21,31,32,33 CFX:42',
    47: '', 48: '', 49: '', 50: 'CM3:1,46', 51: 'CM3:42,43',
    52: '', 53: '', 54: 'BOOT:10,11,12,13',
    55: 'CM3:7,11,13,14,44 CFX:32', 56: 'CM3:23 CFX:11,30',
    57: '', 58: 'CFX:1,7,21,26,38 HEAR:1,2',
    59: 'CM3:45,46,47,49,51', 60: 'CM3:26,45',
    61: 'CM3:4,13,14,30', 62: 'CM3:11,13,14,45',
    63: 'CM3:11,13,14,46', 64: 'CM3:13,14', 65: 'CM3:13,14',
    66: 'CM3:5,30,45,48', 67: 'CM3:4,17,30,31,39 CFX:38,41',
    68: 'CM3:7,12,15,39 CFX:9,41',
    69: 'CFX:5,8,15,18,19,21 CM3:45', 70: 'CM3:26,45,46,49',
    71: '', 72: '', 73: 'CM3:16', 74: 'CM3:46',
    75: 'CM3:16,26,45', 76: '', 77: 'CM3:42',
    78: 'CM3:42,43 BOOT:13,14', 79: '', 80: 'CM3:42,43', 81: '',
}
QCC_ONLY = {47, 48, 49, 52, 71, 72, 76}
QCC_LINK = {15, 44, 45, 60, 69, 70}
PROCEDURE = {
    53: ('배포 및 변경관리 절차', '절차로 관리 / 개별 유닛 해당 없음'),
    57: ('필수성능시험 절차', '시험 절차로 관리 / 개별 유닛 해당 없음'),
    81: ('품질관리 절차', '절차로 관리 / 개별 유닛 해당 없음'),
}
PENDING = {
    50: '감사기록 생성의 세부 유닛 ID 연결 필요',
    74: '감사기록 시간 기록의 세부 유닛 ID 연결 필요',
    79: '감사기록 접근 및 전송의 유닛 ID 연결 필요',
}
# 산출물에는 역할 설명을 반복하지 않는다. 근거와 경계는 검토용 자료에 둔다.
EVIDENCE = {
    50: 'CM3-001 Initialize의 ci_event_log_init, CM3-046 remoteControl의 ci_event_log_write 호출을 확인. 기록 생성 자체의 상세 설계 ID는 별도로 정의되지 않음.',
    51: 'CM3-042/043의 파일 읽기 경로에서 ci_map 및 ci_filesystem_read_with_crc_and_aes128 호출을 확인.',
    53: '제품 회수/교체는 소프트웨어 함수가 아니라 변경관리 절차 요구사항.',
    57: '필수성능시험 수행은 개발 결과를 평가하는 시험 절차 요구사항.',
    60: '현재 설계의 CM3-026/045는 SPI 버퍼/명령 처리. CRC 프로토콜의 QCC 내부 연결은 자료 추가 후 확정해야 하므로 완료로 표시하지 않음.',
    73: 'ci_event_log_write 실패에서 CM3-016의 errorCodeUpdate 호출을 확인.',
    74: 'CM3-046 remoteControl에서 기준 시각을 갱신하고 기록을 생성. 시간 기록 자체의 세부 유닛 ID는 별도로 정의되지 않음.',
    75: '오류 코드 관리/조회 및 응답 전달의 연결 위치. 민감정보 미노출의 검증 완료를 뜻하지 않음.',
    77: 'ci_map_read/write_isd_info는 enable_crc=true, enable_aes=true로 파일 처리.',
    78: 'CM3 설정 파일의 CRC/AES 처리와 BOOT 이미지 CRC/AES 처리의 설계 위치.',
    79: '인가자 로그 접근/기밀 전송에 해당하는 현행 상세 설계 유닛이 명시되지 않음. 번호를 임의로 만들지 않음.',
    80: 'CM3-042의 기본 자료 저장 및 CM3-043의 초기화 경로. 모든 손상 조건의 자동 복구를 검증한 것으로 확대하지 않음.',
    81: '인가된 생성/배포는 품질관리 절차 요구사항.',
}


def mapped_rows():
    assert set(LINKS) == set(range(1, 82))
    rows = []
    for req in DATA['requirements']:
        num = int(req['id'].rsplit('-', 1)[-1])
        selected = []
        for chunk in LINKS[num].split():
            core, numbers = chunk.split(':')
            selected += [f'SU-ID-{core}-{int(n):03}' for n in numbers.split(',')]
        assert len(selected) == len(set(selected)), req['id']
        assert all(uid in UNITS for uid in selected), req['id']
        modules = list(dict.fromkeys(UNITS[uid]['module'] for uid in selected))
        notes = []
        if num in QCC_ONLY | QCC_LINK:
            modules.append('QCC 내부 블루투스 통신 모듈 / 상세 ID 추가 예정')
            notes.append('QCC 내부 유닛 ID 추가 예정')
        if num in PROCEDURE:
            modules.append(PROCEDURE[num][0])
            notes.append(PROCEDURE[num][1])
        if num in PENDING:
            notes.append(PENDING[num])
            if not modules:
                modules.append('CM3 시스템 제어 모듈 / 저장 및 보안 처리')
        assert selected or notes
        rows.append({'id': req['id'], 'modules': modules,
                     'units': [{'id': uid, 'name': UNITS[uid]['name']} for uid in selected],
                     'notes': notes, 'evidence': EVIDENCE.get(num, '현재 상세 설계서의 입력/출력, 처리 및 관련 함수와 요구사항을 대조함.')})
    return rows


def sub(parent, tag, **attrs):
    elem = E.SubElement(parent, q(tag))
    for key, value in attrs.items():
        elem.set(q(key), str(value))
    return elem


def build():
    rows = mapped_rows()
    with zipfile.ZipFile(QA / 'before.docx') as package:
        styles = E.fromstring(package.read('word/styles.xml'))
    ids = {s.find('w:name', N).get(q('val')): s.get(q('styleId'))
           for s in styles.findall('w:style', N) if s.find('w:name', N) is not None}
    doc = E.Element(q('document'), nsmap={'w': N['w']})
    body = sub(doc, 'body')

    def paragraph(parent, value, style, red=False, center=False, after=60):
        p = sub(parent, 'p'); props = sub(p, 'pPr')
        sub(props, 'pStyle', val=ids[style]); sub(props, 'outlineLvl', val=9)
        sub(props, 'snapToGrid', val=0)
        sub(props, 'spacing', before=0, after=after, line=264, lineRule='auto')
        sub(props, 'ind', left=0, right=0, firstLine=0)
        sub(props, 'keepNext', val=1 if style == 'TDC 표 캡션 1' else 0); sub(props, 'keepLines', val=0)
        sub(props, 'widowControl', val=0)
        if center: sub(props, 'jc', val='center')
        else: sub(props, 'jc', val='left')
        for index, line in enumerate(value.split('\n')):
            run = sub(p, 'r'); rp = sub(run, 'rPr')
            sub(rp, 'sz', val=20); sub(rp, 'szCs', val=20)
            if red: sub(rp, 'color', val='C00000'); sub(rp, 'b')
            if index: sub(run, 'br')
            t = sub(run, 't'); t.text = line
        return p

    paragraph(body, '소프트웨어 요구사항을 담당 모듈과 상세 설계 유닛에 연결한다. 유닛 ID와 이름은 소프트웨어 상세 설계서에 따른다.', 'TDC 본문 1', after=120)
    paragraph(body, '표 1. 소프트웨어 요구사항과 상세 설계 유닛 연결', 'TDC 표 캡션 1', after=60)
    tbl = sub(body, 'tbl'); tp = sub(tbl, 'tblPr')
    sub(tp, 'tblStyle', val=ids['TDC 표 1'])
    sub(tp, 'tblW', w=9638, type='dxa'); sub(tp, 'jc', val='left')
    sub(tp, 'tblInd', w=0, type='dxa'); sub(tp, 'tblLayout', type='fixed')
    borders = sub(tp, 'tblBorders')
    for edge in ['top', 'left', 'bottom', 'right', 'insideH', 'insideV']:
        sub(borders, edge, val='single', sz=4, color='A6A6A6')
    margins = sub(tp, 'tblCellMar')
    for edge, value in [('top', 60), ('bottom', 60), ('left', 85), ('right', 85)]:
        sub(margins, edge, w=value, type='dxa')
    widths = [1740, 2670, 5228]
    grid = sub(tbl, 'tblGrid')
    for width in widths: sub(grid, 'gridCol', w=width)
    all_rows = [{'id': '', 'modules': [], 'units': [], 'notes': []}] + rows
    for index, row in enumerate(all_rows):
        tr = sub(tbl, 'tr'); trp = sub(tr, 'trPr'); sub(trp, 'cantSplit')
        if index == 0: sub(trp, 'tblHeader')
        for col, width in enumerate(widths):
            cell = sub(tr, 'tc'); cp = sub(cell, 'tcPr')
            sub(cp, 'tcW', w=width, type='dxa'); sub(cp, 'vAlign', val='center' if index == 0 else 'top')
            if index == 0: sub(cp, 'shd', val='clear', fill='F5F5F5')
            if index == 0:
                value = ['소프트웨어\n요구사항 ID', '모듈 ID / 이름', '상세 설계 유닛 ID / 이름'][col]
                paragraph(cell, value, 'TDC 표 머리행', center=True, after=0)
            elif col == 0:
                paragraph(cell, row['id'], 'TDC 표 항목', center=True, after=0)
            elif col == 1:
                for mi, module in enumerate(row['modules']):
                    paragraph(cell, module.replace(', ', '\n', 1), 'TDC 표 설명', red='추가 예정' in module,
                              after=90 if mi < len(row['modules']) - 1 else 0)
            else:
                for unit in row['units']:
                    paragraph(cell, unit['id'] + '\n' + unit['name'], 'TDC 표 설명', after=90)
                for note in row['notes']:
                    paragraph(cell, note, 'TDC 표 설명', red=('필요' in note or '예정' in note), after=0)
    paragraph(body, '', 'TDC 본문 1', after=0)

    pkg_ns = 'http://schemas.microsoft.com/office/2006/xmlPackage'
    rel_ns = 'http://schemas.openxmlformats.org/package/2006/relationships'
    pkg = E.Element('{' + pkg_ns + '}package', nsmap={'pkg': pkg_ns})
    def part(name, ctype, node):
        p = E.SubElement(pkg, '{' + pkg_ns + '}part', {'{' + pkg_ns + '}name': name, '{' + pkg_ns + '}contentType': ctype})
        E.SubElement(p, '{' + pkg_ns + '}xmlData').append(node)
    def rel(target, kind):
        root = E.Element('{' + rel_ns + '}Relationships', nsmap={None: rel_ns})
        E.SubElement(root, '{' + rel_ns + '}Relationship', Id='rId1',
                     Type='http://schemas.openxmlformats.org/officeDocument/2006/relationships/' + kind, Target=target)
        return root
    part('/_rels/.rels', 'application/vnd.openxmlformats-package.relationships+xml', rel('word/document.xml', 'officeDocument'))
    part('/word/_rels/document.xml.rels', 'application/vnd.openxmlformats-package.relationships+xml', rel('styles.xml', 'styles'))
    part('/word/document.xml', 'application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml', doc)
    part('/word/styles.xml', 'application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml', styles)
    (QA / 'trace-fragment.xml').write_bytes(E.tostring(pkg, xml_declaration=True, encoding='UTF-8', standalone=True))
    (QA / 'trace-map.json').write_text(json.dumps(rows, ensure_ascii=False, indent=2), encoding='utf8')
    counts = Counter(u['id'] for row in rows for u in row['units'])
    print(json.dumps({'requirements': len(rows), 'linked_units': len(counts), 'unit_links': sum(counts.values()),
                      'qcc_pending': sorted(QCC_ONLY | QCC_LINK), 'design_pending': list(PENDING), 'procedure': list(PROCEDURE)}, ensure_ascii=False))


if __name__ == '__main__':
    build()
