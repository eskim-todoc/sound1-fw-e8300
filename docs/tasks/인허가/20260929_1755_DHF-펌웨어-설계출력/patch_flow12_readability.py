"""Preserve the saved author edits; patch only flow sizes, unit gaps and chapter 3."""
from copy import deepcopy
from datetime import datetime, timezone
from hashlib import sha256
from pathlib import Path
import json
import re
import zipfile
from lxml import etree as E

T = Path(__file__).resolve().parent
LIVE = T.parents[2] / '참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어/SDDD-FW-TD24A 소프트웨어 상세 설계서.docx'
QA = T / 'qa/design-readability/flow12-and-module-explanation'
QA.mkdir(parents=True, exist_ok=True)
N = {'w': 'http://schemas.openxmlformats.org/wordprocessingml/2006/main',
     'wp': 'http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing',
     'a': 'http://schemas.openxmlformats.org/drawingml/2006/main'}
def q(s): return '{' + N['w'] + '}' + s
def text(p): return ''.join(p.xpath('.//w:t[not(ancestor::w:del)]/text()', namespaces=N))
def packed(path):
    with zipfile.ZipFile(path) as z: return {n: z.read(n) for n in z.namelist()}

raw = LIVE.read_bytes()
assert sha256(raw).hexdigest() == '30f3530e04d282742084cb3e23007c7ec213277e21a1a106810aab75077c407b', 'Saved document changed; reread before editing.'
backup = QA / 'before.docx'
if backup.exists(): assert backup.read_bytes() == raw
else: backup.write_bytes(raw)
review_copy = LIVE.with_name('[작성자 전용] ' + LIVE.name)
review_hash = sha256(review_copy.read_bytes()).hexdigest() if review_copy.exists() else None
parts = packed(LIVE)
root = E.fromstring(parts['word/document.xml'])
original = deepcopy(root)
body = root.find(q('body'))
next_id = max(int(v) for v in root.xpath('//@w:id', namespaces=N) if v.isdigit()) + 1
stamp = datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
def rev(tag):
    global next_id
    e = E.Element(q(tag), {q('id'): str(next_id), q('author'): '김은수', q('date'): stamp})
    next_id += 1
    return e

def para(value, level):
    p = E.Element(q('p')); pp = E.SubElement(p, q('pPr'))
    E.SubElement(pp, q('pStyle'), {q('val'): f'TDC{level}0'})
    # Track the inserted paragraph mark, as well as its text.
    rp = E.SubElement(pp, q('rPr')); rp.append(rev('ins'))
    if value:
        ins = rev('ins'); p.append(ins); r = E.SubElement(ins, q('r'))
        E.SubElement(r, q('t')).text = value
    return p

def find(value):
    matches = [p for p in body.findall(q('p')) if text(p) == value]
    assert len(matches) == 1, (value, len(matches))
    return matches[0]

added = []
def after(anchor, lines, level=2):
    node = find(anchor)
    for line in lines:
        assert '·' not in line
        p = para(line, level); node.addnext(p); node = p; added.append(line)

after('MCU간 제어 인터페이스', [
    '외부장치의 동작은 시스템 제어, 신호처리, 내부기 실행, 블루투스 통신의 네 기능 모듈로 나누어 설계한다. 하나의 기능을 여러 코어가 나누어 수행하므로, 이 장에서는 각 모듈의 담당 범위와 코어 사이에 전달할 데이터, 실행 순서를 설명한다.',
    '각 모듈은 아래의 역할을 수행하는 유닛으로 나누어 구현한다. 뒤의 상세 설계에서는 CFX, HEAR, CM3별로 담당 모듈과 유닛을 정의하고 입력, 출력, 처리 방법을 설명한다. 부트로더는 응용 프로그램을 시작하기 전의 별도 단계로 다룬다. QCC는 E8300과의 연결을 설명하고 내부 상세 설계는 별도 자료를 반영한다.'
], 1)
after('CFX와 Hear : 시스템 라이브러리, 인터럽트', [
    '시스템 제어는 전원과 연결 상태에 따라 실행할 기능을 정하고 각 코어가 필요한 시점에 동작하도록 한다. 따라서 CM3에는 전원/절전 상태, 터치 입력, LED 표시, 설정 저장과 주변장치 통신을 담당하는 유닛이 필요하다. CFX에는 인터럽트와 반복 실행, 입력 버퍼 준비, 맵/볼륨 변경 반영을 담당하는 유닛이 필요하다.',
    '설정값을 전달하는 일과 그 설정으로 처리를 시작하는 일을 구분한다. CM3가 공유 메모리에 설정한 값은 CFX가 반영하고, FIFO와 HEAR의 완료 이벤트를 확인한 뒤 다음 처리를 진행하도록 한다. 아직 준비되지 않은 입력이나 계산 결과를 사용하지 않도록 상태 확인과 완료 처리를 둔다.'
])
after('CM3는 맵의 T/C 전류와 펄스폭으로 내부기 파라미터를 계산한다. CFX는 전달된 디지털 T/C와 오디오 설정으로 매핑 계수를 계산하고, HEAR의 벡터/주파수 분석 결과로 자극 크기를 만든다.', [
    '입력 소리가 전극별 자극 크기로 바뀌기까지의 단계를 나누어 구현한다. CFX에는 마이크/I2S 입력 선택과 혼합, 두 마이크 빔포밍, AGC, 주파수 대역 대표값 계산, T/C 범위 매핑과 CIS/NofM 처리를 담당하는 유닛을 둔다. HEAR에는 절대값/최대값과 FFT/크기를 계산하는 처리 단위를 둔다.',
    'CM3에는 펄스폭에 따른 프레임 수와 전송 가능한 채널 수, 내부기 DAC 범위와 디지털 T/C를 계산하여 공유하는 유닛을 둔다. 이 설정과 CFX/HEAR의 오디오 계산 결과를 구분해야 맵이 바뀌었을 때도 같은 조건으로 전극별 자극 크기를 만들 수 있다.'
])
after('내부기 실행 모드일 경우, 매핑 데이터와 신호처리 모듈의 결과인 전기 자극 크기를 사용하여 PCM 비트스트림을 생성하게 된다.', [
    '내부기 실행은 연결 준비와 상태 확인, 자극 데이터 전송으로 나누어 구현한다. CM3에는 FPGA 통신, 내부기 전원/연결 상태 관리, 명령 요청과 완료 확인을 담당하는 유닛을 둔다. CFX에는 실행 모드 선택과 PCM 명령 구성, 실시간 자극 패킷과 프레임 출력을 담당하는 유닛을 둔다.',
    '신호처리에서 구한 자극 크기만으로 출력하지 않고, 현재 실행 모드와 맵의 채널/프레임 조건을 함께 사용한다. CM3의 명령 요청과 CFX의 전송 완료를 구분하여 같은 명령을 중복 실행하거나 전송 중인 데이터를 덮어쓰지 않도록 한다.'
])
after('CM3는 수신 명령을 구분하여 리모콘, 매핑 및 시스템 정보 처리를 실행하고 응답을 SPI 버퍼에 준비한다.', [
    '블루투스 명령은 수신, 명령 구분과 실행, 응답 준비로 나누어 처리한다. CM3에는 SPI 수신 데이터 확인, 리모콘/매핑 명령 처리, 시스템 정보 응답, 갱신 데이터 전달을 담당하는 유닛을 둔다. 명령별 동작을 분리하여 수신 버퍼를 정리하기 전에 필요한 처리를 끝내도록 한다.',
    'QCC와 E8300 사이의 제어 명령과 응답은 SPI로 전달하고 스트리밍 오디오는 I2S로 CFX에 전달한다. 명령 통신과 오디오 처리는 입력 경로와 완료 시점이 다르므로 따로 다룬다. QCC 내부의 블루투스 처리는 별도 상세 설계 자료에 따라 추가한다.'
])

# Narrow vocabulary edits, not global technical terminology replacements.
replacements = {
 'I2S 입력의 생산과 믹서의 소비를 구분하여 아직 준비되지 않은 블록을 사용하지 않도록 한다.':
 'I2S 입력 데이터가 준비되는 시점과 믹서가 사용하는 시점을 구분하여 아직 준비되지 않은 블록을 사용하지 않도록 한다.',
 'CFX의 검사 구간 완료를 CM3가 한 번씩 소비하게 한다.':
 'CFX의 검사 구간 완료 상태를 CM3가 한 번씩 확인하고 처리하도록 한다.'}
phrases = {'소비 후': '사용한 뒤', '전송/소비 후': '전송하거나 사용한 뒤', '소비한 명령': '처리한 명령'}
edited = []
for p in body.xpath('.//w:p[not(ancestor::w:del)]', namespaces=N):
    old = text(p)
    new = replacements.get(old, old)
    for a, b in sorted(phrases.items(), key=lambda v: -len(v[0])): new = new.replace(a, b)
    if new == old: continue
    ts = p.xpath('.//w:t[not(ancestor::w:del)]', namespaces=N)
    # These current paragraphs are earlier pending insertions. Do not nest revisions.
    assert all(t.xpath('ancestor::w:ins', namespaces=N) for t in ts), old
    ts[0].text = new
    for t in ts[1:]: t.text = ''
    edited.append({'before': old, 'after': new})

flows = []
for i, p in enumerate(list(body)):
    drawings = p.xpath('.//wp:inline[not(ancestor::w:del)]', namespaces=N)
    if not drawings: continue
    caption = text(body[i+1])
    if not re.search(r'그림\s+\d+\.\s+SU-ID-', caption): continue
    for inline in drawings:
        size = inline.find('wp:extent', N)
        cx, cy = int(size.get('cx')), int(size.get('cy'))
        width = 4320000; height = round(cy * width / cx)
        size.set('cx', str(width)); size.set('cy', str(height))
        for ext in inline.xpath('.//a:xfrm/a:ext', namespaces=N):
            ext.set('cx', str(width)); ext.set('cy', str(height))
        flows.append({'caption': caption, 'before': [cx,cy], 'after': [width,height]})
assert len(flows) == 34

def deleted_paragraph(p):
    return p.tag == q('p') and bool(p.xpath('./w:pPr/w:rPr/w:del', namespaces=N))
def blank(p):
    return p.tag == q('p') and not text(p) and not p.xpath('.//w:drawing|.//w:br|.//w:fldChar|.//w:delText', namespaces=N)
units = []
for p in list(body):
    if p.tag != q('p') or not text(p).startswith('SU-ID-'): continue
    # Some old tracked paragraph deletions split a heading across XML paragraphs.
    # Its leading SU-ID fragment still anchors the actual unit start in Word.
    prev = p.getprevious()
    while prev is not None and deleted_paragraph(prev): prev = prev.getprevious()
    exists = prev is not None and blank(prev)
    if not exists: p.addprevious(para('', 4))
    units.append({'unit': text(p), 'existing_gap': exists})
assert len(units) == 115, len(units)

settings = E.fromstring(parts['word/settings.xml'])
if settings.find(q('trackRevisions')) is None: settings.append(E.Element(q('trackRevisions')))
parts['word/settings.xml'] = E.tostring(settings, encoding='UTF-8', xml_declaration=True, standalone=True)
parts['word/document.xml'] = E.tostring(root, encoding='UTF-8', xml_declaration=True, standalone=True)
with zipfile.ZipFile(QA / 'candidate.docx','w',zipfile.ZIP_DEFLATED) as z:
    for name,data in parts.items(): z.writestr(name,data)

old_parts = packed(backup)
assert all(parts[n] == v for n,v in old_parts.items() if n not in {'word/document.xml','word/settings.xml'})
assert [text(p) for p in original.findall('.//' + q('p')) if p.find('w:pPr/w:pStyle',N) is not None and p.find('w:pPr/w:pStyle',N).get(q('val')) in {'TDC1','TDC2','TDC3','TDC4'}] == [text(p) for p in root.findall('.//' + q('p')) if p.find('w:pPr/w:pStyle',N) is not None and p.find('w:pPr/w:pStyle',N).get(q('val')) in {'TDC1','TDC2','TDC3','TDC4'}]
assert LIVE.read_bytes() == raw
report = {'baseline': sha256(raw).hexdigest(), 'review_copy': review_hash,
          'added_paragraphs': added, 'wording': edited, 'flows': flows, 'units': units,
          'new_unit_gaps': sum(not u['existing_gap'] for u in units),
          'preserved_other_parts': len(old_parts)-2}
(QA/'patch.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k: len(v) if isinstance(v,list) else v for k,v in report.items()},ensure_ascii=False))
