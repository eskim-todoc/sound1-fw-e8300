"""기존 비교 시트의 내용과 레이아웃을 보존하고 문자 차이만 강조한다.

artifact-tool 공개 API에서 셀 부분 서식을 찾지 못해 OOXML rich text를 사용한다.
"""
from collections import defaultdict
from copy import deepcopy
from difflib import SequenceMatcher
from pathlib import Path
import hashlib
import json
import zipfile
from lxml import etree as E

T=Path(__file__).resolve().parent
ROOT=T.parents[3]
P=ROOT/'docs/참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어/SSTC-FW-TD24A 소프트웨어 시스템 시험 기준서.xlsx'
NS={'s':'http://schemas.openxmlformats.org/spreadsheetml/2006/main'}
def q(t):return '{'+NS['s']+'}'+t
def main():
    qa=T/'qa/sstc-character-diff';qa.mkdir(parents=True,exist_ok=True)
    backup=qa/'before.xlsx';assert not backup.exists()
    raw=P.read_bytes();backup.write_bytes(raw)
    with zipfile.ZipFile(P) as z:parts={n:z.read(n) for n in z.namelist()}
    original=dict(parts)
    strings=E.fromstring(parts['xl/sharedStrings.xml']) if 'xl/sharedStrings.xml' in parts else []
    def value(c):
        if c is None:return ''
        if c.get('t')=='s':return ''.join(strings[int(c.find(q('v')).text)].itertext())
        if c.get('t')=='inlineStr':return ''.join(c.xpath('./s:is//s:t/text()',namespaces=NS))
        return c.findtext(q('v')) or ''
    wb=E.fromstring(parts['xl/workbook.xml']);rels=E.fromstring(parts['xl/_rels/workbook.xml.rels'])
    links={x.get('Id'):x.get('Target') for x in rels}
    sh=next(x for x in wb.find(q('sheets')) if x.get('name').endswith('_변경 비교'))
    target=links[sh.get('{http://schemas.openxmlformats.org/officeDocument/2006/relationships}id')]
    name=target.lstrip('/') if target.startswith('/') else 'xl/'+target
    root=E.fromstring(parts[name]);styles=E.fromstring(parts['xl/styles.xml'])
    fonts=styles.find(q('fonts'));xfs=styles.find(q('cellXfs'))
    groups=defaultdict(lambda:defaultdict(list));before_values={c.get('r'):value(c) for c in root.iter(q('c'))}
    for row in root.find(q('sheetData')):
        cells={''.join(filter(str.isalpha,c.get('r'))):c for c in row}
        mark=value(cells.get('A'));identifier=value(cells.get('B'))
        if mark in ('[-]','[+]') and identifier.startswith('SW-FW-SYS-Test-ID-'):
            groups[identifier][mark].append(cells)
    count=0;changed_chars=0
    def rich(c,mask,mark):
        nonlocal count,changed_chars
        v=value(c)
        if not v:return
        assert c.find(q('f')) is None
        font=fonts[int(xfs[int(c.get('s','0'))].get('fontId','0'))]
        for child in list(c):
            if child.tag in (q('v'),q('is')):c.remove(child)
        c.set('t','inlineStr');inline=E.SubElement(c,q('is'))
        start=0
        while start<len(v):
            active=mask[start];end=start+1
            while end<len(v) and mask[end]==active:end+=1
            run=E.SubElement(inline,q('r'));rp=E.SubElement(run,q('rPr'))
            for prop in font:
                if prop.tag in (q('color'),q('b')):continue
                copied=deepcopy(prop)
                if copied.tag==q('name'):copied.tag=q('rFont')
                rp.append(copied)
            if active:E.SubElement(rp,q('b'))
            E.SubElement(rp,q('color'),rgb=('FF9C0006' if mark=='[-]' else 'FF006100') if active else 'FF666666')
            text=E.SubElement(run,q('t'));text.text=v[start:end];text.set('{http://www.w3.org/XML/1998/namespace}space','preserve')
            start=end
        count+=1;changed_chars+=sum(mask)
    for identifier,pair in groups.items():
        for col in 'BCDEFGHIJ':
            cells={m:[r[col] for r in pair.get(m,[]) if col in r] for m in ('[-]','[+]')}
            values={m:''.join(value(c) for c in cs) for m,cs in cells.items()}
            masks={m:[False]*len(v) for m,v in values.items()}
            for op,a,b,c,d in SequenceMatcher(None,values['[-]'],values['[+]'],autojunk=False).get_opcodes():
                if op!='equal':masks['[-]'][a:b]=[True]*(b-a);masks['[+]'][c:d]=[True]*(d-c)
            for mark,cs in cells.items():
                offset=0
                for cell in cs:
                    length=len(value(cell));rich(cell,masks[mark][offset:offset+length],mark);offset+=length
        for mark,rows in pair.items():
            for row in rows:rich(row['A'],[True]*len(value(row['A'])),mark)
    assert {c.get('r'):value(c) for c in root.iter(q('c'))}==before_values
    parts[name]=E.tostring(root,encoding='UTF-8',xml_declaration=True,standalone=True)
    candidate=qa/'candidate.xlsx'
    with zipfile.ZipFile(candidate,'w',zipfile.ZIP_DEFLATED) as z:
        for n,data in parts.items():z.writestr(n,data)
    assert P.read_bytes()==raw
    P.write_bytes(candidate.read_bytes())
    assert all(parts[n]==v for n,v in original.items() if n!=name)
    result={'test_groups':len(groups),'rich_text_cells':count,'emphasized_characters':changed_chars,
            'all_values_unchanged':True,'only_changed_part':name,'sha256':hashlib.sha256(P.read_bytes()).hexdigest()}
    (qa/'checks.json').write_text(json.dumps(result,indent=2),encoding='utf8')
    print(json.dumps(result))
if __name__=='__main__':main()
