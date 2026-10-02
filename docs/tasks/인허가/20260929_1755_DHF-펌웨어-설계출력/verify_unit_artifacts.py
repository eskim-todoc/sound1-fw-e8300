"""Read-only checks of the saved source-based design and verification artifacts."""
from pathlib import Path
import hashlib, json, re, sys, zipfile
from collections import Counter
from urllib.parse import unquote
from lxml import etree
from docx import Document
import openpyxl
from pypdf import PdfReader

T=Path(__file__).resolve().parent
ROOT=T.parents[3]
QA=T/'qa/unit-reconciliation'
OUT=ROOT/'docs/참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어'
live='--live' in sys.argv
def read(name): return json.loads((QA/name).read_text(encoding='utf8'))
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def norm(value): return '' if value is None else str(value).replace('\r\n','\n')
def compact(value): return re.sub(r'\s+','',str(value))
units=read('model.json');plan=read('tables-plan.json');base=read('baseline.json')
lineage=read('lineage.json');modelcheck=read('model-checks.json')
su={u['su'] for u in units};si={u['si'] for u in units}
tests={u['test'] for u in units}
byid={u['su']:u for u in units}
assert len(units)==len(su)==len(tests)==115
assert len(lineage['first_draft'])==325
assert set(lineage['first_draft'].values())==su
assert not modelcheck['missing_symbols'] and not modelcheck['missing_original']
changed_sources=[p for p,h in modelcheck['source_hashes'].items() if sha(ROOT/p)!=h]
assert not changed_sources,changed_sources
for name in ('SDDD-source','SUVC-source','SAD-source','SAD-before'):
    assert sha(Path(base[name]['path']))==base[name]['sha256'],name
expected_modules={
    'CIS 전략과 전략 선택':'신호처리 모듈',
    'NofM 대역 선택':'신호처리 모듈',
    'NofM 선택 대역 교차 배치':'신호처리 모듈',
    '자극 패킷 헤더 구성':'내부 자극 장치 실행 모듈',
    'NofM 패킷 구성과 분할 출력':'내부 자극 장치 실행 모듈'}
for name,module in expected_modules.items():
    matches=[u for u in units if u['core']=='CFX' and u['name']==name]
    assert len(matches)==1 and matches[0]['module']==module,(name,module)
assert Counter(u['module'] for u in units if u['core']=='CFX')=={
    '시스템 제어 모듈':15,'신호처리 모듈':20,'내부 자극 장치 실행 모듈':7}

accepted=Document(QA/'SDDD-final-accepted.docx')
body='\n'.join(p.text for p in accepted.paragraphs)
heading_ids=[re.match(r'(SU-ID-[A-Z0-9]+-\d+),',p.text).group(1)
             for p in accepted.paragraphs if re.match(r'SU-ID-[A-Z0-9]+-\d+,',p.text)]
assert len(heading_ids)==115 and set(heading_ids)==su
assert heading_ids==[u['su'] for u in units], 'reading order and unit numbering'
active_si=None
for p in accepted.paragraphs:
    m=re.match(r'(SI-ID-[A-Z0-9]+-\d+),',p.text)
    if m:active_si=m[1]
    m=re.match(r'(SU-ID-[A-Z0-9]+-\d+),',p.text)
    if m:assert byid[m[1]]['si']==active_si,(m[1],active_si)
for u in units:
    for key in ('purpose','inp','out','steps','case','expected'):
        assert compact(u[key]) in compact(body),(u['su'],key)
    assert u['test'] in body
assert 'QCC 内部' not in body
assert 'QCC 내부 설계' in body and 'E8300 연계' in body
candidate=Document(QA/'SDDD-candidate.docx')
# Word's comparison may split runs. Paragraph text must not be changed.
assert [compact(p.text) for p in candidate.paragraphs if p.text]==[
    compact(p.text) for p in accepted.paragraphs if p.text], 'candidate/final body'
W={'w':'http://schemas.openxmlformats.org/wordprocessingml/2006/main'}
word=OUT/'SDDD-FW-TD24A 소프트웨어 상세 설계서.docx'
with zipfile.ZipFile(word) as z:
    root=etree.fromstring(z.read('word/document.xml'))
    settings=etree.fromstring(z.read('word/settings.xml'))
    ins=len(root.findall('.//w:ins',W));deleted=len(root.findall('.//w:del',W))
    assert ins and deleted
    assert settings.find('w:trackRevisions',W) is not None
    assert z.testzip() is None
assert len(PdfReader(QA/'SDDD-final.pdf').pages)==69
source_doc=Document(QA/'SDDD-source.docx')
rejected=Document(QA/'SDDD-rejected-qa.docx')
assert [compact(p.text) for p in source_doc.paragraphs if p.text]==[
    compact(p.text) for p in rejected.paragraphs if p.text], 'reject body restoration'
def table_texts(doc):
    return [[[compact(c.text) for c in r.cells] for r in t.rows] for t in doc.tables]
assert table_texts(source_doc)==table_texts(rejected), 'reject table restoration'

suvcpath=OUT/'SUVC-FW-TD24A 소프트웨어 유닛 검증 기준서.xlsx' if live else QA/'SUVC-candidate.xlsx'
sitcpath=OUT/'SITC-FW-TD24A 소프트웨어 통합시험 기준서.xlsx' if live else QA/'SITC-candidate.xlsx'
wb=openpyxl.load_workbook(suvcpath,data_only=False,rich_text=False)
def same_rows(sheet,start,rows,trim=False):
    for offset,row in enumerate(rows):
        for col,value in enumerate(row,1):
            actual=sheet.cell(start+offset,col).value
            lhs,rhs=norm(actual),norm(value)
            if trim: lhs,rhs=lhs.strip(),rhs.strip()
            assert lhs==rhs,(sheet.title,start+offset,col,value,actual)
same_rows(wb['기준서_유닛 검증'],30,plan['suvc']['physical'])
same_rows(wb['기준서_유닛 검증_변경 비교'],30,plan['suvc']['diffphysical'])
same_rows(wb['유닛 목록'],1,plan['suvc']['code_rows'])
same_rows(wb['원본 대응표'],1,plan['suvc']['original_rows'],trim=True)
same_rows(wb['1차본 대응표'],1,plan['suvc']['first_draft_rows'])
main=wb['기준서_유닛 검증'];end=29+len(plan['suvc']['physical'])
actual_tests=[main.cell(r,4).value for r in range(30,end+1) if main.cell(r,4).value]
assert len(actual_tests)==115 and set(actual_tests)==tests
assert {main.cell(r,7).value for r in range(30,end+1) if main.cell(r,7).value}==su
assert {u['method'] for u in units}<={'T','D','I','A'}
assert all(main.row_dimensions[r].height<=409 for r in range(30,end+1))
assert len(plan['suvc']['original_rows'])==187
assert len(plan['suvc']['first_draft_rows'])==326
assert Counter(r[0] for r in plan['suvc']['diff'])=={'[-]':186,'[+]':115}
# Rich text is a display-only feature and must not change source/new row text.
with zipfile.ZipFile(suvcpath) as z:
    xmls=[etree.fromstring(z.read(n)) for n in z.namelist() if n.startswith('xl/worksheets/sheet') and n.endswith('.xml')]
    X={'s':'http://schemas.openxmlformats.org/spreadsheetml/2006/main'}
    colors={n.get('rgb') for rt in xmls for n in rt.findall('.//s:rPr/s:color',X)}
    assert {'FF9C2020','FF14632A'}<=colors,colors
    assert z.testzip() is None
before=openpyxl.load_workbook(QA/'SITC-before.xlsx',data_only=False)
after=openpyxl.load_workbook(sitcpath,data_only=False)
allowed={(e['sheet'],e['cell']):e for e in plan['sitc']}
actualchanges=[]
for oldsheet in before:
    sheet=after[oldsheet.title]
    for row in range(1,max(oldsheet.max_row,sheet.max_row)+1):
        for col in range(1,max(oldsheet.max_column,sheet.max_column)+1):
            old=oldsheet.cell(row,col);new=sheet.cell(row,col)
            if norm(old.value)!=norm(new.value):
                key=(sheet.title,new.coordinate)
                assert key in allowed,key
                assert norm(new.value)==norm(allowed[key]['new']),key
                actualchanges.append(key)
for key,e in allowed.items():
    assert norm(after[e['sheet']][e['cell']].value)==norm(e['new'])
    ids=re.findall(r'S[UI]-ID-[A-Z0-9]+-\d+',e['new'])
    assert ids and all(i in su|si for i in ids),(key,ids)
assert set(actualchanges)=={k for k,e in allowed.items() if norm(e['old'])!=norm(e['new'])}
sstc=OUT/'SSTC-FW-TD24A 소프트웨어 시스템 시험 기준서.xlsx'
assert sha(sstc)=='99a92516b1d3d48025a9b038e9b777fca2f3194a2fbc43812bd54df988754c85'
registry=ROOT/'docs/참고/인허가/사운드1/QCC 자료 추가 위치 관리.md'
mdpaths=[T/'작업.md',registry,OUT/'[정리] Sound1 설계 출력 문서 변경사항.md']
link_count=0
for path in mdpaths:
    text=path.read_text(encoding='utf8')
    for target in re.findall(r'\]\(([^)]+)\)',text):
        if re.match(r'^(https?://|#)',target):continue
        location=unquote(target.split('#',1)[0].strip('<>'))
        assert (path.parent/location).exists(),(str(path),target)
        link_count+=1
qcc=registry.read_text(encoding='utf8')
assert '8.2 `QCC 내부 설계`' in qcc and '9 `소프트웨어 상세 설계 검증`' in qcc
assert f'D{end} `SW-FW-UnitTest-BOOT-ID-20`' in qcc
assert len(set(re.findall(r'\| (QCC-[A-Z]+-\d+) \|',qcc)))==18
assert 'QCC' in str(main['C19'].value)+str(main['D19'].value)
for r,n in [(36,'012'),(37,'013'),(38,'014'),(41,'017')]:
    assert after['기준서_통합 시험'][f'C{r}'].value=='SW-FW-IntegrationTest-ID-'+n
result={'scope':'saved-document consistency and independent arithmetic, not target execution',
    'published_excel':live,'unit_counts':dict(Counter(u['core'] for u in units)),
    'unit_count':115,'first_draft_roles':325,'original_tests':186,'original_design_roles':187,
    'word_pages':69,'word_xml_insertions':ins,'word_xml_deletions':deleted,
    'reject_restores_source_paragraphs_and_tables':True,
    'source_files_unchanged':len(modelcheck['source_hashes']),
    'suvc_last_row':end,'suvc_last_test':main.cell(end,4).value,
    'sitc_reference_cells_reviewed':len(allowed),
    'sitc_reference_cells_changed':len(actualchanges),'sstc_unchanged':True,
    'sad_strategy_packet_module_boundary_checked':True,'local_links_checked':link_count,
    'qcc_registry_entries':18,
    'sha256':{'SDDD':sha(word),'SUVC':sha(suvcpath),'SITC':sha(sitcpath)}}
(QA/('final-checks.json' if live else 'candidate-checks.json')).write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps(result,ensure_ascii=False,indent=2))
