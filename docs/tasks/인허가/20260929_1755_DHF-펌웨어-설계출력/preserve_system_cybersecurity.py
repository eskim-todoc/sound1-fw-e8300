"""Artifact 작성 셀을 원본 패키지에 결합하고 보고서 공통 열로 기준서를 만든다.

현재 표지, 기존 사진/예상 그림, 사용자 서식은 재작성하지 않는다.
기준서에는 보고서의 A:J만 복사한다. 참조 원본과 다른 시험 문서는 수정하지 않는다.
"""
import copy
import json
import posixpath
import zipfile
from pathlib import Path
from lxml import etree as E
import preserve_aligned_report_packages as native

TASK = Path(__file__).resolve().parent
QA = TASK / 'qa/system-cybersecurity-rev0'
N = native.N
TAG = '{' + native.NS + '}'


def save(package, path):
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, raw in package.items():
            archive.writestr(name, raw)


def inline(cell, value):
    for node in list(cell):
        cell.remove(node)
    cell.set('t', 'inlineStr')
    ins = E.SubElement(cell, TAG + 'is')
    # 추가 자료 안내만 빨강/파랑으로 표시한다. 기존 결과의 기본 글꼴은 보존한다.
    for line in value.splitlines(keepends=True):
        run = E.SubElement(ins, TAG + 'r')
        if '(사진필요)' in line or '(QCC 검증 결과 필요)' in line:
            pr = E.SubElement(run, TAG + 'rPr')
            E.SubElement(pr, TAG+'rFont', val='맑은 고딕')
            E.SubElement(pr, TAG + 'b')
            E.SubElement(pr, TAG+'sz', val='10')
            E.SubElement(pr, TAG + 'color', rgb='FFC00000' if 'QCC 검증 결과 필요' in line else 'FF0000FF')
        text = E.SubElement(run, TAG + 't')
        text.set('{http://www.w3.org/XML/1998/namespace}space', 'preserve')
        text.text = line


def remove_sheets(package, keep):
    wb = native.parse(package['xl/workbook.xml'])
    rels = native.parse(package['xl/_rels/workbook.xml.rels'])
    sheets = wb.find('s:sheets', N)
    old_index = {}
    for i, sheet in enumerate(list(sheets)):
        if sheet.get('name') not in keep:
            rid = sheet.get('{' + native.RNS + '}id')
            sheets.remove(sheet)
            for rel in list(rels):
                if rel.get('Id') == rid:
                    rels.remove(rel)
        else:
            old_index[str(i)] = str(len(old_index))
    names = wb.find('s:definedNames', N)
    if names is not None:
        for name in list(names):
            old = name.get('localSheetId')
            if old is not None:
                if old not in old_index:
                    names.remove(name)
                else:
                    name.set('localSheetId', old_index[old])
    package['xl/workbook.xml'] = native.serialize(wb)
    package['xl/_rels/workbook.xml.rels'] = native.serialize(rels)


def add_sheet(target, source, name, prefix, limit=None):
    importer = native.Importer(target, source, prefix)
    path = importer.copy_part(native.sheet_path(source, name))
    if name=='사이버보안 시험 연계' and limit is None:
        root=native.parse(target[path])
        for cell in root.findall('.//s:c',N):
            if native.col(cell.get('r'))==11:
                ins=cell.find('s:is',N)
                val=cell.find('s:v',N)
                value=''.join(ins.itertext()) if ins is not None else (val.text or '' if val is not None else '')
                if '(QCC 검증 결과 필요)' in value:
                    inline(cell,value)
        target[path]=native.serialize(root)
    if limit:
        root = native.parse(target[path])
        trim(root, limit)
        target[path] = native.serialize(root)
    # 보조 시트도 원 문서와 같은 용지/여백을 사용한다. 본문 인쇄 설정은 그대로 둔다.
    root=native.parse(target[path])
    main_name='보고서_시스템 시험' if '보고서_시스템 시험' in [x.get('name') for x in native.parse(target['xl/workbook.xml']).find('s:sheets',N)] else '기준서_시스템 시험'
    main=native.parse(target[native.sheet_path(target,main_name)])
    for tag in ['pageMargins','pageSetup']:
        old=root.find('s:'+tag,N)
        if old is not None:root.remove(old)
        settings=main.find('s:'+tag,N)
        if settings is not None:
            cloned=copy.deepcopy(settings)
            cloned.attrib.pop('{'+native.RNS+'}id',None)
            root.append(cloned)
    target[path]=native.serialize(root)
    wb = native.parse(target['xl/workbook.xml'])
    rels = native.parse(target['xl/_rels/workbook.xml.rels'])
    sheets = wb.find('s:sheets', N)
    rid = 'rIdCyber' + str(len(sheets))
    E.SubElement(rels, '{' + native.PNS + '}Relationship',
                 Id=rid, Type=native.RNS + '/worksheet', Target=posixpath.relpath(path, 'xl'))
    E.SubElement(sheets, TAG + 'sheet', name=name,
                 sheetId=str(max(int(s.get('sheetId')) for s in sheets) + 1),
                 attrib={'{' + native.RNS + '}id': rid})
    names = wb.find('s:definedNames', N)
    if names is None:
        names = E.SubElement(wb, TAG + 'definedNames')
    end = 39
    last = 'J' if limit else 'L'
    E.SubElement(names, TAG + 'definedName', name='_xlnm.Print_Area', localSheetId=str(len(sheets)-1)).text = f"'{name}'!$A$1:${last}${end}"
    E.SubElement(names, TAG + 'definedName', name='_xlnm.Print_Titles', localSheetId=str(len(sheets)-1)).text = f"'{name}'!$4:$4"
    target['xl/workbook.xml'] = native.serialize(wb)
    target['xl/_rels/workbook.xml.rels'] = native.serialize(rels)
    importer.flush()


def trim(root, limit):
    for row in root.findall('s:sheetData/s:row', N):
        for cell in list(row):
            if native.col(cell.get('r')) > limit:
                row.remove(cell)
        row.attrib.pop('spans', None)
    cols = root.find('s:cols', N)
    if cols is not None:
        for c in list(cols):
            if int(c.get('min')) > limit:
                cols.remove(c)
            elif int(c.get('max')) > limit:
                c.set('max', str(limit))
    merges = root.find('s:mergeCells', N)
    if merges is not None:
        for m in list(merges):
            a, b = m.get('ref').split(':') if ':' in m.get('ref') else (m.get('ref'), m.get('ref'))
            if native.col(a) > limit:
                merges.remove(m)
            elif native.col(b) > limit:
                assert native.col(a) == 1, ('cross boundary merge', m.get('ref'))
                m.set('ref', a + ':' + chr(64+limit) + ''.join(ch for ch in b if ch.isdigit()))
        merges.set('count', str(len(merges)))
    dimension = root.find('s:dimension', N)
    if dimension is not None:
        end = dimension.get('ref').split(':')[-1]
        dimension.set('ref', 'A1:' + chr(64+limit) + ''.join(ch for ch in end if ch.isdigit()))


def build():
    plan = json.loads((QA / 'author-plan.json').read_text('utf8'))
    target = native.parts(QA / 'system-before.xlsx')
    authored = native.parts(QA / 'system-authored.xlsx')
    main_path = native.sheet_path(target, '보고서_시스템 시험')
    root = native.parse(target[main_path])
    added = native.parse(authored[native.sheet_path(authored, '보고서_시스템 시험')])
    original_cells = {c.get('r'): c for c in root.findall('.//s:c', N)}
    authored_cells = {c.get('r'): c for c in added.findall('.//s:c', N)}
    # 작성 도구가 바꾼 전체 서식 대신 편집 셀의 문자열만 가져온다.
    sst = native.parse(authored['xl/sharedStrings.xml']) if 'xl/sharedStrings.xml' in authored else None
    for change in plan['changes']:
        cell = authored_cells[change['cell']]
        if cell.get('t') == 's':
            value = ''.join(sst[int(cell.find('s:v', N).text)].itertext())
        elif cell.get('t') == 'inlineStr':
            value = ''.join(cell.find('s:is', N).itertext())
        else:
            value = cell.find('s:v', N).text
        assert value == change['after'], change['cell']
        inline(original_cells[change['cell']], value)
    for row in root.findall('s:sheetData/s:row', N):
        heights = [x['height'] for x in plan['changes'] if x['row'] == int(row.get('r')) and x.get('height')]
        if heights:
            row.set('ht', str(max(heights))); row.set('customHeight', '1')
    target[main_path] = native.serialize(root)
    for spec in plan['sheets']:
        add_sheet(target, authored, spec['name'], 'cyber_report')
    save(target, QA / 'system-candidate.xlsx')

    criterion = native.parts(QA / 'oldCriterion-before.xlsx')
    remove_sheets(criterion, ['표지', '기준서_시스템 시험'])
    cp = native.sheet_path(criterion, '기준서_시스템 시험')
    importer = native.Importer(criterion, target, 'cyber_criterion')
    common = importer.convert_sheet(native.parse(target[main_path]))
    trim(common, 10)
    inline(common.find('.//s:c[@r="B2"]', N), plan['criterionTitle'])
    criterion[cp] = native.serialize(common)
    # Main sheet has only native printer settings relationships, not photos.
    rp = native.relpath(main_path)
    if rp in target:
        rels = native.parse(target[rp])
        for rel in rels:
            assert rel.get('Type').endswith('/printerSettings')
            old = native.resolve(main_path, rel.get('Target'))
            mapped = importer.copy_part(old)
            rel.set('Target', posixpath.relpath(mapped, posixpath.dirname(cp)))
        criterion[native.relpath(cp)] = native.serialize(rels)
    importer.flush()
    for spec in plan['sheets']:
        add_sheet(criterion, target, spec['name'], 'cyber_criterion_trace', limit=10)
    wb = native.parse(criterion['xl/workbook.xml'])
    names = wb.find('s:definedNames', N)
    if names is not None:
        for name in names:
            if name.get('localSheetId') == '1' and name.get('name') == '_xlnm.Print_Area':
                name.text = "'기준서_시스템 시험'!$B$2:$J$303"
    criterion['xl/workbook.xml'] = native.serialize(wb)
    save(criterion, QA / 'criterion-candidate.xlsx')
    print('Native report preserved; criteria generated from common columns.')


if __name__ == '__main__':
    build()
