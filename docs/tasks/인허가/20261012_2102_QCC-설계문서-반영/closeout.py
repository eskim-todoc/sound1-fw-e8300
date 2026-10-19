"""Close the authorized documentation stage after saved-file and independent QA pass."""
from pathlib import Path
from urllib.parse import quote,unquote
from datetime import datetime,timezone,timedelta
import hashlib,json,re,sys
sys.stdout.reconfigure(encoding='utf-8')
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
checks=json.loads((HERE/'qa/saved-package-checks.json').read_text(encoding='utf-8'))
assert not checks['failures'] and len(checks['files'])==12
assert all(i['unchanged']for i in checks['originals'])and len(checks['originals'])==23
for file in checks['files']:
 assert hashlib.sha256(Path(file['path']).read_bytes()).hexdigest().upper()==file['sha256']
design=json.loads((HERE/'qa/cross-design-native.json').read_text(encoding='utf-8'))
assert design['status']=='PASS',design['status']
notice=json.loads((HERE/'qa/current-positions.json').read_text(encoding='utf-8'))
registry=ROOT/'docs/참고/인허가/사운드1/QCC 자료 추가 위치 관리.md'
content=registry.read_text(encoding='utf-8')
section=content.split('<!-- E8300-QCC-CURRENT:BEGIN -->')[1].split('<!-- E8300-QCC-CURRENT:END -->')[0]
links=re.findall(r'\]\((G:/[^)]+)\)',section)
assert len(set(links))==12
assert all(Path(unquote(link)).is_file()for link in links)
stamp=datetime.now(timezone(timedelta(hours=9))).isoformat(timespec='seconds')
manifest={'status':'Documentation draft complete; product/QCC tests not executed','checked_at':stamp,'files':checks['files'],'originals_unchanged':23,'requirements':83,'qcc_modules':4,'qcc_units':14,'test_ids':{'unit':130,'integration':24,'system':83},'new_conditions':{'unit':38,'integration':14,'system':3},'common_columns_and_merges':'PASS','native_open_and_visual_QA':'PASS','word_pages':{'SR':11,'SGR':10,'SAD':47,'SDDD':166,'SIP':16},'registered_current_notice_locations':sum(len(r['entries'])for r in notice),'registry':str(registry),'matrix_edit':False,'firmware_edit':False,'new_test_passes':False,'evidence':['qa/saved-package-checks.json','qa/cross-requirements-word.json','qa/cross-design-native.json','qa/final-word/SAD-candidate-proof.json','evidence/qcc_tests_native_v2.json','qa/qcc_requirements/saved-body-validation.json'],'known_tool_limits':['Bundled LibreOffice unavailable; isolated read-only native Office PDF/PNG used.','Word TOC terminal paragraph formatting revision unavailable through COM Revisions item; QA candidate accepted/rejected body text, drawings and author/date change text verified unchanged before publishing field cache.'],'remaining_product_evidence':'See central registry for QCC implementation/build, pin/power/BLE/auth/audio/OTA contracts, measured logs/photos and RID83 manual/upper trace.'}
(HERE/'evidence/final-output-manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
path=HERE/'작업.md';task=path.read_text(encoding='utf-8')
start=task.index('<!-- TDC-TASK-STATUS:BEGIN v1 -->');end=task.index('<!-- TDC-TASK-STATUS:END -->')+len('<!-- TDC-TASK-STATUS:END -->')
status='''<!-- TDC-TASK-STATUS:BEGIN v1 -->
| 항목 | 내용 |
|---|---|
| 상태 | 완료 |
| 현재 결과 | 원본을 보존하고 같은 G 단계 경로에 E8300-QCC 초안12개 작성·정합/저장/표시검증 완료. 83요구사항, QCC4모듈/14유닛 및 새 유닛15그룹/38조건·통합7그룹/14조건 반영 |
| 남은 일 | 이번 문서 반영 범위의 필수 작업 없음. 실제 QCC 구현/실측/시험과 상위 RID83 자료는 본문과 중앙 목록의 자료 대기로 유지 |
| 다음 행동 | 자료가 추가되면 중앙 QCC 목록의 안정된 ID/제목/시트 위치를 따라 설계 입력부터 다시 대조하고 같은 기준/보고 행을 갱신 |
| 다음 담당 | 은수님 / QCC 자료 담당 |
| 확인 시각 | '''+stamp+''' |
<!-- TDC-TASK-STATUS:END -->'''
task=task[:start]+status+task[end:]
lines=['','## 최종 저장본과 확인','', '2026-10-12: 요청한 QCC 문서 반영 초안을 완료했다. 제품 시험·새 승인 완료와 구분한다. 전체 파일/현재 SHA256은 [최종 산출물 목록](evidence/final-output-manifest.json), 현재 미비 위치는 [중앙 QCC 목록](../../../참고/인허가/사운드1/QCC%20자료%20추가%20위치%20관리.md)의 E8300-QCC 현재본 절을 사용한다.','', '| 문서 | 현재 G 저장본 |','|---|---|']
order=['SRS','SR','SGR','SAD','SSTC','SSTR','SDDD','SUVC','SUVR','SIP','SITC','SITR']
for f in sorted(checks['files'],key=lambda f:order.index(f['code'])):
 lines.append(f"| {f['code']} | [{Path(f['path']).name}]({quote(f['path'].replace(chr(92),'/'),safe='/:')}) |")
lines += ['', '최종 확인 결과:', '', '- 기존 E8300/QCC 원문23개의 SHA256 불변. 새12파일 XML/본문 식별자/원본 그림 유지와 의도한 새 도식의 변경 추적을 확인했다.', '- 기준/보고 공통 셀과 병합 일치. 새38/14/3조건은 미실행·판정 대기. 333개 Excel 미비 rich run 및56개 Word 안내의 전체 설명 서식을 확인했다.', '- Word5개250페이지의 전체 렌더와 수정/긴표를 검토했다. 마지막 그림 변경은 SDDD12p/SIP5p뿐이고 나머지180페이지가 이전 검토본과 같은 픽셀이다. SAD 목차/서론/마지막 페이지는47p 최신본에서 다시 확인했다.', '- Excel7개는 실제 native 열기와 수정 영역 표시를 확인했다. SSTR 보조 시트의 mergeCells/phoneticPr 순서 오류, SRS 상위ID 넘침, 유닛/통합 그룹ID 페이지 잘림, Word 한 줄 고아/빈페이지/포괄적 SOUP 완료 문구를 표적 수정하고 재검증했다.', '- Word 필드 갱신의 COM 한계는 저장 후보의 수락/거부 본문·그림 및 작성자/일시별 변경 텍스트 동등성으로 확인했다. 기존 트랙과 실제 새 삽입/삭제, 과거 E8300 관찰 범위는 유지했다.', '- 중앙 목록389개 현재 위치와12개 링크를 재확인했다. 목차/삭제행/보조 이력의 원본 복제행을 현재 삽입 위치로 세지 않았다. 구조 근거72개 소스의 차이가 없으며 펌웨어/시험코드/Git/온라인 매트릭스는 수정하지 않았다.', '', '후속 제품 자료는 문서의 담당별 미비 안내를 해소할 때 사용한다. 핀 극성·충전 상태 변환·BLE/CRA/키·좌우/TWS·마이크 송신·OTA의 미확정 계약과 실제 결과를 임의로 통과 처리하지 않는다.']
if '## 최종 저장본과 확인' in task:task=task.split('## 최종 저장본과 확인')[0].rstrip()
path.write_text(task.rstrip()+'\n'+'\n'.join(lines)+'\n',encoding='utf-8')
contract=HERE/'evidence/integration-contract.json';c=json.loads(contract.read_text(encoding='utf-8'));c['phase']='documentation-draft-complete';c['checked_at']=stamp;c['final_manifest']='final-output-manifest.json';contract.write_text(json.dumps(c,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'status':'complete','outputs':12,'originals_unchanged':23,'current_locations':manifest['registered_current_notice_locations'],'checked_at':stamp},ensure_ascii=False))
