from pathlib import Path
from datetime import datetime,timezone
import json,re,hashlib
Q=Path(__file__).parent;TASK=Q.parent.parent;ROOT=TASK.parents[3]
policy=ROOT/'docs/참고/인허가/사운드1/SW 밸리데이션 문서 작성·검증 지침.md'
titles=re.findall(r'^### (\d+)\. (.+)$',policy.read_text('utf-8-sig'),flags=re.M)
assert [int(n) for n,t in titles]==list(range(1,30))
def read(n):return json.loads((Q/n).read_text('utf-8-sig'))
assert read('content-saved-review.json')['status'].startswith('PASS')
assert read('native-font-check.json')['status'].startswith('PASS')
assert all(read(c+'-field-compare.json')['pass'] for c in ['SR','SGR'])
assert all(read(c+'-visual-final-review.json')['status'].startswith('PASS') for c in ['SR','SGR'])
assert read('post-publish-checks.json')['status']=='PASS_POST_PUBLISH'
scope={
1:('PASS','SR/SGR을 독립적으로 읽히는 통합 문서로 유지. 자료 요청 문구를 실제 미확정 사실로 바꾸고 기존 QCC 설명을 표시. 별도 QCC 요구사항 체계 생성 없음.',['qcc-audit.json','content-saved-review.json']),
2:('NA','SRS Excel/매트릭스 문구 및 추적 수정 요청이 아니므로 변경하지 않음. 최신 외부 편집도 보존.',['published.json']),
3:('PASS','현재 Word에 있는 RID69/83과 문서 연결 이름/ID는 보존 검사 범위. 전체 추적 관계 재검증은 이번 범위 밖.',['content-saved-review.json','SR-field-compare.json']),
4:('PASS','QCC 참고 및 현재 문서 7개를 실제 읽어 반영 사실과 미확정 안내를 구분. 미갱신 표기만으로 기능을 재해석하지 않음.',['qcc-audit.json']),
5:('NA','구조/코드/그림 변경은 하지 않음. 기존 DMIC2 양쪽 연결 설명과 그림을 보존했으며 구조 재분석으로 확대하지 않음.',['content-saved-review.json','SR-field-compare.json']),
6:('PASS','내용 대조→서식 적용→native 전체 표시→빈 페이지/탭 들여쓰기 수정→재출력/내용 재검사의 반복을 완료. 요구/시험 내용은 그대로.',['layout-refinement.json','SGR-visual-review.json','SGR-visual-final-review.json']),
7:('PASS','기존 E8300 문서 구조와 설명을 기반으로 공통 스타일을 적용하여 E8300/QCC 본문 크기와 계층을 일치시킴. 지정 QCC만 굵은 빨강.',['authored.json','native-font-check.json']),
8:('NA','상세설계 본문/함수/유닛 조건은 수정하지 않음.',['published.json']),
9:('NA','시험 방법 I/A/D/T 선택을 변경하는 작업이 아님.',['published.json']),
10:('NA','시험 방법 또는 통과 증거의 재분류 없음.',['published.json']),
11:('NA','시험 준비/조건/절차를 새로 작성하지 않음.',['published.json']),
12:('NA','기준서/보고서 행과 공통 열을 변경하지 않음.',['published.json']),
13:('NA','예상값/관찰값/판정 행을 작성하거나 변경하지 않음.',['published.json']),
14:('NA','RTT 로그를 생성하거나 기록하지 않음.',['published.json']),
15:('NA','시험 조건/검증 번호 편집 없음.',['published.json']),
16:('NA','결과 칸에 새로운 시험 결과를 입력하지 않음.',['published.json']),
17:('NA','시험자 확인 결과의 신규 반영 없음. 기존 설명/미확정 상태 보존만 확인.',['content-saved-review.json']),
18:('PASS','확인된 설계 내용 강조와 실제 시험 통과를 구분. 6개 미확정 사실 유지, _완료는 편집 완료임을 기록.',['content-saved-review.json','post-publish-checks.json']),
19:('NA','과거 지정 수치 처리/재생성 없음.',['published.json']),
20:('PASS','새 예상/실측 그림 생성 없이 기존 그림을 보존. native 표시의 그림/캡션 배치 확인.',['SR-field-compare.json','SGR-field-compare.json','SR-visual-final-review.json','SGR-visual-final-review.json']),
21:('NA','일반 시간 시험/경계 입력/과거 승인 날짜 편집 없음.',['published.json']),
22:('PASS','사용자의 이번 해제 지시를 SR/SGR 두 Word에만 적용. QCC/공동 안내 6개를 해제하고 실제 내용12개 C00000 굵게 10pt/밑줄없음. 미확정6개 검정10pt, E8300 보완 표시 보존. SV22에 해제 범위 기록.',['content-saved-review.json','native-font-check.json','post-publish-checks.json']),
23:('PASS','최종 G 저장본에서 제목/검색 문구를 재수집하여 중앙 목록의 완료본 경로/반영12곳/미확정6곳/확인일 갱신. 전체12개 링크 존재 확인.',['current-positions.json','post-publish-checks.json']),
24:('NA','Excel 서식은 편집하지 않음.',['published.json']),
25:('NA','Excel 본문/비교 시트를 변경하지 않음.',['published.json']),
26:('PASS','6개 안내 문구의 실제 김은수 삭제/삽입과 새 변경의 native 수락/거부 검증. 기존 이력/그림 보존. 현재 서식은 직접 조정하고 기존 format snapshot을 보존했으며 중첩 새 서식 추적을 만들지 않은 범위를 명시.',['native-projection.json','content-final-review.json','content-saved-review.json','SR-field-compare.json','SGR-field-compare.json']),
27:('PASS','같은 G 입력 폴더에 기존 접두어+_완료로 게시하고 로컬 이전 사본 보관. 보호원본23/다른현재10 게시 직전후 불변. SRS/SUVR 외부 해시 변경은 보존하며 이전 검증을 새현재본 검증으로 오인하지 않게 기록.',['published.json','post-publish-checks.json']),
28:('PASS','최종 fields 저장본 SHA와 G 게시본 일치. Word native 11+11쪽 전체 직접 열람/빈 페이지·잘림·목차 검토, 정확 문구/실제 RangeFont18곳 검사. 최초 size속성생략 FAIL은 실제Word값으로 재판정.',['content-saved-review.json','native-font-check.json','SR-visual-final-review.json','SGR-visual-final-review.json','post-publish-checks.json']),
29:('PASS','실제 공통 DOTX 기준의59 TDC 스타일/지원참조를 보존하고 제목·목록·본문 기준선/번호굵기/본문10pt·1.25배·뒤8pt/표·그림·목차를 검사. 빈 페이지와 메모리2탭을 수정해 최종 표시 확인.',['authored.json','layout-refinement.json','native-font-check.json','SR-visual-final-review.json','SGR-visual-final-review.json'])}
entries=[{'id':int(n),'title':title,'status':scope[int(n)][0],'scope_and_result':scope[int(n)][1],'evidence':[str(Q/e) for e in scope[int(n)][2]]} for n,title in titles]
for row in entries:
 for evidence in row['evidence']:assert Path(evidence).exists()
result={'status':'PASS_APPLICABLE_DOCUMENT_SCOPE','checked_at':datetime.now(timezone.utc).astimezone().isoformat(),'owner':'root; qcc_design initial scoping read, final evidence integration by root after ownership handoff','scope':'Only design-input SR/SGR Word formatting, QCC request wording and completed file names; not full product validation','policy_source':str(policy),'policy_sha256':hashlib.sha256(policy.read_bytes()).hexdigest().upper(),'entries':entries,'counts':{s:sum(e['status']==s for e in entries) for s in ['PASS','NA']},'unresolved_requested_edit_checks':[],'product_tests_executed':False,'actual_QCC_data_still_unconfirmed':6}
(Q/'policy-review.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'status':result['status'],'counts':result['counts']},ensure_ascii=False))
