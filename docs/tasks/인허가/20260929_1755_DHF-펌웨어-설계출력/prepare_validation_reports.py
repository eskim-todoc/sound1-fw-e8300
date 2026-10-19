"""기준서 필드를 보존하고 같은 입력의 예상/결과를 보고서 행으로 배치한다."""
import json
import math
from pathlib import Path

TASK = Path(__file__).resolve().parent
QA = TASK / 'qa/validation-reports'
FONT = '맑은 고딕'
RED = '#C00000'


def load(name):
    return json.loads((QA / name).read_text(encoding='utf-8'))


def lines(value, width):
    return sum(max(1, math.ceil(sum(13.4 if ord(ch) > 127 else 7.1 for ch in line) / (width - 18)))
               for line in str(value or '').split('\n'))


def build(kind):
    integration = kind == 'integration'
    criteria = load(f'{kind}-criteria.json')
    criteria_sheet = '기준서_통합 시험' if integration else '기준서_시스템 시험'
    sources = load('sources.json')
    entries = load('integration-results.json') if integration else load('system-results-1.json') + load('system-results-2.json')
    by_id = {e['id']: e for e in entries}
    id_index = 2 if integration else 1
    expected_index = 12 if integration else 9
    actual_index, result_index = expected_index + 1, expected_index + 2
    expected_col, actual_col, last_col = (chr(65 + n) for n in [expected_index, actual_index, result_index])
    first_row = 29 if integration else 36
    raw_tests = [r for r in criteria[criteria_sheet]['rows'] if str(r[id_index] or '').startswith('SW-FW-')]
    assert len(raw_tests) == (17 if integration else 81)
    assert set(by_id) == {r[id_index] for r in raw_tests}, (kind, 'ID mismatch')
    assert len(entries) == len(by_id), 'Duplicate IDs'
    widths = ([90,210,175,270,260,210,230,50,360,320,360,680,680,130] if integration
              else [205,310,50,150,150,350,320,350,680,680,130])
    plan = {
        'kind': kind, 'firstRow': first_row, 'idCol': chr(65 + id_index),
        'expectedCol': expected_col, 'actualCol': actual_col, 'lastCol': last_col,
        'expectedIndex': expected_index, 'columnCount': result_index,
        'widths': widths, 'rows': [], 'meta': [], 'merges': [], 'tests': [], 'images': [],
        'front': {}, 'frontMerges': [], 'frontFormats': [], 'footer': {}, 'footerMerges': [], 'footerFormats': [], 'formulas': {},
    }
    image_manifest = load('expected-images/manifest.json') if (QA / 'expected-images/manifest.json').exists() else []
    image_map = {}
    for im in image_manifest:
        for test_id in im['tests']:
            image_map.setdefault(test_id, []).append(im)
    row = first_row
    comparison_total = 0
    for raw in raw_tests:
        test = by_id[raw[id_index]]
        start = row
        comparisons = test['comparisons']
        assert comparisons, test['id']
        statuses = [c['status'] for c in comparisons]
        if any('대조' in s or s == '실패' for s in statuses):
            judgment = '결과 대조 필요'
        elif test.get('qcc'):
            judgment = 'QCC 결과 필요'
        elif any(s not in ['통과', '코드 분석 확인'] for s in statuses):
            judgment = '결과 기입 필요'
        else:
            judgment = '통과'
        for index, comp in enumerate(comparisons, 1):
            vals = [None] * result_index
            label = f'[{index}] {comp["label"]}'.replace('·', '/')
            vals[expected_index-1] = label + '\n' + comp['expected'].replace('·', '/')
            vals[actual_index-1] = label + '\n' + comp['actual'].replace('·', '/')
            if index == 1:
                vals[:expected_index-1] = raw[1:expected_index]
                vals[-1] = judgment
            h = max(64, lines(vals[expected_index-1], widths[expected_index-1])*14+18,
                    lines(vals[actual_index-1], widths[actual_index-1])*14+18)
            if h > 400:
                raise ValueError(f'{test["id"]} comparison {index} row too tall {h}')
            plan['rows'].append(vals)
            plan['meta'].append({'row': row, 'height': h, 'kind': 'comparison', 'pending': comp['status'] not in ['통과','코드 분석 확인']})
            row += 1
            comparison_total += 1
        comparison_end = row - 1
        # 긴 준비/절차/SU 목록은 한 번만 쓰고 조건별 비교행 높이를 합쳐 표시한다.
        available = sum(m['height'] for m in plan['meta'] if start <= m['row'] <= comparison_end)
        required = max(lines(raw[c], widths[c-1])*14+20 for c in range(1, expected_index))
        if required > available:
            extra = (required-available)/len(comparisons)
            for m in plan['meta']:
                if start <= m['row'] <= comparison_end:
                    m['height'] = math.ceil(m['height']+extra)
                    if m['height'] > 400:
                        raise ValueError(f'{test["id"]} merged fields require extra comparison rows')
        for c in range(1, expected_index):
            col = chr(65+c)
            if row-start > 1:
                plan['merges'].append(f'{col}{start}:{col}{row-1}')
        if row-start > 1:
            plan['merges'].append(f'{last_col}{start}:{last_col}{row-1}')
        # 기준서 원문도 시험 안에 남겨 기준 변경 없이 판정 범위를 찾도록 한다.
        vals = [None]*result_index
        note_start = 'J' if integration else 'G'
        note_end = 'L' if integration else 'I'
        vals[9 if integration else 6] = '통과 기준 원문 / 확인 범위'
        vals[expected_index-1] = raw[expected_index]
        evidence = list(dict.fromkeys(c.get('evidence','') for c in comparisons if c.get('evidence')))
        # 공통 근거/실행 범위는 앞부분에 한 번만 둔다. 시험마다 해시와 작업 경로를 되풀이하지 않는다.
        repeated = ('기준서 main:', '원기준 통과 조건', '원기준 통과 기준', '실제 시스템 시험 완료', '근거 경계:',
                    '파일 기준:', '원기준 방법:', '자료 출처 구분:', '구조 기준 56파일',
                    '근거 경로는', '구조 문서는', '331파일 보호 기준', 'RTT 통합 전용 로거', '원본 1.5 보고서는')
        notes = [n.replace('·', '/') for n in test.get('notes',[]) if not n.startswith(repeated)]
        if judgment == '통과' and evidence:
            repo_prefix = str(TASK.parents[3]).replace('\\','/') + '/'
            notes.append('코드 분석 근거: ' + '\n'.join(e.replace('\\','/').replace(repo_prefix,'') for e in evidence))
        if test.get('qcc'):
            notes.append('(QCC 검증 결과 필요) E8300 측 결과와 QCC 내부/무선 연계 결과를 구분하여 첨부한다.')
        if test.get('photo') and test['id'] not in image_map:
            notes.append('(사진필요) 이 시험의 실제 화면/파형/표시 사진을 첨부한다.')
        # 출처 파일은 구체적인 확인 근거로 한 번만 기록한다.
        note_text = '\n'.join(notes)
        if not note_text:
            note_text = '시험 결과란은 해당 입력에 대한 관찰값을 기록한다.'
        h = max(60, lines(raw[expected_index], widths[expected_index-1])*14+18, lines(note_text,widths[actual_index-1])*14+18)
        if h > 400:
            raise ValueError(f'{test["id"]} notes too tall {h}')
        vals[actual_index-1] = note_text
        plan['rows'].append(vals)
        plan['meta'].append({'row':row,'height':h,'kind':'scope','pending':test.get('qcc',False) or test.get('photo',False) or judgment=='결과 대조 필요'})
        plan['merges'].append(f'{note_start}{row}:{note_end}{row}')
        scope_row=row
        row+=1
        for im in image_map.get(test['id'],[]):
            vals=[None]*result_index
            vals[9 if integration else 6] = f'그림 {len(plan["images"])+1}. '+im['title']+'\n'+im['caption']
            vals[actual_index-1] = '(사진필요)\n같은 입력 조건의 실제 사진/파형을 이 칸에 첨부한다.\n왼쪽은 예상 도식이며 실제 측정 결과가 아니다.'
            plan['rows'].append(vals)
            plan['meta'].append({'row':row,'height':294,'kind':'image','pending':True})
            plan['merges'].append(f'{note_start}{row}:{note_end}{row}')
            plan['images'].append({**im,'row':row,'test':test['id']})
            row+=1
        plan['tests'].append({'id':test['id'],'start':start,'comparisonEnd':comparison_end,'scopeRow':scope_row,'end':row-1,'judgment':judgment,'qcc':bool(test.get('qcc')),'evidence':evidence,'relatedUnits':test.get('related_units',[])})
    plan['bodyEnd']=row-1
    plan['comparisonCount']=comparison_total
    plan['front']={
        'B2':f'{"통합" if integration else "시스템"} 시험 보고서\n인공달팽이관 외부장치 펌웨어',
        'D7':'TD24A-A-EXT','D8':'(시험 빌드 버전 기입)',
        'C11':'인공달팽이관 외부장치 Sound1 (TD24A-A-EXT)',
        'D23':None,'D24':'김은수',
        f'{expected_col}{first_row-2}':'통과 기준\n(예상 결과)',
        f'{actual_col}{first_row-2}':'측정/검토 내용\n(시험 결과)',
    }
    if not integration:
        plan['front'].update({'D23':criteria[criteria_sheet]['rows'][22][3],'D24':'리모콘 및 매핑 명령의 무선 송수신','C24':'QCC5181 블루투스 연계','D29':None,'D30':None,'D31':'김은수'})
    else:
        plan['front'].update({'D22':None,'E27':'시험 항목'})
        for r in range(16,20):
            plan['front'][f'C{r}']=criteria[criteria_sheet]['rows'][r-1][2]
            plan['front'][f'D{r}']=criteria[criteria_sheet]['rows'][r-1][3]
    guide_start='J' if integration else 'G'
    guide_end=last_col
    methods='[I] Inspection : 제어 또는 시각적 확인\n[A] Analysis : 분석적인 증거에 기반한 확인\n[D] Demonstration : 정량적인 측정 없이 동작 특성 확인\n[T] Test : 정량적인 측정을 통한 정량적인 특성 확인'
    prose='같은 입력 조건의 예상 결과와 시험 결과를 같은 행에서 비교한다. 예상 도식은 실측 화면이 아니다.\n현재 확인한 코드 분석은 분석 결과로 적었으며, 실제 관찰값이 없는 항목은 결과 기입 필요로 구분했다. 유닛 시험 결과만으로 통합/시스템 시험을 통과 처리하지 않았다.\n(사진필요)와 (QCC 검증 결과 필요)는 실제 자료를 추가할 자리다.'
    plan['frontMerges']=[f'{guide_start}5:{guide_end}8',f'{guide_start}10:{guide_end}13',f'{guide_start}15:{guide_end}18']
    plan['front'][f'{guide_start}5']=prose
    plan['front'][f'{guide_start}10']=methods
    plan['front'][f'{guide_start}15']=f'기준 문서: {Path(sources[f"{kind}-criteria"]["path"]).name}, Rev.0\n{len(raw_tests)}개 시험의 식별 ID, 방법, 준비 조건, 제한, 절차와 통과 기준을 유지한다. 시험자의 실행 기록 및 판정은 해당 시험 조건에 한하여 작성한다.'
    plan['frontFormats']=[{'range':a,'format':{'font':{'name':FONT,'size':10},'wrapText':True,'verticalAlignment':'center','horizontalAlignment':'left'}} for a in plan['frontMerges']]
    plan['frontFormats'] += [
        {'range':'D8','format':{'font':{'color':RED}}},
        {'range':f'B{first_row-2}:{last_col}{first_row-1}','format':{'font':{'name':FONT,'size':10,'bold':True},'wrapText':True,'horizontalAlignment':'center','verticalAlignment':'center','rowHeight':30}},
        {'range':'C15:H19' if integration else 'C15:F26','format':{'wrapText':True,'rowHeight':32}},
    ]
    qr=row+1
    plan['qccRow']=qr
    plan['footer'][f'{plan["idCol"]}{qr}']='(QCC 검증 결과 필요) QCC 내부 시험은 자료 수령 후 같은 형식으로 이 위치에 추가한다. 기존 E8300 연계 시험의 결과와 구분한다.'
    plan['footerMerges'].append(f'{plan["idCol"]}{qr}:{last_col}{qr}')
    plan['footerFormats'].append({'range':f'{plan["idCol"]}{qr}:{last_col}{qr}','format':{'font':{'name':FONT,'size':10,'color':RED},'wrapText':True,'rowHeight':36}})
    summary=qr+3
    plan['footer'][f'B{summary}']='6. 시험 결과 및 확인할 자료'
    plan['footerFormats'].append({'range':f'B{summary}:{last_col}{summary}','format':{'font':{'name':FONT,'size':12,'bold':True},'rowHeight':28}})
    for i,status in enumerate(['통과','실패','결과 기입 필요','결과 대조 필요','QCC 결과 필요'],1):
        plan['footer'][f'{actual_col}{summary+i}']=status
        plan['formulas'][f'{last_col}{summary+i}']=f'=COUNTIFS({plan["idCol"]}{first_row}:{plan["idCol"]}{plan["bodyEnd"]},"<>",{last_col}{first_row}:{last_col}{plan["bodyEnd"]},"{status}")'
    plan['footerFormats'].append({'range':f'{actual_col}{summary+1}:{last_col}{summary+5}','format':{'font':{'name':FONT,'size':10},'wrapText':True,'rowHeight':26}})
    plan['summaryRange']=f'{actual_col}{summary+1}:{last_col}{summary+5}'
    plan['lastRow']=summary+5
    views=[{'name':'cover','sheet':'표지','range':'B2:O24'}, {'name':'header','range':f'{guide_start}5:{guide_end}18'}]
    picks=[0,len(raw_tests)//2,len(raw_tests)-1]
    for idx in picks:
        t=plan['tests'][idx]
        views.append({'name':f'case-{idx+1}','range':f'{note_start}{t["start"]}:{last_col}{min(t["comparisonEnd"],t["start"]+3)}'})
    if not integration:
        for number in [9,15,35,39,40,46,65,77]:
            t = next(t for t in plan['tests'] if t['id'] == f'SW-FW-SYS-Test-ID-{number}')
            views.append({'name':f'review-{number}','range':f'{expected_col}{t["start"]}:{last_col}{t["comparisonEnd"]}'})
    views.append({'name':'summary','range':plan['summaryRange']})
    plan['views']=views
    (QA/f'{kind}-plan.json').write_text(json.dumps(plan,ensure_ascii=False,indent=2),encoding='utf-8')
    print(kind,len(raw_tests),'tests',comparison_total,'comparisons',len(plan['images']),'images',row-first_row,'body rows')


if __name__=='__main__':
    import sys
    build(sys.argv[1])
