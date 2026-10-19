"""Read-only verification of direct expected/recorded comparisons and image anchors."""
from pathlib import Path
import hashlib
import importlib.util
import json
import sys
import zipfile
from copy import copy
from lxml import etree as E
import openpyxl

TASK=Path(__file__).parent
ROOT=TASK.parents[3]
QA=TASK/'qa/suvr'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    path=Path(sys.argv[1]) if len(sys.argv)>1 else QA/'direct-candidate.xlsx'
    p=read(QA/'direct-plan.json');original=read(QA/'plan.json')
    w=openpyxl.load_workbook(path);before=openpyxl.load_workbook(QA/'before-direct-comparison.xlsx')
    s=w['보고서_유닛 시험'];errors=[]
    for row,meta in zip(p['rows'],p['meta']):
        for c,v in enumerate(row,2):
            if (s.cell(meta['row'],c).value or '')!=(v or ''):errors.append(f'value {meta["row"]}/{c}')
    for u,t in zip(p['units'],original['tests']):
        assert u['id']==t['id']
        for c in range(4,11):
            if s.cell(u['start'],c).value!=t['original'][c-1]:errors.append(f'original field {u["id"]}/{c}')
        if s.cell(u['start'],13).value!=t['result']['judgment']:errors.append(f'judgment {u["id"]}')
        if t['result']['qcc']:
            cells=[s.cell(r,12) for r in range(u['start'],u['end']+1) if '(QCC 검증 결과 필요)' in str(s.cell(r,12).value)]
            if not cells:errors.append(f'QCC missing {u["id"]}')
            for c in cells:
                if c.font.color.type!='rgb' or c.font.color.rgb[-6:]!='C00000':errors.append(f'QCC color {u["id"]}')
    for name in ['표지','입력 및 예상값','RTT 결과 대조','관찰값 확인']:
        a,b=before[name],w[name]
        if a.max_row!=b.max_row or a.max_column!=b.max_column:errors.append(f'dimensions {name}')
        for row in a:
            for c in row:
                v=b[c.coordinate]
                if c.value!=v.value:errors.append(f'preserved value {name}/{c.coordinate}')
                if c.has_style and (copy(c.font)!=copy(v.font) or copy(c.fill)!=copy(v.fill) or copy(c.alignment)!=copy(v.alignment) or c.number_format!=v.number_format):
                    errors.append(f'preserved style {name}/{c.coordinate}')
    if w['표지']._images:errors.append('old signature image remains')
    if len(s._images)!=len(p['images']):errors.append('report image count')
    image_hashes={hashlib.sha256(i._data()).hexdigest() for i in s._images}
    for im in p['images']:
        if sha(QA/'expected-images'/im['file']) not in image_hashes:errors.append(f'image data {im["file"]}')
    for sh in w:
        for row in sh:
            for c in row:
                if c.data_type=='e':errors.append(f'Excel error {sh.title}/{c.coordinate}')
                if isinstance(c.value,str) and len(c.value)>32767:errors.append(f'cell length {sh.title}/{c.coordinate}')
        if any(d.height and d.height>409.5 for d in sh.row_dimensions.values()):errors.append(f'row height {sh.title}')
    formulas={c.coordinate:c.value for row in s for c in row if c.data_type=='f'}
    expect_formulas={f'M{p["summaryRow"]+1}',f'M{p["summaryRow"]+2}'}
    if set(formulas)!=expect_formulas:errors.append(f'formula addresses {formulas}')
    cached=openpyxl.load_workbook(path,data_only=True)['보고서_유닛 시험']
    counts=[cached[f'M{p["summaryRow"]+i}'].value for i in [1,2]]
    if counts!=[103,12]:errors.append(f'cached totals {counts}')
    ns={'x':'http://schemas.openxmlformats.org/drawingml/2006/spreadsheetDrawing','a':'http://schemas.openxmlformats.org/drawingml/2006/main','r':'http://schemas.openxmlformats.org/officeDocument/2006/relationships'}
    anchors=[]
    with zipfile.ZipFile(path) as z:
        assert z.testzip() is None
        for name in z.namelist():
            if not name.startswith('xl/drawings/drawing') or not name.endswith('.xml'):continue
            xml=E.fromstring(z.read(name))
            for an in xml:
                if an.find('x:pic',ns) is None:continue
                f=an.find('x:from',ns)
                anchors.append({'row':int(f.findtext('x:row',namespaces=ns))+1,'col':int(f.findtext('x:col',namespaces=ns)),
                    'ext':{k:int(v) for k,v in an.find('x:ext',ns).attrib.items()} if an.find('x:ext',ns) is not None else None})
    for im in p['images']:
        if not any(a['row']==im['row'] and a['col']==10 for a in anchors):errors.append(f'image missing {im["id"]}/{im["row"]}')
        c=s.cell(im['row'],12)
        if '(사진필요' not in c.value or c.font.color.rgb[-6:]!='C00000':errors.append(f'photo missing/color {im["row"]}')
    source_diffs=[f for f,h in read(QA/'source-before.json').items() if sha(ROOT/f)!=h]
    criteria=ROOT/'docs/참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어/SUVC-FW-TD24A 소프트웨어 유닛 검증 기준서.xlsx'
    if sha(criteria)!='89967fa692c4f51e3be916c73270d75859b4f692c96928c2d73da0cc6c5096fd':errors.append('criteria changed')
    spec=importlib.util.spec_from_file_location('io_preservation',TASK/'스크립트/expected_io_common.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    old=read(TASK/'qa/expected-io/before.json');current=module.snapshot()
    changed=[k for k,h in old.items() if current.get(k)!=h]
    if changed:errors.append(f'protected files changed {changed}')
    result=dict(path=str(path),sha256=sha(path),units=len(p['units']),comparison_rows=sum(m['kind']=='comparison' for m in p['meta']),
        rtt_rows=sum(m['kind']=='rtt' for m in p['meta']),rtt_lines=original['log_lines'],images=anchors,judgments=p['counts'],
        formulas=formulas,source_changes=source_diffs,protected_files=len(old),protected_changes=changed,
        qcc={u['id']:[r for r in range(u['start'],u['end']+1) if '(QCC 검증 결과 필요)' in str(s.cell(r,12).value)] for u in p['units'] if u['qcc']},errors=errors)
    (QA/'direct-checks.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({**result,'images':len(anchors),'errors':errors[:30],'error_count':len(errors)},ensure_ascii=False,indent=2))
    assert not errors and not source_diffs
if __name__=='__main__':main()
