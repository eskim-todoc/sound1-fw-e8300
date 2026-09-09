# -*- coding: utf-8 -*-
"""Sound1 소프트웨어 아키텍쳐 기술서 (SAD) 생성"""
import io, os, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

from docx import Document
from docx.shared import Pt, Cm, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_BREAK
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.enum.section import WD_SECTION
from docx.oxml.ns import qn
from docx.oxml import OxmlElement

FONT = u'맑은 고딕'
HDR_BG = 'D9D9D9'
SUB_BG = 'F2F2F2'
CH_BG = 'FFF2CC'    # 변경
DEL_BG = 'FBE5E5'   # 삭제
NEW_BG = 'E2EFDA'   # 신규
CHK_BG = 'FFF2CC'   # 확인 필요
C = WD_ALIGN_PARAGRAPH.CENTER
L = WD_ALIGN_PARAGRAPH.LEFT
J = WD_ALIGN_PARAGRAPH.JUSTIFY

DOC_NO = 'SAD-FW-SND1'
MODEL = 'SND1-A-EXT'
BODY_W = 17.0   # cm (A4 21 - 좌우 2.0)

# 모듈 약칭
SC = 'System Control Module'
SP = 'Signal Processing Module'
ISD = 'Internal Stimulation Device Execution Module'
BT = 'Bluetooth Communication Module'
BL = 'Bootloader Module'


# ---------------------------------------------------------------- 유틸
def set_font(run, size=10, bold=False, color=None, italic=False):
    run.font.size = Pt(size)
    run.bold = bold
    run.italic = italic
    if color:
        run.font.color.rgb = RGBColor(*color)
    rPr = run._element.get_or_add_rPr()
    rf = rPr.find(qn('w:rFonts'))
    if rf is None:
        rf = OxmlElement('w:rFonts')
        rPr.append(rf)
    for a in ('w:eastAsia', 'w:ascii', 'w:hAnsi'):
        rf.set(qn(a), FONT)


def shade(cell, color):
    tcPr = cell._tc.get_or_add_tcPr()
    shd = OxmlElement('w:shd')
    shd.set(qn('w:val'), 'clear')
    shd.set(qn('w:color'), 'auto')
    shd.set(qn('w:fill'), color)
    tcPr.append(shd)


def cell_text(cell, text, size=9, bold=False, align=L, bg=None, color=None):
    cell.text = ''
    p = cell.paragraphs[0]
    p.paragraph_format.space_before = Pt(2)
    p.paragraph_format.space_after = Pt(2)
    p.alignment = align
    for i, ln in enumerate(text.split('\n')):
        if i:
            p.add_run().add_break()
        set_font(p.add_run(ln), size, bold, color)
    if bg:
        shade(cell, bg)


def fixed(t):
    t.autofit = False
    tblPr = t._tbl.tblPr
    for el in tblPr.findall(qn('w:tblLayout')):
        tblPr.remove(el)
    lay = OxmlElement('w:tblLayout')
    lay.set(qn('w:type'), 'fixed')
    tblPr.append(lay)


def set_grid(t, widths):
    """tblGrid 의 gridCol 을 직접 지정 - 병합 셀이 있어도 안전하다"""
    tbl = t._tbl
    grid = tbl.find(qn('w:tblGrid'))
    if grid is not None:
        tbl.remove(grid)
    grid = OxmlElement('w:tblGrid')
    for w in widths:
        gc = OxmlElement('w:gridCol')
        gc.set(qn('w:w'), str(int(round(w * 567))))  # cm -> twips
        grid.append(gc)
    tbl.insert(list(tbl).index(tbl.find(qn('w:tblPr'))) + 1, grid)


def set_widths(t, widths, skip_rows=0):
    set_grid(t, widths)
    for row in list(t.rows)[skip_rows:]:
        cells = row.cells
        if len(cells) != len(widths):
            continue
        seen = set()
        for i, w in enumerate(widths):
            key = id(cells[i]._tc)
            if key in seen:
                continue
            seen.add(key)
            cells[i].width = Cm(w)


def repeat_header(row):
    trPr = row._tr.get_or_add_trPr()
    el = OxmlElement('w:tblHeader')
    el.set(qn('w:val'), 'true')
    trPr.append(el)


def para(doc, text='', size=10, bold=False, sb=3, sa=3, align=J, indent=0.0, color=None):
    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(sb)
    p.paragraph_format.space_after = Pt(sa)
    p.paragraph_format.line_spacing = 1.35
    if indent:
        p.paragraph_format.left_indent = Cm(indent)
    p.alignment = align
    if text:
        set_font(p.add_run(text), size, bold, color)
    return p


def head(doc, num, text, level=1):
    sizes = {1: 15, 2: 12.5, 3: 11, 4: 10.5}
    sb = {1: 20, 2: 14, 3: 10, 4: 8}
    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(sb[level])
    p.paragraph_format.space_after = Pt(5)
    p.paragraph_format.keep_with_next = True
    p.alignment = L
    if level >= 3:
        p.paragraph_format.left_indent = Cm(0.4 * (level - 2))
    set_font(p.add_run(u'%s  %s' % (num, text)), sizes[level], True)
    return p


def bullet(doc, text, size=10, indent=0.6):
    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(1)
    p.paragraph_format.space_after = Pt(1)
    p.paragraph_format.left_indent = Cm(indent + 0.5)
    p.paragraph_format.first_line_indent = Cm(-0.5)
    p.paragraph_format.line_spacing = 1.3
    set_font(p.add_run(u'- ' + text), size)
    return p


def caption(doc, text, size=9.5):
    p = doc.add_paragraph()
    p.alignment = C
    p.paragraph_format.space_before = Pt(4)
    p.paragraph_format.space_after = Pt(14)
    set_font(p.add_run(text), size, True)
    return p


def table(doc, headers, widths, rows, hsize=9, bsize=9, halign=None):
    t = doc.add_table(rows=1, cols=len(headers))
    t.style = 'Table Grid'
    t.alignment = WD_TABLE_ALIGNMENT.CENTER
    fixed(t)
    for i, h in enumerate(headers):
        cell_text(t.rows[0].cells[i], h, hsize, True, C, HDR_BG)
    repeat_header(t.rows[0])
    for r in rows:
        row = t.add_row()
        bg = r[-1] if isinstance(r[-1], tuple) else None
        vals = r[:-1] if bg is not None else r
        for i, v in enumerate(vals):
            al = halign[i] if halign else L
            cell_text(row.cells[i], v, bsize, False, al, bg[0] if bg else None)
    set_widths(t, widths)
    return t


def add_picture(doc, path, width_cm):
    p = doc.add_paragraph()
    p.alignment = C
    p.paragraph_format.space_before = Pt(8)
    p.paragraph_format.space_after = Pt(2)
    p.add_run().add_picture(path, width=Cm(width_cm))
    return p


def page_field(paragraph):
    """PAGE / NUMPAGES 필드"""
    def fld(instr):
        r = paragraph.add_run()
        f1 = OxmlElement('w:fldChar'); f1.set(qn('w:fldCharType'), 'begin')
        it = OxmlElement('w:instrText'); it.set(qn('xml:space'), 'preserve'); it.text = instr
        f2 = OxmlElement('w:fldChar'); f2.set(qn('w:fldCharType'), 'end')
        r._r.append(f1); r._r.append(it); r._r.append(f2)
        set_font(r, 8.5)
    fld(' PAGE ')
    set_font(paragraph.add_run(' / '), 8.5)
    fld(' NUMPAGES ')


# ---------------------------------------------------------------- 문서 시작
doc = Document()
sec = doc.sections[0]
sec.page_width = Cm(21.0)
sec.page_height = Cm(29.7)
sec.top_margin = Cm(2.0)
sec.bottom_margin = Cm(1.5)
sec.left_margin = Cm(2.0)
sec.right_margin = Cm(2.0)
sec.header_distance = Cm(1.0)

st = doc.styles['Normal']
st.font.size = Pt(10)
st.font.name = FONT
_rpr = st.element.get_or_add_rPr()
_rf = _rpr.find(qn('w:rFonts'))
if _rf is None:
    _rf = OxmlElement('w:rFonts'); _rpr.append(_rf)
for _a in ('w:eastAsia', 'w:ascii', 'w:hAnsi'):
    _rf.set(qn(_a), FONT)

# ---- 머리글 ----
hdr = sec.header
hdr.paragraphs[0].text = ''
ht = hdr.add_table(rows=4, cols=4, width=Cm(BODY_W))
ht.style = 'Table Grid'
fixed(ht)
hw = [3.6, 6.4, 3.4, 3.6]
cell_text(ht.rows[0].cells[0], MODEL, 9, True, C)
cell_text(ht.rows[0].cells[1], u'소프트웨어 아키텍쳐 기술서', 10, True, C)
cell_text(ht.rows[0].cells[2], u'표준번호', 8.5, True, C, SUB_BG)
cell_text(ht.rows[0].cells[3], DOC_NO, 8.5, False, C)
cell_text(ht.rows[1].cells[2], u'제(개)정일자', 8.5, True, C, SUB_BG)
cell_text(ht.rows[1].cells[3], u'YYYY.MM.DD', 8.5, False, C)
cell_text(ht.rows[2].cells[2], u'제(개)정 번호', 8.5, True, C, SUB_BG)
cell_text(ht.rows[2].cells[3], u'0', 8.5, False, C)
cell_text(ht.rows[3].cells[2], u'페 이 지', 8.5, True, C, SUB_BG)
pg = ht.rows[3].cells[3].paragraphs[0]
pg.alignment = C
page_field(pg)
ht.rows[0].cells[0].merge(ht.rows[3].cells[0])
ht.rows[0].cells[1].merge(ht.rows[3].cells[1])
set_widths(ht, hw)

# ================================================================ 표지
p = doc.add_paragraph()
p.paragraph_format.space_before = Pt(150)
tp = doc.add_table(rows=1, cols=1)
tp.style = 'Table Grid'
tp.alignment = WD_TABLE_ALIGNMENT.CENTER
fixed(tp)
c = tp.rows[0].cells[0]
c.text = ''
p0 = c.paragraphs[0]
p0.alignment = C
p0.paragraph_format.space_before = Pt(26)
p0.paragraph_format.space_after = Pt(8)
set_font(p0.add_run(u'소프트웨어 아키텍쳐 기술서'), 24, True)
p1 = c.add_paragraph()
p1.alignment = C
p1.paragraph_format.space_after = Pt(26)
set_font(p1.add_run(u'(%s)' % MODEL), 14, False)
set_widths(tp, [BODY_W])

para(doc, '', sa=40)

# 서명표
ts = doc.add_table(rows=5, cols=4)
ts.style = 'Table Grid'
ts.alignment = WD_TABLE_ALIGNMENT.CENTER
fixed(ts)
for i, h in enumerate(['', u'작성', u'검토', u'승인']):
    cell_text(ts.rows[0].cells[i], h, 10, True, C, HDR_BG)
for ri, lab in enumerate([u'성함', u'직책', u'일자', u'서명']):
    cell_text(ts.rows[ri + 1].cells[0], lab, 9.5, True, C, SUB_BG)
    for ci in range(1, 4):
        cell_text(ts.rows[ri + 1].cells[ci], '', 9.5, False, C)
        ts.rows[ri + 1].height = Cm(1.0)
set_widths(ts, [2.6, 4.8, 4.8, 4.8])

para(doc, '', sa=30)

# 개정 이력
para(doc, u'개정 이력', 11, True, sb=10, sa=5, align=L)
tr = doc.add_table(rows=1, cols=4)
tr.style = 'Table Grid'
tr.alignment = WD_TABLE_ALIGNMENT.CENTER
fixed(tr)
for i, h in enumerate([u'No.', u'개정 내용', u'개정 위치', u'개정 날짜']):
    cell_text(tr.rows[0].cells[i], h, 9.5, True, C, HDR_BG)
rows_rev = [(u'0', u'최초 제정', u'-', u'YYYY.MM.DD')]
for n in range(1, 10):
    rows_rev.append((str(n), '', '', ''))
for r in rows_rev:
    row = tr.add_row()
    for i, v in enumerate(r):
        cell_text(row.cells[i], v, 9, False, C if i in (0, 2, 3) else L)
set_widths(tr, [1.6, 8.4, 3.5, 3.5])

doc.add_page_break()

# ================================================================ 1. 서론
head(doc, u'1.', u'서론', 1)

head(doc, u'1.1.', u'개요', 2)
para(doc, u'본 문서는 인공와우 외부장치(외부기) 펌웨어를 개발하기 위한 소프트웨어 아키텍쳐를 설계하고 기술한다.')

head(doc, u'1.2.', u'목적', 2)
para(doc, u'인공와우 외부장치 소프트웨어의 요구사항을 포함한 소프트웨어 아키텍쳐를 설계함으로써, 소프트웨어 개발 절차에 따라 안전하고 일관성 및 정확성을 보장하는 소프트웨어가 개발되는 것을 목적으로 한다.')

head(doc, u'1.3.', u'대상', 2)
para(doc, u'소프트웨어 개발 프로젝트에 참여하는 모든 구성원은 이 문서를 참고하도록 한다.')

head(doc, u'1.4.', u'적용 범위', 2)
para(doc, u'본 문서는 외부기의 메인 칩셋인 Ezairo 8300 에서 동작하는 소프트웨어를 대상으로 한다. 외부기에 함께 실장되는 블루투스 콤보 칩셋(QCC5181)의 펌웨어는 별도로 관리되므로 본 문서에서는 시스템 구성 요소와 인터페이스로만 다루고 내부 아키텍쳐는 기술하지 않는다.')
table(doc,
      [u'구분', u'대상', u'본 문서에서의 취급'],
      [3.0, 6.0, 8.0],
      [
          (u'포함', u'Ezairo 8300 - CFX Core', u'모듈 및 항목까지 기술'),
          (u'포함', u'Ezairo 8300 - HEAR Core', u'모듈 및 항목까지 기술'),
          (u'포함', u'Ezairo 8300 - Cortex-M3', u'모듈 및 항목까지 기술'),
          (u'포함', u'부트로더 및 무선 갱신(OTA)', u'모듈 및 항목까지 기술'),
          (u'제외', u'QCC5181 펌웨어', u'시스템 구성 요소 및 인터페이스로만 기술'),
          (u'제외', u'생산 및 서비스용 캘리브레이션 실행체', u'제품 출하 펌웨어에 포함되지 않음'),
          (u'제외', u'FPGA 로직, 내부 이식장치 펌웨어', u'별도 문서에서 관리'),
      ],
      halign=[C, L, L])

head(doc, u'1.5.', u'관련문서', 2)
head(doc, u'1.5.1.', u'프로젝트', 3)
table(doc, [u'No', u'문서번호', u'문서 제목'], [1.6, 5.4, 10.0],
      [(u'1', u'SDP-FW-SND1', u'소프트웨어 개발 계획서'),
       (u'2', u'SGR-FW-SND1', u'소프트웨어 일반 요구사항'),
       (u'3', u'SR-FW-SND1', u'소프트웨어 요구사항')],
      halign=[C, C, L])

head(doc, u'1.5.2.', u'표준 및 규제', 3)
table(doc, [u'No', u'문서번호', u'문서 제목'], [1.6, 5.4, 10.0],
      [(u'1', u'IEC 62304', u'Medical device software - Software life cycle processes'),
       (u'2', u'ISO 14971', u'Medical devices - Application of risk management to medical devices')],
      halign=[C, C, L])

doc.add_page_break()

# ================================================================ 2. 하드웨어 플랫폼
head(doc, u'2.', u'하드웨어 플랫폼', 1)
para(doc, u'인공와우 시스템 전체의 하드웨어 플랫폼은 아래 <그림 1.>과 같으며, 본 문서가 다루는 소프트웨어는 Ezairo 8300 에서 동작한다.')

IMG = u'E:/workspace/projects/sound1-fw-e8300-backup/docs/tasks/인허가/20260909_1129_sad-sound1/그림'
add_picture(doc, IMG + u'/그림1_시스템_블록도.png', 16.6)
caption(doc, u'< 그림 1. 인공와우 시스템 블록도 >')

para(doc, u'외부기를 구성하는 주요 부품과 소프트웨어의 관계는 다음과 같다.')
table(doc, [u'구성 요소', u'역할', u'소프트웨어와의 관계'], [4.0, 6.0, 7.0],
      [
          (u'Ezairo 8300', u'메인 칩셋. CFX Core, HEAR Core, Cortex-M3 로 구성', u'본 문서가 기술하는 소프트웨어가 동작한다'),
          (u'디지털 마이크 (2)', u'주변 소리를 PDM 으로 입력', u'CFX 오디오 입력 및 믹서 항목이 수신한다'),
          (u'QCC5181', u'블루투스 콤보. 무선 통신, 배터리 및 충전 관리', u'SPI 및 I2S 로 연결된다. 펌웨어는 별도 관리'),
          (u'FPGA', u'자극 비트스트림 생성 및 링크 구동', u'Cortex-M3 가 I2C 로 설정하고 CFX 가 PCM 을 공급한다'),
          (u'TX PMIC', u'경피 유도 링크 전력 제어', u'Cortex-M3 가 I2C 로 제어한다'),
          (u'터치 센서 (IQS323)', u'사용자 입력(롱 터치) 검출', u'Cortex-M3 가 I2C 로 상태를 읽는다'),
          (u'EEPROM', u'맵 데이터 및 설정 저장', u'Cortex-M3 가 SPI 로 접근한다'),
          (u'LED', u'동작 상태 표시', u'Cortex-M3 가 출력을 제어한다'),
          (u'송신 코일', u'내부 이식장치와의 경피 유도 링크', u'FPGA 및 TX PMIC 를 통해 구동된다'),
      ])

doc.add_page_break()

# ================================================================ 3. 소프트웨어 아키텍쳐
head(doc, u'3.', u'소프트웨어 아키텍쳐', 1)
para(doc, u'앞 절의 하드웨어 플랫폼을 기반으로 동작하는 인공와우 외부장치 소프트웨어의 아키텍쳐는 <그림 2.>와 같이 설계되었다. 각 프로세서 별로 소프트웨어가 개별적으로 실행되므로, 프로세서 별로 아키텍쳐를 설계하고 각각을 모듈과 항목의 2단 구조로 분해한다.')
add_picture(doc, IMG + u'/그림2_소프트웨어_아키텍쳐_블록도.png', 16.6)
caption(doc, u'< 그림 2. 인공와우 소프트웨어 아키텍쳐 블록도 >')

# ---- 3.1 CFX ----
head(doc, u'3.1.', u'CFX Core 용 소프트웨어 아키텍쳐', 2)
para(doc, u'CFX Core 는 오디오 입력부터 자극 신호 생성까지의 실시간 신호처리를 담당한다.')

head(doc, u'3.1.1.', u'시스템 제어 모듈', 3)
para(doc, u'하드웨어 설정, 공유 메모리 접근, 오디오 입력 및 믹싱과 관련된 유닛들로 구성되어 CFX Core 의 전체 동작을 제어한다. 소프트웨어 항목은 다음과 같다.')
head(doc, u'3.1.1.1.', u'시스템 초기화 항목', 4)
para(doc, u'CFX 코어와 오디오 입력 하드웨어(ADC, 디지털 마이크 인터페이스, I2S, 인터럽트)를 초기화한다.')
head(doc, u'3.1.1.2.', u'시스템 제어 항목', 4)
para(doc, u'오디오 입력 인터럽트를 기준으로 신호처리 주기를 형성하고, 신호처리 모듈과 내부 자극 장치 실행 모듈의 실행 순서를 제어하며, HEAR Core 의 Function Chain 을 호출한다. 이 항목이 완성되면 CFX Core 의 소프트웨어 구현이 완료된다.')
head(doc, u'3.1.1.3.', u'오디오 입력 항목', 4)
para(doc, u'두 개의 디지털 마이크로부터 PDM 신호를 수신하고 표본화 주파수로 변환한다. 블루투스 콤보 칩셋에서 I2S 로 전달되는 오디오도 입력 소스로 취급한다.')
head(doc, u'3.1.1.4.', u'오디오 믹서 항목', 4)
para(doc, u'입력 소스를 선택하고, 두 마이크의 신호를 지연 후 합산하여 지향성을 형성한 뒤 신호처리 모듈에 전달한다.')

head(doc, u'3.1.2.', u'신호처리 모듈', 3)
para(doc, u'사운드 신호를 전기 자극 신호로 변환하기 위한 오디오 신호처리 유닛들로 구성되며, 최종적으로 시스템 제어 항목에서 호출되어 실행된다.')
head(doc, u'3.1.2.1.', u'자동 이득 제어 항목', 4)
para(doc, u'입력 신호의 크기에 따라 이득을 조절하여 후단 처리의 동작 범위를 유지한다.')
head(doc, u'3.1.2.2.', u'주파수 분석 항목', 4)
para(doc, u'입력 신호를 주파수 분석하여 대역별 크기를 산출한다.')
head(doc, u'3.1.2.3.', u'비선형 매핑 항목', 4)
para(doc, u'대역별 크기를 매핑 데이터에 따라 자극 크기로 변환한다.')
head(doc, u'3.1.2.4.', u'자극 기법 항목', 4)
para(doc, u'설정된 자극 기법에 따라 자극할 대역과 순서를 결정한다. 지원하는 기법은 다음과 같다.')
bullet(doc, u'CIS - 모든 대역을 정해진 순서로 자극한다.')
bullet(doc, u'NofM - 대역별 크기를 기준으로 상위 N 개 대역을 선택하여 자극한다.')

head(doc, u'3.1.3.', u'내부 자극 장치 실행 모듈', 3)
para(doc, u'내부 자극 장치에 전달되는 비트스트림 생성과 관련된 유닛들로 구성되어 있다.')
head(doc, u'3.1.3.1.', u'PCM 제어 항목', 4)
para(doc, u'Cortex-M3 가 지정한 PCM 출력 모드에 따라 내부 이식장치에 전달되는 PCM 을 생성한다.')

# ---- 3.2 HEAR ----
head(doc, u'3.2.', u'HEAR Core 용 소프트웨어 아키텍쳐', 2)
head(doc, u'3.2.1.', u'하드웨어 가속 연산 모듈', 3)
para(doc, u'HEAR Core 의 소프트웨어는 디지털 신호처리 라이브러리를 묶은 Function Chain 으로 동작이 실행되도록 되어 있으며, Function Chain 은 CFX Core 의 시스템 제어 항목에서 호출한다.')
head(doc, u'3.2.1.1.', u'Function Chain 항목', 4)
para(doc, u'신호처리에 필요한 반복 연산을 하드웨어 가속기에서 수행하고 그 결과를 CFX Core 가 읽을 수 있는 위치에 기록한다.')

doc.add_page_break()

# ---- 3.3 CM3 ----
head(doc, u'3.3.', u'Cortex-M3 용 소프트웨어 아키텍쳐', 2)
para(doc, u'Cortex-M3 는 시스템 전체의 상태 관리와 외부 장치 제어를 담당한다.')

head(doc, u'3.3.1.', u'시스템 제어 모듈', 3)
para(doc, u'하드웨어 설정, 공유 메모리 접근, 보조장치 제어 등과 관련된 유닛들로 구성되어 시스템의 전체 동작을 제어한다. 소프트웨어 항목은 다음과 같다.')
head(doc, u'3.3.1.1.', u'시스템 초기화 항목', 4)
para(doc, u'Cortex-M3 와 주변 하드웨어(I2C, SPI, 타이머, 터치 센서, FPGA)를 초기화한다.')
head(doc, u'3.3.1.2.', u'시스템 제어 항목', 4)
para(doc, u'다른 모듈 및 유닛의 파라미터를 확인하여 시스템 동작을 제어한다. 이 항목이 완성되면 Cortex-M3 의 소프트웨어 구현이 완료된다.')
head(doc, u'3.3.1.3.', u'사용자 입력 항목', 4)
para(doc, u'터치 센서의 상태를 주기적으로 읽어 롱 터치를 판별하고, 그 결과로 전원 모드 변경을 요청한다.')
head(doc, u'3.3.1.4.', u'전원 및 배터리 상태 항목', 4)
para(doc, u'블루투스 콤보 칩셋이 SPI 로 전달하는 배터리 잔량, 충전기 연결 상태, 크래들 뚜껑 상태를 수신하여 시스템 상태에 반영한다. 저전압 상태에서는 사용자에게 주기적으로 알린다.')
head(doc, u'3.3.1.5.', u'상태 표시 항목', 4)
para(doc, u'외부기의 동작 상태, 충전 상태, 배터리 저전압 상태를 구분 가능한 LED 패턴으로 출력한다.')
head(doc, u'3.3.1.6.', u'오류 처리 항목', 4)
para(doc, u'설정 데이터가 유효 범위를 벗어나거나 주변 장치 통신에 실패한 경우 오류를 기록하고 안전한 상태로 전이한다.')

head(doc, u'3.3.2.', u'신호처리 모듈', 3)
para(doc, u'CFX Core 가 공유한 매핑 데이터를 사용하여 C/T 전류 크기를 C/T 자극 레벨로 변환하여 CFX Core 에 공유하는 유닛으로 구성되어 있다.')
head(doc, u'3.3.2.1.', u'자극 파라미터 산출 항목', 4)
para(doc, u'매핑 데이터와 볼륨 설정으로부터 자극 레벨을 산출하여 공유 메모리에 기록한다.')

head(doc, u'3.3.3.', u'내부 자극 장치 실행 모듈', 3)
para(doc, u'내부 자극 장치 제어 및 자극 출력 실행과 관련된 유닛들로 구성되어 있다.')
head(doc, u'3.3.3.1.', u'내부기 실행 제어 항목', 4)
para(doc, u'내부 자극 장치 실행과 관련된 보조장치 설정, 내부 자극 장치 설정 및 내부 자극 장치 연결 검사와 관련된 유닛을 실행하여 내부기를 제어한다. 등록된 내부 이식장치와만 근접 통신을 연결한다.')
head(doc, u'3.3.3.2.', u'매핑 실행 항목', 4)
para(doc, u'매핑 프로그램의 요청에 따라 특정 자극 출력, 실시간 자극 출력, 임피던스 측정, eCAP 측정, 맵 데이터 읽기 및 쓰기를 실행한다.')
head(doc, u'3.3.3.3.', u'출력 억제 항목', 4)
para(doc, u'시동 과정과 종료 과정에서는 자극 출력을 생성하지 않는다.')

head(doc, u'3.3.4.', u'블루투스 통신 모듈', 3)
para(doc, u'리모콘 앱 및 매핑 프로그램과의 통신과 관련된 유닛들로 구성되어 있다.')
head(doc, u'3.3.4.1.', u'블루투스 통신 제어 항목', 4)
para(doc, u'블루투스 콤보 칩셋에서 SPI 통신으로 수신된 명령에 따라 관련 유닛을 실행하고 응답을 회신한다. 명령은 규정된 프로토콜에 부합하고 CRC 검사를 통과한 경우에만 실행한다.')
head(doc, u'3.3.4.2.', u'리모콘 명령 항목', 4)
para(doc, u'맵 번호 변경, 오디오 볼륨 조절, 자극 볼륨 조절, LED 알림 설정, 자극 알림 설정 명령을 처리하고 현재 설정과 배터리 상태를 응답한다. 내부기 제어용 Passkey 매칭이 확인된 뒤에 리모콘 제어를 활성화한다.')
head(doc, u'3.3.4.3.', u'매핑 명령 항목', 4)
para(doc, u'시스템 상태 확인, 특정 자극 출력, 실시간 자극 출력 진행 명령을 처리하고 결과를 응답한다.')

doc.add_page_break()

# ---- 3.4 부트로더 [착탈 A] ----
head(doc, u'3.4.', u'부트로더 소프트웨어 아키텍쳐', 2)
para(doc, u'부트로더는 응용 소프트웨어보다 먼저 실행되어 하드웨어를 초기 상태로 만들고, 비휘발성 저장장치에 보관된 '
          u'실행 이미지를 검증 및 복호화하여 메모리에 적재한 뒤 각 코어를 기동한다. 또한 무선으로 전달된 갱신 '
          u'이미지를 보관하고, 다음 기동에서 적용하거나 실패 시 이전 이미지로 되돌린다.')

head(doc, u'3.4.1.', u'부트 제어 모듈', 3)
para(doc, u'기동 직후의 하드웨어 설정과 동작 모드 판정, 코어 기동을 담당한다.')
head(doc, u'3.4.1.1.', u'시스템 초기화 항목', 4)
para(doc, u'인터럽트를 모두 차단하고 필수 입출력을 설정한다. 블루투스 콤보 칩셋을 절전 상태로 두고 내부기 연결 '
          u'상태를 미연결로 표시하여, 기동 과정에서 자극이나 무선 동작이 일어나지 않게 한다. '
          u'코어 간 공유 메모리의 적재 완료 표시를 지운다.')
head(doc, u'3.4.1.2.', u'동작 모드 판정 항목', 4)
para(doc, u'지정된 입력 핀의 레벨을 확인하여 정상 기동과 서비스 모드 중 하나를 선택한다. '
          u'서비스 모드는 생산 및 정비 과정에서 저장된 이미지를 확인하기 위한 경로이며 자극 출력을 수행하지 않는다.')
head(doc, u'3.4.1.3.', u'코어 기동 항목', 4)
para(doc, u'적재와 검증을 통과한 이미지로 Cortex-M3 와 CFX Core 를 기동한다. 기동에 필요한 시작 주소와 '
          u'스택 위치는 이미지 헤더가 제공한다. 검증에 실패하면 기동하지 않고 오류 상태로 남는다.')

head(doc, u'3.4.2.', u'이미지 적재 모듈', 3)
para(doc, u'저장장치의 실행 이미지를 읽어 검증, 복호화, 적재하는 유닛들로 구성된다.')
head(doc, u'3.4.2.1.', u'부트 대상 결정 항목', 4)
para(doc, u'시스템 변수에 기록된 기동 대상 번호를 읽어 적재할 이미지 목록 파일을 결정한다. '
          u'번호가 지정되지 않은 경우 저장장치의 기본 목록을 사용한다.')
head(doc, u'3.4.2.2.', u'무결성 검증 항목', 4)
para(doc, u'이미지 헤더와 각 데이터 구획에 대해 순환 중복 검사 값을 산출하여 기록된 값과 비교한다. '
          u'불일치가 있으면 적재를 중단하고 오류를 기록한다.')
head(doc, u'3.4.2.3.', u'복호화 항목', 4)
para(doc, u'저장장치에는 실행 이미지가 암호화된 상태로 보관된다. 장치에 내장된 키로 복호화한 뒤 적재한다. '
          u'이로써 저장 매체를 물리적으로 분리하더라도 실행 코드를 그대로 얻을 수 없다.')
head(doc, u'3.4.2.4.', u'메모리 적재 및 보호 항목', 4)
para(doc, u'복호화된 구획을 각 코어의 프로그램 메모리 영역에 전송하고, 전송이 끝난 영역을 쓰기 금지 상태로 '
          u'잠근다. 응용 소프트웨어가 실행 중에 자신의 코드 영역을 덮어쓰는 것을 막는다.')

head(doc, u'3.4.3.', u'무선 갱신 모듈', 3)
para(doc, u'무선으로 전달된 갱신 이미지를 보관하고 적용하며, 실패 시 복구한다.')
head(doc, u'3.4.3.1.', u'갱신 이미지 수신 항목', 4)
para(doc, u'블루투스 콤보 칩셋을 경유하여 전달되는 갱신 이미지를 저장장치에 기록한다. '
          u'수신은 응용 소프트웨어가 수행하고, 부트로더는 기록된 결과를 사용한다.')
head(doc, u'3.4.3.2.', u'적용 대상 선택 항목', 4)
para(doc, u'실행 이미지는 두 개의 보관 위치에 번갈아 저장된다. 기동 시 보관 정보를 읽어 이번에 사용할 '
          u'위치를 결정한다.')
head(doc, u'3.4.3.3.', u'복구 항목', 4)
para(doc, u'선택된 이미지가 검증을 통과하지 못하면 직전에 정상 동작하던 위치의 이미지로 되돌려 기동한다. '
          u'갱신 실패로 제품이 기동하지 못하는 상태가 되지 않게 한다.')

# ---- 3.5 QCC [확장 자리] ----
head(doc, u'3.5.', u'블루투스 콤보 칩셋(QCC5181) 소프트웨어 아키텍쳐', 2)
para(doc, u'블루투스 콤보 칩셋의 펌웨어는 본 문서의 범위에 포함되지 않는다. 해당 펌웨어는 별도의 개발 및 형상 관리 체계에서 관리되며, 본 문서는 시스템 구성 요소로서의 위치와 Ezairo 8300 과의 인터페이스만 기술한다.')
para(doc, u'본 문서가 외부기 전체의 소프트웨어 아키텍쳐 기술서로 확장되는 경우, 블루투스 콤보 칩셋 펌웨어의 모듈 및 항목은 본 절에 기술한다. 아래 표는 Ezairo 8300 소프트웨어가 해당 펌웨어에 의존하는 기능의 목록이며, 확장 시 각 기능이 어느 모듈에 대응하는지를 이 표에서 이어 기술한다.')
table(doc, [u'의존 기능', u'Ezairo 8300 이 기대하는 동작', u'전달 경로'], [4.6, 8.0, 4.4],
      [
          (u'무선 통신', u'리모콘 앱 및 매핑 프로그램의 명령을 수신하여 전달하고, 응답을 회신한다', u'SPI'),
          (u'통신 보안', u'페어링을 적용하여 데이터 통신 보안을 확보한다', u'SPI'),
          (u'배터리 관리', u'배터리 잔량을 측정하여 주기적으로 전달한다', u'SPI'),
          (u'충전 관리', u'충전기 연결 상태와 크래들 뚜껑 상태를 전달한다', u'SPI'),
          (u'오디오 전달', u'외부 오디오 스트림을 전달한다', u'I2S'),
          (u'갱신 중계', u'무선으로 수신한 갱신 이미지를 전달한다', u'SPI'),
      ])

doc.add_page_break()

# ================================================================ 4. 인터페이스
head(doc, u'4.', u'프로세서 간 인터페이스', 1)
para(doc, u'본 절은 각 소프트웨어가 경계에서 주고받는 데이터를 규정한다. 각 프로세서 내부의 구현은 앞 절에서 기술하며, 본 절은 경계에서 오가는 내용만 다룬다.')
table(doc, [u'경계', u'매체', u'방향', u'주고받는 내용'], [3.6, 2.6, 2.4, 8.4],
      [
          (u'CFX Core <-> Cortex-M3', u'공유 메모리', u'양방향', u'오디오 입력 최대값, 자극 파라미터, 매핑 데이터, 시스템 상태 및 제어 요청'),
          (u'CFX Core -> HEAR Core', u'Function Chain', u'단방향 호출', u'신호처리 가속 연산 요청 및 결과'),
          (u'Cortex-M3 <-> QCC5181', u'SPI', u'양방향', u'리모콘 및 매핑 명령과 응답, 배터리 잔량, 충전기 연결 상태, 크래들 뚜껑 상태, 갱신 이미지'),
          (u'CFX Core <-> QCC5181', u'I2S', u'양방향', u'오디오 스트림'),
          (u'Cortex-M3 -> FPGA', u'I2C', u'단방향', u'자극 출력 설정'),
          (u'CFX Core -> FPGA', u'PCM', u'단방향', u'자극 비트스트림'),
          (u'Cortex-M3 -> TX PMIC', u'I2C', u'단방향', u'경피 유도 링크 전력 설정'),
          (u'Cortex-M3 <-> 터치 센서', u'I2C', u'양방향', u'터치 센서 설정 및 검출 상태'),
          (u'Cortex-M3 <-> EEPROM', u'SPI', u'양방향', u'맵 데이터 및 설정 값'),
      ])

doc.add_page_break()

# ================================================================ 5. 추적성
head(doc, u'5.', u'소프트웨어 요구사항 추적성', 1)
para(doc, u'소프트웨어 요구사항이 어느 소프트웨어 모듈로 사상되는지를 아래 표에 기술한다. 상태 열은 이전 세대(1.5세대) 요구사항 대비 본 제품에서의 처리를 나타낸다.')
table(doc, [u'상태', u'의미'], [3.0, 14.0],
      [(u'유지', u'이전 세대와 동일하게 적용된다'),
       (u'변경', u'요구사항은 유지되나 이를 실현하는 구성 요소가 변경되었다'),
       (u'삭제', u'해당 기능이 제품에서 제거되어 적용되지 않는다'),
       (u'신규', u'본 제품에서 새로 추가되었다'),
       (u'확인 필요', u'본 문서 작성 시점에 확정되지 않았다')],
      halign=[C, L])

para(doc, '', sa=8)

RID = [
    (1, u'전채널 자극률', ISD, u'유지', u''),
    (2, u'채널별 자극률', ISD, u'유지', u''),
    (3, u'자극 모드', ISD, u'유지', u''),
    (4, u'선행 펄스 모양', ISD, u'유지', u''),
    (5, u'자극 크기', SP, u'유지', u''),
    (6, u'자극 펄스 폭', ISD, u'유지', u''),
    (7, u'자극 최대 전하량 제한', ISD, u'유지', u''),
    (8, u'AGC 볼륨 10단계', SP, u'유지', u''),
    (9, u'FFT 주파수 분석', SP, u'유지', u''),
    (10, u'주파수 분석 대역수(최대 32개)', SP, u'유지', u''),
    (11, u'CIS 자극 기법', ISD, u'유지', u''),
    (12, u'신호처리 주기', SP, u'유지', u''),
    (13, u'맵 데이터 개수(4개)', ISD, u'유지', u''),
    (14, u'자극 볼륨 4단계', SP, u'유지', u''),
    (15, u'오디오 입력 소스 최소 2개', SP, u'유지', u''),
    (16, u'사운드 입력 주파수 최대 8kHz', SC, u'유지', u''),
    (17, u'PCM 출력(FPGA 입력)', ISD, u'유지', u''),
    (18, u'매핑 데이터 기반 자극 파라미터', SP + u' / ' + ISD, u'유지', u''),
    (19, u'사용자 입력을 사용한 전원 모드 변경', SC, u'변경', u'가속도 센서(더블 클릭) -> 터치 센서(롱 터치)'),
    (20, u'외부기 동작상태 LED 출력', SC, u'유지', u''),
    (21, u'충전기 연결 상태 LED 출력', SC, u'변경', u'USB 전원 커넥터 제거. 크래들 연결 상태를 SPI 로 수신'),
    (22, u'배터리 저전압 상태 LED 출력', SC, u'변경', u'배터리 측정 주체가 블루투스 콤보 칩셋으로 이동'),
    (23, u'배터리 저전압 상태 자극 출력', SC + u' / ' + SP, u'변경', u'배터리 측정 주체 이동에 따른 변경'),
    (24, u'리모콘 명령 수신(처리), 맵 번호 변경', BT, u'유지', u''),
    (25, u'리모콘 명령 수신(처리), 오디오 볼륨 조절', BT, u'유지', u''),
    (26, u'리모콘 명령 수신(처리), 자극 볼륨 조절', BT, u'유지', u''),
    (27, u'리모콘 명령 수신(처리), LED 알림 설정', BT, u'유지', u''),
    (28, u'리모콘 명령 수신(처리), 자극 알림 설정', BT, u'유지', u''),
    (29, u'리모콘 명령 응답(송신), 배터리 상태', BT, u'유지', u''),
    (30, u'리모콘 명령 응답(송신), 맵 번호', BT, u'유지', u''),
    (31, u'리모콘 명령 응답(송신), 오디오 볼륨', BT, u'유지', u''),
    (32, u'리모콘 명령 응답(송신), 자극 볼륨 설정', BT, u'유지', u''),
    (33, u'리모콘 명령 응답(송신), LED 알림 설정', BT, u'유지', u''),
    (34, u'리모콘 명령 응답(송신), 자극 알림 설정', BT, u'유지', u''),
    (35, u'매핑 명령 수신(처리), 시스템 상태 확인', BT, u'유지', u''),
    (36, u'매핑 명령 수신(처리), 특정 자극 출력', BT, u'유지', u''),
    (37, u'매핑 명령 수신(처리), 실시간 자극 출력 진행', BT, u'유지', u''),
    (38, u'매핑 명령 응답(송신), 시스템 상태', BT, u'유지', u''),
    (39, u'매핑 명령 응답(송신), 특정 자극 출력', BT, u'유지', u''),
    (40, u'매핑 명령 응답(송신), 실시간 자극 출력 진행', BT, u'유지', u''),
    (41, u'I2C 통신, FPGA', SC, u'유지', u''),
    (42, u'I2C 통신, 터치 센서', SC, u'변경', u'가속도 센서 -> 터치 센서(IQS323)'),
    (43, u'I2C 통신, Tx PMIC', SC, u'유지', u''),
    (44, u'리모콘 앱과 블루투스를 사용한 무선통신', BT, u'변경', u'무선 칩셋이 블루투스 콤보 칩셋으로 변경'),
    (45, u'매핑 프로그램과 블루투스를 사용한 무선통신', BT, u'변경', u'무선 칩셋이 블루투스 콤보 칩셋으로 변경'),
    (46, u'등록된 내부이식장치와만 근접 통신을 연결한다', ISD, u'유지', u''),
    (47, u'내부기 연결 상태에서 블루투스 활성화', SC, u'유지', u''),
    (48, u'블루투스 통신에 페어링 기능을 적용한 보안 데이터 통신', BT, u'변경', u'페어링 수행 주체가 블루투스 콤보 칩셋으로 이동'),
    (49, u'통신 데이터 보안 규격 적용', BT, u'확인 필요', u'적용 규격 버전 및 암호화 방식 확정 필요'),
    (50, u'블루투스 통신 접속 로그 기록', BT, u'확인 필요', u'기록 주체가 블루투스 콤보 칩셋으로 이관되었는지 확정 필요'),
    (51, u'설정 데이터가 유효 범위를 벗어날 경우 에러 모드 진입', SP + u' / ' + SC, u'유지', u''),
    (52, u'내부기 제어용 Passkey 매칭 확인 후 리모콘 제어 활성화', BT, u'유지', u''),
    (53, u'소프트웨어 유지보수 절차를 따른다', u'N/A', u'유지', u''),
    (54, u'Cortex-M3 실행코드 크기 제한', u'N/A', u'확인 필요', u'본 제품 기준 상한 값 재산출 필요'),
    (55, u'배터리 저전압을 주기적으로 반복하여 알려준다', SC, u'유지', u''),
    (56, u'볼륨 증가를 단계적으로 진행', SC, u'유지', u''),
    (57, u'필수 성능 시험 진행', u'N/A', u'유지', u''),
    (58, u'실시간 신호처리', SC + u' / ' + SP, u'유지', u''),
    (59, u'제어 명령이 프로토콜에 맞게 전송되어야 실행한다', BT, u'유지', u''),
    (60, u'CRC 기능이 내장된 통신 프로토콜 사용', BT, u'유지', u''),
    (61, u'정상사용을 위한 준비상태 표시', SC, u'유지', u''),
    (62, u'충전기가 연결되면 연결을 확인할 수 있어야 한다(충전모드 표시)', SC, u'변경', u'USB 전원 커넥터 -> 크래들'),
    (63, u'배터리 잔량을 표시할 수 있어야 한다', SC, u'변경', u'배터리 측정 주체가 블루투스 콤보 칩셋으로 이동'),
    (64, u'구분이 쉬운 LED 패턴 설계', SC, u'유지', u''),
    (65, u'사회 통념적으로 사용되는 색상을 LED 패턴에 적용', SC, u'유지', u''),
    (66, u'이어피스가 연결되면 이어피스 연결 상태가 확인되어야 한다', u'-', u'삭제', u'이어피스가 제품에서 제거됨'),
    (67, u'시동과정에서는 출력을 생성하지 않게 한다', SC + u' / ' + ISD, u'유지', u''),
    (68, u'종료과정에서는 출력을 생성하지 않게 한다', SC + u' / ' + ISD, u'유지', u''),
    (69, u'두 개의 마이크를 사용한 지향성 오디오 입력', SP, u'신규', u'마이크 2개 구성으로 변경됨'),
    (70, u'터치 센서 롱 터치에 의한 전원 제어', SC, u'신규', u''),
    (71, u'배터리, 충전기, 크래들 뚜껑 상태 수신', SC, u'신규', u'블루투스 콤보 칩셋이 측정하여 SPI 로 전달'),
    (72, u'I2S 를 통한 외부 오디오 입력 경로', SP, u'신규', u''),
    (73, u'NofM 자극 기법', ISD, u'신규', u''),
    (74, u'SPI 통신, 블루투스 콤보 칩셋', SC, u'신규', u''),
    (75, u'기동 시 실행 이미지 무결성 검증 후 코어 기동', BL, u'신규', u''),
    (76, u'무선(OTA) 펌웨어 갱신', BL, u'신규', u''),
    (77, u'실행 이미지를 암호화하여 보관하고 기동 시 복호화', BL, u'신규', u'저장 매체 분리 시 코드 노출 방지'),
    (78, u'적재한 코드 영역을 쓰기 금지로 보호', BL, u'신규', u'실행 중 코드 영역 훼손 방지'),
    (79, u'갱신 실패 시 직전 정상 이미지로 복구', BL, u'신규', u'갱신 실패로 기동 불능이 되지 않게 함'),
]

BGMAP = {u'변경': CH_BG, u'삭제': DEL_BG, u'신규': NEW_BG, u'확인 필요': CHK_BG}

t = doc.add_table(rows=2, cols=5)
t.style = 'Table Grid'
t.alignment = WD_TABLE_ALIGNMENT.CENTER
fixed(t)
cell_text(t.rows[0].cells[0], u'Software requirements', 8.5, True, C, HDR_BG)
cell_text(t.rows[0].cells[2], u'Software Module', 8.5, True, C, HDR_BG)
cell_text(t.rows[0].cells[3], u'상태', 8.5, True, C, HDR_BG)
cell_text(t.rows[0].cells[4], u'비고', 8.5, True, C, HDR_BG)
t.rows[0].cells[0].merge(t.rows[0].cells[1])
cell_text(t.rows[1].cells[0], u'Requirements ID', 8.5, True, C, HDR_BG)
cell_text(t.rows[1].cells[1], u'Subject', 8.5, True, C, HDR_BG)
cell_text(t.rows[1].cells[2], u'Software Module', 8.5, True, C, HDR_BG)
cell_text(t.rows[1].cells[3], u'상태', 8.5, True, C, HDR_BG)
cell_text(t.rows[1].cells[4], u'비고', 8.5, True, C, HDR_BG)
repeat_header(t.rows[0])
repeat_header(t.rows[1])

for rid, subj, mod, stat, note in RID:
    row = t.add_row()
    bg = BGMAP.get(stat)
    cell_text(row.cells[0], u'SW-FW-RID-%d' % rid, 8, False, C, bg)
    cell_text(row.cells[1], subj, 8, False, L, bg)
    cell_text(row.cells[2], mod, 7.5, False, L, bg)
    cell_text(row.cells[3], stat, 8, stat != u'유지', C, bg)
    cell_text(row.cells[4], note, 7.5, False, L, bg)
set_widths(t, [2.7, 5.1, 4.3, 1.6, 3.3], skip_rows=1)
_h = t.rows[0].cells
_h[0].width = Cm(7.8)
for _i, _w in ((2, 4.3), (3, 1.6), (4, 3.3)):
    _h[_i].width = Cm(_w)

doc.add_page_break()

# ================================================================ 6. 검증
head(doc, u'6.', u'소프트웨어 아키텍쳐 검증', 1)

qa = [
    (u'소프트웨어 요구사항이 모두 포함되었는가?',
     u'예. 소프트웨어 요구사항 추적성을 통하여 모든 소프트웨어 요구사항이 소프트웨어 아키텍쳐의 모듈로 사상되었음을 확인하였다. 다만 상태가 «확인 필요»인 요구사항은 확정 후 본 문서를 개정한다.'),
    (u'소프트웨어 모듈(항목) 간 연계성 및 하드웨어 사이의 연계성이 확인되는가?',
     u'예. 하드웨어 아키텍쳐를 기반으로 소프트웨어 아키텍쳐를 설계하였으며, 프로세서 간 및 주변 장치와의 경계는 별도의 인터페이스 절에 규정하였다.'),
    (u'SOUP 항목의 적절한 운영이 지원되는가?',
     u'블루투스 콤보 칩셋의 펌웨어는 본 문서의 범위 밖에서 별도로 관리되며, Ezairo 8300 소프트웨어는 규정된 인터페이스를 통해서만 해당 펌웨어에 의존한다. 그 외 SOUP 항목은 없다.'),
]
for i, (q, a) in enumerate(qa, 1):
    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(10)
    p.paragraph_format.space_after = Pt(2)
    p.paragraph_format.left_indent = Cm(0.8)
    p.paragraph_format.first_line_indent = Cm(-0.8)
    set_font(p.add_run(u'%d)  %s' % (i, q)), 10.5, True)
    p = doc.add_paragraph()
    p.paragraph_format.space_after = Pt(2)
    p.paragraph_format.left_indent = Cm(0.8)
    p.paragraph_format.line_spacing = 1.35
    p.alignment = J
    set_font(p.add_run(u': ' + a), 10)

OUT = u'E:/workspace/projects/sound1-fw-e8300-backup/docs/참고/인허가/사운드1'
if not os.path.isdir(OUT):
    os.makedirs(OUT)
path = OUT + u'/[01] %s 소프트웨어 아키텍쳐 기술서.docx' % DOC_NO
doc.save(path)
print('SAVED:', path)
