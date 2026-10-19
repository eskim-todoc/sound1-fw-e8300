"""Artifact에서 편집한 ID 문자열만 원본 XLSX에 반영한다. 다른 셀과 서식은 보존한다."""
import copy
import json
import re
import shutil
import sys
from pathlib import Path

TASK=Path(__file__).resolve().parent
sys.path.insert(0,str(TASK.parent/'20260929_1755_DHF-펌웨어-설계출력'))
import inspect_system_cybersecurity as reader
import preserve_aligned_report_packages as native
import preserve_system_cybersecurity as bridge

QA=TASK/'qa/srs-parent-id-20261012'
MAIN='소프트웨어 입력 요구사항 사양서'
COMPARE=MAIN+'_변경 비교'


def plan():
    return json.loads((QA/'plan.json').read_text('utf8'))


def build():
    data=plan()
    authored=reader.extract(QA/'authored.xlsx')['sheets']
    package=native.parts(QA/'before.xlsx')
    strings=native.parse(package['xl/sharedStrings.xml'])
    target_indices={}
    for edit in data['changes']:
        assert authored[edit['sheet']]['cells'][edit['cell']]==edit['after']
        part=native.sheet_path(package,edit['sheet'])
        root=native.parse(package[part])
        cell=next(c for c in root.findall('.//s:c',native.N) if c.get('r')==edit['cell'])
        if cell.get('t')=='inlineStr':
            # 비교 셀의 문자별 색상과 굵기를 유지하며 접두어만 바꾼다.
            address=re.escape(edit['cell'].encode('ascii'))
            pattern=rb'<c\b[^>]*\br="'+address+rb'"[^>]*>.*?</c>'
            matches=list(re.finditer(pattern,package[part],re.S))
            assert len(matches)==1
            match=matches[0]
            replacement=match.group().replace(b'SYS-RID-FW',b'SYS-RID-SP-FW')
            package[part]=package[part][:match.start()]+replacement+package[part][match.end():]
            continue
        assert cell.get('t')=='s', '원본 공유 문자열 이외의 형식은 별도 검토한다.'
        index=int(cell.find('s:v',native.N).text)
        assert target_indices.get(index,edit['after'])==edit['after']
        target_indices[index]=edit['after']
    for index,expected in target_indices.items():
        for text in strings[index].findall('.//s:t',native.N):
            text.text=(text.text or '').replace('SYS-RID-FW','SYS-RID-SP-FW')
        assert ''.join(strings[index].xpath('.//s:t/text()',namespaces=native.N))==expected
    package['xl/sharedStrings.xml']=native.serialize(strings)
    bridge.save(package,QA/'candidate.xlsx')
    verify()


def verify(published=False):
    data=plan()
    before=reader.extract(QA/'before.xlsx')
    candidate=Path(data['target']) if published else QA/'candidate.xlsx'
    after=reader.extract(candidate)
    allowed={(e['sheet'],e['cell']):e for e in data['changes']}
    actual=set()
    assert before['sheets'].keys()==after['sheets'].keys()
    for name,sh in before['sheets'].items():
        other=after['sheets'][name]
        for key in ['merges','dimension']:
            assert sh[key]==other[key]
        for cell in sh['cells'].keys()|other['cells'].keys():
            old,new=sh['cells'].get(cell),other['cells'].get(cell)
            if old!=new:
                edit=allowed[(name,cell)]
                assert (old,new)==(edit['before'],edit['after'])
                assert re.findall(r'\d+',old)==re.findall(r'\d+',new)
                actual.add((name,cell))
    assert actual==allowed.keys()
    p0,p1=native.parts(QA/'before.xlsx'),native.parts(candidate)
    assert p0.keys()==p1.keys()
    changed_inline_parts=set()
    for edit in data['changes']:
        part=native.sheet_path(p0,edit['sheet'])
        root=native.parse(p0[part])
        cell=next(c for c in root.findall('.//s:c',native.N) if c.get('r')==edit['cell'])
        if cell.get('t')=='inlineStr': changed_inline_parts.add(part)
    for name,raw in p0.items():
        if name in changed_inline_parts:
            assert p1[name].replace(b'SYS-RID-SP-FW',b'SYS-RID-FW')==raw,name
        elif name!='xl/sharedStrings.xml': assert p1[name]==raw,name
    s0,s1=native.parse(p0['xl/sharedStrings.xml']),native.parse(p1['xl/sharedStrings.xml'])
    assert len(s0)==len(s1)
    for old,new in zip(s0,s1):
        restored=copy.deepcopy(new)
        for text in restored.findall('.//s:t',native.N):
            if text.text is not None:
                text.text=text.text.replace('SYS-RID-SP-FW','SYS-RID-FW')
        assert native.serialize(old)==native.serialize(restored)
    cells=after['sheets'][MAIN]['cells']
    ids={int(n) for value in cells.values() for n in re.findall(r'SYS-RID-SP-FW-(\d+)',value)}
    assert ids==set(range(1,51)),ids
    assert len([v for v in cells.values() if re.fullmatch(r'SW-FW-RID-\d+',v)])==81
    current_compare=after['sheets'][COMPARE]['cells']
    for cell,value in current_compare.items():
        row=re.search(r'\d+',cell).group()
        if current_compare.get('A'+row,'').strip()!='[-]': assert not re.search(r'SYS-RID-FW-\d+',value)
    result={'changedCells':len(actual),'mainCells':sum(n==MAIN for n,a in actual),'compareCells':sum(n==COMPARE for n,a in actual),'parentIds':len(ids),'softwareRequirements':81,'nonTextPackagePartsUnchanged':True,'deletionHistoryUnchanged':True,'sha256':after['sha256']}
    (QA/('published-verification.json' if published else 'verification.json')).write_text(json.dumps(result,ensure_ascii=False,indent=2),'utf8')
    print(json.dumps(result,ensure_ascii=False))


def publish():
    verify()
    data=plan()
    assert reader.sha(Path(data['target']))==reader.sha(QA/'before.xlsx'),'검토 중 원본이 변경되어 교체를 중단한다.'
    shutil.copyfile(QA/'candidate.xlsx',data['target'])
    verify(published=True)


if __name__=='__main__':
    {'build':build,'verify':verify,'publish':publish}[sys.argv[1]]()
