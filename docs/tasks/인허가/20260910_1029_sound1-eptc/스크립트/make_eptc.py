# -*- coding: utf-8 -*-
"""Sound1 필수성능시험 기준서 Word 생성기."""

from __future__ import annotations

import io
import math
import os
import shutil
import sys
import zipfile
from pathlib import Path

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")

from docx import Document
from docx.enum.section import WD_SECTION
from docx.enum.table import WD_CELL_VERTICAL_ALIGNMENT, WD_ROW_HEIGHT_RULE, WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_BREAK
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Pt, RGBColor, Twips


ROOT = Path(__file__).resolve().parents[5]
TASK = Path(__file__).resolve().parents[1]
TMP = TASK / "tmp"
MEDIA = TMP / "reference-media"
FIG = TASK / "그림"
SOURCE = TMP / "reference.docx"
OUT_DIR = ROOT / "docs" / "참고" / "인허가" / "사운드1"
OUT = OUT_DIR / "[Sound1] EPTC-SND1 필수성능시험 기준서_rev0.docx"

DOC_NO = "EPTC-SND1"
MODEL = "SND1-A-EXT"
REV = "0"
REV_DATE = "2026.09.10"
FORM_NO = "F7030-02 (Rev.1)"

FONT = "맑은 고딕"
BODY_W = 17.0
BLUE = (0x00, 0x55, 0xA5)
NAVY = "17365D"
MID_BLUE = "D9EAF7"
LIGHT_BLUE = "EAF4FF"
HEADER_BG = "D9E2F3"
SUB_BG = "F2F2F2"
GREEN_BG = "E2F0D9"
ORANGE_BG = "FCE4D6"
YELLOW_BG = "FFF2CC"
WHITE = "FFFFFF"
BLACK = (0, 0, 0)

C = WD_ALIGN_PARAGRAPH.CENTER
L = WD_ALIGN_PARAGRAPH.LEFT
R = WD_ALIGN_PARAGRAPH.RIGHT
J = WD_ALIGN_PARAGRAPH.JUSTIFY


def ensure_media() -> None:
    MEDIA.mkdir(parents=True, exist_ok=True)
    if (MEDIA / "image28.png").exists():
        return
    with zipfile.ZipFile(SOURCE) as zf:
        for name in zf.namelist():
            if name.startswith("word/media/") and not name.endswith("/"):
                (MEDIA / Path(name).name).write_bytes(zf.read(name))


def set_font(run, size=10, bold=False, color=BLACK, italic=False, name=FONT):
    run.font.name = name
    run.font.size = Pt(size)
    run.bold = bold
    run.italic = italic
    if color is not None:
        run.font.color.rgb = RGBColor(*color)
    rpr = run._element.get_or_add_rPr()
    fonts = rpr.find(qn("w:rFonts"))
    if fonts is None:
        fonts = OxmlElement("w:rFonts")
        rpr.append(fonts)
    for attr in ("w:eastAsia", "w:ascii", "w:hAnsi"):
        fonts.set(qn(attr), name)


def shade(cell, fill):
    tcpr = cell._tc.get_or_add_tcPr()
    shd = tcpr.find(qn("w:shd"))
    if shd is None:
        shd = OxmlElement("w:shd")
        tcpr.append(shd)
    shd.set(qn("w:val"), "clear")
    shd.set(qn("w:color"), "auto")
    shd.set(qn("w:fill"), fill)


def set_cell_margins(cell, top=70, start=80, bottom=70, end=80):
    tc = cell._tc
    tcpr = tc.get_or_add_tcPr()
    mar = tcpr.first_child_found_in("w:tcMar")
    if mar is None:
        mar = OxmlElement("w:tcMar")
        tcpr.append(mar)
    for key, val in (("top", top), ("start", start), ("bottom", bottom), ("end", end)):
        node = mar.find(qn(f"w:{key}"))
        if node is None:
            node = OxmlElement(f"w:{key}")
            mar.append(node)
        node.set(qn("w:w"), str(val))
        node.set(qn("w:type"), "dxa")


def set_cell_text(cell, text, size=9, bold=False, align=L, bg=None, color=BLACK):
    cell.text = ""
    cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
    set_cell_margins(cell)
    p = cell.paragraphs[0]
    p.alignment = align
    p.paragraph_format.space_before = Pt(1)
    p.paragraph_format.space_after = Pt(1)
    p.paragraph_format.line_spacing = 1.15
    lines = str(text).split("\n")
    for i, line in enumerate(lines):
        if i:
            p.add_run().add_break()
        set_font(p.add_run(line), size=size, bold=bold, color=color)
    if bg:
        shade(cell, bg)


def no_row_split(row):
    trpr = row._tr.get_or_add_trPr()
    cant = OxmlElement("w:cantSplit")
    trpr.append(cant)


def repeat_header(row):
    trpr = row._tr.get_or_add_trPr()
    h = OxmlElement("w:tblHeader")
    h.set(qn("w:val"), "true")
    trpr.append(h)


def fixed(table):
    table.autofit = False
    tblpr = table._tbl.tblPr
    old = tblpr.find(qn("w:tblLayout"))
    if old is not None:
        tblpr.remove(old)
    layout = OxmlElement("w:tblLayout")
    layout.set(qn("w:type"), "fixed")
    tblpr.append(layout)


def set_grid(table, widths):
    widths_dxa = [int(round(Cm(width).twips)) for width in widths]
    table_width_dxa = sum(widths_dxa)

    tblpr = table._tbl.tblPr
    tblw = tblpr.find(qn("w:tblW"))
    if tblw is None:
        tblw = OxmlElement("w:tblW")
        tblpr.append(tblw)
    tblw.set(qn("w:type"), "dxa")
    tblw.set(qn("w:w"), str(table_width_dxa))

    # 표 외곽선이 셀 본문의 시작선과 일치하도록 첫 셀의 안쪽 여백을 사용한다.
    first_tcpr = table.cell(0, 0)._tc.get_or_add_tcPr()
    first_margin = first_tcpr.find(qn("w:tcMar"))
    start_margin = first_margin.find(qn("w:start")) if first_margin is not None else None
    indent_dxa = int(start_margin.get(qn("w:w"))) if start_margin is not None else 80
    tblind = tblpr.find(qn("w:tblInd"))
    if tblind is None:
        tblind = OxmlElement("w:tblInd")
        tblpr.append(tblind)
    tblind.set(qn("w:type"), "dxa")
    tblind.set(qn("w:w"), str(indent_dxa))

    grid = table._tbl.find(qn("w:tblGrid"))
    if grid is not None:
        table._tbl.remove(grid)
    grid = OxmlElement("w:tblGrid")
    for width_dxa in widths_dxa:
        col = OxmlElement("w:gridCol")
        col.set(qn("w:w"), str(width_dxa))
        grid.append(col)
    tblpr = table._tbl.find(qn("w:tblPr"))
    table._tbl.insert(list(table._tbl).index(tblpr) + 1, grid)

    for i, width_dxa in enumerate(widths_dxa):
        table.columns[i].width = Twips(width_dxa)
    for row in table.rows:
        if len(row.cells) != len(widths_dxa):
            raise ValueError(f"표 열 수 불일치: {len(row.cells)} != {len(widths_dxa)}")
        for i, cell in enumerate(row.cells):
            width_dxa = widths_dxa[i]
            cell.width = Twips(width_dxa)
            tcpr = cell._tc.get_or_add_tcPr()
            tcw = tcpr.find(qn("w:tcW"))
            if tcw is None:
                tcw = OxmlElement("w:tcW")
                tcpr.append(tcw)
            tcw.set(qn("w:type"), "dxa")
            tcw.set(qn("w:w"), str(width_dxa))


def add_table(doc, headers, rows, widths, aligns=None, size=8.7, header_bg=HEADER_BG):
    table = doc.add_table(rows=1, cols=len(headers))
    table.style = "Table Grid"
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    fixed(table)
    aligns = aligns or [L] * len(headers)
    for i, h in enumerate(headers):
        set_cell_text(table.rows[0].cells[i], h, size=size, bold=True, align=C, bg=header_bg)
    repeat_header(table.rows[0])
    no_row_split(table.rows[0])
    for values in rows:
        row = table.add_row()
        for i, value in enumerate(values):
            set_cell_text(row.cells[i], value, size=size, align=aligns[i])
        no_row_split(row)
    set_grid(table, widths)
    return table


def add_paragraph(doc, text="", size=10, bold=False, align=J, before=2, after=3, color=BLACK, keep=False):
    p = doc.add_paragraph()
    p.alignment = align
    p.paragraph_format.space_before = Pt(before)
    p.paragraph_format.space_after = Pt(after)
    p.paragraph_format.line_spacing = 1.25
    p.paragraph_format.keep_with_next = keep
    if text:
        set_font(p.add_run(text), size=size, bold=bold, color=color)
    return p


def add_bullet(doc, text, level=0, color=BLACK):
    p = doc.add_paragraph()
    p.paragraph_format.left_indent = Cm(0.8 + 0.45 * level)
    p.paragraph_format.first_line_indent = Cm(-0.45)
    p.paragraph_format.space_before = Pt(1)
    p.paragraph_format.space_after = Pt(2)
    p.paragraph_format.line_spacing = 1.22
    set_font(p.add_run("• "), size=10, bold=True, color=color)
    set_font(p.add_run(text), size=10, color=color)
    return p


def add_numbered_step(doc, number, text):
    p = doc.add_paragraph()
    p.paragraph_format.left_indent = Cm(0.9)
    p.paragraph_format.first_line_indent = Cm(-0.6)
    p.paragraph_format.space_before = Pt(1)
    p.paragraph_format.space_after = Pt(3)
    p.paragraph_format.line_spacing = 1.22
    set_font(p.add_run(f"{number}. "), size=10, bold=True, color=BLUE)
    set_font(p.add_run(text), size=10)
    return p


def add_heading(doc, number, title, level=1):
    p = doc.add_paragraph(style=f"Heading {min(level, 5)}")
    p.paragraph_format.keep_with_next = True
    p.paragraph_format.space_before = Pt({1: 16, 2: 12, 3: 9, 4: 7, 5: 5}[level])
    p.paragraph_format.space_after = Pt(4)
    p.paragraph_format.left_indent = Cm(0.25 * max(0, level - 2))
    set_font(p.add_run(f"{number}. {title}" if number else title), size={1: 15, 2: 13, 3: 11.5, 4: 10.5, 5: 10}[level], bold=True, color=(0x17, 0x36, 0x5D))
    return p


def add_new_page_heading(doc, number, title, level=1):
    """새 페이지에서 시작하되 이미 페이지 선두인 경우 빈 페이지를 만들지 않는다."""
    p = add_heading(doc, number, title, level)
    p.paragraph_format.page_break_before = True
    return p


def add_label(doc, text):
    p = add_paragraph(doc, before=4, after=2, keep=True)
    set_font(p.add_run(text), size=10, bold=True, color=BLUE)
    return p


def add_note(doc, title, text, bg=LIGHT_BLUE):
    table = doc.add_table(rows=1, cols=1)
    table.style = "Table Grid"
    fixed(table)
    cell = table.cell(0, 0)
    shade(cell, bg)
    set_cell_margins(cell, top=110, start=130, bottom=110, end=130)
    set_grid(table, [BODY_W])
    p = cell.paragraphs[0]
    p.paragraph_format.space_after = Pt(2)
    set_font(p.add_run(title + "  "), size=9.5, bold=True, color=BLUE)
    set_font(p.add_run(text), size=9.5)
    return table


def add_picture(doc, path, width_cm, caption=None):
    p = doc.add_paragraph()
    p.alignment = C
    p.paragraph_format.space_before = Pt(6)
    p.paragraph_format.space_after = Pt(2)
    p.add_run().add_picture(str(path), width=Cm(width_cm))
    if caption:
        cp = add_paragraph(doc, caption, size=9, bold=True, align=C, before=1, after=7, color=(0x33, 0x4E, 0x68))
        cp.paragraph_format.keep_with_next = False
    return p


def field(paragraph, instruction, placeholder=""):
    run = paragraph.add_run()
    begin = OxmlElement("w:fldChar")
    begin.set(qn("w:fldCharType"), "begin")
    instr = OxmlElement("w:instrText")
    instr.set(qn("xml:space"), "preserve")
    instr.text = instruction
    separate = OxmlElement("w:fldChar")
    separate.set(qn("w:fldCharType"), "separate")
    text = OxmlElement("w:t")
    text.text = placeholder
    end = OxmlElement("w:fldChar")
    end.set(qn("w:fldCharType"), "end")
    run._r.extend([begin, instr, separate, text, end])
    set_font(run, size=8.5)
    return run


def add_page_number(paragraph):
    field(paragraph, " PAGE ", "1")
    set_font(paragraph.add_run(" / "), size=8.5)
    field(paragraph, " NUMPAGES ", "1")


def set_repeat_table_header_and_keep(table):
    repeat_header(table.rows[0])
    for row in table.rows:
        no_row_split(row)


def add_common_initial(doc):
    add_heading(doc, "5.1.2", "시험 준비 및 초기 조건", 3)
    steps = [
        "자극 출력 측정 지그의 수조에 PBS 용액을 표시선까지 채운다.",
        "지그에 2.96 kΩ 저항을 결합하고 디지털 멀티미터로 실제 저항값을 측정하여 기록한다.",
        "내부기 완제품을 지그에 결합한다. 전극 표면의 기포는 전극이 손상되지 않도록 가볍게 제거한다.",
        "내부기 코일 위에 5 mm 스페이서를 올리고 Sound1 외부기를 지정 방향으로 위치시킨다.",
        "저항 양단에 오실로스코프 프로브를 연결하고, 전압을 실측 저항값으로 나누어 자극 전류를 계산한다.",
        "개발자용 리모콘 앱을 BLE 제어 경로로 연결하고 LED를 비활성화한다.",
        "Bluetooth Classic 오디오 스트리밍과 통화를 종료한다. I2S 스트리밍 상태에서는 1-DMIC로 전환되므로 본 시험에 사용하지 않는다.",
        "오디오 시험은 매핑 Live 상태가 아닌 정상 독립형 자극 상태에서 수행한다. 정상 상태에서 2-DMIC 빔포밍 경로가 사용된다.",
    ]
    for i, text in enumerate(steps, 1):
        add_numbered_step(doc, i, text)


def add_measure_pass(doc, measure_steps, pass_lines, note=None):
    add_label(doc, "측정 방법")
    for i, text in enumerate(measure_steps, 1):
        add_numbered_step(doc, i, text)
    add_label(doc, "통과 기준")
    for text in pass_lines:
        add_bullet(doc, text)
    if note:
        add_note(doc, "근거", note)


def adc(spl, attenuation):
    return int(math.floor(262143 * (10 ** ((spl - 120 + attenuation) / 20))))


def fmt(value):
    return f"{value:,}"


def adc_rows(lo, hi):
    return [[str(d), fmt(adc(d, -8.293))] for d in range(lo, hi + 1)]


def add_adc_range_table(doc, lo, hi):
    add_table(doc, ["음압 [dB SPL]", "Sound1 완제품 ADC"], adc_rows(lo, hi), [8.5, 8.5], [C, C], size=9)


def setup_document():
    ensure_media()
    doc = Document()
    sec = doc.sections[0]
    sec.page_width = Cm(21.0)
    sec.page_height = Cm(29.7)
    sec.orientation = 0
    # 4행 머리글 표와 본문이 겹치지 않도록 본문 시작선을 머리글 아래에 둔다.
    sec.top_margin = Cm(2.0)
    sec.bottom_margin = Cm(1.5)
    sec.left_margin = Cm(2.0)
    sec.right_margin = Cm(2.0)
    sec.header_distance = Cm(0.9)
    sec.footer_distance = Cm(0.7)

    normal = doc.styles["Normal"]
    normal.font.name = FONT
    normal.font.size = Pt(10)
    normal.paragraph_format.line_spacing = 1.2
    rpr = normal.element.get_or_add_rPr()
    fonts = rpr.find(qn("w:rFonts"))
    if fonts is None:
        fonts = OxmlElement("w:rFonts")
        rpr.append(fonts)
    for attr in ("w:eastAsia", "w:ascii", "w:hAnsi"):
        fonts.set(qn(attr), FONT)

    for level in range(1, 6):
        style = doc.styles[f"Heading {level}"]
        style.font.name = FONT
        style.font.color.rgb = RGBColor(0x17, 0x36, 0x5D)
        style.font.bold = True
        rpr = style.element.get_or_add_rPr()
        fonts = rpr.find(qn("w:rFonts"))
        if fonts is None:
            fonts = OxmlElement("w:rFonts")
            rpr.append(fonts)
        for attr in ("w:eastAsia", "w:ascii", "w:hAnsi"):
            fonts.set(qn(attr), FONT)

    # Word/프린터별 홀짝 머리글 폭 계산 차이를 피하기 위해 식별정보를 단일 하단 띠에 둔다.
    header = sec.header
    header.paragraphs[0].text = ""
    footer = sec.footer
    p = footer.paragraphs[0]
    p.text = ""
    p.alignment = C
    p.paragraph_format.space_before = Pt(0)
    p.paragraph_format.space_after = Pt(0)
    set_font(p.add_run(FORM_NO), size=7.3, color=(0x00, 0x80, 0x00))
    set_font(p.add_run(f"   |   {DOC_NO} · {MODEL} · Rev.{REV} · {REV_DATE}   |   "), size=7.3)
    add_page_number(p)
    ppr = p._p.get_or_add_pPr()
    pbdr = OxmlElement("w:pBdr")
    top = OxmlElement("w:top")
    top.set(qn("w:val"), "single")
    top.set(qn("w:sz"), "8")
    top.set(qn("w:space"), "4")
    top.set(qn("w:color"), NAVY)
    pbdr.append(top)
    ppr.append(pbdr)

    settings = doc.settings._element
    update = settings.find(qn("w:updateFields"))
    if update is None:
        update = OxmlElement("w:updateFields")
        settings.append(update)
    update.set(qn("w:val"), "true")
    return doc


def add_cover(doc):
    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(145)
    p.alignment = C
    set_font(p.add_run("필수 성능 시험 기준서"), size=25, bold=True, color=(0x17, 0x36, 0x5D))
    p = add_paragraph(doc, MODEL, size=16, bold=True, align=C, before=15, after=40, color=BLUE)

    table = doc.add_table(rows=5, cols=4)
    table.style = "Table Grid"
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    fixed(table)
    values = [
        ["", "작성", "검토", "승인"],
        ["성함", "", "", ""],
        ["직책", "", "", ""],
        ["일자", "", "", ""],
        ["서명", "", "", ""],
    ]
    for r, row in enumerate(values):
        for c, value in enumerate(row):
            set_cell_text(table.cell(r, c), value, size=10, bold=(r == 0 or c == 0), align=C, bg=HEADER_BG if r == 0 else (SUB_BG if c == 0 else None))
        table.rows[r].height = Cm(1.15 if r < 4 else 2.1)
        table.rows[r].height_rule = WD_ROW_HEIGHT_RULE.AT_LEAST
        no_row_split(table.rows[r])
    set_grid(table, [2.5, 4.83, 4.83, 4.84])


def add_revision_and_toc(doc):
    add_new_page_heading(doc, "", "개정이력(Revision History)", 1)
    rows = [["0", "Sound1 필수성능시험 기준서 최초 작성", "전면", REV_DATE]] + [["", "", "", ""] for _ in range(11)]
    t = add_table(doc, ["개정번호", "개정내역(Description of changes)", "개정 위치", "개정 일자"], rows, [2.3, 8.0, 3.1, 3.6], [C, L, C, C], size=8.7)
    for row in t.rows[1:]:
        row.height = Cm(0.72)
        row.height_rule = WD_ROW_HEIGHT_RULE.AT_LEAST

    add_new_page_heading(doc, "", "목차", 1)
    p = doc.add_paragraph()
    field(p, ' TOC \\o "1-3" \\h \\z ', "목차는 문서를 열 때 자동 갱신됩니다.")


def add_intro(doc):
    add_new_page_heading(doc, "1", "서론", 1)
    add_heading(doc, "1.1", "개요", 2)
    add_bullet(doc, "본 문서는 인공달팽이관 Sound1 외부장치의 필수 성능 기준과 시험 절차 및 통과 기준을 정의한다.")
    add_bullet(doc, "시험자는 본 문서에 기재된 장비 구성, 입력 조건, 관측 지점, 기대 출력과 허용 범위를 사용하여 각 시험을 독립적으로 수행한다.")
    add_heading(doc, "1.2", "관련문서", 2)
    add_table(doc, ["No.", "문서 번호 또는 파일명", "문서 제목"], [
        ["1", "SAD-FW-SND1", "Sound1 소프트웨어 아키텍쳐 기술서"],
        ["2", "SDDD-FW-SND1", "Sound1 소프트웨어 상세 설계서"],
        ["3", "Sound1 입력 음압 대비 ADC 출력 산출 근거", "Sound1 완제품 ADC 판정 기준 산출 근거"],
    ], [1.3, 5.5, 10.2], [C, C, L], size=9)

    add_heading(doc, "2", "개발 대상 제품", 1)
    add_heading(doc, "2.1", "품목명", 2)
    add_bullet(doc, "인공달팽이관장치[Stimulator, electrical, auditory, cochlear], (A09320.01)")
    add_heading(doc, "2.2", "모델명", 2)
    add_bullet(doc, f"외부기: {MODEL}")
    add_bullet(doc, "호환 내부기: TD23A-A-IMP, TD23B-A-IMP, TD20A-A-IMP")
    add_heading(doc, "2.3", "사용 목적(Intended Use)", 2)
    add_bullet(doc, "중증 청각장애 환자의 청각을 회복할 목적으로 사용하는 기기")

    add_heading(doc, "3", "적용 규제 및 규격", 1)
    add_table(doc, ["No.", "구분", "규격"], [
        ["1", "매뉴얼 / 라벨", "EN 1041:2008 / ISO 15223-1:2016"],
        ["2", "생체 적합성", "ISO 10993-1:2009 / ISO 10993-3:2014 / ISO 10993-5:2009 / ISO 10993-10:2010 / ISO 10993-11:2017"],
        ["3", "의료기기의 전기·기계적 안전", "IEC 60601-1:2012"],
        ["4", "위험관리", "ISO 14971:2007"],
    ], [1.2, 5.0, 10.8], [C, L, L], size=9)


def add_equipment(doc):
    add_heading(doc, "4", "필수 성능 시험 요약", 1)
    add_heading(doc, "4.1", "필요 장비 및 지그", 2)
    items = [
        ("1", "오실로스코프", MEDIA / "image4.png", "자극 출력 파형의 전압·주파수·위상 폭·선행 위상을 측정한다."),
        ("2", "내부기 출력/측정 지그", MEDIA / "image5.png", "PBS 용액에서 내부기 전극 출력을 부하저항 양단 전압으로 측정한다."),
        ("3", "스페이서(5 mm)", MEDIA / "image6.png", "내부기와 외부기 코일 간격을 일정하게 유지한다."),
        ("4", "디지털 멀티미터", MEDIA / "image7.png", "2.96 kΩ 부하저항의 실제 저항값을 확인한다."),
        ("5", "스피커", MEDIA / "image8.jpeg", "오디오 분석기의 시험 신호를 Sound1 마이크에 음향으로 전달한다. Yamaha HS5 또는 동등품."),
        ("6", "부하저항", MEDIA / "image9.png", "내부기 자극 출력 측정용 2.96 kΩ 저항."),
        ("7", "개발자용 리모콘 앱", None, "프로그램·볼륨·LED·오디오 입력 최대값을 설정·확인한다."),
        ("8", "내부기 완제품", MEDIA / "image11.png", "Sound1에서 수신한 자극 파라미터를 전기 자극 펄스로 생성한다."),
        ("9", "오디오 분석기", MEDIA / "image12.png", "시험 신호 생성과 GRAS 43AG-7 측정 마이크의 음압·주파수 측정에 사용한다. APx525 B Series, 소프트웨어 v9.1.1 또는 동등 구성."),
        ("10", "측정용 방음 부스", MEDIA / "image13.png", "외부 소음을 차단하고 스피커·시료·측정 마이크를 배치한다."),
        ("11", "32채널 로직 애널라이저", None, "ID-9에서 전극 1~32의 자극 유무를 2 ms 이상 동시에 관측한다."),
    ]
    table = doc.add_table(rows=1, cols=4)
    table.style = "Table Grid"
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    fixed(table)
    for i, value in enumerate(["No.", "장비 및 지그", "대표 형상", "사용 목적"]):
        set_cell_text(table.rows[0].cells[i], value, size=8.5, bold=True, align=C, bg=HEADER_BG)
    repeat_header(table.rows[0])
    for no, name, picture, purpose in items:
        row = table.add_row()
        set_cell_text(row.cells[0], no, size=8.5, align=C)
        set_cell_text(row.cells[1], name, size=8.5, align=C)
        set_cell_text(row.cells[2], "", size=8.5, align=C)
        if picture and picture.exists():
            p = row.cells[2].paragraphs[0]
            p.alignment = C
            p.add_run().add_picture(str(picture), width=Cm(2.5))
        set_cell_text(row.cells[3], purpose, size=8.3, align=L)
        row.height_rule = WD_ROW_HEIGHT_RULE.AT_LEAST
        no_row_split(row)
    set_grid(table, [1.0, 3.5, 3.2, 9.3])

    add_heading(doc, "4.1.1", "측정장비 교정 정보", 3)
    add_bullet(doc, "시험에 사용한 장비의 식별정보와 교정만료일은 실제 시험 시 아래 표에 기록한다.")
    add_table(doc, ["No.", "장비명", "모델명", "장비번호", "식별번호", "교정만료일"], [
        ["1", "오실로스코프", "", "", "", ""],
        ["2", "디지털 멀티미터", "", "", "", ""],
        ["3", "오디오 분석기", "", "", "", ""],
        ["4", "32채널 로직 애널라이저", "", "", "", ""],
    ], [0.9, 3.0, 3.0, 3.0, 3.0, 4.1], [C, C, C, C, C, C], size=7.9)

    add_heading(doc, "4.2", "필요 시료", 2)
    add_table(doc, ["No.", "시료", "상태", "제조번호"], [["1", "Sound1 외부기", "기구 조립 완제품", ""]], [1.2, 5.0, 5.2, 5.6], [C, C, C, C], size=9)

    add_heading(doc, "4.3", "시험 목록", 2)
    rows = [
        ["1", "SYS-EP-TEST-ID-1", "마이크의 사운드 입력 크기 범위"],
        ["2", "SYS-EP-TEST-ID-2", "마이크의 사운드 입력 주파수 범위"],
        ["3", "SYS-EP-TEST-ID-3", "자극 출력 펄스 주파수"],
        ["4", "SYS-EP-TEST-ID-4", "자극 출력 펄스 위상 폭"],
        ["5", "SYS-EP-TEST-ID-5", "자극 출력 펄스 선행 위상"],
        ["6", "SYS-EP-TEST-ID-6", "입력 사운드 크기에 따른 자극 출력 크기"],
        ["7", "SYS-EP-TEST-ID-7", "주파수 분석에 따른 자극 채널"],
        ["8", "SYS-EP-TEST-ID-8", "채널별 자극 출력 순서"],
        ["9", "SYS-EP-TEST-ID-9", "N-of-M 상위 16개 대역 선택"],
    ]
    add_table(doc, ["No.", "시험 ID", "시험 개요"], rows, [1.2, 5.0, 10.8], [C, C, L], size=9)


def add_setup(doc):
    add_heading(doc, "4.4", "공통 시험 구성 및 설정", 2)
    add_heading(doc, "4.4.1", "자극 출력 측정 지그 구성", 3)
    steps = [
        "수조에 PBS 용액을 표시선까지 채우고, 내부기 전극을 용액에 담근다.",
        "2.96 kΩ 부하저항을 지그에 결합하고 멀티미터로 실제 저항값을 기록한다.",
        "전극 표면의 기포를 제거한다. 기포는 접촉 임피던스를 높여 출력 파형을 왜곡할 수 있다.",
        "내부기 코일 위에 5 mm 스페이서를 배치하고 그 위에 외부기 코일을 정렬한다.",
        "오실로스코프 프로브를 부하저항 양단에 연결한다.",
    ]
    for i, text in enumerate(steps, 1):
        add_numbered_step(doc, i, text)
    add_picture(doc, MEDIA / "image15.png", 10.5, "그림 1. 내부기·출력 지그·스페이서 결합 예")
    add_picture(doc, MEDIA / "image17.jpeg", 7.2, "그림 2. 부하저항 실측 예")

    add_heading(doc, "4.4.2", "방음 부스 및 음향 계측 구성", 3)
    add_bullet(doc, "스피커, Sound1 외부기, GRAS 43AG-7 측정 마이크를 방음 부스 안에 배치한다.")
    add_bullet(doc, "측정 마이크와 Sound1 마이크의 수음점이 가능한 한 같은 음장을 보도록 근접 배치한다.")
    add_bullet(doc, "Sound1은 착용 방향의 빔 축이 스피커를 향하도록 놓는다. 좌측 기본 상태에서는 외부기를 정면에서 보았을 때 소리가 왼쪽에서 오른쪽으로 입사하도록 정렬하고, 축 오차는 30° 이내로 한다.")
    add_picture(doc, MEDIA / "image22.png", 14.0, "그림 3. 오디오 분석기·스피커·시료·측정 마이크 구성")
    add_picture(doc, MEDIA / "image18.png", 7.0, "그림 4. 방음 부스 내 배치 예")

    add_heading(doc, "4.4.3", "오디오 분석기 설정", 3)
    add_numbered_step(doc, 1, "GRAS 측정 마이크의 TEDS 정보를 읽고 감도와 교정정보를 적용한다.")
    add_numbered_step(doc, 2, "Generator를 Sine으로 설정하고 시험별 주파수와 출력 레벨을 입력한다.")
    add_numbered_step(doc, 3, "Measurements에서 측정 마이크의 RMS 음압과 주파수를 확인한다.")
    add_numbered_step(doc, 4, "표시 음압이 목표 범위에 들도록 Generator 레벨을 조정한다.")
    add_picture(doc, MEDIA / "image23.png", 6.6, "그림 5. TEDS 감도 설정 예")
    add_picture(doc, MEDIA / "image25.png", 8.0, "그림 6. 정현파 Generator 설정 예")
    add_picture(doc, MEDIA / "image26.png", 7.2, "그림 7. RMS 음압·주파수 확인 예")

    add_heading(doc, "4.4.4", "Sound1 동작 상태", 3)
    add_bullet(doc, "BLE 제어 연결은 유지한다.")
    add_bullet(doc, "Bluetooth Classic 오디오 스트리밍과 통화는 종료한다. I2S가 활성화되면 CFX가 1-DMIC로 전환하므로 마이크 입력 시험 조건이 달라진다.")
    add_bullet(doc, "매핑 Live 상태는 사용하지 않는다. 매핑 Live는 1-DMIC로 고정되므로 정상 2-DMIC 빔포밍 시험과 구분한다.")
    add_bullet(doc, "별도 지시가 없으면 프로그램 1, 정상 독립형 자극, LED 비활성 상태를 사용한다.")

    add_heading(doc, "4.5", "ADC 값의 정의", 2)
    add_bullet(doc, "본 문서의 ADC 값은 개발자용 리모콘 앱이 표시하는 maxAudioInput을 말한다.")
    add_bullet(doc, "CFX는 오디오 믹서 출력 16표본에 5비트 우측 시프트를 적용하고, HEAR에서 절대값 최대를 구해 CM3 공유메모리로 전달한다.")
    add_bullet(doc, "표현 가능한 산술 상한은 262,143이다. 환산식의 120 dB는 수치 정규화 기준점이며 Sound1 제품의 최대 입력 사양이 아니다.")
    add_bullet(doc, "Sound1의 최대 입력 사양은 110 dB SPL이며, 기구 감쇠를 반영한 완제품 명목 ADC는 31,907이다.")
    add_bullet(doc, "ADC 값은 peak-hold가 아닌 16표본 블록의 순간 최대값이다. 각 음향 조건을 1초 이상 유지하면서 연속 수신값의 최대값을 기록한다.")
    add_bullet(doc, "완제품 판정식: ADC = floor(262143 × 10^((dB SPL − 120 − 8.293) / 20))")
    add_note(doc, "판정 원칙", "시험 대상은 기구 조립 완제품이므로 -8.293 dB 기준을 사용한다. PCB 상태 -3.644 dB 열은 부록 1의 원인 분석용 참고자료이며 합격 판정에 사용하지 않는다.", GREEN_BG)


def add_id1(doc):
    add_new_page_heading(doc, "5", "필수 성능 시험", 1)
    add_heading(doc, "5.1", "시험 ID : SYS-EP-TEST-ID-1", 2)
    add_heading(doc, "5.1.1", "시험 설명", 3)
    add_bullet(doc, "Sound1 완제품의 마이크 입력이 무소음·최소·일상·최대 음압 조건에서 규정된 ADC 범위로 전달되는지 확인한다.")
    add_common_initial(doc)
    tests = [
        ("5.1.3.1", "무소음 입력 음압 확인", "스피커 OFF", 32, 4, "-3/+2 dB", 29, 34, "스피커 출력을 끄고 방음 부스의 RMS 음압과 앱의 ADC 최대값을 1초 이상 기록한다."),
        ("5.1.3.2", "최소 입력 음압 확인", "1,048 Hz", 35, 5, "±5 dB", 30, 40, "1,048 Hz 정현파를 재생하고 측정 음압을 정확히 35 dB SPL로 조정한 뒤 ADC 최대값을 기록한다."),
        ("5.1.3.3", "일상 입력 음압 확인", "1,048 Hz", 70, 319, "±5 dB", 65, 75, "1,048 Hz 정현파를 재생하고 측정 음압을 정확히 70 dB SPL로 조정한 뒤 ADC 최대값을 기록한다."),
        ("5.1.3.4", "최대 입력 음압 확인", "1,048 Hz", 110, 31907, "-5/+0 dB", 105, 110, "시료와 측정 마이크를 스피커에 근접 배치한다. 1,048 Hz 정현파를 재생하고 측정 음압을 정확히 110 dB SPL로 조정한 뒤 ADC 최대값을 기록한다."),
    ]
    add_heading(doc, "5.1.3", "시험벡터 및 판정 기준", 3)
    rows = []
    for i, (_, title, freq, target, nominal, tol, lo, hi, _) in enumerate(tests, 1):
        rows.append([str(i), title.replace(" 확인", ""), freq, str(target), fmt(nominal), tol, f"{lo}~{hi}", f"{fmt(adc(lo, -8.293))}~{fmt(adc(hi, -8.293))}"])
    add_table(doc, ["No.", "조건", "주파수", "고정 입력\n[dB SPL]", "명목 ADC", "입력 허용오차", "허용 음압\n[dB SPL]", "ADC 합격 범위"], rows,
              [0.8, 2.2, 2.0, 2.2, 2.0, 2.4, 2.5, 2.9], [C] * 8, size=7.3)
    for num, title, freq, target, nominal, tol, lo, hi, method in tests:
        add_heading(doc, num, title, 4)
        lines = [method]
        if freq != "스피커 OFF":
            lines.append("Sound1의 빔 축과 스피커 축 정렬이 유지되는지 확인한다.")
        add_measure_pass(doc, lines, [f"고정 입력 {target} dB SPL에서 명목 출력은 ADC {fmt(nominal)}이다.", f"입력 허용오차 {tol}, 즉 {lo}~{hi} dB SPL에 대응하는 ADC {fmt(adc(lo, -8.293))}~{fmt(adc(hi, -8.293))}이면 합격이다.", "음압 또는 ADC가 합격 범위를 벗어나면 불합격이다."])
    add_note(doc, "최대 입력 판정", "110 dB SPL은 Sound1 제품의 최대 입력 사양이다. 따라서 최대점의 음압 허용오차는 상한을 초과하지 않는 -5/+0 dB로 적용한다.", ORANGE_BG)


def add_id2(doc):
    add_new_page_heading(doc, "5.2", "시험 ID : SYS-EP-TEST-ID-2", 2)
    add_heading(doc, "5.2.1", "시험 설명", 3)
    add_bullet(doc, "Sound1의 오디오 입력이 저주파, 중간 주파수 및 Nyquist 인접 주파수에서 규정된 크기로 관측되는지 확인한다.")
    add_heading(doc, "5.2.2", "시험 준비 및 초기 조건", 3)
    add_bullet(doc, "5.1.2의 시험 준비와 동일하다. Bluetooth Classic 오디오 스트리밍은 비활성화한다.")
    add_heading(doc, "5.2.3", "시험 진행", 3)
    cases = [
        ("5.2.3.1", "최소 입력 주파수 확인", "100 Hz", "±1 Hz"),
        ("5.2.3.2", "중간 입력 주파수 확인", "4,037 Hz", "±1 Hz"),
        ("5.2.3.3", "최대 입력 주파수 확인", "7,968.75 Hz", "±0.5 Hz"),
    ]
    add_table(doc, ["No.", "고정 주파수", "주파수 허용오차", "고정 음압", "명목 ADC", "음압 허용오차", "ADC 합격 범위"],
              [[str(i), freq, ftol, "70 dB SPL", "319", "±5 dB", "179~567"] for i, (_, _, freq, ftol) in enumerate(cases, 1)],
              [0.8, 2.5, 2.7, 2.5, 2.0, 2.5, 4.0], [C] * 7, size=8.0)
    for num, title, freq, ftol in cases:
        add_heading(doc, num, title, 4)
        add_measure_pass(doc, [
            f"오디오 분석기에서 {freq} 정현파를 재생한다.",
            "측정 마이크의 RMS 음압이 70 dB SPL이 되도록 출력 레벨을 조정한다.",
            "신호를 1초 이상 연속 유지하고 앱이 수신한 ADC 값 중 최대값을 기록한다.",
        ], [f"주파수는 {freq} {ftol}, 음압은 70 dB SPL ±5 dB여야 한다.", "명목 출력은 ADC 319이며, ADC 179~567이면 합격이다."],
        "7,968.75 Hz는 16 kHz 표본화의 Nyquist 직전 FFT bin 255에 해당한다. 7,990 Hz에서 발생할 수 있는 100 ms 폴링 위상 고착을 피하면서 대역 상단 시험 목적을 유지한다." if "7,968" in freq else None)


def add_id3_to_id5(doc):
    add_new_page_heading(doc, "5.3", "시험 ID : SYS-EP-TEST-ID-3", 2)
    add_heading(doc, "5.3.1", "시험 설명", 3)
    add_bullet(doc, "맵 2의 24밴드 자극에서 전체 펄스열과 채널별 반복 주파수를 확인한다.")
    add_heading(doc, "5.3.2", "시험 준비 및 초기 조건", 3)
    add_bullet(doc, "5.1.2의 자극 출력 지그와 오실로스코프 구성을 사용한다.")
    add_heading(doc, "5.3.3", "24밴드 자극 펄스 주파수 확인", 3)
    add_table(doc, ["고정 입력", "명목 기대 출력", "허용오차", "합격 범위"], [
        ["프로그램 2, 연속 자극 펄스열", "전체 반복 24 kHz", "±5%", "22.8~25.2 kHz"],
        ["프로그램 2, 동일 채널 반복", "채널별 반복 1 kHz", "±5%", "950~1,050 Hz"],
    ], [4.7, 4.7, 2.7, 4.9], [L, C, C, C], size=8.5)
    add_measure_pass(doc, [
        "개발자용 리모콘 앱에서 프로그램 2를 선택한다.",
        "오실로스코프에서 연속 자극 펄스의 전체 반복 주파수를 측정한다.",
        "크기가 가장 작은 1번 채널과 최대 출력으로 설정된 16번 채널을 기준으로 채널을 식별하고 같은 채널 파형의 반복 주파수를 측정한다.",
    ], ["전체 자극 펄스 주파수: 24 kHz ±5% (22.8~25.2 kHz)", "채널별 자극 출력 주파수: 1 kHz ±5% (950~1,050 Hz)"])

    add_new_page_heading(doc, "5.4", "시험 ID : SYS-EP-TEST-ID-4", 2)
    add_heading(doc, "5.4.1", "시험 설명", 3)
    add_bullet(doc, "프로그램별로 설정된 최소·조정·최대 자극 펄스 위상 폭을 확인한다.")
    add_heading(doc, "5.4.2", "시험 준비 및 초기 조건", 3)
    add_bullet(doc, "5.1.2의 자극 출력 지그와 오실로스코프 구성을 사용한다.")
    add_heading(doc, "5.4.3", "시험 진행", 3)
    add_table(doc, ["프로그램", "고정 입력", "명목 위상 폭", "허용오차", "합격 범위"], [
        ["2", "최소 위상 폭 설정", "13 us", "±5%", "12.35~13.65 us"],
        ["3", "조정 위상 폭 설정", "14 us", "±5%", "13.30~14.70 us"],
        ["4", "최대 위상 폭 설정", "255 us", "±5%", "242.25~267.75 us"],
    ], [2.3, 4.0, 3.2, 2.8, 4.7], [C, L, C, C, C], size=8.5)
    for num, title, program, width in [
        ("5.4.3.1", "최소 펄스 위상 폭", 2, 13),
        ("5.4.3.2", "조정 펄스 위상 폭", 3, 14),
        ("5.4.3.3", "최대 펄스 위상 폭", 4, 255),
    ]:
        lo, hi = width * 0.95, width * 1.05
        if program == 4:
            doc.add_page_break()
        add_heading(doc, num, title, 4)
        add_measure_pass(doc, [f"프로그램 {program}을 선택한다.", "오실로스코프의 시간 커서를 자극 펄스 한 위상의 시작과 끝에 놓고 위상 폭을 측정한다."], [f"펄스 위상 폭이 {width} us ±5% ({lo:.2f}~{hi:.2f} us)이어야 한다."])

    add_new_page_heading(doc, "5.5", "시험 ID : SYS-EP-TEST-ID-5", 2)
    add_heading(doc, "5.5.1", "시험 설명", 3)
    add_bullet(doc, "프로그램에 설정된 자극 펄스의 선행 위상 방향이 PCM 출력에 동일하게 반영되는지 확인한다.")
    add_heading(doc, "5.5.2", "시험 준비 및 초기 조건", 3)
    add_bullet(doc, "5.1.2의 자극 출력 지그와 오실로스코프 구성을 사용한다.")
    add_heading(doc, "5.5.3", "시험 진행", 3)
    add_table(doc, ["고정 입력", "명목 기대 출력", "허용 불일치", "합격 조건"], [
        ["프로그램 1", "첫 위상 음(-)", "0회", "관측 펄스 전부 음(-) 선행"],
        ["프로그램 3", "첫 위상 양(+)", "0회", "관측 펄스 전부 양(+) 선행"],
    ], [3.3, 4.0, 3.2, 6.5], [C, C, C, L], size=8.5)
    add_heading(doc, "5.5.3.1", "음(-) 선행 위상", 4)
    add_measure_pass(doc, ["프로그램 1을 선택한다.", "오실로스코프로 연속 펄스의 첫 위상 방향을 확인한다."], ["첫 위상이 음(-) 방향이어야 한다.", "관측되는 모든 펄스의 선행 위상이 같은 방향이어야 한다."])
    add_heading(doc, "5.5.3.2", "양(+) 선행 위상", 4)
    add_measure_pass(doc, ["프로그램 3을 선택한다.", "N-of-M에서 실제 자극이 관측되는 채널 하나 이상을 선택해 첫 위상 방향을 확인한다."], ["첫 위상이 양(+) 방향이어야 한다.", "관측되는 모든 자극 펄스의 선행 위상이 같은 방향이어야 한다."], "현행 초기 맵에서 프로그램 3은 N-of-M 전략과 양(+) 선행 위상을 함께 사용한다. 자극 전략은 선택 채널을 결정하고, 선행 위상 필드는 각 PCM 펄스의 위상 방향을 결정하므로 두 판정은 독립이다.")


def add_id6(doc):
    add_new_page_heading(doc, "5.6", "시험 ID : SYS-EP-TEST-ID-6", 2)
    add_heading(doc, "5.6.1", "시험 설명", 3)
    add_bullet(doc, "동일한 T/C 레벨에서 입력 음압과 사용자 볼륨에 따라 자극 전류가 규정된 범위로 변하는지 확인한다.")
    add_heading(doc, "5.6.2", "시험 준비 및 초기 조건", 3)
    add_bullet(doc, "5.1.2의 시험 구성을 사용한다.")
    add_bullet(doc, "프로그램 1, T 레벨 500 uA, C 레벨 1,500 uA를 사용한다.")
    add_bullet(doc, "3,815 Hz 정현파를 사용하고 26번 채널의 부하저항 양단 전압을 측정한다.")
    add_bullet(doc, "자극 전류는 측정 전압 / 실측 부하저항으로 계산한다.")
    add_heading(doc, "5.6.3", "시험 진행 및 통과 기준", 3)
    rows = [
        ["1", "최소 A", "1", "35 / ±1", "5 / 5~6", "537", "-7/+7", "530~544"],
        ["2", "최소 B", "10", "35 / ±1", "5 / 5~6", "596", "±19", "577~615"],
        ["3", "일상 A", "1", "70 / ±1", "319 / 284~358", "936", "-15/+14", "921~950"],
        ["4", "일상 B", "10", "70 / ±1", "319 / 284~358", "1,500", "-8/+0", "1,492~1,500"],
        ["5", "최대 A", "1", "110 / -1/+0", "31,907 / 28,437~31,907", "1,500", "-8/+0", "1,492~1,500"],
        ["6", "최대 B", "10", "110 / -1/+0", "31,907 / 28,437~31,907", "1,500", "-8/+0", "1,492~1,500"],
    ]
    add_table(doc, ["No.", "조건", "볼륨", "고정 음압 /\n허용오차 [dB]", "명목 ADC /\n합격 범위", "명목 전류\n[uA]", "전류 허용오차\n[uA]", "전류 합격 범위\n[uA]"], rows,
              [0.7, 1.5, 1.1, 2.8, 3.6, 2.0, 2.4, 2.9], [C] * 8, size=6.9)
    add_label(doc, "측정 방법")
    for i, text in enumerate([
        "표의 조건 순서대로 볼륨과 목표 음압을 설정한다.",
        "각 조건에서 ADC가 표의 범위에 들어온 상태를 1초 이상 유지한다.",
        "26번 채널의 양·음 위상 중심 전압 크기를 측정하고 전류로 환산한다.",
        "표의 허용 자극 전류 범위와 비교한다.",
    ], 1):
        add_numbered_step(doc, i, text)
    add_label(doc, "통과 기준")
    add_bullet(doc, "6개 조건 모두에서 명목 입력과 명목 출력의 대응을 확인하고, 실측 음압·ADC·자극 전류가 표의 합격 범위 안에 있어야 한다.")
    add_bullet(doc, "입력 음압이 증가할 때 자극 전류가 감소하거나 C 레벨을 초과해서는 안 된다.")
    add_note(doc, "ADC 변경 범위", "1.5세대와 자극 맵·T/C·볼륨·비선형 매핑 조건은 같고, Sound1에서는 기구 감쇠로 입력 ADC만 달라진다. 따라서 입력 ADC 범위는 Sound1 완제품 표로 교체하고, 자극 전류 판정 범위는 1.5세대 기준을 유지한다.")


def add_id7(doc):
    add_new_page_heading(doc, "5.7", "시험 ID : SYS-EP-TEST-ID-7", 2)
    add_heading(doc, "5.7.1", "시험 설명", 3)
    add_bullet(doc, "512점 FFT의 32-band 배정에 따라 입력 주파수의 주 에너지가 대응 자극 채널로 이동하는지 확인한다.")
    add_heading(doc, "5.7.2", "시험 준비 및 초기 조건", 3)
    add_bullet(doc, "5.1.2의 시험 구성을 사용한다.")
    add_bullet(doc, "프로그램 1(32밴드 CIS), 볼륨 1, T 레벨 500 uA, C 레벨 1,500 uA를 사용한다.")
    add_bullet(doc, "각 조건에서 앱의 ADC와 오실로스코프의 채널별 자극 전류를 함께 기록한다.")
    add_heading(doc, "5.7.3", "시험 진행 및 통과 기준", 3)
    rows = [
        ["1", "93.75", "70 / ±1", "319 / 284~358", "ch1: 872 (-17/+16) / 855~888", "ch2~32: 500 ±50 / 450~550"],
        ["2", "125.00", "70 / ±1", "319 / 284~358", "ch1: 934 (-17/+16) / 917~950", "ch2: 872 (-17/+16) / 855~888\nch3~32: 500 ±50 / 450~550"],
        ["3", "156.25", "70 / ±1", "319 / 284~358", "ch2: 934 (-17/+16) / 917~950", "ch1,3: 872 (-17/+16) / 855~888\nch4~32: 500 ±50 / 450~550"],
        ["4", "2,843.75", "70 / ±1", "319 / 284~358", "ch23: 934 (-17/+16) / 917~950", "ch24: 872 (-17/+16) / 855~888\n그 외: 500 ±50 / 450~550"],
        ["5", "3,031.25", "70 / ±1", "319 / 284~358", "ch24: 934 (-17/+16) / 917~950", "그 외: 500 ±50 / 450~550"],
        ["6", "3,250.00", "75 / ±1", "567 / 505~636", "ch25: 934 (-17/+16) / 917~950", "그 외: 500 ±50 / 450~550"],
    ]
    add_table(doc, ["No.", "고정 주파수\n[Hz] (±0.5)", "고정 음압 /\n허용오차 [dB]", "명목 ADC /\n합격 범위", "주 기대 전류 [uA]\n명목(오차) / 범위", "인접·비대상 기대 전류 [uA]\n명목(오차) / 범위"], rows,
              [0.7, 2.1, 2.2, 2.6, 4.4, 5.0], [C, C, C, C, L, L], size=6.8)
    add_label(doc, "측정 방법")
    for i, text in enumerate([
        "표의 고정 주파수와 고정 음압을 순서대로 재생하고 주파수·음압·ADC를 기록한다.",
        "저역 시험은 채널 1~3, 고역 시험은 채널 23~25의 자극 전류를 측정한다.",
        "각 조건에서 표에 명시된 주 기대 채널, 인접 채널과 비대상 채널의 자극 전류를 측정한다.",
        "실측 전류를 각 셀의 명목값과 오차 및 합격 범위에 대조한다.",
    ], 1):
        add_numbered_step(doc, i, text)
    add_label(doc, "통과 기준")
    add_bullet(doc, "6개 시험의 실측 음압·ADC와 채널별 자극 전류가 모두 표의 합격 범위 안에 있어야 한다.")
    add_bullet(doc, "주 기대 채널이 표와 다르거나 비대상 채널이 450~550 uA를 벗어나면 불합격이다.")
    add_note(doc, "판정 근거", "표의 934 uA는 FFT 중심 bin의 명목 응답, 872 uA는 Hann 창에 의해 인접 bin으로 분배되는 명목 응답, 500 uA는 T 레벨 명목값이다. 비대칭 합격 범위는 1.5세대 실기 판정 범위를 중심값 기준으로 정리해 적용했다.")


def add_id8(doc):
    add_new_page_heading(doc, "5.8", "시험 ID : SYS-EP-TEST-ID-8", 2)
    add_heading(doc, "5.8.1", "시험 설명", 3)
    add_bullet(doc, "프로그램 2에 정의된 채널 순서대로 자극이 출력되는지 펄스 포락선으로 확인한다.")
    add_heading(doc, "5.8.2", "시험 준비 및 초기 조건", 3)
    add_bullet(doc, "5.1.2의 자극 출력 지그와 오실로스코프 구성을 사용한다.")
    add_bullet(doc, "프로그램 2는 채널별 T 레벨과 C 레벨이 같으므로 음향 입력과 무관하게 아래 고정 전류열을 출력해야 한다.")
    add_heading(doc, "5.8.3", "채널별 자극 출력 순서", 3)
    map2 = [500, 543, 586, 630, 673, 717, 760, 804, 847, 891, 934, 978, 1021, 1065, 1108, 1500, 1195, 1239, 1282, 1326, 1369, 1413, 1456, 1500]
    def tol_range(value):
        return f"{math.ceil(value * 0.9):,}~{math.floor(value * 1.1):,}"
    rows = []
    for i in range(12):
        rows.append([str(i + 1), fmt(map2[i]), tol_range(map2[i]), str(i + 13), fmt(map2[i + 12]), tol_range(map2[i + 12])])
    add_table(doc, ["채널", "명목 전류 [uA]", "±10% 합격 범위", "채널", "명목 전류 [uA]", "±10% 합격 범위"], rows,
              [1.6, 3.0, 3.9, 1.6, 3.0, 3.9], [C] * 6, size=8.0)
    add_measure_pass(doc, ["음향 신호를 인가하지 않고 프로그램 2를 선택한다.", "오실로스코프의 시간축에 채널 1~24의 연속 펄스가 한 주기 이상 보이도록 설정한다.", "각 펄스의 채널 순서와 전류를 기록한다."], ["출력 순서는 정확히 채널 1→24이고 매 주기 반복되어야 한다.", "각 채널의 명목 전류와 실측값의 차이가 ±10% 이내이며 표의 합격 범위 안에 있어야 한다.", "누락·중복·역순 펄스가 하나라도 있으면 불합격이다."])
    add_picture(doc, MEDIA / "image27.png", 5.0, "그림 8. 자극 출력 펄스 파형 관측 예")


def add_id9(doc):
    add_new_page_heading(doc, "5.9", "시험 ID : SYS-EP-TEST-ID-9", 2)
    add_heading(doc, "5.9.1", "시험 설명", 3)
    add_bullet(doc, "N-of-M 전략이 32개 주파수 대역 중 크기가 큰 상위 16개만 선택해 자극으로 출력하는지 확인한다.")
    add_bullet(doc, "판정 대상은 ① 대역-전극 대응 ② 자극 채널 수 상한 16 ③ 크기 우선 선택 ④ 17개 입력의 선택 경계다.")
    add_picture(doc, FIG / "그림_NofM_관측경로.png", 16.5, "그림 9. N-of-M 시험의 입력·선택·관측 경로")
    add_heading(doc, "5.9.2", "시험 준비 및 초기 조건", 3)
    add_bullet(doc, "프로그램 3(N-of-M), 주파수 밴드 32, 전 채널 T 레벨 500 uA·C 레벨 1,500 uA를 사용한다.")
    add_bullet(doc, "T 레벨 미만 자극 묵음을 활성화한다. 선택 대역 중 T 미만 대역은 더미 패킷으로 대체되어 실제 펄스가 나타나지 않는다.")
    add_bullet(doc, "32채널 로직 애널라이저를 전극 1~32에 순서대로 연결하고 2 ms 이상을 동시에 캡처한다.")
    add_bullet(doc, "멀티톤은 부록 2의 시험 톤을 사용한다. N개 성분의 위상은 Schroeder 위상 φk = −180° × k × (k−1) / N으로 고정해 반복성과 낮은 파고율을 확보한다.")
    add_bullet(doc, "각 성분의 상대 크기는 dB로 설정한다. −20 dB 성분의 선형 진폭은 기준 성분의 0.1배다.")
    add_heading(doc, "5.9.3", "시험 벡터 요약", 3)
    rows = [
        ["1", "단일톤 32회", "band k 시험 톤 1개", "70.00 / ±1.00", "319 / 179~567", "전극 k+1 정확히 1개"],
        ["2", "32톤 동률", "band 0~31, 각 69.95", "85.00 / ±1.00", "1,794 / 1,008~3,190", "서로 다른 전극 정확히 16개"],
        ["3", "저역 우세", "band 0~15: 72.92\nband 16~31: 52.92", "85.00 / ±1.00", "1,794 / 1,008~3,190", "전극 1~16"],
        ["4", "고역 우세", "band 0~15: 52.92\nband 16~31: 72.92", "85.00 / ±1.00", "1,794 / 1,008~3,190", "전극 17~32"],
        ["5A", "17톤 경계", "band 8~24: 72.96\nband 16만 52.96", "85.00 / ±1.00", "1,794 / 1,008~3,190", "전극 9~25 중 17 제외"],
        ["5B", "17톤 경계", "band 8~24: 72.96\nband 20만 52.96", "85.00 / ±1.00", "1,794 / 1,008~3,190", "전극 9~25 중 21 제외"],
    ]
    add_table(doc, ["No.", "시험", "고정 성분 입력\n[dB SPL]", "총 음압 / 허용오차\n[dB SPL]", "명목 ADC / 합격 범위", "명목 기대 출력"], rows,
              [0.8, 2.1, 4.2, 3.1, 3.3, 3.5], [C, C, L, C, C, C], size=6.8)
    add_bullet(doc, "모든 시험 톤의 주파수 허용오차는 ±0.5 Hz, 멀티톤의 각 성분 레벨 허용오차는 표의 고정값 대비 ±0.5 dB다.")
    add_bullet(doc, "ADC 합격 범위는 완제품 편차 판정을 위해 총 음압 목표의 ±5 dB 대응값을 사용한다. 음향 발생기의 총 음압 자체는 표의 ±1 dB로 맞춘다.")

    tests = [
        ("5.9.4.1", "대역별 자극 채널 확인", ["부록 2의 band k 시험 톤을 하나씩 70.00 dB SPL로 재생한다(k=0~31, 32회).", "주파수 ±0.5 Hz, 음압 ±1 dB로 맞추고 ADC 명목값 319와 합격 범위 179~567을 확인한 뒤 2 ms 이상 캡처한다."], ["매회 실제 자극 펄스가 관측되는 전극이 정확히 1개여야 한다.", "전극 번호는 band k에 대응하는 k+1과 정확히 일치해야 한다. 32회 중 불일치 허용 횟수는 0회다."]),
        ("5.9.4.2", "자극 채널 수 상한", ["부록 2의 32개 시험 톤을 각 69.95 dB SPL로 합성한다.", "총 음압을 85.00 dB SPL ±1 dB로 맞추고 ADC 명목값 1,794와 합격 범위 1,008~3,190을 확인한 뒤 2 ms 이상 캡처한다."], ["자극이 관측되는 서로 다른 전극 수가 정확히 16개여야 한다.", "15개 이하 또는 17개 이상이면 불합격이다."]),
        ("5.9.4.3", "대역 선택 정확성 - 저역 우세", ["band 0~15는 각 72.92 dB SPL, band 16~31은 각 52.92 dB SPL로 합성한다.", "총 음압 85.00 dB SPL ±1 dB, 성분별 ±0.5 dB로 맞춘 뒤 2 ms 이상 캡처한다."], ["자극 전극 집합이 1~16과 정확히 일치해야 한다.", "집합 불일치 허용 전극 수는 0개다."]),
        ("5.9.4.4", "대역 선택 정확성 - 고역 우세", ["band 0~15는 각 52.92 dB SPL, band 16~31은 각 72.92 dB SPL로 합성한다.", "총 음압 85.00 dB SPL ±1 dB, 성분별 ±0.5 dB로 맞춘 뒤 2 ms 이상 캡처한다."], ["자극 전극 집합이 17~32와 정확히 일치해야 한다.", "집합 불일치 허용 전극 수는 0개다."]),
        ("5.9.4.5", "선택 경계", ["band 8~24의 17개 톤을 각 72.96 dB SPL로 합성하고 band 16만 52.96 dB SPL로 설정한다.", "총 음압 85.00 dB SPL ±1 dB로 맞춰 2 ms 이상 캡처한 뒤, band 16을 72.96 dB SPL로 복원하고 band 20만 52.96 dB SPL로 바꾸어 반복한다."], ["첫 시험은 전극 9~25 중 전극 17만 제외한 16개가 자극되어야 한다.", "두 번째 시험은 전극 9~25 중 전극 21만 제외한 16개가 자극되어야 한다.", "제외 전극 불일치 허용 수는 각 시험 0개다."]),
    ]
    add_heading(doc, "5.9.4", "시험 진행", 3)
    for num, title, methods, criteria in tests:
        add_heading(doc, num, title, 4)
        add_measure_pass(doc, methods, criteria)
    add_note(doc, "결정적 판정", "저역 우세만으로는 동률 우선순위 때문에 주파수 분석 고장을 구분하지 못할 수 있다. 고역 우세 시험에서 전극 17~32가 선택되어야 크기 기반 선택이 실제로 동작함을 확인할 수 있으므로 두 시험을 반드시 함께 수행한다.", ORANGE_BG)


def add_appendix1(doc):
    add_new_page_heading(doc, "부록 1", "Sound1 입력 음압 대비 ADC 출력 테이블", 1)
    add_bullet(doc, "1.5세대와 Sound1은 동일 규격 마이크를 사용하며, Sound1의 PCB·기구 감쇠를 dB 오프셋으로 적용했다.")
    add_bullet(doc, "Sound1 완제품 열만 합격 판정에 사용한다. PCB 열은 기구 감쇠 원인 분석용 참고값이다.")
    add_bullet(doc, "값은 floor(262143 × 10^((dB SPL − 120 + 감쇠) / 20))으로 산출한다. 감쇠는 PCB -3.644 dB, 완제품 -8.293 dB다.")
    add_bullet(doc, "수식의 120 dB는 ADC 환산 정규화 기준점이다. Sound1 제품의 최대 입력 사양과 본 표의 판정 상한은 110 dB SPL이다.")
    headers = ["dB SPL", "1.5세대", "Sound1 PCB\n(참고)", "Sound1 완제품\n(판정)", "dB SPL", "1.5세대", "Sound1 PCB\n(참고)", "Sound1 완제품\n(판정)"]
    rows = []
    left = list(range(29, 70))
    right = list(range(70, 111))
    for d1, d2 in zip(left, right):
        rows.append([
            str(d1), fmt(adc(d1, 0)), fmt(adc(d1, -3.644)), fmt(adc(d1, -8.293)),
            str(d2), fmt(adc(d2, 0)), fmt(adc(d2, -3.644)), fmt(adc(d2, -8.293)),
        ])
    table = add_table(doc, headers, rows, [1.35, 1.7, 2.0, 2.2, 1.35, 1.7, 2.0, 2.2], [C] * 8, size=7.2)
    for c in (3, 7):
        shade(table.rows[0].cells[c], GREEN_BG)


def add_appendix2(doc):
    add_new_page_heading(doc, "부록 2", "32-band 주파수 배정 및 시험 톤", 1)
    add_bullet(doc, "표본화 16 kHz, FFT 512점이므로 bin 간격은 31.25 Hz다.")
    add_bullet(doc, "bin 0~3과 253~255는 -1로 배정되어 분석에서 제외되고, bin 4~252가 band 0~31에 배정된다.")
    ranges = [
        (4,4),(5,5),(6,6),(7,7),(8,8),(9,10),(11,12),(13,14),(15,16),(17,18),(19,21),(22,24),(25,27),(28,31),(32,35),(36,40),
        (41,45),(46,51),(52,57),(58,64),(65,72),(73,81),(82,91),(92,102),(103,115),(116,129),(130,144),(145,161),(162,180),(181,202),(203,226),(227,252)
    ]
    tones = [125,156.25,187.5,218.75,250,312.5,375,437.5,500,562.5,625,718.75,812.5,937.5,1062.5,1187.5,1343.75,1531.25,1718.75,1906.25,2156.25,2406.25,2718.75,3031.25,3406.25,3843.75,4281.25,4781.25,5343.75,6000,6718.75,7500]
    rows = []
    for band, ((lo, hi), tone) in enumerate(zip(ranges, tones)):
        rows.append([str(band), str(band + 1), f"{lo}~{hi}" if lo != hi else str(lo), f"{lo*31.25:,.2f}~{hi*31.25:,.2f}" if lo != hi else f"{lo*31.25:,.2f}", f"{tone:,.2f}"])
    add_table(doc, ["band", "전극", "FFT bin", "대역 [Hz]", "시험 톤 [Hz]"], rows, [1.5, 1.5, 2.5, 6.0, 5.5], [C, C, C, C, C], size=8.2)


def add_appendix3_4(doc):
    add_new_page_heading(doc, "부록 3", "N-of-M 자극 방식 시험 근거", 1)
    add_table(doc, ["항목", "Sound1 구현 및 시험 의미"], [
        ["입력", "512점 FFT 결과를 32개 대역으로 묶고 각 대역의 최대 magnitude를 사용한다."],
        ["점수", "진폭과 대역 인덱스를 하나의 점수로 구성한다. 같은 크기에서는 낮은 인덱스, 즉 저주파 대역이 우선된다."],
        ["선택", "최대 32개 대역 중 상위 16개를 선택한다."],
        ["묵음", "선택됐더라도 진폭이 T 레벨 미만이면 실제 자극 대신 더미 패킷을 출력한다."],
        ["출력", "채널당 3 PCM 프레임을 사용하고 1 ms에 최대 8채널을 출력하므로 선택 16채널의 한 주기는 2 ms다."],
        ["관측", "2 ms 이상 구간에서 전극별 실제 펄스 유무를 동시에 확인한다."],
        ["비대상", "N-of-M의 인터리빙 출력 순서는 필수성능 판정 대상이 아니며, 상위 16개 선택 집합을 판정한다."],
    ], [4.0, 13.0], [C, L], size=9)

    add_heading(doc, "부록 4", "1.5세대 대비 Sound1 변경사항", 1)
    rows = [
        ["1", "마이크", "단일 마이크", "2-DMIC + 착용 방향 DAS 빔포밍", "ID-1·2·6·7·9 공통 조건"],
        ["2", "Bluetooth", "BLE 제어", "QCC 기반 BLE 제어 + Classic/I2S 오디오", "Classic 오디오 비활성 조건 추가"],
        ["3", "ADC 판정", "1.5세대 테이블", "기구 감쇠 -8.293 dB 완제품 표", "ADC 범위 전면 갱신"],
        ["4", "최대 입력", "120 dB SPL / ADC 262,143", "110 dB SPL / ADC 31,907", "Sound1 제품 최대 입력으로 변경"],
        ["5", "ID-2 상단", "7,990 Hz", "7,968.75 Hz", "Nyquist 인접성과 폴링 위상 오차 완화"],
        ["6", "ID-7 저역", "100 / 114 / 128 Hz", "93.75 / 125 / 156.25 Hz", "31.25 Hz FFT bin 격자 반영"],
        ["7", "ID-7 고역", "2,869 / 3,038 / 3,217 Hz", "2,843.75 / 3,031.25 / 3,250 Hz", "band 22~24 배정 반영"],
        ["8", "맵 3", "양(+) 선행 위상 시험", "양(+) 선행 위상 + N-of-M", "ID-5와 ID-9에서 각 필드 독립 판정"],
        ["9", "N-of-M", "없음", "상위 16개 대역 선택", "ID-9 신설"],
        ["10", "시험 자립성", "외부 Excel/OLE 링크 존재", "고정 입력·명목 출력·오차·범위 내장", "오프라인 단독 수행 가능"],
    ]
    add_table(doc, ["No.", "구분", "1.5세대", "Sound1", "문서 반영"], rows, [0.9, 2.4, 4.0, 4.4, 5.3], [C, C, L, L, L], size=8)


def finalize(doc):
    props = doc.core_properties
    props.title = "Sound1 필수 성능 시험 기준서"
    props.subject = "Sound1 외부기의 필수 성능 시험 절차 및 통과 기준"
    props.author = "김은수"
    props.keywords = "Sound1, EPTC, 필수성능, ADC, N-of-M"
    props.comments = ""
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    tmp = OUT.with_suffix(".tmp.docx")
    doc.save(tmp)
    if OUT.exists():
        OUT.unlink()
    tmp.replace(OUT)
    print(OUT)


def main():
    doc = setup_document()
    add_cover(doc)
    add_revision_and_toc(doc)
    add_intro(doc)
    add_equipment(doc)
    add_setup(doc)
    add_id1(doc)
    add_id2(doc)
    add_id3_to_id5(doc)
    add_id6(doc)
    add_id7(doc)
    add_id8(doc)
    add_id9(doc)
    add_appendix1(doc)
    add_appendix2(doc)
    add_appendix3_4(doc)
    finalize(doc)


if __name__ == "__main__":
    main()
