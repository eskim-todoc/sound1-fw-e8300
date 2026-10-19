from __future__ import annotations

import copy
import hashlib
import json
import re
import zipfile
from datetime import datetime, timezone
from pathlib import Path

from lxml import etree as ET

TASK = Path(__file__).parent
EVIDENCE = TASK / 'evidence'
QA = TASK / 'qa' / 'qcc_design'
QA.mkdir(parents=True, exist_ok=True)
BASE = Path('G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF/03_설계 출력/SW/외부기')
DATA = json.loads((EVIDENCE / 'qcc_design.json').read_text(encoding='utf-8'))
CONTRACT = json.loads((EVIDENCE / 'integration-contract.json').read_text(encoding='utf-8'))
W = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'
NS = {'w': W}
XML_SPACE = '{http://www.w3.org/XML/1998/namespace}space'
AUTHOR = '김은수'
DATE = datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace('+00:00', 'Z')


def q(tag):
    return '{' + W + '}' + tag


def sub(parent, tag, attrs=None):
    el = ET.SubElement(parent, q(tag))
    for key, val in (attrs or {}).items():
        el.set(q(key), str(val))
    return el


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def current_text(el):
    if any(a.tag == q('del') for a in el.iterancestors()):
        return ''
    if el.tag == q('tr') and el.find('w:trPr/w:del', NS) is not None:
        return ''
    if el.tag == q('t'):
        return el.text or ''
    return ''.join(current_text(c) for c in el)


def source_ref(u):
    return f"QCC_SDDD-FW-TD24A 소프트웨어 상세 설계서_byssi 원문 {u['original_id']} / {u['title']}"


def rid_list(u):
    ids = list(u['requirement_candidates'])
    if u['id'] == 'SU-ID-QCC-008':
        ids.append('SW-FW-RID-82')
    if u['id'] == 'SU-ID-QCC-009':
        ids.extend(['SW-FW-RID-46', 'SW-FW-RID-47'])
    return ', '.join(dict.fromkeys(ids))


def unit_test_ids(u):
    numbers = {1: [4], 2: [5], 3: [6], 4: [7, 8], 5: [1], 6: [2], 7: [3], 8: [10, 8], 9: [9, 8], 10: [11], 11: [12], 12: [13], 13: [14], 14: [15]}[int(u['id'][-3:])]
    return ', '.join('SW-FW-UnitTest-QCC-ID-' + str(i) for i in numbers)


class Editor:
    def __init__(self, code):
        self.code = code
        self.original = next(BASE.glob('[[]E8300[]] ' + code + '-*.docx'))
        self.output = BASE / self.original.name.replace('[E8300]', '[E8300-QCC]', 1)
        self.original_hash = sha(self.original)
        self.input_hash = sha(self.output)
        with zipfile.ZipFile(self.original) as z:
            self.entries = [(i, z.read(i.filename)) for i in z.infolist()]
        self.parts = {i.filename: data for i, data in self.entries}
        self.root = ET.fromstring(self.parts['word/document.xml'])
        self.original_root = copy.deepcopy(self.root)
        self.body = self.root.find('w:body', NS)
        self.baseline_blocks = list(self.body)
        ids = [int(x) for x in self.root.xpath('.//@w:id', namespaces=NS) if x.isdigit()]
        self.next_id = max(ids, default=0) + 1000
        self.new_ids = set()
        if self.original_hash != self.input_hash:
            with zipfile.ZipFile(self.output) as z:
                candidate = ET.fromstring(z.read('word/document.xml'))
                if any(z.read(n) != self.parts[n] for n in self.parts if n != 'word/document.xml'):
                    raise RuntimeError('담당 외 package 변경을 발견하여 덮어쓰지 않음')
            self.new_ids = {x for x in candidate.xpath('.//@w:id', namespaces=NS) if x.isdigit() and int(x) >= self.next_id}
            self.root = candidate
            if ET.tostring(self.reject_own(), method='c14n') != ET.tostring(self.original_root, method='c14n'):
                raise RuntimeError('기존 담당 외 편집을 발견하여 덮어쓰지 않음')
            self.root = copy.deepcopy(self.original_root)
            self.body = self.root.find('w:body', NS)
            self.baseline_blocks = list(self.body)
            self.new_ids = set()
        self.added_blocks = []
        self.replaced = []
        self.style = {'body': 'TDC20', 'h2': 'TDC2', 'h3': 'TDC3', 'h4': 'TDC4', 'table': 'TDC21', 'thead': 'TDC0', 'cell': 'TDC7', 'cap': 'TDC22'}
        if code == 'SIP':
            self.style = {'body': 'TDCSIP4', 'h2': 'TDCSIP9', 'h3': 'TDCSIP10', 'h4': 'TDCSIP11', 'table': 'TDCSIP24', 'thead': 'TDCSIP14', 'cell': 'TDCSIP15', 'cap': 'TDCSIP30'}

    def rev(self, tag, parent):
        el = sub(parent, tag, {'id': self.next_id, 'author': AUTHOR, 'date': DATE})
        self.new_ids.add(str(self.next_id))
        self.next_id += 1
        return el

    def run(self, parent, text, missing=None, bold=False, size=20):
        r = sub(parent, 'r')
        rp = sub(r, 'rPr')
        sub(rp, 'rFonts', {'ascii': '맑은 고딕', 'hAnsi': '맑은 고딕', 'eastAsia': '맑은 고딕'})
        if bold:
            sub(rp, 'b')
        if missing:
            sub(rp, 'color', {'val': {'QCC': 'C00000', 'E8300': '0070C0', 'joint': '7030A0'}[missing]})
            sub(rp, 'u', {'val': 'single'})
        sub(rp, 'sz', {'val': 24 if missing else size})
        sub(rp, 'szCs', {'val': 24 if missing else size})
        for n, line in enumerate(text.split('\n')):
            if n:
                sub(r, 'br')
            t = sub(r, 't')
            t.set(XML_SPACE, 'preserve')
            t.text = line
        return r

    def paragraph(self, text, style=None, missing=None, heading=False):
        p = ET.Element(q('p'))
        pp = sub(p, 'pPr')
        sub(pp, 'pStyle', {'val': style or self.style['body']})
        sub(pp, 'spacing', {'after': 80 if not heading else 120})
        if heading:
            sub(pp, 'keepNext')
        else:
            sub(pp, 'keepNext', {'val': 0})
            sub(pp, 'keepLines', {'val': 0})
        pr = sub(pp, 'rPr')
        self.rev('ins', pr)
        ins = self.rev('ins', p)
        heading_size = 24 if style == self.style['h3'] else 22 if style == self.style['h4'] else 26
        self.run(ins, text, missing=missing, size=heading_size if heading else 20)
        self.added_blocks.append(p)
        return p

    def missing(self, text, kind='QCC'):
        prefix = {'QCC': '[QCC 자료 입력]', 'E8300': '[E8300 보완]', 'joint': '[QCC/E8300 자료 입력]'}[kind]
        return self.paragraph(prefix + ' ' + text, missing=kind)

    def table(self, rows, widths=None):
        count = len(rows[0])
        widths = widths or ([1600, 8038] if count == 2 else [2200, 3100, 4338])
        assert sum(widths) <= 9638
        t = ET.Element(q('tbl'))
        tp = sub(t, 'tblPr')
        sub(tp, 'tblStyle', {'val': self.style['table']})
        sub(tp, 'tblW', {'w': sum(widths), 'type': 'dxa'})
        sub(tp, 'tblInd', {'w': 0, 'type': 'dxa'})
        sub(tp, 'tblLayout', {'type': 'fixed'})
        borders = sub(tp, 'tblBorders')
        for side in ['top', 'left', 'bottom', 'right', 'insideH', 'insideV']:
            sub(borders, side, {'val': 'single', 'sz': 4, 'color': '808080'})
        margins = sub(tp, 'tblCellMar')
        for side in ['top', 'bottom']:
            sub(margins, side, {'w': 80, 'type': 'dxa'})
        for side in ['left', 'right']:
            sub(margins, side, {'w': 100, 'type': 'dxa'})
        grid = sub(t, 'tblGrid')
        for w in widths:
            sub(grid, 'gridCol', {'w': w})
        for index, row in enumerate(rows):
            tr = sub(t, 'tr')
            trp = sub(tr, 'trPr')
            if index == 0:
                sub(trp, 'tblHeader')
                sub(trp, 'cantSplit')
            self.rev('ins', trp)
            for width, value in zip(widths, row):
                tc = sub(tr, 'tc')
                cp = sub(tc, 'tcPr')
                sub(cp, 'tcW', {'w': width, 'type': 'dxa'})
                sub(cp, 'vAlign', {'val': 'center'})
                if index == 0:
                    sub(cp, 'shd', {'val': 'clear', 'fill': 'D9D9D9'})
                p = sub(tc, 'p')
                pp = sub(p, 'pPr')
                sub(pp, 'pStyle', {'val': self.style['thead'] if index == 0 else self.style['cell']})
                sub(pp, 'spacing', {'after': 0})
                sub(pp, 'keepNext', {'val': 0})
                sub(pp, 'jc', {'val': 'center' if index == 0 else 'left'})
                ins = self.rev('ins', p)
                self.run(ins, str(value), bold=index == 0)
        self.added_blocks.append(t)
        return t

    def replace(self, index, text, missing=None):
        p = self.baseline_blocks[index]
        self.replace_p(p, text, missing=missing, locator={'baseline_body_index': index})

    def replace_p(self, p, text, missing=None, locator=None):
        assert p.tag == q('p')
        old = current_text(p)
        # Preserve older revisions. The new deletion only encloses current runs.
        for r in list(p.xpath('.//w:r[not(ancestor::w:del) and w:t]', namespaces=NS)):
            parent = r.getparent()
            pos = parent.index(r)
            deletion = self.rev('del', parent)
            parent.remove(deletion)
            parent.insert(pos, deletion)
            parent.remove(r)
            deletion.append(r)
            for t in r.findall('w:t', NS):
                t.tag = q('delText')
        ins = self.rev('ins', p)
        self.run(ins, text, missing=missing)
        self.replaced.append({**(locator or {}), 'before': old, 'after': text})

    def append_table_row(self, table_index, cells):
        row_table = self.table([cells], widths=[1804, 3917, 3917])
        tr = row_table.find('w:tr', NS)
        row_table.remove(tr)
        tr.find('w:trPr/w:tblHeader', NS).getparent().remove(tr.find('w:trPr/w:tblHeader', NS))
        for p in tr.xpath('.//w:p', namespaces=NS):
            p.find('w:pPr/w:pStyle', NS).set(q('val'), 'TDCf')
            p.find('w:pPr/w:jc', NS).set(q('val'), 'left')
            for b in list(p.xpath('.//w:rPr/w:b', namespaces=NS)):
                b.getparent().remove(b)
        for shd in list(tr.xpath('.//w:tcPr/w:shd', namespaces=NS)):
            shd.getparent().remove(shd)
        self.baseline_blocks[table_index].append(tr)
        return tr

    def before(self, index, elements):
        anchor = self.baseline_blocks[index]
        position = self.body.index(anchor)
        for element in elements:
            self.body.insert(position, element)
            position += 1

    def after(self, index, elements):
        anchor = self.baseline_blocks[index]
        position = self.body.index(anchor) + 1
        for element in elements:
            self.body.insert(position, element)
            position += 1

    def reject_own(self):
        r = copy.deepcopy(self.root)
        # New paragraph marks remove the entire new paragraph; table rows remove added rows.
        for p in list(r.xpath('.//w:p', namespaces=NS)):
            mark = p.find('w:pPr/w:rPr/w:ins', NS)
            if mark is not None and mark.get(q('id')) in self.new_ids:
                p.getparent().remove(p)
        for tr in list(r.xpath('.//w:tr', namespaces=NS)):
            mark = tr.find('w:trPr/w:ins', NS)
            if mark is not None and mark.get(q('id')) in self.new_ids:
                tr.getparent().remove(tr)
        for t in list(r.xpath('.//w:tbl', namespaces=NS)):
            if not t.findall('w:tr', NS):
                t.getparent().remove(t)
        for el in reversed(list(r.xpath('.//w:ins|.//w:del', namespaces=NS))):
            if el.get(q('id')) not in self.new_ids:
                continue
            parent = el.getparent()
            if el.tag == q('ins'):
                parent.remove(el)
            else:
                pos = parent.index(el)
                parent.remove(el)
                for child in list(el):
                    for t in child.findall('w:delText', NS):
                        t.tag = q('t')
                    parent.insert(pos, child)
                    pos += 1
        return r

    def finish(self):
        reject = self.reject_own()
        assert ET.tostring(reject, method='c14n') == ET.tostring(self.original_root, method='c14n'), self.code + ' reject reconnect failed'
        docbytes = ET.tostring(self.root, encoding='UTF-8', xml_declaration=True, standalone=True)
        with zipfile.ZipFile(self.output, 'w', zipfile.ZIP_DEFLATED) as z:
            for info, data in self.entries:
                z.writestr(info, docbytes if info.filename == 'word/document.xml' else data)
        current = current_text(self.root)
        assert '[QCC 자료 입력] 여기에' not in current
        assert sha(self.original) == self.original_hash
        markers = []
        for i, block in enumerate(self.body):
            for paragraph_index, paragraph in enumerate([block] if block.tag == q('p') else block.xpath('.//w:p', namespaces=NS)):
                if any(a.tag == q('tr') and a.find('w:trPr/w:del', NS) is not None for a in paragraph.iterancestors()):
                    continue
                text = current_text(paragraph)
                if re.match(r'\[(QCC 자료 입력|E8300 보완|QCC/E8300 자료 입력)\]', text):
                    for r in paragraph.xpath('.//w:r[not(ancestor::w:del) and w:t]', namespaces=NS):
                        if current_text(r):
                            markers.append({'body_index_0_based': i, 'paragraph_index_in_block': paragraph_index, 'search': text, 'color': r.xpath('./w:rPr/w:color/@w:val', namespaces=NS), 'size_half_points': r.xpath('./w:rPr/w:sz/@w:val', namespaces=NS), 'underline': r.xpath('./w:rPr/w:u/@w:val', namespaces=NS)})
        new_revisions = [e for e in self.root.xpath('.//w:ins|.//w:del', namespaces=NS) if e.get(q('id')) in self.new_ids]
        assert all(e.get(q('author')) == AUTHOR for e in new_revisions)
        added = [{'body_index_0_based': self.body.index(e), 'kind': ET.QName(e).localname, 'search': current_text(e)[:400]} for e in self.added_blocks if e.getparent() is self.body]
        original_parts_unchanged = [n for n in self.parts if n != 'word/document.xml']
        with zipfile.ZipFile(self.output) as z:
            assert all(z.read(n) == self.parts[n] for n in original_parts_unchanged)
        (QA / f'{self.code}_current.txt').write_text(current, encoding='utf-8')
        (QA / f'{self.code}_reject_own.xml').write_bytes(ET.tostring(reject, encoding='UTF-8', xml_declaration=True))
        return {'code': self.code, 'original': str(self.original), 'output': str(self.output), 'original_sha256_before': self.original_hash, 'original_sha256_after': sha(self.original), 'output_sha256': sha(self.output), 'replaced_paragraphs': self.replaced, 'added_blocks': added, 'missing_markers': markers, 'track_validation': {'new_revision_count': len(new_revisions), 'author': AUTHOR, 'own_revision_ids': sorted(self.new_ids, key=int), 'reject_own_c14n_equals_original': True, 'accepted_current_search_text_written': True, 'original_non_document_parts_bytes_unchanged': True, 'original_media_headers_footers_styles_preserved': True}, 'rendering': '최종 Office COM 전체 페이지 QA는 root 담당. 이 기록은 OOXML/본문 연결성 검증이며 실기 또는 승인 결과 아님.'}


SCOPE = '이 절은 제공된 QCC 설계 자료를 외부기 QCC 기능으로 반영한 설계 설명이다. 원문의 스마트케이스·CC 표기는 잔존 표기로 취급하며 원문 식별자는 대응표에 보존한다. 충전케이스와의 실제 통신 기능은 외부기 QCC가 사용하는 별도 주변장치 기능으로 구분한다. QCC 소스, 라이브러리 빌드 정보와 실측 로그가 입력되지 않았으므로 구현 또는 시험 완료를 뜻하지 않는다.'


def boundary_table(e):
    return e.table([
        ['경계', '담당 기능', '연계 경로'],
        ['외부기 / E8300', 'CFX·HEAR 오디오 처리, CM3 상태/명령 처리, 자극 파라미터와 내부기 제어', 'QCC→CM3 SPI, QCC→CFX I2S, CM3→QCC 제어 신호'],
        ['외부기 / QCC', 'System management, HW interface, Bluetooth, Security의 4개 application 모듈', 'Qualcomm 내장 라이브러리 API, BLE/Classic 무선 연결, PIO/NVM/배터리'],
        ['주변 장치', '리모콘·매핑 프로그램·Classic 오디오 장치·충전케이스', 'BLE AUTH/GAIA, Classic 오디오, 충전케이스 통신'],
        ['내부기', 'E8300에서 전달하는 자극 명령과 PCM 출력의 상대 장치', 'E8300의 내부기 제어/자극 경로. QCC가 직접 자극을 계산·출력하는 경로로 기술하지 않음'],
    ])


def contract_paragraphs(e):
    return [
        e.paragraph('SPI 연계: E8300의 수신 정의는 0x34(data[0] 배터리 %, data[1] 충전기 미연결 0 / 연결 1, data[2] 뚜껑 열림 1 / 닫힘 2), 0x35(data[0] LED 상태, OTA_EZAIRO=4일 때 갱신 연결 1), 0x36(data[0] 오디오 미연결 0 / 연결 1, data[1] 종류 미상 0 / 크래들 1 / 기타 2)이다. 응답은 각각 명령값과 성공값 1이다. 0x36은 연결 1·종류 1일 때만 크래들 오디오 상태를 설정한다.'),
        e.paragraph('메시지 경계: QCC BLE Data 메시지는 type 1바이트와 data 0~19바이트로 총 1~20바이트이다. E8300 SPI 전송 프레임은 별도 길이 1바이트를 포함한 21바이트이다. 무선 메시지와 물리 SPI 프레임을 같은 계층의 길이로 취급하지 않는다.'),
        e.paragraph('제어 핀: E8300 QCC_CTRL(DIO24)은 High=정상 동작 요청, Low=종료 요청이며 초기값은 Low이다. ISD_CHECK(DIO13)는 High=내부기 연결, Low=미연결이며 초기값은 Low이다. CFX I2S_FLAG(DIO29)는 Low=입력 유효이다. QCC의 shutdown_ind 또는 I2S enable이라는 논리 이름만으로 물리 핀 극성을 변경하지 않는다.'),
        e.paragraph('오디오 경로: QCC Classic 수신 오디오는 I2S로 CFX에 전달한다. SRS의 출력 사양은 24bit·16kHz이다. E8300 PCM1은 slave, 24bit, MSB 우선/정렬, 상승 에지, 2word 설정이며 16표본 블록을 처리한다. 16kHz 표본율과 0~8kHz 오디오 대역 요구는 구분한다. CFX는 일반 I2S 게인 1과 마이크 Gain A, 크래들 I2S Gain B와 마이크 Gain A, 매핑 연결 중 두 게인 1을 적용하며 혼합 후 2로 나누지 않는다.'),
        e.missing('QCC shutdown_ind의 on/off 본문·그림이 상충한다. 실제 PIO 번호·전압·논리 반전, 기동/종료 대기와 I2S enable→I2S_FLAG Low 유효의 물리 대응을 확인해야 한다.'),
        e.missing('충전 미연결/연결/충전 중/비충전의 4상태를 0x34 충전기 연결 0/1로 변환하는 표, 0x34~0x36 통보 시점/길이/오류 응답과 GAIA addressing·type별 데이터 정의를 제출해야 한다.'),
        e.missing('24bit·16kHz의 QCC 출력 설정, I2S 클럭/프레임·좌우 배정과 시작/중단 시 유효 신호 파형을 확인해야 한다. E8300 정적 수신 설정과 제공 설계의 대응만 확인했으며 실제 오디오/핀 파형 측정은 미제공이다.'),
    ]


def update_sad_forward_trace(e):
    selected = {
        15: [1, 5, 8], 44: [2, 4, 9, 11], 45: [2, 4, 9, 11],
        47: [9, 12], 48: [9, 10, 12], 49: [1, 9, 10, 12],
        52: [7, 10, 12, 13, 14], 60: [11], 69: [3, 4, 5, 8],
        70: [2, 3, 4, 9, 11], 71: [10, 12], 72: [12],
        76: [7, 9, 10, 11, 12, 13, 14],
    }
    gaps = {
        15: ('QCC', '24bit·16kHz I2S 클럭/프레임·유효 신호의 실제 설정과 파형 자료가 필요하다.'),
        44: ('QCC', '리모콘 AUTH 접근 권한과 GAIA 주소·명령별 데이터/응답의 실제 계약 및 실행 자료가 필요하다.'),
        45: ('QCC', '매핑 권한·키 역할, AUTH UUID 및 GAIA 주소·명령별 데이터/응답의 실제 계약과 실행 자료가 필요하다.'),
        47: ('joint', '기존 무제어명령 조건과 BLE 무송수신180초 조건의 범위·타이머 리셋 및 허용오차/경계 처리를 함께 확인해야 한다.'),
        48: ('QCC', 'BLE pairing 이후 AUTH 접근과 인증 전 데이터 차단의 실제 UUID/권한 설정 및 조건별 실행 자료가 필요하다.'),
        49: ('QCC', 'BLE 버전·암호화 활성 설정과 라이브러리/프로파일, AES-CCM 보호 범위 및 실제 링크 검증 자료가 필요하다. CRA 인증 설계를 전송 암호화의 근거로 사용하지 않는다.'),
        52: ('QCC', 'SHA-256의 매핑 키 생성과 AES-CMAC의 인증값 계산 역할, 원문 HMAC/ENC 명칭 및 입력 직렬화/태그 길이의 정합성을 확정해야 한다.'),
        60: ('QCC', 'BLE/GAIA CRC의 실제 알고리즘·적용 바이트·저장 순서와 오류 응답/검증 자료가 필요하다.'),
        69: ('QCC', 'Classic 프로파일/코덱·좌우 배정, 오디오/I2S 시작·중단과 상태통보 타이밍 및 실행 자료가 필요하다.'),
        70: ('QCC', '실제 BLE 광고/연결 구성과 주소·회사ID·바이트 길이, 편이/양이 데이터 배정 및 실행 자료가 필요하다.'),
        71: ('QCC', '인증4상태의 실제 응답, 미완료30초 타이머 시작/리셋·허용오차 및 정보 미노출 결과가 필요하다.'),
        72: ('QCC', '연속5회 실패 누적/초기화·전원 재기동, 60초 제한의 시작/해제·재접속과 시간 경계 결과가 필요하다.'),
        76: ('QCC', '설치 shared_service_key/clinician_master_key와 생성 clinic_service_key, NVM ID/길이·암호화/접근 제한과 난수 bootstrap 및 실제 보호 결과가 필요하다.'),
    }
    replaced_cells = 0
    table = e.baseline_blocks[148]
    for tr in table.findall('w:tr', NS):
        if tr.find('w:trPr/w:del', NS) is not None:
            continue
        cells = tr.findall('w:tc', NS)
        if len(cells) != 3:
            continue
        match = re.fullmatch(r'SW-FW-RID-(\d+)', current_text(cells[0]).strip())
        if not match or int(match.group(1)) not in selected:
            continue
        rid = int(match.group(1))
        units = [DATA['units'][i - 1] for i in selected[rid]]
        modules = [m for m in DATA['modules'] if m['id'] in {u['module_id'] for u in units}]
        for ci, entries in [(1, [m['id'] + ' ' + m['title'] for m in modules]), (2, [u['id'] + ' ' + u['title'] for u in units])]:
            placeholders = [p for p in cells[ci].findall('w:p', NS) if '[QCC 자료 입력]' in current_text(p)]
            assert len(placeholders) == 1, (rid, ci)
            e.replace_p(placeholders[0], '\n'.join(entries), locator={'table_baseline_body_index': 148, 'requirement': f'SW-FW-RID-{rid}', 'cell_index': ci})
            replaced_cells += 1
        cells[2].append(e.missing(gaps[rid][1], gaps[rid][0]))
    assert replaced_cells == 26
    changes = json.loads((EVIDENCE / 'qcc_requirements_change_plan.json').read_text(encoding='utf-8'))
    positions = {15: 'F20', 16: 'F21', 47: 'F56', 52: 'F61', 54: 'F64', 71: 'F65', 72: 'F66', 76: 'F70', 82: 'F49', 83: 'F80'}
    values = {x['cell']: x['value'].strip().split('\n\n', 1)[0] for x in changes['cell_edits'] if x['cell'] in positions.values()}
    requirements = {rid: values[cell] for rid, cell in positions.items()}
    for tr in e.baseline_blocks[145].findall('w:tr', NS):
        if tr.find('w:trPr/w:del', NS) is not None:
            continue
        cells = tr.findall('w:tc', NS)
        if len(cells) != 3:
            continue
        match = re.fullmatch(r'SW-FW-RID-(\d+)', current_text(cells[0]).strip())
        if not match or int(match.group(1)) not in requirements:
            continue
        rid = int(match.group(1))
        paragraphs = [p for p in cells[1].findall('w:p', NS) if current_text(p)]
        if paragraphs:
            e.replace_p(paragraphs[0], requirements[rid], locator={'table_baseline_body_index': 145, 'requirement': f'SW-FW-RID-{rid}', 'cell_index': 1})
            for p in paragraphs[1:]:
                e.replace_p(p, '', locator={'table_baseline_body_index': 145, 'requirement': f'SW-FW-RID-{rid}', 'cell_index': 1})
    for rid in [82, 83]:
        module = 'SI-ID-QCC-003 블루투스 모듈' if rid == 82 else '제품 사용 설명서 / 실행 모듈 배정 대상 아님'
        unit = 'SU-ID-QCC-008 Bluetooth Classic 유닛' if rid == 82 else '제품 문서 항목 / 실행 유닛 배정 대상 아님'
        tr2 = e.append_table_row(148, [f'SW-FW-RID-{rid}', module, unit])
        gap = '외부기 마이크의 QCC 입력 경로·Classic 송신 프로파일/코덱·상대 장치·동시 송수신과 실제 자료가 필요하다.' if rid == 82 else '제품 사용 설명서 파일과 정식 상위 요구사항 식별/추적을 보완해야 한다.'
        for tr in [tr2]:
            tr.findall('w:tc', NS)[2].append(e.missing(gap, 'QCC' if rid == 82 else 'E8300'))


def author_sad():
    e = Editor('SAD')
    update_sad_forward_trace(e)
    e.replace(137, 'QCC 내부 소프트웨어는 시스템 관리, HW 인터페이스, 블루투스, 보안의 4개 application 모듈과 14개 유닛으로 구성한다. 아래 설명과 추적표는 제공 설계에 근거하며 미비 사항은 해당 위치에 표시한다.')
    els = [e.paragraph(SCOPE), boundary_table(e)]
    for m in DATA['modules']:
        els.append(e.paragraph(m['id'] + ' ' + m['title'], e.style['h3'], heading=True))
        els.append(e.paragraph(m['responsibility'] + '.'))
        names = [u['id'] + ' ' + u['title'] for u in DATA['units'] if u['module_id'] == m['id']]
        els.append(e.paragraph('소속 유닛: ' + '; '.join(names) + '.'))
    els += contract_paragraphs(e)
    els.append(e.paragraph('모듈 사이의 데이터 경로: 시스템 관리가 PIO/전원/장치정보를 수집하여 Bluetooth 활성과 SPI 중계를 조정한다. Bluetooth의 AUTH service는 보안 처리의 Challenge/Response/Confirm/Status를 전달하며 일반 BLE Data는 인증·권한 조건을 만족한 메시지를 시스템 관리에 전달한다. 보안 처리는 toolbox와 보안 NVM을 사용하고, 보안 NVM은 HW 인터페이스 NVM을 통해 실제 저장 영역에 접근한다. Classic 오디오는 내장 라이브러리를 거쳐 I2S로 E8300에 전달한다.'))
    els.append(e.missing('인증 전 데이터 차단/앱별 권한, 실제 AUTH UUID와 characteristic 길이, CRA 알고리즘·키 역할, NVM 보호·난수 bootstrap, 무통신/실패 제한 경계, QCC 자체 OTA와 기동 이미지 무결성의 상세 설계 및 구현 근거가 필요하다.'))
    e.after(137, els)
    rows = [['QCC 유닛 / 원문 ID', '모듈', '연결 요구사항']]
    for u in DATA['units']:
        rows.append([u['id'] + '\n' + u['title'] + '\n원문 ' + u['original_id'], u['module_id'], rid_list(u)])
    rows.append(['SW-FW-RID-83 사용자 설명서', '제품 문서 / E8300 보완', '사용 설명서의 안전 예방조치·사용 방법·문제 해결. QCC 실행 유닛에 억지로 배정하지 않음'])
    e.after(148, [e.paragraph('표 QCC-1. 제공 QCC 설계의 요구사항·모듈·유닛 연결', e.style['cap']), e.table(rows), e.paragraph('이 표는 기존 E8300의 추적성 표를 보완한다. 기능 연결은 설계 대응을 나타내며 실제 유닛 소스/시험 합격을 뜻하지 않는다. SW-FW-RID-82의 외부기 마이크 Classic 송신은 SU-ID-QCC-008의 추가 설계 대상이나 제공 상세설계는 수신 오디오 중심이므로 송신 경로는 자료 미비로 남긴다.'), e.missing('SW-FW-RID-82 마이크의 QCC 입력 경로, Classic 프로파일/코덱, 상대 장치와 동시 송수신·I2S 방향 및 설계·검증 자료가 필요하다.'), e.missing('SW-FW-RID-83 제품 사용 설명서 파일과 정식 상위 요구사항 연결을 보완해야 한다.', 'E8300')])
    e.after(155, [e.paragraph('QCC 지원 소프트웨어: 제공 설계는 Qualcomm BSP/Built-in library의 초기화, Classic/TWS/BLE, 전원·NVM 기능을 application API로 사용하고 내장 라이브러리를 직접 수정하지 않는다고 설명한다.'), e.missing('Qualcomm ADK/BSP와 Classic/TWS/BLE/GAIA/암호·NVM 라이브러리의 정확한 제품명·버전·구성·API, 알려진 문제 및 사용 검토 자료를 SOUP 목록에 연결해야 한다.')])
    e.replace(159, '기존 E8300의 SW-FW-RID-1~81 추적과 신규 SW-FW-RID-82~83을 구분하여 검토하였다. 제공 QCC 설계의 4모듈·14유닛 연결은 QCC-1 표에 추가하였다. RID-82 송신 상세설계, RID-83 문서/상위 추적과 본문의 자료 미비 사항은 아직 완료로 판단하지 않는다.')
    e.replace(162, 'E8300 공유 메모리·SPI·I2S·제어 핀의 기존 연계와 제공 QCC 모듈 간 기능 연결을 대조하였다. E8300 정적 인터페이스 정의는 유지한다. QCC 종료 극성·충전 상태 변환·실제 무선/오디오/보안 계약의 상충과 실측 미제공은 해당 미비 안내로 보존한다. 본 검토는 제공 설계의 분석이며 구현 또는 통합시험 통과를 의미하지 않는다.')
    return e.finish()


def author_sddd():
    e = Editor('SDDD')
    e.replace(94, '기존 그림의 장치/칩 경계를 유지하고 QCC의 기능 경계는 아래 표와 QCC 소프트웨어 상세 설계의 4모듈·14유닛으로 구분한다. QCC는 무선·PIO·전원·보안·중계를 담당하며 E8300의 CFX/HEAR 처리와 CM3 내부기 제어/자극 실행을 대신하는 블록으로 해석하지 않는다.')
    e.after(94, [boundary_table(e)])
    e.replace(2550, 'QCC 측 연계는 아래의 제공 설계와 E8300 정적 수신 정의를 대조하여 기술한다. E8300 CM3/CFX 기존 유닛의 명령·오디오·제어 핀 처리는 유지하며 QCC 소스/실측이 필요한 계약은 미비로 표시한다.')
    e.after(2552, contract_paragraphs(e))
    e.replace(2554, 'QCC 내부 설계는 4개 모듈과 14개 유닛으로 정리한다. SU-ID-QCC 식별자는 본 문서의 ID이며 원문의 SW-BT-DS-ID/SW-FW-DS-ID를 함께 보존한다. 입력·출력·처리는 제공 상세설계의 내용이고 실제 코드 구현 확인과는 구분한다.')
    els = [e.paragraph(SCOPE)]
    for m in DATA['modules']:
        els.append(e.paragraph(m['id'] + ' ' + m['title'], e.style['h3'], heading=True))
        els.append(e.paragraph(m['responsibility'] + '.'))
        for u in [x for x in DATA['units'] if x['module_id'] == m['id']]:
            els.append(e.paragraph(u['id'] + ' ' + u['title'], e.style['h4'], heading=True))
            processing = list(u['process'])
            if u['id'] == 'SU-ID-QCC-013':
                processing = ['생산 시 저장하는 리모콘 shared_service_key와 매핑 clinician_master_key를 구분한다. 제공 설계는 SHA-256(clinician_master_key, serial_num) 결과의 앞 16바이트를 clinic_service_key로 생성한다고 설명한다.', '인증값 계산에 사용하는 Nonce_A/Nonce_B는 각각 16바이트이다. 원문은 HMAC_A/HMAC_B 및 ENC라는 이름을 사용하면서 알고리즘을 AES-CMAC이라고 설명한다. A/B의 nonce 결합 순서를 구분하지만 정확한 primitive·입력 직렬화·태그 길이가 미확정이므로 단정적인 실행 계산식으로 사용하지 않는다.', 'NVM은 AES-128을 사용하고 BD_ADDR 6바이트와 random 10바이트를 연결한 16바이트 키를 사용한다고 설명한다. 실제 모드/IV/무결성/키 bootstrap은 확인되지 않았다.', 'SHA-256의 키 생성 역할과 AES-CMAC의 인증값 계산 역할을 분리하여 검토하고, SRS의 앱별 알고리즘 구분과 상세설계의 역할 정합성은 자료 미비로 보존한다.']
            if u['id'] == 'SU-ID-QCC-012':
                processing += ['최신 SRS/시험 기준의 인증 상태는 실패 0 / 성공 1 / 진행 중 2 / 타임아웃 3이다. 인증 미완료 30초 타임아웃, 연속 5회 실패 후 60초 제한, 무송수신 180초 해제는 요구·시험 조건으로 연결하며 타이머 시작/리셋·카운터와 경계 처리 구현은 추가 설계가 필요하다.']
            if u['id'] == 'SU-ID-QCC-008':
                processing += ['SW-FW-RID-82는 외부기 마이크 오디오의 Classic 송신을 요구한다. 이 원문 유닛은 상대 장치에서 받은 오디오의 I2S 출력을 설명하므로 송신 처리가 이미 설계/구현되었다고 해석하지 않는다.']
            rows = [
                ['항목', '제공 설계 내용'],
                ['목적 / 원문 추적', u['module_id'] + '\n' + source_ref(u)],
                ['상위 SW 요구사항', rid_list(u)],
                ['유닛 시험 연결', unit_test_ids(u) + ('\nID-8은 Bluetooth activation의 연계 확인이다.' if u['id'] in ['SU-ID-QCC-008', 'SU-ID-QCC-009'] else '')],
                ['입력', '\n'.join(u['input'])],
                ['출력', '\n'.join(u['output'])],
                ['처리', '\n'.join(f'{i + 1}. {x}' for i, x in enumerate(processing))],
            ]
            els.append(e.table(rows))
            gaps = list(u['missing_or_conflicting'])
            if u['id'] == 'SU-ID-QCC-008':
                gaps.append('RID-82 마이크 입력·Classic 송신 프로파일/코덱·상대장치·동시 송수신과 검증 자료')
            els.append(e.missing('; '.join(gaps) + '.'))
    els += [e.paragraph('키·NVM 식별자 대응', e.style['h3'], heading=True), e.table([
        ['원문 위치 / 이름', '제공 설명', '확정되지 않은 대응'],
        ['일반 NVM 표: AUTH_CLLINC_KEY / AUTH_SHARED_KEY', '외부기 일련번호·장치 타입·보안 난수와 함께 저장', '오탈자 포함 원문 ID 보존. 보안 NVM의 ID·길이·역할로 변환하는 표 필요'],
        ['보안 NVM 표: AUTH_RAND_NUM / AUTH_SERVICE_KEY / AUTH_CLINIC_MASTER_KEY', '보안 데이터는 toolbox 암복호화 후 HW NVM을 통해 읽기/쓰기', 'master key의 원문 설명은 리모콘으로 적혀 매핑 역할과 상충. 모든 데이터 암호화 주장과 random 키 재료 읽기의 bootstrap 미비'],
        ['toolbox: shared_service_key / clinician_master_key / clinic_service_key', '설치키와 SHA-256으로 생성한 매핑 service key 구분', '저장하는 키·생성하는 키·앱 권한, 입력 직렬화/길이, AES-CMAC/HMAC/ENC 명칭 정합성 필요'],
    ]), e.missing('실제 AUTH service/characteristic UUID, Challenge/Response/Confirm/Status 길이·엔디언·접근 권한을 입력해야 한다. Confirm은 제공 표/본문의 client write를 설계 대응으로 적되 그림의 notify와 상충을 해소해야 한다.'), e.missing('QCC 자체 OTA의 이미지/인증/실패 복구와 E8300 이미지 전달 경로를 분리한 설계, QCC 기동 이미지 무결성의 알고리즘·검사 실패 처리·대상 범위 및 구현 근거가 필요하다. E8300 CRC/암호화 검사를 QCC 검사의 근거로 사용하지 않는다.'), e.missing('무제어 명령 180초와 BLE 무송수신 180초의 타이머 리셋 범위, 인증 실패 누적·연결 해제/재접속·60초 제한 종료의 경계와 E8300 통보 영향을 함께 확인해야 한다.', 'joint')]
    e.after(2554, els)
    e.replace(2556, '제공 설계 4모듈·14유닛의 역할, 입력·출력·처리와 원문 식별자 및 상위 SW 요구사항을 연결하였다. E8300 정적 인터페이스와 QCC 제공 자료의 상충은 미비 표시로 보존하였다. 실제 QCC 소스, 설정값, 바이너리 및 실행 로그는 미제공이므로 상세설계 검토를 구현 또는 시험 통과로 확대하지 않는다.')
    e.replace(2558, '유닛 사이 데이터의 단위·저장 형식·처리 완료·초기 상태·오류 처리가 서로 일치해야 한다. QCC에서는 종료 극성, 충전 상태 변환, 실제 AUTH/GAIA 포맷, 알고리즘과 설치키/생성키 역할 및 난수 bootstrap이 아직 정합성 확인 대상이다. 수치·배열·파형·실측 비교는 유닛/통합/시스템 시험의 해당 기준 행과 결과 행에서 수행하고 미실행을 통과로 기록하지 않는다.')
    return e.finish()


def author_sip():
    e = Editor('SIP')
    e.replace(91, 'QCC 구현 준비는 SI-ID-QCC-002 HW 인터페이스의 PIO/Power/NVM과 SI-ID-QCC-004 보안의 키/NVM 계약에서 시작한다. 이후 시스템 초기화·상태·장치 구성·Bluetooth 상태를 연결한다. E8300 인터페이스 정의를 유지하고 QCC PIO 극성·전원 우선순위의 자료 미비를 선행 조건으로 확인한다.')
    e.replace(97, 'SI-ID-QCC-003은 Classic, BLE, AUTH service, BLE Data를 단계적으로 연결한다. AUTH service는 보안 처리·toolbox·보안 NVM과 연계하고, 인증된 BLE 명령은 시스템 상태 유닛을 통해 SPI로 중계한다. Classic 오디오 수신/I2S와 외부기 마이크 송신은 서로 다른 방향으로 검토한다.')
    e.after(97, [e.paragraph(SCOPE), e.table([
        ['모듈', '유닛', '선행 계약 / 준비 자료'],
        ['SI-ID-QCC-002 HW 인터페이스', 'SU-ID-QCC-005 PIO\nSU-ID-QCC-006 Power\nSU-ID-QCC-007 NVM', '핀 매핑/초기값, 전원·충전 상태, NVM ID/길이·오류 처리'],
        ['SI-ID-QCC-004 보안', 'SU-ID-QCC-012 보안 처리\nSU-ID-QCC-013 toolbox\nSU-ID-QCC-014 보안 NVM', '설치키/생성키·키표 대응, nonce/primitive, 보안 NVM bootstrap, AUTH 접근 권한'],
        ['SI-ID-QCC-001 시스템 관리', 'SU-ID-QCC-001 초기화\nSU-ID-QCC-002 시스템 상태\nSU-ID-QCC-003 장치 상태\nSU-ID-QCC-004 Bluetooth 상태', 'HW/BSP 초기화 API, SP 제어 신호/정보, Standalone/TWS 구성·좌우 정책'],
        ['SI-ID-QCC-003 Bluetooth', 'SU-ID-QCC-008 Classic\nSU-ID-QCC-009 BLE\nSU-ID-QCC-010 AUTH\nSU-ID-QCC-011 BLE Data', '무선 설정, UUID/GAIA 데이터 계약, 보안 처리·시스템 중계, I2S 설정'],
    ]), e.missing('실제 QCC 소스/빌드·ADK/BSP 버전과 단위 실행 로그를 제출해야 한다. 아래 단계는 통합 계획이며 구현/단위시험이 완료된 것으로 가정하지 않는다.')])
    e.replace(150, 'QCC 내부 통합은 아래 QCC-I-01~07 단계와 선행 조건에 따라 수행한다. 해당 단계의 기준/보고 행은 통합시험 기준서와 보고서에 같은 구조로 적용하며 미실행·실측 미제공 상태를 보존한다.')
    rows = [
        ['단계 / 대상', '연결 및 확인 내용', '완료 근거 / 보류 조건'],
        ['QCC-I-01 HW 기반\n005+006+007', 'PIO 입력/출력/주변기능과 Power·NVM을 연결한다. 전원 인가·충전 상태 전환·읽기/쓰기 실패·초기값을 각각 확인한다.', '실제 핀/전압·배터리 임계값·전압→잔량·NVM 길이와 실패 처리 필요. 충전 상태 4종→0x34 binary 미비 보존'],
        ['QCC-I-02 보안 기반\n013+014+007', '일반/보안 NVM의 설치키와 생성키, random 및 읽기/쓰기·암복호화 경로를 연결한다.', 'primitive 입력/출력 벡터, SHA-256 직렬화, AES 모드/IV/무결성, rand_num bootstrap 필요. 합성 결과를 실측으로 사용하지 않음'],
        ['QCC-I-03 시스템 상태\n001+002+003+004+005+006', '설정→system→내장 library→scheduler→Power→Bluetooth→HW→PIO 초기화를 연결한다. SP 준비/종료, 내부기 연결/분리, Standalone/TWS와 장치명을 조건별로 확인한다.', 'QCC shutdown_ind 상충, 대기/디바운스·좌우/단독 구성과 충전 예외 우선순위를 해소한 기준 필요'],
        ['QCC-I-04 BLE 인증\n009+010+012+013+014', 'pairing 후 AUTH 접근, Patient 1/Clinician 2, Challenge→Response→Confirm→Status를 연결한다. 실패0/성공1/진행2/타임아웃3, invalid type·wrong response·미완료30초를 비교한다.', '실제 UUID/길이·권한, Confirm write/notify 상충, 30초 시작/리셋/허용오차 필요. 실패/미인증 데이터 허용 범위 확인'],
        ['QCC-I-05 BLE 데이터\n009+011+002+004+012', '인증·권한별 GAIA type/data와 SPI 중계를 연결한다. 유효1~20byte와 invalid length/address/type, 연결 해제와 재접속을 비교한다. 무송수신180초 및 연속5회 실패 후60초 제한도 확인한다.', 'GAIA 주소·메시지/CRC·응답, E8300 무제어명령 기준과 무TX/RX 리셋 범위, 실패카운터/60초 경계 자료 필요'],
        ['QCC-I-06 오디오\n008+003+004+005+002', 'Classic/Audio 연결·오디오만 해제·전체 해제 및 TWS/단독을 연결한다. 24bit16kHz I2S와0x36종류/I2S_FLAG를 E8300 CFX 경로에 연결한다. RID82 마이크 Classic 송신을 별도 방향으로 확인한다.', '프로파일/코덱/좌우·클럭/프레임/시작중단 유효신호와 실제 자료 필요. RID82 송신 상세설계 미비'],
        ['QCC-I-07 전체 및 갱신\n4모듈+E8300', 'SPI 전원·LED/갱신·오디오 상태, 리모콘/매핑 요청→실행→응답, 종료/충전/연결 전환을 통합한다. QCC 자체 OTA와 E8300 이미지 전달·부트 검사도 분리한다.', 'QCC 구현·실측, OTA 자체/중계·재시도·실패복구 및 QCC 무결성 자료 필요. E8300 과거 결과를 QCC 통과로 재사용하지 않음'],
    ]
    stage_tests = {1: '018/019', 2: '024', 3: '019/023', 4: '020/021', 5: '020/021/023', 6: '022', 7: '018~024 및 기존 E8300 시험의 해당 조건'}
    for i, row in enumerate(rows[1:], 1):
        row[2] += '\n통합 기준 연결: SW-FW-IntegrationTest-ID-' + stage_tests[i]
    e.after(150, [e.table(rows), *contract_paragraphs(e), e.paragraph('시험 기록: 유닛 시험은 15그룹(SW-FW-UnitTest-QCC-ID-1~15)이 14설계유닛에 대응한다. Bluetooth activation 그룹은 SU-ID-QCC-004가 주담당이고008/009가 연계된다. 통합과 시스템 시험은 상위 요구 기능을 함께 적고 준비 조건·입력·구체 예상 메시지/값·절차·통과 기준을 본문 행에 둔다. 보고서는 기준서의 공통 열·행/병합 구조를 그대로 유지하면서 같은 입력 조건 옆에 실제 시험/측정 내용과 판정을 추가한다.'), e.paragraph('신규 통합시험의 ID는 SW-FW-IntegrationTest-ID-018 전원 상태/SPI, 019 연결·종료 신호, 020 AUTH 패턴 중계, 021 실제 CRA와 제어 게이트/시간 제한, 022 Classic/I2S 오디오, 023 BLE 편이·양이 데이터, 024 보안 NVM/키 생성/CRA 계산이다. AUTH 패턴 중계 시험을 실제 CRA 통과로 확대하지 않으며, 각 단계를 해당 기준서·보고서의 같은 입력 조건과 연결한다.'), e.paragraph('수치·메시지·시간 비교는 T로 수행하고 단순 설계 검토와 구분한다. 생성한 예상 SPI/BLE 데이터, 정적 코드 분석과 실제 로그/파형을 구분한다. 제공 QCC 시험 자료는 기준이므로 신규 결과는 미실행·판정 대기이며 실제 수집 자료가 없으면 통과로 기록하지 않는다. 사진/파형이 필요한 자리는 (사진필요)로 두고 예상 도식은 실측이 아님을 표시한다.'), e.missing('QCC 통합 실행 로그, 실제 입력/응답 전체 메시지, 오디오/핀 파형·장비/빌드 식별 및 시험자 확인 기록이 필요하다. 미해소 설계 상충은 시험 판정에도 유지한다.')])
    e.replace(156, '[QCC 자료 입력] QCC 자체 펌웨어 갱신과 E8300 이미지 전달의 명령·버퍼·순서·재시도·인증·오류/중단 복구를 각각 정의하고 구현/통합 결과를 제출해야 한다. E8300 OTA 슬롯1~2와 부트 상태 슬롯1~4, 0x35 갱신 연결 통보를 구분하며 QCC 기동 이미지 무결성 검사 범위·알고리즘·실패 처리도 별도로 확인해야 한다.', missing='QCC')
    return e.finish()


def main():
    reports = [author_sad(), author_sddd(), author_sip()]
    result = {'schema': 'qcc-design-authored-v1', 'date': DATE, 'owner': 'qcc_design', 'status': '3개 사본 본문/추적 반영 및 OOXML 연결성 검증 완료; root 최종 Word COM 렌더/전체페이지 QA 대기', 'contract': str(EVIDENCE / 'integration-contract.json'), 'original_policy': '원본 6개 DOCX 및 코드/공통MD 불변. root가 생성한 사본3개만 수정. artifact operation marker 재실행 없음.', 'modules': DATA['modules'], 'units': [{k: u[k] for k in ['id', 'original_id', 'title', 'module_id']} for u in DATA['units']], 'documents': reports, 'source_sha256_end': [{'path': x['path'], 'sha256': sha(Path(x['path']))} for x in DATA['source_sha256_initial']]}
    (EVIDENCE / 'qcc_design_authored.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'outputs': [{'code': r['code'], 'sha256': r['output_sha256'], 'new_revision_count': r['track_validation']['new_revision_count'], 'markers': len(r['missing_markers'])} for r in reports]}, ensure_ascii=True))


if __name__ == '__main__':
    main()
