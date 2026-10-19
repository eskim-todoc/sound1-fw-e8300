from pathlib import Path
import json,datetime
from integrate import sha
H=Path(__file__).resolve().parent
R=H.parent/'native-excel'/'tests'
P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'))
ids=json.loads((H/'native-id-positions-final.json').read_text(encoding='utf-8'))
native=json.loads((R.parent/'tests-native.json').read_text(encoding='utf-8'))
source={k:sha(Path(b['path'])) for k,b in P['books'].items()}
for n in native:
    assert n['sourceUnchanged'] is True
    assert n['sourceSHA256'].lower()==source[n['code'].split('-')[0]]
assert len(native)==8
assert not json.loads((H/'system-internal-boundary-final.json').read_text(encoding='utf-8'))
geometry=json.loads((H/'pdf-geometry-triage-final.json').read_text(encoding='utf-8'))
assert [(x['key'],b['page']) for x in geometry for b in x['boundary_candidates']]==[('SUVR',13),('SUVR',84),('SUVR',133),('SUVR',150)]
def file_record(path):
    p=Path(path);assert p.is_file()
    return {'path':str(p),'sha256':sha(p)}
record={
    'checked_at':datetime.datetime.now().astimezone().isoformat(),
    'final_native_display_pass':True,
    'source_hashes':source,
    'status':'통과',
    'scope':'현재 6개 저장본의 Excel 읽기 전용 Open/전체 인쇄 8파트와 변경 구간의 실제 native 픽셀 검토 통과. 전체 788쪽 contact는 빈 페이지/큰 배치/표 좌우 누락 개요 검사이며, 정밀 glyph 검사는 명시된 full PNG/crop 및 페이지 경계 후보에 한정한다. 이 범위를 전체 788쪽 모든 글자 정밀 검토나 실제 제품시험 통과로 확대하지 않는다.',
    'product_execution':'실제 제품시험을 새로 실행하지 않았다. 신규 38유닛/14통합/3시스템 조건은 미실행/판정 대기다. 기존 관찰은 같은 입력의 과거 범위에서 보존된다.',
    'native_export_evidence':file_record(R.parent/'tests-native.json'),
    'parts':[],
    'actual_pixel_reviews':[],
    'unresolved_display_failures':[],
    'previous_failures_retained':[
        file_record(H/'native-boundary-round1-review.json'),
        file_record(H/'native-visual-current-scope.json'),
        file_record(H/'system-height-candidates.json'),
        {'path':str(H/'system-height-before-native'),'kind':'immutable previous failing native PDF/crop archive'},
    ],
    'unresolved_product_evidence':'CRA 독립 예상 벡터/직렬화/키 역할, 시간 허용오차와 초기화 계약, SPI 길이필드/일부 상태 인코딩, 통화 경로의 실제 수집값/프로파일/동시 동작, 제품 설명서 실제 검토 등 본문 담당별 미비 안내 유지. 문서 검사 통과와 구분한다.'
}
for item in ids:
    code=item['code'];n=item['pages'];folder=R/code
    assert item['sourceSHA256'].lower()==source[code.split('-')[0]]
    assert not item['missing_literal_id_labels']
    assert not item['sparse_page_candidates']
    record['parts'].append({
        'code':code,'source_sha256':item['sourceSHA256'].lower(),
        'native_pdf':file_record(item['pdf']),'pages':n,
        'all_png_hashes':[{'page':i,**file_record(folder/f'page-{i:03d}.png')} for i in range(1,n+1)],
        'contact_images':[file_record(folder/f'contact-{s:03d}-{min(s+11,n):03d}.png') for s in range(1,n+1,12)],
        'literal_ID_count':len(item['id_positions']),
        'literal_ID_scope':'저장본 ID 순서/개수와 실제 PDF 위치의 지원 검사. ID 텍스트 존재만으로 glyph 표시 통과로 판단하지 않음.',
    })
assert sum(x['pages'] for x in record['parts'])==788
syscrops=json.loads((H/'system-height-native-final-crops.json').read_text(encoding='utf-8'))
assert len(syscrops)==34
for x in syscrops:
    assert x['sourceSHA256'].lower()==source[x['key']]
    assert sha(Path(x['crop']))==x['crop_sha256']
record['actual_pixel_reviews'].append({
    'codes':['SSTC-main','SSTR-main'],'status':'통과',
    'reviewer':'qcc_tests actual image view',
    'scope':'잘린 15개 기준 셀의 동일 입력 조건을 양문서에서 26행 높이 보완 후 직접 재검토했다. SYS27/28/29/33/34/36/38/46/49/51/55/61/67/68/74 및82/83 양쪽 34개 상세 crop에서 처음/마지막 문장과 인접 예상/관찰/판정 표시를 확인했다. SYS52(기준2쪽/보고1쪽),SYS75(양쪽1쪽) 추가 상세 crop5개도 직접 확인했다. 시스템 본체143쪽 contact12개 및 cyber30쪽 contact3개 전체 개요 검사에서 빈 페이지/좌우 표 누락 없음.',
    'repaired_criteria_cells':['I128','I130','I132','I144','I146','I151','I155','I188','I194','I197','I237','I260','I278','I280','J218'],
    'heights_max_pt':285.25,
    'crop_evidence':file_record(H/'system-height-native-final-crops.json'),
    'crop_pixels':syscrops,
    'extra_evidence':file_record(H/'final-system-guard-crops.json'),
    'internal_border_candidate_count':0,
    'supporting_border_scan':file_record(H/'system-internal-boundary-final.json'),
    'findings':'절차의 마지막 문장과 현재 날짜/입력 비교가 셀 안에서 완결된다. SYS75 현재 녹색 판정은 경계와 겹치지 않는다. SYS82/83 실제 결과 없음/미실행/판정 대기와 미비 안내가 읽힌다. SYS52 과거 관찰과 새 CRA 조건 대기가 구분된다.',
})
unitrefs=[H.parent/'qcc_requirements'/'unit-contact-overview-qa.json',H.parent/'qcc_requirements'/'independent-unit-native-qa.json']
record['actual_pixel_reviews'].append({
    'codes':['SUVC-main','SUVR-main'],'status':'통과',
    'reviewer':'qcc_requirements independent actual view plus qcc_tests native detail view',
    'scope':'130유닛/305+260쪽 전체 유효 contact48개는 독립 개요 검사. 독립 full PNG45개 및 QCC38조건 꼬리34쪽의 상세 glyph 검사 범위는 별도 증거에 고정되어 있다. qcc_tests는 CFX31C1 SUVC52/SUVR46쪽의 전체64원소와 PASS N=64 F=0 표시를 직접 확인하고, 경계 후보4곳의 전 페이지 하단/다음 페이지 첫 전체 줄을 실제 crop로 읽었다.',
    'independent_evidence':[file_record(p) for p in unitrefs],
    'additional_full_pixels':[file_record(R/'SUVC-main'/'page-052.png'),file_record(R/'SUVR-main'/'page-046.png')],
    'boundary_detail_pixels':[file_record(H/'unit-long-boundary-neighbors'/f'SUVR-{a}-{b}.png') for a,b in [(13,14),(84,85),(133,134),(150,151)]],
    'findings':'네 긴 RTT/배열의 전 페이지 하단 흔적과 같은 전체 줄이 다음 페이지에서 온전히 읽힌다. 배열 끝/값/판정 누락이 없는 정상 이어짐이며 짧은 비교 설명 잘림으로 판단하지 않는다. 확인 범위를 전체565쪽의 모든 glyph 정밀 검사로 확대하지 않는다.',
})
detail=H/'integration-native-details-final'
record['actual_pixel_reviews'].append({
    'codes':['SITC-main','SITR-main'],'status':'통과','reviewer':'qcc_tests actual native image view',
    'scope':'24통합/25+25쪽 전체 contact6개 개요 및 모든 페이지 경계 후보 검사. 신규14조건은 양문서19..25쪽 오른쪽 crop14개와 통합22 F141 양쪽23쪽 왼쪽 crop2개를 실제로 직접 읽었다.',
    'detail_pixels':[file_record(detail/f'{k}-{pn:03d}-right.png') for k in ['SITC','SITR'] for pn in range(19,26)]+[file_record(detail/f'{k}-023-left.png') for k in ['SITC','SITR']],
    'findings':'21byte 전송 경계, 독립CRA 계산/시간조건, I2S 수신 경계의CFX005/008/015 공식 이름/역할, GAIA/NVM 입출력과 관찰/판정대기가 셀 안에 들어간다. 짧은 설명 경계 잘림/빈 페이지가 없고 실제 제품시험 결과는 만들지 않았다.',
})
record['supporting_current_checks']=[file_record(H/'pdf-geometry-triage-final.json'),file_record(H/'native-id-positions-final.json'),file_record(H/'system-height-plan.json')]
(H/'final-native-display-qa.json').write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'final_native_display_pass':True,'parts':len(record['parts']),'pages':sum(x['pages'] for x in record['parts']),'source_hashes':source},ensure_ascii=True))
