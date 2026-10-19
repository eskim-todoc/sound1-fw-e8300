"""감사기록의 기존 유닛 연결과 자료 입력 표시만 보완한다."""
from copy import deepcopy
import json
import re
import shutil
import sys
import zipfile

from lxml import etree as E
from inspect_sad_unit_trace import TASK, ROOT, SAD, SDDD, SRS, N, q, sha, text

QA = TASK / 'qa/sad-audit-trace-20261012'
PREVIOUS = TASK / 'qa/sad-unit-trace-20261012'
PREFIX = '[QCC 자료 입력]'


def parts(path):
    with zipfile.ZipFile(path) as z:
        return {k: z.read(k) for k in z.namelist()}


def current_rows(root):
    return {text(row.find('w:tc', N)): row for row in root.findall('.//w:tr', N)
            if row.find('w:trPr/w:del', N) is None
            and re.fullmatch(r'SW-FW-RID-\d+', text(row.find('w:tc', N)))}


def run(value, mode=None):
    r = E.Element(q('r'))
    if mode:
        pr = E.SubElement(r, q('rPr'))
        E.SubElement(pr, q('b'))
        E.SubElement(pr, q('color'), {q('val'): 'C00000' if mode == 'qcc' else '0070C0'})
        E.SubElement(pr, q('sz'), {q('val'): '24' if mode == 'qcc' else '20'})
        E.SubElement(pr, q('szCs'), {q('val'): '24' if mode == 'qcc' else '20'})
        if mode == 'qcc':
            E.SubElement(pr, q('u'), {q('val'): 'single'})
    for i, line in enumerate(value.split('\n')):
        if i:
            E.SubElement(r, q('br'))
        E.SubElement(r, q('t'), {'{http://www.w3.org/XML/1998/namespace}space': 'preserve'}).text = line
    return r


def edit_inserted(p, fragments):
    # 원문의 삭제 이력은 보존하고 같은 작성자의 미확정 삽입 안만 고친다.
    ins = p.findall('w:ins', N)
    assert len(ins) == 1 and not p.findall('w:r', N), text(p)
    assert ins[0].get(q('author')) == '김은수'
    for child in list(ins[0]):
        ins[0].remove(child)
    for value, mode in fragments:
        ins[0].append(run(value, mode))


def main():
    QA.mkdir(parents=True, exist_ok=True)
    rebuilding = '--rebuild' in sys.argv
    assert rebuilding or not (QA / 'before.docx').exists(), '기존 검토 사본은 덮어쓰지 않는다.'
    prior = json.loads((PREVIOUS / 'inputs.json').read_text(encoding='utf8'))
    assert sha(SDDD) == prior['sources'][str(SDDD)], '상세 설계서 외부 변경 확인 필요'
    sources = [SAD, SDDD, SRS,
        ROOT/'src/2__cm3/Cortex-M3-src/main.c',
        ROOT/'src/2__cm3/Cortex-M3-src/systemControl/initialize.c',
        ROOT/'src/2__cm3/Cortex-M3-src/systemControl/driver_timmer.c',
        ROOT/'src/2__cm3/Cortex-M3-src/BleCommunication/remoteControl.c',
        ROOT/'src/2__cm3/Gen1_5/FS/ci_event_log.c',
        ROOT/'src/2__cm3/Gen1_5/common/ci_timer.c',
        ROOT/'src/2__cm3/Gen1_5/common/ci_printf.h',
        ROOT/'src/2__cm3/Gen1_5/ui/tdc_ui_command.c',
        ROOT/'src/2__cm3/processorDirective.h']
    snapshot = {str(p): sha(p) for p in sources}
    if rebuilding:
        assert snapshot == json.loads((QA/'sources.json').read_text(encoding='utf8'))
    else:
        shutil.copyfile(SAD, QA/'before.docx')
        (QA/'sources.json').write_text(json.dumps(snapshot, ensure_ascii=False, indent=2), encoding='utf8')
    package = parts(SAD)
    root = E.fromstring(package['word/document.xml'])
    rows = current_rows(root)
    assert len(rows) == 81
    changes = []
    # 기존 삽입 이력 안의 QCC 안내만 동일 검색어/서식으로 바꾼다.
    markers = {
        'QCC 내부 블루투스 통신 모듈 / 상세 ID 추가 예정': PREFIX+'\n내부 블루투스 통신 모듈 ID / 이름',
        'QCC 내부 유닛 ID 추가 예정': PREFIX+'\n내부 유닛 ID / 이름',
        'QCC 내부 모듈, 항목 및 유닛의 상세 내용은 QCC 펌웨어 자료에 따라 본 항목에 추가한다.':
            PREFIX+' QCC 내부 모듈, 항목 및 유닛의 상세 내용을 여기에 추가한다.',
    }
    for p in root.findall('.//w:p', N):
        old = text(p)
        if old in markers:
            edit_inserted(p, [(markers[old], 'qcc')])
            changes.append({'before': old, 'after': markers[old]})
        elif old.endswith('QCC 내부 모듈 간 연계는 관련 자료 추가 후 확인한다.'):
            first = old.removesuffix(' QCC 내부 모듈 간 연계는 관련 자료 추가 후 확인한다.')
            tail = PREFIX+' QCC 내부 모듈 간 연계 자료를 추가하고 확인 내용을 반영한다.'
            edit_inserted(p, [(first, None), ('\n'+tail, 'qcc')])
            changes.append({'before': old, 'after': first+'\n'+tail})
        elif old.endswith('QCC 단독 요구사항의 내부 설계 연결은 QCC 자료 추가 후 확인한다.'):
            first = old.removesuffix(' QCC 단독 요구사항의 내부 설계 연결은 QCC 자료 추가 후 확인한다.')
            tail = PREFIX+' QCC 담당 요구사항의 내부 설계 연결을 추가하고 확인 내용을 반영한다.'
            edit_inserted(p, [(first, None), ('\n'+tail, 'qcc')])
            changes.append({'before': old, 'after': first+'\n'+tail})
    assert len(changes) == 29
    notes = {
        50: '[E8300 보완] 위 유닛의 상세 설계에 감사기록 초기화와 생성/저장 처리를 추가한다.',
        74: '[E8300 보완] 위 유닛의 상세 설계에 기준 시간 갱신과 감사기록의 시간 저장 처리를 추가한다.',
        79: '[E8300 확인] 로그 조회 경로의 담당 유닛이다. 인가자 접근 제한과 외부 전송 보호 조건을 확인하여 상세 설계에 반영한다.',
    }
    for rid, note in notes.items():
        cell = rows[f'SW-FW-RID-{rid}'].findall('w:tc', N)[2]
        p = cell.findall('w:p', N)[-1]
        changes.append({'id': rid, 'before': text(p), 'after': note})
        edit_inserted(p, [(note, 'e8300')])

    # 시간 초기화는 기존 초기화 유닛으로 연결한다. 새 유닛/번호를 만들지 않는다.
    row50 = rows['SW-FW-RID-50'].findall('w:tc', N)
    row74 = rows['SW-FW-RID-74'].findall('w:tc', N)
    row79 = rows['SW-FW-RID-79'].findall('w:tc', N)
    ids = [int(v) for v in root.xpath('//@w:id', namespaces=N) if v.isdigit()]
    next_id = max(ids)+1

    def clone_paragraph(source):
        nonlocal next_id
        p = deepcopy(source)
        for element in p.iter():
            for key in list(element.attrib):
                if key.endswith(('}paraId', '}textId')):
                    del element.attrib[key]
            if q('id') in element.attrib:
                element.set(q('id'), str(next_id))
                next_id += 1
        return p

    row74[1].insert(1, clone_paragraph(row50[1].find('w:p', N)))
    row74[2].insert(1, clone_paragraph(row50[2].find('w:p', N)))
    edit_inserted(row79[1].find('w:p', N), [('SI-ID-CM3-001\n시스템 제어 모듈', None)])
    p = clone_paragraph(row50[2].find('w:p', N))
    edit_inserted(p, [('SU-ID-CM3-005\n정상 반복 처리', None)])
    row79[2].insert(1, p)

    # 문서의 나머지 부분과 기존 삭제 이력은 그대로 둔다.
    package['word/document.xml'] = E.tostring(root, encoding='UTF-8', xml_declaration=True, standalone=True)
    with zipfile.ZipFile(QA/'candidate.docx', 'w', zipfile.ZIP_DEFLATED) as z:
        for name, data in package.items():
            z.writestr(name, data)
    (QA/'changes.json').write_text(json.dumps(changes, ensure_ascii=False, indent=2), encoding='utf8')
    print(json.dumps({'candidate': str(QA/'candidate.docx'), 'qcc_markers': 29, 'e8300_notes': 3}, ensure_ascii=False))


if __name__ == '__main__':
    main()
