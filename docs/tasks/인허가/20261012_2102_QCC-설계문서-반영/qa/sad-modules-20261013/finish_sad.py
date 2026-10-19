"""Publish the reviewed SAD only, then refresh existing current records."""
from pathlib import Path
from zipfile import ZipFile
import hashlib,json,shutil,datetime,subprocess,sys,re
QA=Path(__file__).resolve().parent;TASK=QA.parent.parent
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def save(p,v):Path(p).write_text(json.dumps(v,ensure_ascii=False,indent=2),encoding='utf-8')
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
stamp=datetime.datetime.now().astimezone().isoformat()
baseline=read(QA/'source-manifest.json');plan=read(QA/'authored.json')
assert all(v is True for k,v in plan['checks'].items() if isinstance(v,bool))
assert plan['checks']['all_tables_raw_XML_byte_identical']
native=read(QA/'native-checks.json');after=next(x for x in native if x['mode']=='after')
visual=read(QA/'visual-review.json');assert visual['status']=='PASS'
revision=read(QA/'native-projection-checks.json')
assert all(x['nativeAcceptMatchesCurrent'] and x['nativeRejectMatchesBefore'] for x in revision)
assert after['allNativeTableTextPreserved'] and after['outsideTableModuleIdCount']==0
assert after['sha256'].upper()==sha(plan['candidate'])==plan['candidate_sha256'].upper()
assert visual['candidate_sha256'].upper()==sha(plan['candidate'])
protected=read(TASK/'evidence/source-baseline.json')
for f in baseline['files']:assert sha(f['path'])==f['sha256'].upper(),f['code']
for f in protected:assert sha(f['path'])==f['sha256'].upper(),f['path']
source=Path(plan['source']);assert sha(source)==plan['source_sha256'].upper()
backup=QA/'prepublish-backup.docx'
if backup.exists():assert sha(backup)==sha(source)
else:shutil.copy2(source,backup)
shutil.copyfile(plan['candidate'],source)
assert sha(source)==sha(plan['candidate'])
published={'checked_at':stamp,'changed_codes':['SAD'],'files':[], 'protected_originals':[]}
for f in baseline['files']:
    expected=plan['candidate_sha256'] if f['code']=='SAD' else f['sha256']
    assert sha(f['path'])==expected.upper()
    published['files'].append({'code':f['code'],'path':f['path'],'sha256':sha(f['path']),
        'previous_sha256':f['sha256'],'changed':f['code']=='SAD'})
for f in protected:
    assert sha(f['path'])==f['sha256'].upper()
    published['protected_originals'].append({'path':f['path'],'sha256':f['sha256'],'unchanged':True})
save(QA/'published.json',published)
subprocess.run([sys.executable,str(TASK/'collect_positions.py'),'--record',str(QA/'current-positions.json'),'--clean-main'],check=True)
positions=read(QA/'current-positions.json')
manifest=baseline
manifest['status']='SAD QCC module headings and matching TOC entries use module names without module IDs; trace tables and other documents preserved'
manifest['checked_at']=stamp
manifest['scope']='Current 12 E8300-QCC DHF documents; this follow-up changes SAD only. Eleven other combined documents and 23 protected originals unchanged.'
manifest['current_location_count']=sum(len(x['entries']) for x in positions)
for f in manifest['files']:
    f['changed_from_current_baseline']=f['code']=='SAD'
    if f['code']!='SAD':continue
    f['previous_followup_sha256']=f['sha256'];f['sha256']=sha(source)
    f['native_previews']=[{'pdf':after['pdf'],'pdf_sha256':sha(after['pdf']),'pages':after['pages'],
      'source_sha256':sha(source),'checked_at':stamp,'scope':'Actual Word read-only full accepted preview; full-history deliverable preserved',
      'candidate_matches_published_bytes':True}]
    f['changes_from_previous_followup']={'scope':'Four QCC module heading ID prefixes and four matching TOC labels only',
      'all_tables_XML_preserved':True,'all_native_table_text_preserved':True,'module_and_unit_trace_IDs_preserved':True,
      'other_package_parts_preserved':True,'new_change_author':'김은수'}
manifest['SAD_module_name_followup']={'module_ids_removed_from_headings':['SI-ID-QCC-001','SI-ID-QCC-002','SI-ID-QCC-003','SI-ID-QCC-004'],
 'heading_count':4,'toc_label_count':4,'module_and_unit_ids_retained_in_trace_tables':True,
 'source_snapshot':str(QA/'source-manifest.json'),'no_other_DHF_file_changes':True,
 'no_new_product_execution_or_approval':True}
manifest['evidence']['previous_followup_manifest']=str(QA/'source-manifest.json')
manifest['evidence']['SAD_module_names_current']={
    'authored':str(QA/'authored.json'),'native':str(QA/'native-checks.json'),
    'new_revision_native_QA':str(QA/'native-projection-checks.json'),
    'visual':str(QA/'visual-review.json'),'policy':str(QA/'policy-review.json'),'published':str(QA/'published.json')}
manifest['evidence']['current_positions']=str(QA/'current-positions.json')
manifest['evidence']['current_followup_scope']='SAD only; evidence for previous documents remains available through previous_followup_manifest. Other eleven document hashes are unchanged.'
for key,name in {'current_package_checks':'authored.json','native_word_acceptance':'native-checks.json','new_word_revision_QA_projection':'native-projection-checks.json','visual_review':'visual-review.json','policy_review':'policy-review.json','published_hashes_and_protected_files':'published.json','saved_G_checks':'published.json'}.items():
    manifest['evidence'][key]=str(QA/name)
save(TASK/'evidence/final-output-manifest.json',manifest)

policy={'checked_at':stamp,'scope':'SAD module label formatting only','status':'PASS_SCOPED','checks':[]}
applies={1:'기능/역할은 원문 그대로이며 QCC도 같은 모듈 명칭 체계로 유지',
3:'설명 제목에서는 모듈 ID를 제거하고 두 추적표의 모듈/유닛 ID와 XML을 보존',
4:'현재 G 문서와 E8300 동종 제목, 실제 목차/모듈 본문 확인',
6:'제목과 목차를 함께 정리하고 상세설계 추적표 및 다른 DHF 11종의 보존 대조',
7:'E8300과 같은 모듈 이름 표기; 본문 역할 설명 및 스타일/표 폭/그림 불변',
22:'기존 미비 안내 전체 문구/서식/담당 접두어 보존',
23:'G 저장본의 현재 본문에서 중앙 위치 목록 재수집',
26:'새 삭제/삽입 추적과 기존 이력 보존, 새변경 XML 거부 복원 및 QA 사본의 실제 Word 수락/거부 확인',
27:'SAD 같은 경로/접두어 유지; 나머지 현재본11/보호원본23 불변; 신규 서명/승인/전역 설정 변경 없음',
28:'저장본은 검증 후보와 같은 SHA256; 실제 Word 전체 표시/표 보존/목차 정합 확인',
29:'기존 문단/런/번호/스타일/필드/추적표와 그림 보존; 새 모듈 이름도 같은 서식 유지'}
for n in range(1,30):policy['checks'].append({'principle':n,'applies':n in applies,
  'verdict':'통과' if n in applies else '해당없음',
  'evidence':applies.get(n,'모듈 이름 앞의 ID 표기만 정리하므로 요구사항 내용, 실행 구조/상세 구현, 시험 조건·수치·결과·사진·날짜 또는 Excel을 변경하지 않음')})
save(QA/'policy-review.json',policy)
task=TASK/'작업.md';text=task.read_text(encoding='utf-8-sig')
begin='<!-- TDC-TASK-STATUS:BEGIN v1 -->';end='<!-- TDC-TASK-STATUS:END -->'
status='''<!-- TDC-TASK-STATUS:BEGIN v1 -->
| 항목 | 내용 |
|---|---|
| 상태 | 완료(SAD의 QCC 모듈 설명 표기 정리) |
| 현재 결과 | QCC 모듈 제목4곳과 목차4곳의 모듈 ID 접두어 제거. 모듈 이름/역할 및 상세설계 추적표의 모듈·유닛 ID 보존 |
| 남은 일 | 이번 SAD 정정 없음. 기존 미비 사항과 별도 배포 설명서 최종 판정 대기는 유지 |
| 다음 행동 | 이번 수정에 필요한 추가 행동 없음 |
| 다음 담당 | 없음 |
| 확인 시각 | '''+stamp+''' |
<!-- TDC-TASK-STATUS:END -->'''
text=text[:text.index(begin)]+status+text[text.index(end)+len(end):]
text+='''

## 2026-10-13 후속 정리: 아키텍처의 QCC 모듈 이름

은수님 요청에 따라 SAD의 3.4.3~3.4.6 모듈 제목4곳과 같은 목차4곳에서 SI-ID-QCC-001~004 접두어만 제거했다. 시스템 관리/HW 인터페이스/블루투스/보안 모듈의 이름과 역할 설명은 보존했다. 모듈 ID는 상세설계에서 부여하고 아키텍처의 상세설계 추적표에서 관련 모듈/유닛을 연결한다는 원칙을 SV 지침3번에도 반영했다. 상세설계 추적 테이블을 포함한 모든 표의 원전 XML과 실제 Word 수락 후 표 내용을 대조했고 모듈/유닛 ID는 그대로다.

qcc_design은 현재 SAD와 E8300 동종 제목을 조사하고 8개 표기를 수정하는 스크립트 초안을 준비했다. 주 작업은 스크립트를 보완·실행하여 후보와 증거를 생성하고 실제 Word 검증, 표시 검토, G 저장과 공통 기록을 맡았다. 일반 역할 설명이나 실행 구조, 그림, 상세설계 및 시험 문서는 수정하지 않았다. 새 변경의 XML 거부 복원, 기존 이력/스타일/번호/그림/필드 보존을 확인했다. 과거 이력을 수락한 검증용 문단에서 새 변경의 실제 Word 수락/거부도 확인했으며 기존 이력이 포함된 납품 문서의 선택적 COM 거부가 검증됐다고 확대하지 않는다.

Windows 번들에는 LibreOffice가 없어 packaged render_docx.py는 실패했다. 설치된 LibreOffice로 대체하지 않고 실제 Microsoft Word의 읽기 전용 수락 보기에서 전후 전체 PDF를 출력해 모든 쪽을 렌더했다. 이전 검증본과 동일한 페이지는 픽셀 동일성을 대조하고 달라진 페이지는 원본 크기로 열어 글자/목차/제목/페이지 배치를 확인했다. 판정과 실제 페이지 번호는 아래 표시 검사에 기록했다. 전역 Word 사용자 설정은 변경하지 않았다.

검증한 후보와 같은 SHA256으로 G의 SAD 현재본만 저장했다. 다른 통합 문서11개와 보호 원본23개는 불변이다. 중앙 QCC 위치를 저장본에서 다시 수집하고 현재 산출물 목록을 갱신했다. 기존 제품 시험 대기/미비 상태와 사용설명서 초안 검사 범위는 그대로다.

근거: [수정 범위와 패키지 검사](qa/sad-modules-20261013/authored.json), [실제 Word 검사](qa/sad-modules-20261013/native-checks.json), [새변경 수락/거부 QA](qa/sad-modules-20261013/native-projection-checks.json), [전체 표시 검사](qa/sad-modules-20261013/visual-review.json), [29개 원칙 대조](qa/sad-modules-20261013/policy-review.json), [게시/원본 보존](qa/sad-modules-20261013/published.json).
'''
task.write_text(text,encoding='utf-8')
print(json.dumps({'published':'SAD','other_combined_unchanged':11,'protected_unchanged':23,'locations':manifest['current_location_count']}))
