import package as m
from pathlib import Path
import re,os,json

root=Path(r'E:/workspace/projects/sound1-fw-e8300-backup')
folder=Path(r'G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF/04_설계 검증/SW/외부기')
base=folder/'[E8300-QCC] SSTR-FW-TD24A 소프트웨어 시스템 시험 보고서.xlsx'
personal=folder/'[김은수 전용] SSTR-FW-TD24A 소프트웨어 시스템 시험 보고서.xlsx'
target=folder/'[E8300-QCC] SSTR-FW-TD24A 소프트웨어 시스템 시험 보고서_자료첨부.xlsx'
pl=m.readj('plan.json');state=m.readj('final-state.json');validation=m.readj('validation.json');native=m.readj('native-qa.json')
sha=m.digest(m.HERE/'candidate.xlsx')
assert sha==state['candidateSHA']==validation['candidateSHA256']==native['sourceSHA256']
assert not validation['errors'] and state['nativeAllInsideK'] and state['nativeAll8cm']
assert m.digest(base)==pl['baseSHA'] and m.digest(personal)==pl['personalSHA'],'An input changed; do not publish a stale result.'
if target.exists():
    assert m.digest(target)==sha,'Existing user output differs; do not overwrite.'
else:
    with target.open('xb') as f:
        f.write((m.HERE/'candidate.xlsx').read_bytes());f.flush();os.fsync(f.fileno())
assert m.digest(target)==sha
assert m.digest(base)==pl['baseSHA'] and m.digest(personal)==pl['personalSHA']
pub={'output':str(target),'outputSHA256':sha,'baseSHA256':pl['baseSHA'],'personalSHA256':pl['personalSHA'],'sourcesUnchanged':True,'saved':True}
m.savej('publication.json',pub)

link=f'[{target.name}](<{target.as_posix()}>)'
registry=root/'docs/참고/인허가/사운드1/QCC 자료 추가 위치 관리.md'
body=registry.read_text(encoding='utf-8')
section=['<!-- SSTR-PHOTO-20261016:BEGIN -->','## 시스템 시험 보고서 자료첨부 사본 — 2026-10-16','',link,'',
'이 파일은 최신 E8300-QCC 보고서에 김은수 전용 저장본의 녹색 밑줄 수정과 지정 삭제를 반영한 별도 사본이다. 원본 보고서와 작성자 전용 파일은 보존했다. 83개 시험 ID와 순서는 유지했고, 삭제한 25개 조건은 재번호하지 않았다. 시험·검증·세부 번호에 연결한 실제 사진 123장을 세로 8cm로 첨부했다. 보조자료의 현재 조건/결과/판정을 같은 본문과 맞췄으며 변경 전후는 `_비교` 시트에서 확인한다.','',
'시험28 검증2의 알림 ON/OFF와 시험32 조건2의 시작 볼륨은 확인 대기다. 해당 J/K와 L에 불일치 및 `확인 필요`를 표시했다. 시험7 오류 LED 자료 요청과 사진51/54/73/74/77/80의 RAM·미기동·합성 입력·시험 파일·실제 날짜 범위를 보존했다. 사진 첨부, 작성자 확인 기록, 제품 시험 통과와 정식 승인은 서로 다른 상태다.','',
'아래 위치는 최종 저장본의 현재 시트에서 직접 검색했다. `_비교` 이력의 삭제/원문은 제외했다. 같은 접두어의 보존과 실제 자료 대기 여부는 해당 문구로 구분한다.','',
'| 식별 | 담당 접두어 | 현재 검색 위치 | 상태/확인일 |','|---|---|---|---|']
for i,(prefix,sheets) in enumerate(state['prefixLocations'].items(),1):
    pos='; '.join('`'+sh+'!'+', '.join(cells)+'`' for sh,cells in sheets.items())
    section.append(f'| SSTR-PHOTO-{i:02d} | `{prefix}` | {pos} | 본문 표시 유지, 실제 결과/대기는 해당 셀 확인 / 2026-10-16 |')
section+=['','기준서 원본은 이번 사본 작성 요청의 수정 대상이 아니다. 사진용 결과 행과 지정 삭제를 적용한 이 사본을 정식 기준서와 같은 행 구조로 확정하는 검토는 별도로 남아 있다.','<!-- SSTR-PHOTO-20261016:END -->','']
new='\n'.join(section)+'\n'
if '<!-- SSTR-PHOTO-20261016:BEGIN -->' in body:
    body=re.sub(r'<!-- SSTR-PHOTO-20261016:BEGIN -->.*?<!-- SSTR-PHOTO-20261016:END -->\n*',new,body,flags=re.S)
else:body=body.replace('<!-- E8300-QCC-CURRENT:BEGIN -->',new+'<!-- E8300-QCC-CURRENT:BEGIN -->',1)
registry.write_text(body,encoding='utf-8')

task=m.HERE.parent.parent/'작업.md';body=task.read_text(encoding='utf-8')
newstatus='''<!-- TDC-TASK-STATUS:BEGIN v1 -->
| 항목 | 내용 |
|---|---|
| 상태 | 자료첨부 사본 저장 완료 / 시험28·32 내용 확인 대기 |
| 현재 결과 | E8300-QCC 사본에 녹색 밑줄183셀·삭제25조건·사진123장 반영, 세로8cm·개행·정렬·병합 검증. 원본 보존 |
| 남은 일 | 시험28 알림 ON/OFF, 시험32 시작 볼륨 확인. 기존 QCC/연계 자료 대기 및 기준서와 정식 행 구조 동등성 확정은 별도 |
| 다음 행동 | 사용자 확인을 받으면 해당 조건·결과·판정 및 보조자료를 같은 사본에 반영 |
| 다음 담당 | 은수님 확인 / Codex 반영 |
| 확인 시각 | 2026-10-16 |
<!-- TDC-TASK-STATUS:END -->'''
body=re.sub(r'<!-- TDC-TASK-STATUS:BEGIN v1 -->.*?<!-- TDC-TASK-STATUS:END -->',lambda _:newstatus,body,count=1,flags=re.S)
checks={
1:('적용','기존 E8300-QCC 현재본을 기준으로 단일 보고서를 유지. 기능에 맞는 보조자료 ID 정정. 별도 QCC 추적 체계를 만들지 않음.'),
2:('해당없음','SRS/설계입출력 매트릭스는 수정하지 않음.'),
3:('확인','저장본의 시험83개 ID/순서, 기존 요구사항 열 E/F 유지. 사진123개는 시험·검증·세부 순서로 일대일 연결.'),
4:('확인','두 원본의 실제 셀·리치텍스트·삭제표시를 읽고 녹색183셀/부분2셀/삭제25조건을 분석. 사진 파일 직접 확인 범위는 photo-inventory.json에 구분.'),
5:('해당없음','시스템 구조/펌웨어 동작 설계의 신규 분석·변경 없음.'),
6:('부분 확인','본문→보조자료→사이버 연계→표시를 반복 대조. 기준서/다른 산출물 변경은 이번 요청 밖이며 정식 동등성 검토는 남김.'),
7:('확인','E8300 기반 QCC 사본의 서식을 유지하고 개인본의 실제 셀별 정렬·개행·핑크 ID·녹색 밑줄을 반영.'),
8:('해당없음','상세설계서 수정 없음.'),
9:('확인','시험 방법 열 D의 원문을 유지. 새 I/A/D/T 판정 생성 없음.'),
10:('확인','사진·RTT 첨부만으로 시험 방법이나 신규 제품 통과를 만들지 않음.'),
11:('확인 대기','시험28/32의 조건·결과 불일치를 해당 J/K와 L에 표시. 나머지 지정 준비/절차 수정 및 삭제조건73의 잔존 f_sync/f_close 요구 정리.'),
12:('미실행/범위 예외','사용자가 이번 보고서 사본에 사진용 결과 행 추가·공통 셀 병합·지정 셀 삭제를 명시. 기준서 원본은 보존하므로 기준서/보고서 전체 행 동등성 통과로 기록하지 않음.'),
13:('부분 확인','남은 조건과 검증을 같은 시험 구간에서 대조. 시험28/32는 불일치 원문 보존 및 판정 확인 필요.'),
14:('확인','실제 RTT 사진을 바이트 그대로 첨부. RAM/BOOT_NOT_RUN/SYNTHETIC/시험파일/no system PASS 범위를 결과 본문에 명시.'),
15:('확인','삭제25조건은 재번호하지 않음. 시험·검증 번호와 모든 사진 매핑 오류0, 세부 순서 누락0.'),
16:('확인','시험자 녹색 결과를 반영하고 실제 사진만 사용. 임의 수치/측정 화면/새 실행 결과 생성 없음.'),
17:('확인','녹색183셀은 작성자 수정으로 유지, 시험75의 확인을 통과로 바꾸지 않음. 문서 검사와 제품 시험을 구분.'),
18:('확인','기존 최신 QCC82/83 및 부분녹색 주변 QCC 미실행 상태 보존. 사진의 보조 범위를 시스템 전체 통과로 확대하지 않음.'),
19:('해당없음','새 예상값/실측값 생성 또는 과거 수치 일괄 대체 없음.'),
20:('확인','실측 사진123장/미식별 IMG_5080 제외, 기존 예상 그림 미디어 보존. 지정 삭제25조건과 중복 보조행25개 제거.'),
21:('확인','사진의 날짜를 수정하지 않음. 시험74 첨부 화면의 실제2026-06-13~15를 설명하여 일반 시간 기본일과 혼동하지 않음.'),
22:('확인','변경 안 된 QCC/공동 표시와 실제 미비를 보존. 새28/32 E8300 보완은 파란색·밑줄·기본+2pt. 시험7 LED 자료 요청 유지.'),
23:('확인','중앙 QCC 위치 목록에 새 사본과 최종 현재 셀 좌표 등록. 비교/삭제 이력은 현재 위치에서 제외.'),
24:('확인','개인본 실제 정렬/개행 대조, 원본사진 SHA/8cm/병합/비어있지 않은 병합 내부 셀 검사. 처음 발견한 사진3곳 돌출과 C35정렬 수정 후 네이티브 재검사.'),
25:('확인',f'본문은 적용 상태, 변경 전후 {len(pl["comparisons"])}건은 보고서_시스템 시험_비교에 추가. 앞선 비교 이력 보존.'),
26:('해당없음','Word 파일 수정 없음.'),
27:('확인','G드라이브 동일 폴더 _자료첨부 사본만 신규 저장. 입력2개 SHA 시작/종료 동일, 개인 전용 읽기 전용. 기존 표지/서명/승인은 새 승인으로 변경하지 않음.'),
28:('확인/내용 대기','최종 XLSX 구조 검사 오류0, 실제 Excel 읽기 전용 열기/출력, 사진123장 높이8cm·K열 내부 수용. 문서2항목 확인 대기는 해소로 기록하지 않음.'),
29:('해당없음','Excel 대상. Word 작성 형식 검사 대상 없음.')}
entry=['<!-- SSTR-PHOTO-20261016:BEGIN -->','## 2026-10-16 시스템 시험 보고서 자료첨부 사본','',link,'',
'입력은 2026-10-16 13:53 저장 QCC 보고서와 14:38 저장 김은수 전용 보고서다. 열린 개인 문서의 미저장 메모리는 수정/자동 저장하지 않았다. 원본 두 파일과 사진을 보존하고 요청한 새 사본만 작성했다. 시험38 검증1의 잘못된 시험36 참조는 같은 기능인 시험35(시스템 상태 확인 0x67)로 정리했다.','',
'최종 본문83개 시험, 녹색 수정183셀(부분 수정2셀 포함), 삭제25조건(원본26물리행), 사진123장/89검증 그룹이다. 개인본에 없는 최신 QCC82/83을 보존했다. 보조자료의 현재 조건·결과·판정은 본문과 동기화하고 삭제된 조건의 보조행25개를 제거했다. 삭제/동기화가 제품 시험의 추가 실행이나 승인이라는 의미는 아니다.','',
'두 내용 질문(시험28 검증2 ON/OFF, 시험32 조건2 시작 볼륨)은 아직 답변이 없어 실제 값을 임의로 바꾸지 않았다. 해당 본문과 판정은 확인 필요다. 기준서 원본의 변경은 사용자 요청 범위 밖이며, 사본의 정식 확정 시 공통 행 구조를 맞추는 후속 검토가 남아 있다.','',
f'출력 SHA256: `{sha}`. 원본 보존/저장 근거는 [publication.json](qa/photo-attach-20261016/publication.json), [validation.json](qa/photo-attach-20261016/validation.json), [native-qa.json](qa/photo-attach-20261016/native-qa.json)에서 확인한다.','',
'독립 검사는 요구사항 담당의 [수정 매핑](qa/photo-attach-20261016/edit-audit.json)·[보조자료 대응](qa/photo-attach-20261016/H/support-update-plan.json), 사진 담당의 [사진 검사](qa/photo-attach-20261016/photo-candidate-qa.json), 설계 담당의 [네이티브 표시 검사](qa/photo-attach-20261016/layout-qa.json)로 분담했다. 파일 편집은 루트만 수행했다. 대표 페이지 직접보기와 전체 접촉시트 검토 범위는 각 기록에 구분한다.','',
'| 원칙 | 판정 | 실제 적용/근거와 재검사 |','|---|---|---|']
for i,(status,reason) in checks.items():entry.append(f'| {i} | {status} | {reason} |')
entry+=['','<!-- SSTR-PHOTO-20261016:END -->','']
new='\n'.join(entry)
if '<!-- SSTR-PHOTO-20261016:BEGIN -->' in body:body=re.sub(r'<!-- SSTR-PHOTO-20261016:BEGIN -->.*?<!-- SSTR-PHOTO-20261016:END -->\n*',lambda _:new,body,flags=re.S)
else:body+='\n'+new
task.write_text(body,encoding='utf-8')
print(json.dumps(pub,ensure_ascii=True))
