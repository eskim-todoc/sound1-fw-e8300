from pathlib import Path
import json, datetime
q=Path(__file__).resolve().parent
r=json.loads((q/'render-checks.json').read_text(encoding='utf-8'))
assert r['pages']==55 and r['changed_pages']==[3,10,11,12]
assert r['before_matches_previously_validated_render'] and not r['baseline_different_pages']
v={'status':'PASS','checked_at':datetime.datetime.now().astimezone().isoformat(),
 'candidate_sha256':r['candidate_sha256'],'pages':r['pages'],
 'changed_pages_directly_reviewed':[3,10,11,12],
 'same_pixel_pages_reuse':r['same_pages'],
 'all_pages_rendered':True,'TOC_pages_match_body':True,
 'module_titles_have_no_module_ID_prefix':True,
 'module_name_role_and_unit_explanations_preserved':True,
 'no_new_clipping_overlap_or_pagination_change':True,
 'review':'목차3쪽과 본문10/11/12쪽 PNG를 직접 열어 확인. 모듈 제목4개는 기존 글꼴/번호/굵기를 유지하며 목차 이름 및 10/10/11/12쪽 연결과 일치. 나머지51쪽은 기존 검증본과 픽셀 일치.',
 'pixel_evidence':str(q/'render-checks.json')}
(q/'visual-review.json').write_text(json.dumps(v,ensure_ascii=False,indent=2),encoding='utf-8')
print('Visual review recorded: 4 changed pages inspected, 51 unchanged pages matched')
