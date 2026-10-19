import package as m
from pathlib import Path
import json,re,os
H=m.HERE
pl=m.readj('style-plan.json');v=m.readj('style-validation.json');pub=m.readj('publication.json')
candidate=H/'style-after.xlsx';sha=m.digest(candidate);target=Path(pub['output'])
assert not v['errors'] and sha==v['candidateSHA']
native=m.readj('style-native-export.json');layout=m.readj('style-layout-final.json');content=m.readj('style-content-final.json');photo=m.readj('style-photo-final.json')
assert native['source_unchanged'] and native['closed_without_saving'] and sha in json.dumps(native)
assert content['status'] in ['PASS','통과'],content['status']
assert layout['status'] in ['PASS','통과'],layout['status']
assert photo.get('current_status',photo['status']) in ['PASS','통과']
assert sha in json.dumps(content) and sha in json.dumps(layout) and sha in json.dumps(photo)
assert m.digest(target)==pl['baselineSHA'],'The current deliverable changed; preserve user edits before publishing.'
sources=[target.with_name('[E8300-QCC] SSTR-FW-TD24A 소프트웨어 시스템 시험 보고서.xlsx'),target.with_name('[김은수 전용] SSTR-FW-TD24A 소프트웨어 시스템 시험 보고서.xlsx')]
sourceHashes={str(s):m.digest(s) for s in sources}
assert sourceHashes[str(sources[1])]==pl['personalSHA']
tmp=target.with_name(target.name+'.style.tmp');assert not tmp.exists()
with tmp.open('xb') as f:f.write(candidate.read_bytes());f.flush();os.fsync(f.fileno())
assert m.digest(tmp)==sha and m.digest(target)==pl['baselineSHA']
os.replace(tmp,target)
assert m.digest(target)==sha and {str(s):m.digest(s) for s in sources}==sourceHashes
b=m.Book(target);loc={}
for sh,cs in b.cells.items():
 if sh.endswith('_비교'):continue
 for a,c in cs.items():
  text=b.value(a,sh)
  for prefix in ['[E8300 보완]','[QCC/E8300 자료 입력]','[QCC 자료 입력]']:
   if prefix in text:loc.setdefault(prefix,{}).setdefault(sh,[]).append(a)
pub.setdefault('history',[]).append({'outputSHA256':pub['outputSHA256'],'userSavedBaselineSHA256':pl['baselineSHA'],'scope':'핑크 시험 전체 내용 및 서식 정리 전'})
pub.update(outputSHA256=sha,sourcesUnchanged=True,styleRevision='34~293행 10pt/외곽 빈 줄 범위 제한/핑크70 시험 전체 내용 반영')
m.savej('publication.json',pub)
state=m.readj('final-state.json');state.update(previousSHA=state['candidateSHA'],candidateSHA=sha,prefixLocations=loc,styleValidation='style-validation.json',pinkFullContentTests=pl['pinkIDs']);m.savej('final-state.json',state)
result={'output':str(target),'beforeSHA':pl['baselineSHA'],'afterSHA':sha,'sourcesBeforeAndAfter':sourceHashes,'backup':str(H/'style-before.xlsx'),'photoCount':123,'pinkTestCount':70,'bodyRange':'보고서_시스템 시험!34:293','substantiveChanges':len(pl['contentChanges']),'comparisonRows':[pl['headerRow'],pl['lastRow']],'saved':True,'checks':{'structure':'style-validation.json','content':'style-content-final.json','photos':'style-photo-final.json','layout':'style-layout-final.json','native':'style-native-export.json'}}
m.savej('style-publication.json',result)

root=Path(r'E:/workspace/projects/sound1-fw-e8300-backup');reg=root/'docs/참고/인허가/사운드1/QCC 자료 추가 위치 관리.md';body=reg.read_text(encoding='utf-8');a=body.index('<!-- SSTR-PHOTO-20261016:BEGIN -->');z=body.index('<!-- SSTR-PHOTO-20261016:END -->',a)
section=body[a:z]
section=section.replace('김은수 전용 저장본의 녹색 밑줄 수정과 지정 삭제를 반영한 별도 사본이다.','김은수 전용 저장본의 수정과 지정 삭제를 반영한 별도 사본이다. 후속 요청으로 핑크색 확인 완료 70개 시험의 내용/준비/제한/절차/통과 기준 전체를 반영했다.')
section=section.replace('시험7 오류 LED 자료 요청과 사진51/54/73/74/77/80의 RAM·미기동·합성 입력·시험 파일·실제 날짜 범위를 보존했다.','핑크 시험 결과의 [E8300 보완] 문단과 [QCC/E8300 자료 입력] 항목은 사용자 지시로 제거했다. 시험7 K51의 보완 문단도 제거 대상이며, K53 및 사진51/54/73/74/77/80의 일반 범위 설명은 보존했다.')
add='''
후속 서식 정리: `보고서_시스템 시험!34:293`만 위/아래 빈 줄 한 개와 10pt를 적용하고, 그 밖의 모든 셀/시트에서 외곽 빈 줄을 제거했다. 핑크 시험의 녹색 밑줄을 검정색/밑줄 없음으로 정리했으며 행/병합/83개 ID 및 사진123장/8cm/세부 순서를 보존했다. 본문과 연결된 보조자료도 맞췄다. 이 요청의 10pt 및 지정 안내 삭제는 해당 사본 범위의 명시 예외다.

시험15/16/69/74/78은 개인본에 기재된 동일 시험 범위와 판정으로 맞췄다. 시험54와59의 QCC 추가 검증 대기, 시험75의 `확인`, 비핑크의 미비 사항은 유지한다. 단순 사진 첨부나 안내 제거를 새 제품 시험 통과로 보지 않는다. 최종 확인: 2026-10-16, [저장 및 검증 근거](../../../tasks/인허가/20261012_2102_QCC-설계문서-반영/qa/photo-attach-20261016/style-publication.json).

'''
# Use the exact repository-relative link from this registry's directory.
add=add.replace('../../../tasks/','../../../tasks/')
section=section.replace('아래 위치는 최종 저장본의 현재 시트에서 직접 검색했다.',add+'아래 위치는 최종 저장본의 현재 시트에서 직접 검색했다.')
section=re.sub(r'^\| SSTR-PHOTO-.*\n','',section,flags=re.M)
rows=[]
for i,(prefix,sheets) in enumerate(loc.items(),1):
 pos='; '.join('`'+sh+'!'+', '.join(cells)+'`' for sh,cells in sheets.items())
 rows.append(f'| SSTR-PHOTO-{i:02d} | `{prefix}` | {pos} | 지정 핑크 안내 제거 후 남은 실제 표시 / 2026-10-16 |')
section=section.replace('|---|---|---|---|\n','|---|---|---|---|\n'+'\n'.join(rows)+'\n',1)
reg.write_text(body[:a]+section+body[z:],encoding='utf-8')

task=H.parent.parent/'작업.md';body=task.read_text(encoding='utf-8')
status='''<!-- TDC-TASK-STATUS:BEGIN v1 -->
| 항목 | 내용 |
|---|---|
| 상태 | 자료첨부 사본의 핑크 시험 전체 내용/서식 재정리 완료 |
| 현재 결과 | 핑크70개 시험 내용·기준 반영, 본문34~293행10pt/빈 줄, 그 외 외곽 빈 줄 제거. 사진123장/8cm/순서/셀 수용 재검사 통과 |
| 남은 일 | 이번 편집은 완료. 기존 순수 QCC 미비 및 정식 기준서와의 동등성 검토는 해당 기록 유지 |
| 다음 행동 | 후속 자료 또는 수정 요청에 따라 현재 _자료첨부 사본에서 계속 작업 |
| 다음 담당 | 사용자 요청 / Codex |
| 확인 시각 | 2026-10-16 |
<!-- TDC-TASK-STATUS:END -->'''
body=re.sub(r'<!-- TDC-TASK-STATUS:BEGIN v1 -->.*?<!-- TDC-TASK-STATUS:END -->',lambda _:status,body,count=1,flags=re.S)
entry=f'''
## 2026-10-16 자료첨부 사본 재정리: 핑크 시험 전체 내용과 셀 서식

은수님이 지정한 `_자료첨부`만 수정했다. 작업 시작 시 기존 게시 후 사용자가 제목4셀의 빈 줄을 정리한 현재 저장본 SHA `{pl['baselineSHA']}`를 새 기준으로 잡아 보존했다. 개인전용 원본은 읽기만 했고 기존 복구본을 덮어쓰지 않았다. 본 수정 전 복구본은 `qa/photo-attach-20261016/style-before.xlsx`다.

- 핑크 TEST ID 70개의 C:J 의미 앵커687개를 전체 대조/반영했다. 녹색 외 일반 본문도 포함한다. 삭제된25조건과 사진 추가 행/공통 셀 병합은 유지했다. 시험28 I158/K159의 ON, 시험32 J170/K170의2→3, 시험38 K179의35참조를 보존했다. 개인본의 명백한 오타2곳과 삭제조건40/73의 잔존 절차를 재정리했다.
- 결과의 지정 E8300 보완 문단 및 공동 QCC 항목을 제거하고 핑크의 녹색 밑줄을 검정/밑줄 없음으로 맞췄다. K51의 E8300 보완 문단 전체 제거는 사용자 명시 범위다. 일반 사진 범위 설명은 유지했다. 원본 관찰196조건이 빠지지 않았고 본문/보조자료224조건을 대조했다.
- 본문34~293행 사용셀2967개와 리치텍스트를10pt로 맞췄다. 내용1109셀에 위/아래 빈 줄 한 개를 적용하고 다른 셀/7시트의 외곽 빈 줄을 제거했다. 사진/비사진 행 높이와 사진 상단 위치를 계산한 뒤 실제 Excel에서 확인했다. 사진123개 bytes/이름/시험·검증·세부순서/8cm/종횡비/셀 수용과 기존 그림은 보존됐다.
- 개인본 범위로 돌아간15/16/69/74/78의 판정은 원본 시험자 기록으로 맞췄다. 순수 QCC 미비가 남은54/59 대기와75의확인을 보존했다. 보조 안내D212/D235의 옛 통과 표시를 본문과 맞췄다. 비핑크11개 및 개인본에 없는82/83의 내용은 변경하지 않았다.
- 본문은 적용된 상태이며 내용변경{len(pl['contentChanges'])}건과 서식 범위는 기존 `_비교` 시트{pl['headerRow']}~{pl['lastRow']}행에 기록했다. 앞선 비교 이력은 문구/수식/강조를 보존하고 요청한 외곽 빈 줄만 제거했다.

| SV 원칙 | 판정 | 이번 저장본의 근거와 재검사 |
|---|---|---|
| 1,3,4 | 통과 | 단일 보고서/기존83ID·순서·상위 추적 보존, 핑크70의 C:J687앵커를 개인 저장본과 실제 대조. [독립 내용검사](qa/photo-attach-20261016/style-content-final.json) |
| 2,5,8 | 해당없음 | SRS/입출력 매트릭스/구조/상세설계를 새로 작성하거나 구현 변경하지 않음 |
| 6 | 통과(이번 범위) | 본문→보조자료→판정 반복대조. 두 보조 안내의 불일치를 수정 후 재검사. 다른 산출물은 이번 요청 범위 밖 |
| 7,9,10,11,13 | 통과(전사/정리) | 개인본의 시험 내용/방법/조건/기준으로 반영. 명백한 오타·삭제조건 잔여문구 및28/32정정 재검사. 새로운 방법/측정값을 추정하지 않음 |
| 12 | 미실행 | 별도 기준서 전체와의 동등성은 이번 보고서 전용 수정 범위 밖. 기존 사진용 행/병합 및 지정 삭제 유지 |
| 14,15,16,17,18,19,20,21 | 통과(보존/전사) | 관찰196조건, 판정70개 정책, 원본사진123개/실제날짜·RTT범위 보존. 실제 제품 시험이나 승인 새로 실행하지 않음. [사진 전수 검사](qa/photo-attach-20261016/style-photo-final.json) |
| 22,23 | 통과(명시 예외 적용) | 지정 핑크 안내만 제거,34~293행10pt는 이번 명시 지시 우선. 순수 QCC 미비 및 비핑크 보존, 중앙 현재 위치 목록 갱신 |
| 24,25 | 통과 | 몸체만 외곽 빈 줄/10pt, 모든 외부 셀 패딩 없음. 현재본문과 `_비교` 분리, 실제정렬/병합/사진별 행높이 검사. [구조·서식 검사](qa/photo-attach-20261016/style-validation.json) |
| 26,29 | 해당없음 | Word 수정 없음. Excel 요청 범위임 |
| 27,28 | 통과(문서) | 원본 SHA불변, 현재파일 변경 감지 후 저장, 최종 XLSX 실제Excel읽기전용 열기/출력 및 시각검사. [네이티브 확인](qa/photo-attach-20261016/style-native-export.json), [시각 검토](qa/photo-attach-20261016/style-layout-final.json), [저장](qa/photo-attach-20261016/style-publication.json) |

최종 SHA256 `{sha}`. 구조/내용/사진/시각 검토의 검사 범위와 미실행 범위는 각 QA에 구분했다. 문서 정리 완료는 새 제품 시험 통과나 정식 승인을 뜻하지 않는다.
'''
task.write_text(body+entry,encoding='utf-8')
assert target.exists() and m.digest(target)==sha
print(json.dumps(result,ensure_ascii=True))
