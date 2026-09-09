# -*- coding: utf-8 -*-
"""Sound1 소프트웨어 상세 설계서 (SDDD) 생성 - 1단계: CFX 장까지"""
import io, os, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

from docx import Document
from docx.shared import Pt, Cm, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.oxml.ns import qn
from docx.oxml import OxmlElement

import cfx_units as CU
import cm3_units as CM
import boot_units as BO

FONT = u'맑은 고딕'
HDR_BG = 'D9D9D9'
SUB_BG = 'F2F2F2'
XREF_BG = 'EDF3FA'
C = WD_ALIGN_PARAGRAPH.CENTER
L = WD_ALIGN_PARAGRAPH.LEFT
J = WD_ALIGN_PARAGRAPH.JUSTIFY

DOC_NO = 'SDDD-FW-SND1'
MODEL = 'SND1-A-EXT'
BODY_W = 17.0
TASK = u'E:/workspace/projects/sound1-fw-e8300-backup/docs/tasks/인허가'
IMG_SAD = TASK + u'/20260909_1129_sad-sound1/그림'
IMG = TASK + u'/20260909_1419_sddd-sound1/그림'


# ---------------------------------------------------------------- 유틸
def set_font(run, size=10, bold=False, color=None):
    run.font.size = Pt(size)
    run.bold = bold
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


def cell_text(cell, text, size=9, bold=False, align=L, bg=None):
    cell.text = ''
    p = cell.paragraphs[0]
    p.paragraph_format.space_before = Pt(2)
    p.paragraph_format.space_after = Pt(2)
    p.alignment = align
    for i, ln in enumerate(text.split('\n')):
        if i:
            p.add_run().add_break()
        set_font(p.add_run(ln), size, bold)
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
    tbl = t._tbl
    g = tbl.find(qn('w:tblGrid'))
    if g is not None:
        tbl.remove(g)
    g = OxmlElement('w:tblGrid')
    for w in widths:
        gc = OxmlElement('w:gridCol')
        gc.set(qn('w:w'), str(int(round(w * 567))))
        g.append(gc)
    tbl.insert(list(tbl).index(tbl.find(qn('w:tblPr'))) + 1, g)


def set_widths(t, widths, skip_rows=0):
    set_grid(t, widths)
    for row in list(t.rows)[skip_rows:]:
        cells = row.cells
        if len(cells) != len(widths):
            continue
        seen = set()
        for i, w in enumerate(widths):
            k = id(cells[i]._tc)
            if k in seen:
                continue
            seen.add(k)
            cells[i].width = Cm(w)


def repeat_header(row):
    trPr = row._tr.get_or_add_trPr()
    el = OxmlElement('w:tblHeader')
    el.set(qn('w:val'), 'true')
    trPr.append(el)


def para(doc, text='', size=10, bold=False, sb=3, sa=3, align=J, indent=0.0):
    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(sb)
    p.paragraph_format.space_after = Pt(sa)
    p.paragraph_format.line_spacing = 1.35
    if indent:
        p.paragraph_format.left_indent = Cm(indent)
    p.alignment = align
    if text:
        set_font(p.add_run(text), size, bold)
    return p


def head(doc, num, text, level=1):
    sizes = {1: 15, 2: 12.5, 3: 11.5, 4: 10.5, 5: 10}
    sb = {1: 20, 2: 15, 3: 12, 4: 10, 5: 8}
    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(sb[level])
    p.paragraph_format.space_after = Pt(5)
    p.paragraph_format.keep_with_next = True
    p.alignment = L
    if level >= 3:
        p.paragraph_format.left_indent = Cm(0.35 * (level - 2))
    set_font(p.add_run(u'%s  %s' % (num, text)), sizes[level], True)
    return p


def field(doc, label, value, indent=0.9):
    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(0)
    p.paragraph_format.space_after = Pt(1)
    p.paragraph_format.left_indent = Cm(indent)
    p.alignment = L
    set_font(p.add_run(u'%s : ' % label), 9.5, True)
    set_font(p.add_run(value), 9.5, False)
    return p


def bullet(doc, text, indent=1.25):
    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(0)
    p.paragraph_format.space_after = Pt(1)
    p.paragraph_format.left_indent = Cm(indent + 0.42)
    p.paragraph_format.first_line_indent = Cm(-0.42)
    p.paragraph_format.line_spacing = 1.28
    p.alignment = J
    set_font(p.add_run(u'- ' + text), 9.5)
    return p


def caption(doc, text):
    p = doc.add_paragraph()
    p.alignment = C
    p.paragraph_format.space_before = Pt(4)
    p.paragraph_format.space_after = Pt(12)
    set_font(p.add_run(text), 9.5, True)


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
        for i, v in enumerate(r):
            cell_text(row.cells[i], v, bsize, False, (halign[i] if halign else L))
    set_widths(t, widths)
    return t


def picture(doc, path, width_cm):
    p = doc.add_paragraph()
    p.alignment = C
    p.paragraph_format.space_before = Pt(8)
    p.paragraph_format.space_after = Pt(2)
    p.add_run().add_picture(path, width=Cm(width_cm))


def page_field(paragraph):
    def fld(instr):
        r = paragraph.add_run()
        a = OxmlElement('w:fldChar'); a.set(qn('w:fldCharType'), 'begin')
        b = OxmlElement('w:instrText'); b.set(qn('xml:space'), 'preserve'); b.text = instr
        c = OxmlElement('w:fldChar'); c.set(qn('w:fldCharType'), 'end')
        r._r.append(a); r._r.append(b); r._r.append(c)
        set_font(r, 8.5)
    fld(' PAGE ')
    set_font(paragraph.add_run(' / '), 8.5)
    fld(' NUMPAGES ')


# ---------------------------------------------------------------- 문서
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
_r = st.element.get_or_add_rPr()
_f = _r.find(qn('w:rFonts'))
if _f is None:
    _f = OxmlElement('w:rFonts'); _r.append(_f)
for _a in ('w:eastAsia', 'w:ascii', 'w:hAnsi'):
    _f.set(qn(_a), FONT)

# 머리글
hdr = sec.header
hdr.paragraphs[0].text = ''
ht = hdr.add_table(rows=4, cols=4, width=Cm(BODY_W))
ht.style = 'Table Grid'
fixed(ht)
cell_text(ht.rows[0].cells[0], MODEL, 9, True, C)
cell_text(ht.rows[0].cells[1], u'소프트웨어 상세 설계서', 10, True, C)
for r, (k, v) in enumerate([(u'표준번호', DOC_NO), (u'제(개)정일자', u'YYYY.MM.DD'),
                            (u'제(개)정 번호', u'0')]):
    cell_text(ht.rows[r].cells[2], k, 8.5, True, C, SUB_BG)
    cell_text(ht.rows[r].cells[3], v, 8.5, False, C)
cell_text(ht.rows[3].cells[2], u'페 이 지', 8.5, True, C, SUB_BG)
pg = ht.rows[3].cells[3].paragraphs[0]
pg.alignment = C
page_field(pg)
ht.rows[0].cells[0].merge(ht.rows[3].cells[0])
ht.rows[0].cells[1].merge(ht.rows[3].cells[1])
set_widths(ht, [3.6, 6.4, 3.4, 3.6])

# ---------------- 표지
p = doc.add_paragraph()
p.paragraph_format.space_before = Pt(150)
tp = doc.add_table(rows=1, cols=1)
tp.style = 'Table Grid'
tp.alignment = WD_TABLE_ALIGNMENT.CENTER
fixed(tp)
cc = tp.rows[0].cells[0]
cc.text = ''
p0 = cc.paragraphs[0]
p0.alignment = C
p0.paragraph_format.space_before = Pt(26)
p0.paragraph_format.space_after = Pt(8)
set_font(p0.add_run(u'소프트웨어 상세 설계서'), 24, True)
p1 = cc.add_paragraph()
p1.alignment = C
p1.paragraph_format.space_after = Pt(26)
set_font(p1.add_run(u'(%s)' % MODEL), 14)
set_widths(tp, [BODY_W])

para(doc, '', sa=40)
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
para(doc, u'개정 이력', 11, True, sb=10, sa=5, align=L)
tr = doc.add_table(rows=1, cols=4)
tr.style = 'Table Grid'
tr.alignment = WD_TABLE_ALIGNMENT.CENTER
fixed(tr)
for i, h in enumerate([u'No.', u'개정 내용', u'개정 위치', u'개정 날짜']):
    cell_text(tr.rows[0].cells[i], h, 9.5, True, C, HDR_BG)
rows = [(u'0', u'최초 제정', u'-', u'YYYY.MM.DD')] + [(str(n), '', '', '') for n in range(1, 10)]
for r in rows:
    row = tr.add_row()
    for i, v in enumerate(r):
        cell_text(row.cells[i], v, 9, False, C if i in (0, 2, 3) else L)
set_widths(tr, [1.6, 8.4, 3.5, 3.5])

doc.add_page_break()

# ---------------- 1. 서론
head(doc, u'1.', u'서론', 1)
head(doc, u'1.1.', u'개요', 2)
para(doc, u'본 문서는 인공와우 외부장치 제품의 펌웨어를 구현하기 위한 소프트웨어의 유닛을 설계하고 기술한다.')
head(doc, u'1.2.', u'목적', 2)
para(doc, u'인공와우 외부장치 소프트웨어의 아키텍쳐를 기반으로 하위 유닛을 상세하게 설계함으로써, '
          u'소프트웨어 개발 절차에 따라 안전하고 일관성 및 정확성을 보장하는 소프트웨어가 개발되는 것을 목적으로 한다.')
head(doc, u'1.3.', u'대상', 2)
para(doc, u'본 제품의 펌웨어 개발 프로젝트에 참여하는 모든 구성원은 이 문서를 참고하도록 한다.')
head(doc, u'1.4.', u'식별자 체계', 2)
para(doc, u'소프트웨어 항목과 유닛은 다음 규칙으로 식별한다. 유닛 검증 기준서와 보고서는 이 식별자를 그대로 참조한다.')
table(doc, [u'식별자', u'의미', u'번호 부여'], [4.0, 7.0, 6.0],
      [(u'SI-ID-<프로세서>-<번호>', u'소프트웨어 항목 (Software Item)', u'프로세서별 1부터 연속'),
       (u'SU-ID-<프로세서>-<번호>', u'소프트웨어 유닛 (Software Unit)', u'프로세서별 1부터 연속, 모듈이 바뀌어도 이어짐')],
      halign=[C, L, L])
para(doc, u'일부 유닛에는 «참조» 항목이 붙는다. 이는 별도로 관리되는 유닛 및 모듈 분해 자산의 라벨이며, '
          u'해당 유닛의 검증 순서와 의존 관계가 그 자산에 정의되어 있음을 뜻한다.', sb=6)

head(doc, u'1.5.', u'유닛 기술 서식', 2)
para(doc, u'본 문서는 구현 지시서다. 각 유닛은 구현자가 코드를 보지 않고도 만들 수 있도록 다음 항목으로 기술한다.')
table(doc, [u'항목', u'무엇을 적는가', u'필수'], [3.4, 11.0, 2.6],
      [(u'입력 파라미터', u'유닛이 받는 값. 없으면 «없음»', u'필수'),
       (u'반환 파라미터', u'유닛이 돌려주는 값. 없으면 «없음»', u'필수'),
       (u'수정 파라미터', u'유닛이 바꾸는 상태. 전역 상태, 레지스터, 공유 메모리를 포함한다', u'필수'),
       (u'참조', u'별도 유닛 분해 자산의 라벨', u'해당 시'),
       (u'목적', u'이 유닛이 왜 필요한가. 없애면 무엇이 안 되는가', u'필수'),
       (u'기능 및 동작', u'무엇을 어떤 순서와 조건으로 하는가. 구현 가능한 수준으로 적는다', u'필수'),
       (u'설계 근거', u'왜 그 방식을 골랐는가. 다른 방식이 왜 안 되는가', u'해당 시'),
       (u'예외 및 제약', u'실패했을 때의 처리, 지켜야 할 순서와 경계 조건', u'해당 시')],
      halign=[L, L, C])
para(doc, u'«설계 근거»와 «예외 및 제약»은 그 유닛에 판단이 필요한 지점이 있을 때만 붙인다. '
          u'자명한 유닛에는 붙이지 않는다.', sb=4)

head(doc, u'1.6.', u'적용 범위', 2)
table(doc, [u'구분', u'대상', u'본 문서에서의 취급'], [3.0, 6.0, 8.0],
      [(u'포함', u'CFX Core', u'항목 및 유닛까지 기술'),
       (u'포함', u'HEAR Core', u'항목 및 유닛까지 기술'),
       (u'포함', u'Cortex-M3', u'항목 및 유닛까지 기술'),
       (u'포함', u'부트로더 및 무선 갱신(OTA)', u'항목 및 유닛까지 기술'),
       (u'제외', u'블루투스 콤보 칩셋 펌웨어', u'인터페이스로만 기술'),
       (u'제외', u'생산 및 서비스용 캘리브레이션 실행체', u'제품 출하 펌웨어에 포함되지 않음'),
       (u'제외', u'제3자 제공 구성요소 (파일 시스템, 암복호화 라이브러리, 디버그 출력)',
        u'SOUP 로 취급하며 유닛으로 분해하지 않음')],
      halign=[C, L, L])

head(doc, u'1.7.', u'관련문서', 2)
head(doc, u'1.7.1.', u'프로젝트', 3)
table(doc, [u'No', u'문서번호', u'문서 제목'], [1.6, 5.4, 10.0],
      [(u'1', u'SDP-FW-SND1', u'소프트웨어 개발 계획서'),
       (u'2', u'SGR-FW-SND1', u'소프트웨어 일반 요구사항'),
       (u'3', u'SR-FW-SND1', u'소프트웨어 요구사항'),
       (u'4', u'SAD-FW-SND1', u'소프트웨어 아키텍쳐 기술서')],
      halign=[C, C, L])
head(doc, u'1.7.2.', u'표준 및 규제', 3)
table(doc, [u'No', u'문서번호', u'문서 제목'], [1.6, 5.4, 10.0],
      [(u'1', u'IEC 62304', u'Medical device software - Software life cycle processes'),
       (u'2', u'ISO 14971', u'Medical devices - Application of risk management to medical devices')],
      halign=[C, C, L])

doc.add_page_break()

# ---------------- 2. 하드웨어 구조
head(doc, u'2.', u'하드웨어 구조', 1)
para(doc, u'본 문서가 다루는 소프트웨어가 동작하는 하드웨어 구성은 아래 <그림 1.>과 같다.')
picture(doc, IMG_SAD + u'/그림1_시스템_블록도.png', 16.6)
caption(doc, u'< 그림 1. 인공와우 시스템 블록도 >')

# ---------------- 3. 프로세서 간 제어 인터페이스
head(doc, u'3.', u'프로세서 간 제어 인터페이스', 1)
para(doc, u'각 프로세서의 소프트웨어는 아래 <그림 2.>의 구조로 나뉘며, 경계에서 아래 표의 데이터를 주고받는다. '
          u'각 프로세서 내부의 유닛 설계는 이어지는 장에서 기술한다.')
picture(doc, IMG_SAD + u'/그림2_소프트웨어_아키텍쳐_블록도.png', 16.6)
caption(doc, u'< 그림 2. 소프트웨어 아키텍쳐 블록도 >')
table(doc, [u'경계', u'매체', u'주고받는 내용'], [4.2, 2.8, 10.0],
      [(u'CFX Core <-> Cortex-M3', u'공유 메모리',
        u'오디오 입력 최대값, 자극 파라미터, 매핑 데이터, 시스템 상태 및 제어 요청'),
       (u'CFX Core -> HEAR Core', u'Function Chain', u'신호처리 가속 연산 요청 및 결과'),
       (u'Cortex-M3 <-> 블루투스 콤보 칩셋', u'SPI',
        u'리모콘 및 매핑 명령과 응답, 배터리 잔량, 충전기 연결 상태, 크래들 뚜껑 상태, 갱신 이미지'),
       (u'CFX Core <-> 블루투스 콤보 칩셋', u'I2S', u'오디오 스트림'),
       (u'CFX Core -> FPGA', u'PCM', u'자극 비트스트림'),
       (u'Cortex-M3 -> FPGA', u'I2C', u'자극 출력 설정'),
       (u'Cortex-M3 -> TX PMIC', u'I2C', u'경피 유도 링크 전력 설정'),
       (u'Cortex-M3 <-> 터치 센서', u'I2C', u'터치 센서 설정 및 검출 상태'),
       (u'Cortex-M3 <-> EEPROM', u'SPI', u'맵 데이터 및 설정 값')])

doc.add_page_break()

# ---------------- 4. CFX
FLOWMAP = {
    'cfx_init': (u'그림3_CFX_시스템초기화.png', u'< 그림 3. CFX 시스템 초기화 항목 >'),
    'cfx_loop': (u'그림4_CFX_주제어.png', u'< 그림 4. CFX 주 제어 항목 >'),
    'cfx_standby': (u'그림5_CFX_저전력대기.png', u'< 그림 5. CFX 저전력 대기 항목 >'),
    'cfx_dispatch': (u'그림6_CFX_신호디스패치.png', u'< 그림 6. CFX 신호 디스패치 항목 >'),
    'cfx_sp': (u'그림7_CFX_신호처리실행.png', u'< 그림 7. CFX 신호처리 실행 항목 >'),
    'cfx_pcm': (u'그림8_CFX_PCM제어.png', u'< 그림 8. CFX PCM 제어 항목 >'),
    'cm3_init': (u'그림9_CM3_시스템초기화.png', u'< 그림 9. Cortex-M3 시스템 초기화 항목 >'),
    'cm3_loop': (u'그림10_CM3_주제어.png', u'< 그림 10. Cortex-M3 주 제어 항목 >'),
    'cm3_syscontrol': (u'그림11_CM3_시스템상태제어.png', u'< 그림 11. Cortex-M3 시스템 상태 제어 항목 >'),
    'cm3_sleep': (u'그림12_CM3_저전력대기.png', u'< 그림 12. Cortex-M3 저전력 대기 항목 >'),
    'cm3_isd': (u'그림13_CM3_내부기실행제어.png', u'< 그림 13. Cortex-M3 내부기 실행 제어 항목 >'),
    'cm3_ble': (u'그림14_CM3_블루투스통신제어.png', u'< 그림 14. Cortex-M3 블루투스 통신 제어 항목 >'),
    'boot_init': (u'그림15_BOOT_시스템초기화.png', u'< 그림 15. 부트로더 시스템 초기화 항목 >'),
    'boot_mode': (u'그림16_BOOT_동작모드판정.png', u'< 그림 16. 부트로더 동작 모드 판정 항목 >'),
    'boot_load': (u'그림17_BOOT_기동실행.png', u'< 그림 17. 부트로더 기동 실행 항목 >'),
    'boot_verify': (u'그림18_BOOT_무결성검증복호화.png', u'< 그림 18. 부트로더 무결성 검증 및 복호화 항목 >'),
    'boot_slot': (u'그림19_BOOT_적용대상선택.png', u'< 그림 19. 부트로더 적용 대상 선택 항목 >'),
}

head(doc, u'4.', u'CFX Core 소프트웨어 상세 설계', 1)
para(doc, u'CFX Core 는 오디오 입력부터 자극 데이터 생성까지의 실시간 신호처리를 담당한다. '
          u'하드웨어 브링업과 코어 기동은 부트로더가 수행하므로, CFX Core 의 초기화는 '
          u'오디오 경로와 인터페이스 설정부터 시작한다.')

CHECKLIST = TASK + u'/20260909_1419_sddd-sound1/유닛 제외 체크리스트.md'


def load_exclusions():
    """체크리스트에서 제외 대상을 읽는다. 체크(x)된 것이 제외 대상이다."""
    ex_units = set()
    ex_items = set()
    if not os.path.exists(CHECKLIST):
        return ex_units, ex_items, False
    import re as _re
    cur_item = None
    for line in io.open(CHECKLIST, encoding='utf-8'):
        m = _re.match(r'\s*-\s*\[([ xX])\]\s*\*\*\[항목\]\s*(\d+\.\d+\.\d+)\.', line)
        if m:
            cur_item = m.group(2)
            if m.group(1).lower() == 'x':
                ex_items.add(cur_item)
            continue
        m = _re.match(r'\s*-\s*\[([ xX])\]\s*`([A-Z0-9]+)-(\d+)`', line)
        if m and m.group(1).lower() == 'x':
            ex_units.add((m.group(2), int(m.group(3))))
    return ex_units, ex_items, True


EX_UNITS, EX_ITEMS, HAS_CHECKLIST = load_exclusions()


def render_chapter(chap_no, sections, core):
    # 제외 대상을 걸러 낸 사본을 만든다 (원본 데이터는 건드리지 않는다)
    kept = []
    si = 0
    for modname, blocks in sections:
        si += 1
        bi = 0
        keep_blocks = []
        for blk in blocks:
            bi += 1
            key = u'%d.%d.%d' % (chap_no, si, bi)
            if key in EX_ITEMS:
                continue
            units = [u for u in blk['units'] if (core, u['id']) not in EX_UNITS]
            if not units:
                continue
            nb = dict(blk)
            nb['units'] = units
            keep_blocks.append(nb)
        if keep_blocks:
            kept.append((modname, keep_blocks))

    # 남은 유닛에 연속 번호를 다시 부여한다 (사본에만 적용)
    seq = 0
    for _, blocks in kept:
        for blk in blocks:
            copies = []
            for u in blk['units']:
                seq += 1
                c = dict(u)
                c['id'] = seq
                copies.append(c)
            blk['units'] = copies

    nsec = 0
    for modname, blocks in kept:
        nsec += 1
        head(doc, u'%d.%d.' % (chap_no, nsec), modname, 2)
        nblk = 0
        for blk in blocks:
            nblk += 1
            num = u'%d.%d.%d.' % (chap_no, nsec, nblk)
            title = (u'%s, %s' % (blk['id'], blk['name'])) if blk['id'] else blk['name']
            head(doc, num, title, 3)
            para(doc, blk['intro'], 10, sb=2, sa=6)
            if blk.get('flow'):
                fn, cap = FLOWMAP[blk['flow']]
                picture(doc, IMG + u'/' + fn, 11.4)
                caption(doc, cap)
            for k, u in enumerate(blk['units'], 1):
                head(doc, u'%s%d.' % (num, k),
                     u'SU-ID-%s-%03d, %s' % (core, u['id'], u['name']), 4)
                field(doc, u'입력 파라미터', u['inp'])
                field(doc, u'반환 파라미터', u['ret'])
                field(doc, u'수정 파라미터', u['mod'])
                if u.get('xref'):
                    field(doc, u'참조', u['xref'])
                if u.get('purpose'):
                    field(doc, u'목적', u'')
                    bullet(doc, u['purpose'])
                field(doc, u'기능 및 동작', u'')
                for dline in u['desc']:
                    bullet(doc, dline)
                if u.get('why'):
                    field(doc, u'설계 근거', u'')
                    for wline in u['why']:
                        bullet(doc, wline)
                if u.get('limit'):
                    field(doc, u'예외 및 제약', u'')
                    for lline in u['limit']:
                        bullet(doc, lline)


render_chapter(4, CU.CFX_SECTIONS, 'CFX')

doc.add_page_break()

# ---------------- 5~7 예정 장
head(doc, u'5.', u'Cortex-M3 소프트웨어 상세 설계', 1)
para(doc, u'Cortex-M3 는 시스템 전체의 상태 관리와 외부 장치 제어를 담당한다. 입력을 모아 상태를 전이시키고 '
          u'그 결과를 전원, 무선, 내부기, 표시에 반영한다. 하드웨어 브링업과 코어 기동은 부트로더가 '
          u'수행하므로, Cortex-M3 의 초기화는 통신 인터페이스와 보조 장치 설정부터 시작한다.')
render_chapter(5, CM.CM3_SECTIONS, 'CM3')

doc.add_page_break()

head(doc, u'6.', u'부트로더 소프트웨어 상세 설계', 1)
para(doc, u'부트로더는 전원 인가 직후 응용 소프트웨어보다 먼저 실행된다. 하드웨어를 초기 상태로 만들고, '
          u'저장장치에 암호화되어 보관된 실행 이미지를 검증 및 복호화하여 각 코어의 프로그램 메모리에 '
          u'적재한 뒤 코어를 기동한다. 무선으로 전달된 갱신 이미지는 다음 기동에서 적용하며, 적용에 '
          u'실패하면 직전에 정상 동작하던 이미지로 되돌린다.')
para(doc, u'제3자가 제공하는 파일 시스템, 암복호화 라이브러리, 디버그 출력 구성요소는 SOUP 로 취급하여 '
          u'유닛으로 분해하지 않는다.', sb=2)
render_chapter(6, BO.BOOT_SECTIONS, 'BOOT')

doc.add_page_break()

head(doc, u'7.', u'블루투스 콤보 칩셋 소프트웨어 상세 설계', 1)
para(doc, u'블루투스 콤보 칩셋의 펌웨어는 본 문서의 범위에 포함되지 않는다. 해당 펌웨어는 별도의 개발 및 '
          u'형상 관리 체계에서 관리되며, 본 문서는 해당 칩셋과의 경계에서 오가는 데이터만 앞 장에서 기술한다.')
para(doc, u'본 문서가 외부기 전체의 소프트웨어 상세 설계서로 확장되는 경우, 해당 펌웨어의 항목과 유닛은 '
          u'본 장에 기술한다. 아래 표는 본 문서가 다루는 소프트웨어가 해당 펌웨어에 의존하는 기능의 목록이며, '
          u'확장 시 각 기능에 대응하는 유닛을 이 표에서 이어 기술한다.')
table(doc, [u'의존 기능', u'기대하는 동작', u'전달 경로'], [4.6, 8.0, 4.4],
      [(u'무선 통신', u'리모콘 앱 및 매핑 프로그램의 명령을 수신하여 전달하고, 응답을 회신한다', u'SPI'),
       (u'통신 보안', u'페어링을 적용하여 데이터 통신 보안을 확보한다', u'SPI'),
       (u'배터리 관리', u'배터리 잔량을 측정하여 주기적으로 전달한다', u'SPI'),
       (u'충전 관리', u'충전기 연결 상태와 크래들 뚜껑 상태를 전달한다', u'SPI'),
       (u'오디오 전달', u'외부 오디오 스트림을 전달한다', u'I2S'),
       (u'갱신 중계', u'무선으로 수신한 갱신 이미지를 전달한다', u'SPI')])

# ---------------- 8. 검증
head(doc, u'8.', u'소프트웨어 상세 설계 검증', 1)
qa = [
    (u'소프트웨어 아키텍쳐가 모두 구현되도록 설계 되었는가?',
     u'예. 소프트웨어 아키텍쳐 기술서가 정의한 각 모듈의 항목이 본 문서의 소프트웨어 항목으로 대응되며, '
     u'각 항목은 하나 이상의 유닛으로 분해되었다.'),
    (u'소프트웨어 아키텍쳐와 상충되는 것은 없는가?',
     u'예. 모듈 구성과 프로세서 간 인터페이스는 소프트웨어 아키텍쳐 기술서와 동일하며, '
     u'본 문서는 그 하위의 유닛만을 추가로 정의한다.'),
    (u'각 유닛의 인터페이스가 명확히 정의되었는가?',
     u'예. 모든 유닛에 입력 파라미터, 반환 파라미터, 수정 파라미터를 기술하였으며, '
     u'그 내용은 실제 구현의 함수 인터페이스와 대조하여 확인하였다.'),
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
path = OUT + u'/[02] %s 소프트웨어 상세 설계서.docx' % DOC_NO
tmp = path + u'.tmp'
doc.save(tmp)
try:
    if os.path.exists(path):
        os.remove(path)
    os.rename(tmp, path)
except OSError:
    print(u'[경고] 원본 파일이 열려 있어 교체하지 못했다. 임시 파일로 저장했다:')
    print(u'       %s' % tmp)
    path = tmp
ncfx = sum(len(b['units']) for _, bs in CU.CFX_SECTIONS for b in bs)
ncm3 = sum(len(b['units']) for _, bs in CM.CM3_SECTIONS for b in bs)
nbo = sum(len(b['units']) for _, bs in BO.BOOT_SECTIONS for b in bs)
print(u'SAVED: %s' % path)
print(u'CFX %d / CM3 %d / BOOT %d / 합계 %d건 (원본 기준)' % (ncfx, ncm3, nbo, ncfx + ncm3 + nbo))
if HAS_CHECKLIST:
    print(u'체크리스트 반영 - 제외 항목 %d개 · 제외 유닛 %d개' % (len(EX_ITEMS), len(EX_UNITS)))
else:
    print(u'체크리스트 없음 - 전체 수록')
