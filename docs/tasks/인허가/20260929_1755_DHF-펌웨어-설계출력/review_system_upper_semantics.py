"""Second, content-focused review separate from the package/Excel checks."""
from pathlib import Path
import json
import re
from align_system_upper_methods import QA, workbook, package, q, NS, cell
from lxml import etree as E

plan=json.loads((QA/'plan.json').read_text(encoding='utf8'))
baseline=json.loads((QA/'baseline.json').read_text(encoding='utf8'))
report_plan=next(p for p in plan['books'] if p['code']=='SSTR')
current=workbook(Path(report_plan['candidate']))['보고서_시스템 시험']['cells']
old=baseline['books']['SSTR']['보고서_시스템 시험']['cells']
tests=sorted({e['test'] for e in report_plan['edits'] if e['test'] is not None})
assert tests==[1,2,5,6,8,9,11,12,53,56,57,58,59,61,62,65,75,79]

# Numeric limits are expectations, not freshly measured results.
assert 32*750==24*1000==24000
assert 750*0.95==712.5 and 750*1.05==787.5
assert round(14*0.95,2)==13.30 and round(14*1.05,2)==14.70
assert 500*0.9==450 and 500*1.1==550
assert 1500*0.9==1350 and round(1500*1.1)==1650

# The numerical HEAR observations are copied only from the already recorded unit report.
units=baseline['SUVR']['보고서_유닛 시험']['cells']
unit_corpus='\n'.join(units.values())
for expected in ['8372210','8372226','8372222']:
    assert expected in unit_corpus, 'Unit result source missing: '+expected
    assert f'{int(expected):,}' in current['K53']
assert '소프트웨어 유닛 검증 보고서의 HEAR-ID-2' in current['K53']
assert 'CM3-ID-5' in current['K233']

# Added conditions lack new measurements; their notes/decisions must retain that boundary.
for address in ['K37','K43','K44','K52','K64','K203','K239','K245','K248']:
    assert '[E8300 보완]' in current[address]
    assert old[address].strip() in current[address]
for address in ['L35','L41','L44','L50','L64','L203','L239','L244','L247','L261','L274','L219']:
    assert '기존 결과 통과' in current[address] and '확인 대기' in current[address]
assert '실측 화면' not in ''.join(e['after'] for e in report_plan['edits'])
assert current['J261'].find('상위 문서는 충전기 연결 시 흰색')>=0
assert '상위의 주의 색상은 노란색' in current['J275']
assert '기본 CIS 맵은 1~32 순서' in current['H64']
assert '최소 증가 시간의 수치/단위' in current['J239']

# New references resolve to existing units, never newly created unit identifiers.
unit_ids=set(re.findall(r'SW-FW-UnitTest-([A-Z0-9]+-ID-\d+)', unit_corpus))
refs=set(re.findall(r'\b(?:HEAR|CM3|CFX)-ID-\d+\b',current['K53']+current['J233']+current['K233']))
assert refs <= unit_ids, (refs, unit_ids)
preserved=all(current.get(a,'')==v for a,v in old.items() if re.fullmatch(r'[B-E]\d+',a)
              and 34<=int(a[1:])<=281)
assert preserved
summary={'review':'Content-focused second pass','condition_test_count':len(tests),'condition_test_ids':tests,
         'test_identifiers_descriptions_methods_software_links_unchanged':preserved,
         'added_numeric_limits_checked':True,'HEAR_results_existing_unit_source':True,
         'existing_unit_references_resolve':True,'new_measurements_not_created':True,
         'old_pass_and_added_pending_scope_distinguished':True,
         'method_56':'Existing D retained for qualitative increase behavior. Approved quantitative minimum and duration evidence remain pending; no new quantitative test completion claimed.',
         'issues':[]}
(QA/'semantic-review.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps(summary,ensure_ascii=False,indent=2))
