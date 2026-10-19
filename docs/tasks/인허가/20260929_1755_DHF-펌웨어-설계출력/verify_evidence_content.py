"""저장 후보의 시험 식별자/방법/자료 안내와 로컬 링크를 검사한다."""
from __future__ import annotations
import json
import re
from urllib.parse import unquote
from pathlib import Path
from openpyxl import load_workbook
from review_evidence_collection import QA, ROOT, DHF, digest

base=json.loads((QA/'baseline.json').read_text(encoding='utf-8'))
books={};result={'tests':{},'qcc':{},'links':[]}
for b in base['books']:
    book=load_workbook(QA/f"{b['key']}-candidate.xlsx",rich_text=True)
    books[b['key']]=(b,book)
    rows={str(row[b['idCol']-1].value):row[0].row for row in book[b['sheet']]
          if re.fullmatch(r'SW-FW-(?:UnitTest-[A-Z0-9]+|IntegrationTest|SYS-Test)-ID-\d+',str(row[b['idCol']-1].value))}
    assert rows=={t['id']:t['row'] for t in b['tests']},b['key']
    result['tests'][b['key']]=len(rows)
    if b['key'].endswith('-report'):
        col=chr(65+b['common'])
        markers=[]
        for identifier,row in rows.items():
            v=str(book[b['sheet']][f'{col}{row}'].value)
            if '(QCC 검증 결과 필요)' in v:
                markers.append({'id':identifier,'row':row,'cell':f'{col}{row}'})
        result['qcc'][b['key']]=markers
incoming,inputbook=books['system-input'];outgoing,outbook=books['system-criterion']
inrows={t['id']:t['row'] for t in incoming['tests']}
for test in outgoing['tests']:
    assert str(inputbook[incoming['sheet']][f"D{inrows[test['id']]}"].value)==str(outbook[outgoing['sheet']][f"D{test['row']}"].value),(test['id'],'method mismatch')
uc,ub=books['unit-criterion'];ur,urb=books['unit-report']
assert str(ub[uc['sheet']]['F49'].value)=='T, D'
assert '사인파 표본이 아니다' in str(ub[uc['sheet']]['I49'].value)
assert '청취 확인 필요' in str(urb[ur['sheet']]['M49'].value)
assert '실제 타깃 서비스 콘솔에 reboot를 보내지 않는다' in str(ub[uc['sheet']]['H142'].value).replace('제품/타깃','타깃') or '실제 제품/타깃 서비스 콘솔에 reboot를 보내지 않는다' in str(ub[uc['sheet']]['H142'].value)
for cell in ('J34','J35'):
    before=next(t['cells'][cell] for t in outgoing['tests'] if cell in t['cells'])
    assert str(outbook[outgoing['sheet']][cell].value)==before
for row in (45,104):
    sr,srb=books['system-report']
    assert '[계측 준비 필요]' in str(srb[sr['sheet']][f'K{row}'].value)
files=[DHF/'04_설계 검증/SW/펌웨어/유닛 시험 일괄 실행 매뉴얼.md',
       DHF/'04_설계 검증/SW/펌웨어/유닛 시험 예상 입출력/CFX/CFX-020.md',
       ROOT/'docs/참고/인허가/사운드1/QCC 자료 추가 위치 관리.md']
for file in files:
    for target in re.findall(r'\]\(([^)]+)\)',file.read_text(encoding='utf-8')):
        target=target.split('#',1)[0]
        if not target or re.match(r'[a-zA-Z]+:',target):continue
        path=(file.parent/unquote(target)).resolve()
        assert path.exists(),(file,target)
        result['links'].append({'file':str(file),'target':str(path)})
tracker=files[-1].read_text(encoding='utf-8')
for key,markers in result['qcc'].items():
    for item in markers:
        row=item['row'];number=int(item['id'].rsplit('-',1)[1])
        if key=='unit-report':
            core=re.search(r'UnitTest-([A-Z0-9]+)-',item['id'])[1]
            anchor=f'| {core}-{number:03d} | D{row} | L{row} |'
        elif key=='integration-report':anchor=f'| {number} | C{row} | N{row} |'
        else:anchor=f'| {number} | B{row} | K{row} |'
        assert anchor in tracker,(key,item['id'],'QCC tracker location mismatch')
result['qccLocationsMatch']=sum(map(len,result['qcc'].values()))
publication=QA/'publication.json'
if publication.exists():
    published=json.loads(publication.read_text(encoding='utf-8'))
    for item in published['files']:
        assert digest(Path(item['path']))==item['afterSha256']==digest(QA/f"{item['key']}-candidate.xlsx"),item['key']
    result['savedBooksMatch']=len(published['files'])
layout=json.loads((QA/'rendered-pages.json').read_text(encoding='utf-8'))
assert all(not c['tailNotFound'] and not c['emptyPages'] for c in layout)
result['reviewedPages']=sum(c['pages'] for c in layout)
for _,book in books.values():book.close()
(QA/'content-checks.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'tests':result['tests'],'qccCounts':{k:len(v) for k,v in result['qcc'].items()},'localLinks':len(result['links']),
                  'qccLocationsMatch':result['qccLocationsMatch'],'savedBooksMatch':result.get('savedBooksMatch',0),
                  'reviewedPages':result['reviewedPages']},ensure_ascii=False))
