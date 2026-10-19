from pathlib import Path
from urllib.parse import quote,unquote
from datetime import datetime,timezone
from zipfile import ZipFile
from lxml import etree as E
import json,hashlib,re
Q=Path(__file__).resolve().parent;TASK=Q.parent.parent;ROOT=TASK.parents[3]
def read(p):return json.loads(Path(p).read_text('utf-8-sig'))
def save(p,v):Path(p).write_text(json.dumps(v,ensure_ascii=False,indent=2),encoding='utf-8')
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
pub=read(Q/'published.json');assert pub['status'].startswith('PASS')
pos=read(Q/'current-positions.json');manifest=read(TASK/'evidence/final-output-manifest.json')
stamp=datetime.now(timezone.utc).astimezone().isoformat()
registry=ROOT/'docs/참고/인허가/사운드1/QCC 자료 추가 위치 관리.md'
registry_text=registry.read_text('utf-8-sig')
section=registry_text.split('<!-- E8300-QCC-CURRENT:BEGIN -->')[1].split('<!-- E8300-QCC-CURRENT:END -->')[0]
checks=[]
for d in pub['published']:
 code=d['code'];assert Path(d['path']).exists() and not Path(d['source']).exists()
 assert sha(d['path'])==d['sha256']
 assert quote(str(d['path']).replace('\\','/'),safe='/:') in section
 record=next(r for r in pos if r['code']==code)
 assert Path(record['path'])==Path(d['path'])
 red=[x for x in record['entries'] if x['prefix']=='QCC 반영 내용(굵은 빨강)']
 pending=[x for x in record['entries'] if x['prefix']=='접두어 해제 후 미확정']
 assert len(red)==(7 if code=='SR' else 5) and len(pending)==(4 if code=='SR' else 2)
 assert not any(x['prefix'] in ('[QCC 자료 입력]','[QCC/E8300 자료 입력]') for x in record['entries'])
 checks.append({'code':code,'published_sha256':sha(d['path']),'matches_verified_file':True,'registry_link_exists':True,'red_locations':len(red),'unconfirmed_locations':len(pending),'old_name_absent':True})
for f in pub['preserved']:assert sha(f['path'])==f['after_sha256']
for r in pos:assert Path(r['path']).exists()
manifest['current_location_count']=sum(len(r['entries']) for r in pos)
manifest['input_word_style_followup']['post_publish_check']=str(Q/'post-publish-checks.json')
save(TASK/'evidence/final-output-manifest.json',manifest)

p=TASK/'작업.md';text=p.read_text('utf-8-sig')
for d in pub['published']:
 old=Path(d['source']);new=Path(d['path'])
 text=text.replace('['+old.name+']('+quote(old.as_posix(),safe='/:')+')','['+new.name+']('+quote(new.as_posix(),safe='/:')+')')
status='''<!-- TDC-TASK-STATUS:BEGIN v1 -->
| 항목 | 내용 |
|---|---|
| 상태 | 완료(설계 입력 SR/SGR Word 서식 및 QCC 안내 정리) |
| 현재 결과 | 두 문서를 TDC 공통 형식으로 정리해 같은 G 폴더에 _완료 이름으로 저장. QCC 안내 6곳 정리/실제 내용 12곳 굵은 빨강, 각각 11쪽 검사 통과 |
| 남은 일 | 이번 편집 범위 없음. 미확정 QCC 버전/사용량/설정/검토 결과와 실제 제품 검증은 본문/중앙 목록의 대기 상태 유지 |
| 다음 행동 | 후속 자료가 제공되면 해당 미확정 사실과 영향 문서/시험 조건을 갱신 |
| 다음 담당 | 사용자(후속 자료), Codex(지정된 후속 문서 작업) |
| 확인 시각 | '''+stamp+''' |
<!-- TDC-TASK-STATUS:END -->'''
text=re.sub(r'<!-- TDC-TASK-STATUS:BEGIN v1 -->.*?<!-- TDC-TASK-STATUS:END -->',lambda m:status,text,count=1,flags=re.S)
text+='''

## 2026-10-13 설계 입력 Word 서식과 QCC 안내 정리

은수님의 이번 요청은 설계 입력의 `[E8300-QCC]` SR/SGR Word 두 문서에 적용했다. 실제 TDC DOTX의 제목/본문/표 스타일을 기준으로 제목 번호의 굵기, 수준별 들여쓰기, 본문 10pt/1.25배/뒤 8pt, 꼭지와 검증 질문, 표 및 그림 배치를 정리했다. 목차를 갱신하고 SR의 페이지 번호를 문서 전체와 일치시켰다. SGR 검토에서 새 빈 페이지를 발견하여 개정이력 뒤의 수동 나눔을 실제 서론 제목으로 옮겼고, 재출력한 최종 두 문서의 각 11쪽을 전부 확인했다.

제공된 QCC 자료와 현재 입력/설계 문서를 직접 대조했다. QCC/공동 입력 요청 안내 6곳은 제거하고, 그중 아직 확인되지 않은 버전/사용량/설정/검토 결과를 일반 검정 본문의 구체적인 미확정 설명으로 남겼다. 확인된 마이크/오디오/통신/개발 구성 및 메모리 범위 요구 등의 관련 실제 내용 12곳은 굵은 빨강(`#C00000`), 본문 10pt로 표시했다. 메모리의 요구 범위를 실제 사용량 측정으로 바꾸거나 새 시험 결과를 만들지 않았다. 기존 E8300 보완 표시는 유지했다. 이 사용자 해제 범위는 SV 원칙22와 중앙 QCC 목록에 기록했다.

완료 파일은 기존 접두어를 유지하고 같은 폴더에서 `_완료`로 이름을 바꿨다. 편집 전 사본은 이 작업의 QA 폴더에 보존했다. 기존 `[E8300]`/QCC 참고 원본 23개와 다른 현재본 10개는 게시 직전/직후 SHA256을 대조하여 이번 작업에서 변경하지 않았음을 확인했다. SRS/SUVR은 이전 산출물 목록 이후의 외부 변경을 발견하여 실제 현재 해시를 기록하고 보존했다. 해당 Excel의 최신 내용이나 렌더까지 이번에 다시 검증했다고 기록하지 않았다.

본문 문구 삽입/삭제는 김은수의 실제 변경 추적으로 남겼고, 기존 문구/저자/일자/서식 변경 이력을 보존했다. 여백·꼭지·들여쓰기 등 현재 직접 서식 조정에는 중첩된 새 서식 변경 이력을 만들지 않았다. 실제 Word에서 새 안내 문구 변경의 수락/거부를 검사했고, 필드 갱신 전후의 일반 본문 수락/거부 내용과 그림/기존 문구 이력을 대조했다. 최종 저장본의 59개 TDC 스타일 및 상속을 포함한 실제 강조 서식과 native 전체 표시를 별도로 확인했다. Word 전역 사용자/이니셜은 변경하지 않았다. `_완료`는 이번 편집의 완료이며 자료 완비, 제품 시험 통과나 승인을 뜻하지 않는다.

근거: [QCC 자료 대조](qa/input-style-20261013/qcc-audit.json), [최종 저장 내용/서식](qa/input-style-20261013/content-saved-review.json), [SR 전체 표시](qa/input-style-20261013/SR-visual-final-review.json), [SGR 전체 표시](qa/input-style-20261013/SGR-visual-final-review.json), [Word 수락/거부 검사](qa/input-style-20261013/native-projection.json), [29개 원칙 적용 범위](qa/input-style-20261013/policy-review.json), [게시/원본 보존](qa/input-style-20261013/published.json), [저장 후 경로/위치 검사](qa/input-style-20261013/post-publish-checks.json).
'''
p.write_text(text,encoding='utf-8')
# Current task deliverable links must identify the new names and be reachable.
for d in pub['published']:
 assert Path(d['path']).name in text
 assert quote(Path(d['path']).as_posix(),safe='/:') in text
record={'status':'PASS_POST_PUBLISH','checked_at':stamp,'documents':checks,'all_12_current_registry_paths_exist':True,'registry_location_count':manifest['current_location_count'],'protected_originals_unchanged':23,'other_current_files_unchanged':10,'previous_manifest_external_edits_preserved':manifest['input_word_style_followup']['external_current_hash_changes_preserved'],'native_verified_pages':22,'current_task_links_updated':True,'scoped_policy22_exception_present':'2026-10-13 해제 범위' in (ROOT/'docs/참고/인허가/사운드1/SW 밸리데이션 문서 작성·검증 지침.md').read_text('utf-8-sig'),'product_test_execution':False}
save(Q/'post-publish-checks.json',record)
print(json.dumps(record,ensure_ascii=False,indent=2))
