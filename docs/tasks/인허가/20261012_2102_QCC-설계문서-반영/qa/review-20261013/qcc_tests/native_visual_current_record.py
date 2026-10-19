from pathlib import Path
import json,hashlib
from integrate import sha
H=Path(__file__).resolve().parent;R=H.parent/'native-excel'/'tests'
native=json.loads((R.parent/'tests-native.json').read_text(encoding='utf-8'))
ids=json.loads((H/'native-id-positions-final.json').read_text(encoding='utf-8'))
record={'final_native_display_pass':False,'status':'시스템 15셀 실제 높이 잘림 보완 후보의 native 재검사 대기',
 'review_limits':'전체 contact는 빈 페이지/큰 배치/표 범위 개요 검사. 정밀 glyph 검사는 별도 native 경계 전수 후보와 명시한 full PNG/crop에 한정한다. 문자열 존재/예상 높이는 픽셀 검사 대신 쓰지 않았다.',
 'files':[],'partial_display_reviews':[], 'previous_failures_retained':['native-boundary-round1-review.json','system-height-before-native','system-height-candidates.json']}
for x in ids:
 code=x['code'];n=x['pages'];folder=R/code
 contacts=[folder/f'contact-{s:03d}-{min(s+11,n):03d}.png' for s in range(1,n+1,12)]
 record['files'].append({'code':code,'sourceSHA256':x['sourceSHA256'],'pdf':x['pdf'],'pdf_sha256':sha(Path(x['pdf'])),'pages':n,
  'contact_images':[{'path':str(p),'sha256':sha(p)} for p in contacts],
  'all_png_hashes':[{'page':i,'sha256':sha(folder/f'page-{i:03d}.png')} for i in range(1,n+1)]})
record['partial_display_reviews']=[
 {'codes':['SUVC-main','SUVR-main'],'status':'통과','scope':'130유닛/최신305+260페이지 전체 유효contact48개는 요구 담당 독립 개요검사. 45개fullPNG의 실제glyph검사범위와 QCC38조건 꼬리34PNG/current common17,350셀 exact는 별도증거 참조.',
  'evidence':[str(H.parent/'qcc_requirements'/'unit-contact-overview-qa.json'),str(H.parent/'qcc_requirements'/'independent-unit-native-qa.json')],
  'extra_native_pixel_checks':['SUVC-main/page-052.png','SUVR-main/page-046.png','unit-long-boundary-neighbors/SUVR-13-14.png','unit-long-boundary-neighbors/SUVR-84-85.png','unit-long-boundary-neighbors/SUVR-133-134.png','unit-long-boundary-neighbors/SUVR-150-151.png'],
  'findings':'CFX31 C1 64원소와 PASS N=64 F=0이 전체 표시됨. 네 긴 RTT/배열의 전 페이지 하단 흔적은 다음 페이지에서 동일 행 전체가 읽히는 정상 이어짐이며 값/끝번호/판정이 숨지 않았다. 짧은 비교 설명의 페이지 경계 후보는 없음.'},
 {'codes':['SITC-main','SITR-main'],'status':'통과','scope':'24통합/25+25페이지 실제 contact6개 전수개요와 모든 페이지 경계 char/border 후보를 실제 native 픽셀 대상으로검사. 기존 표 전체범위/신규 그룹 번호/관찰열/마지막 행 표시, 빈 페이지/짧은 설명 경계 잘림 없음.',
  'findings':'공통값/rich/행/병합과 새14조건 미실행은 qa.json/current-values.json의 저장본검사와 연결. 실제 제품시험 실행은 하지 않음.'},
 {'codes':['SSTC-cyber','SSTR-cyber'],'status':'통과','scope':'최신18+12페이지 실제contact3개의 전체표/결과열/끝행 개요검사. 짧은 준비/절차 높이 보완은 main시트만이며 cyber 본문/행/서식은 불변.',
  'findings':'현재 notice/예상/미실행이 유지되고 전체 빈페이지나 좌우 표 누락 없음. 개요검사를 전글자 정밀검사로 표현하지 않음.'},
 {'codes':['SSTC-main','SSTR-main'],'status':'실패','scope':'최신73+72페이지의 실제 경계crop 및 추가짧은문단crop에서 criterion I128/130/132/144/146/151/155/188/194/197/237/260/278/280,J218 잘림 확인.',
  'correction_candidate':'system-height-candidates.json','final_native_display_pass':False}]
(H/'native-visual-current-scope.json').write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'record':'native-visual-current-scope.json','final_native_display_pass':False,'native_parts':len(record['files'])}))
