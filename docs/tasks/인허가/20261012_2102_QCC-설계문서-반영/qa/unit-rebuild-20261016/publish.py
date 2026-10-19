from rebuild import *
from urllib.parse import quote
import os

TASK=H.parent.parent/'작업.md'
REG=H.parents[4]/'참고/인허가/사운드1/QCC 자료 추가 위치 관리.md'
BEGIN='<!-- SUVR-REBUILD-20261016:BEGIN -->';END='<!-- SUVR-REBUILD-20261016:END -->'
def task_record(done=False):
 text=TASK.read_text(encoding='utf-8');p=readj('plan.json')
 status='재작성 완료 / 저장 검증 통과' if done else '유닛 보고서 재작성 중 / 내용 보존 검사 통과, native 표시 검사 진행'
 content=f'''{BEGIN}
## 2026-10-16 유닛 검증 보고서 재작성

상태: {status}.

사용자 요청은 설계 검증의 `[E8300]` 보고서를 기준으로 기존 `[E8300-QCC]`에서 신규 QCC 시험만 맨 아래에 추가하고 `_재작성.xlsx`를 만드는 것이다. 기존 E8300 내용/행 높이/열 너비/병합/인쇄 서식을 보존한다. 참조 원본, 기존 보고서와 기준서, 펌웨어는 수정하지 않는다.

입력은 `김은수_작성/[E8300] SUVR-FW-TD24A 소프트웨어 유닛 검증 보고서.xlsx`와 같은 단계 외부기의 `[E8300-QCC] SUVR-FW-TD24A 소프트웨어 유닛 검증 보고서.xlsx`다. [입력 해시](qa/unit-rebuild-20261016/baseline.json)를 고정하고 원본을 읽기 전용으로 확인했다.

수용 기준은 기존 115개 시험의 내용/서식 보존, 신규 QCC 15개 시험/38조건의 누락 및 중복 없음, 미실행/판정 대기 보존, 끝 검토 질문과 인쇄 범위 보존, 저장본의 실제 Excel 표시 확인이다. 기존 E8300 시험은 1726행에서 끝나며 신규 항목은 1727~1764행, 뒤 검토 영역은 기존 위치에서 38행 이동한다. 공통 안내 D19는 실제 QCC 도구와 남은 자료로, D24는 기존 E8300 시험자의 확인 범위로 한정한다.

제작은 주 작업자가 전담한다. 독립 내용 검토자는 QCC 신규 ID/유닛/조건 및 기준서 공통 내용 대조, 독립 시각 검토자는 소유한 숨김 Excel 인스턴스의 읽기 전용 열기/렌더를 맡는다. 같은 후보 해시에서 결과를 수렴하며 원본 및 다른 작업자의 편집은 되돌리지 않는다.

- [신규 범위 조사](qa/unit-rebuild-20261016/content-inventory.json): 기존 115개와 QCC 15개 시험, 14개 상세설계 유닛, 38조건. 기존 QCC본의 E8300 부분은 지정 베이스와 265개 비공란 셀에서 달라서 베이스 구간을 그대로 보존한다.
- [구조와 보존 검사](qa/unit-rebuild-20261016/validation.json): 기존 22,353개 셀, 행 속성, 병합, 열폭, 표지와 무관한 부품을 원본과 직접 대조. 신규 341개 비공란 셀과 공통 안내 2개를 별도 비교 시트 343건으로 기록한다. 비교 표식은 본문에 넣지 않는다.
{END}
'''
 if BEGIN in text:text=text[:text.index(BEGIN)]+content+text[text.index(END)+len(END):]
 else:text+='\n'+content
 TASK.write_text(text,encoding='utf-8')

def publish():
 baseline=readj('baseline.json');sha=digest(H/'candidate.xlsx')
 evidence={name:readj(name) for name in ['validation.json','content-final.json','layout-final.json']}
 for name,result in evidence.items():
  assert result['status'] in ['PASS','통과'],(name,result['status'])
  assert sha in json.dumps(result),name
 assert digest(BASE)==baseline['baseSHA'] and digest(QCC)==baseline['qccSHA']
 assert not OUT.exists(),'Existing user file must not be overwritten.'
 with OUT.open('xb') as f:f.write((H/'candidate.xlsx').read_bytes());f.flush();os.fsync(f.fileno())
 assert digest(OUT)==sha and digest(BASE)==baseline['baseSHA'] and digest(QCC)==baseline['qccSHA']
 result={'saved':True,'output':str(OUT),'outputSHA':sha,'baseUnchanged':True,'qccUnchanged':True,'existingTests':115,'addedTests':15,'addedUnits':14,'addedConditions':38,'newRange':'보고서_유닛 시험!1727:1764','validation':list(evidence)}
 savej('publication.json',result)
 task_record(True)
 task=TASK.read_text(encoding='utf-8')
 audit='''
| SV 원칙 | 범위와 근거 | 문서 검사 판정 |
|---|---|---|
| 1, 3, 4 | 기존 115 시험/상세설계 연결을 보존하고 신규 QCC 15 시험/14 유닛/38 조건의 ID와 본문을 대조 | 통과(이식 범위) |
| 2, 5, 8 | SRS/상위 매트릭스/펌웨어 구조/상세설계의 새 정의는 없음 | 해당없음 |
| 6, 7 | 원본 E8300 서식을 기준으로 QCC 추가 범위만 이식. 비교/본문/미비 목록을 최종 후보에서 재검사 | 통과 |
| 9~11, 13~21 | 신규 시험의 방법, 준비, 절차, 조건/검증 번호, 예상 및 실제 결과 문구를 원본 QCC 보고서와 대조. 전38조건 미실행/판정 대기 유지, 새 실행 결과/측정값 생성 없음 | 통과(기존 내용 이식/판정 보존). 제품 시험 미실행 |
| 12 | QCC 신규38조건의 B:K 공통380셀을 현재 기준서와 ID/조건으로 대조 | 통과: 정확한 본문 일치. 기존115 시험 전체와 기준서의 재정비는 요청 범위 밖이므로 미실행 |
| 22, 23 | 남은 QCC/공동 미비 문구와 지정 서식 보존. 새 사본의 실제 좌표를 중앙 목록에 등록 | 통과 |
| 24, 25 | 기존 행높이/열폭/병합/테두리/정렬 보존, 새38조건과 비교343건 표시 검증. [-]/[+]는 별도 비교 시트 | 통과 |
| 26, 29 | Word 문서 변경 없음 | 해당없음 |
| 27, 28 | 원본 2개 해시 불변, 지정 G 폴더 새 파일, 최종 후보 Excel 읽기 전용 열기/표시 검토와 실제 저장본 해시 대조 | 통과 |

독립 [내용 검사](qa/unit-rebuild-20261016/content-final.json), [native 표시 검사](qa/unit-rebuild-20261016/layout-final.json), [저장 근거](qa/unit-rebuild-20261016/publication.json)를 같은 최종 후보로 확인했다. 문서 재작성 완료는 QCC 제품 시험 통과 또는 새 승인이 아니다.
'''
 task=task.replace(END,audit+'\n'+END,1)
 current='''<!-- TDC-TASK-STATUS:BEGIN v1 -->
| 항목 | 내용 |
|---|---|
| 상태 | 유닛 검증 보고서 재작성 사본 작성 및 저장 검증 완료 |
| 현재 결과 | E8300 기존115시험의 내용/서식 보존, QCC15시험38조건을 끝에 추가. 기준서 공통380셀 일치, native 표시 검사 통과 |
| 남은 일 | 이번 재작성 범위 없음. QCC 실제 시험 실행/자료/판정 대기는 본문과 중앙 목록에 유지 |
| 다음 행동 | 후속 자료 또는 수정 요청 시 해당 사본의 관련 항목 갱신 |
| 다음 담당 | 사용자 요청 / Codex |
| 확인 시각 | 2026-10-16 |
<!-- TDC-TASK-STATUS:END -->'''
 task=re.sub(r'<!-- TDC-TASK-STATUS:BEGIN v1 -->.*?<!-- TDC-TASK-STATUS:END -->',lambda _:current,task,count=1,flags=re.S)
 TASK.write_text(task,encoding='utf-8')
 text=REG.read_text(encoding='utf-8');loc=readj('validation.json')['prefixLocations']
 link=quote(str(OUT).replace('\\','/'),safe='/:')
 sec=f'''\n<!-- SUVR-REBUILD-20261016:BEGIN -->
## 유닛 검증 보고서 재작성 사본 — 2026-10-16

[유닛 검증 보고서 재작성 사본]({link})은 `김은수_작성`의 E8300 보고서를 베이스로 기존115시험의 내용/서식을 보존하고, QCC 신규15시험(14개 상세설계 유닛)의38조건을 목록 끝에 추가한 문서다. 기존 두 보고서는 보존한다. 새 사본의 QCC 시험은 `보고서_유닛 시험!D1727:D1764`의 `SW-FW-UnitTest-QCC-ID-1~15`로 찾는다. 38조건의 실제 실행 결과는 미제공이며 미실행/판정 대기를 유지한다.

| 식별 | 담당/검색 문구 | 현재 본문 위치 | 상태/확인일 |
|---|---|---|---|
'''
 for i,(prefix,addresses) in enumerate(loc.items(),1):sec+=f'| QCC-SUVR-REBUILD-{i:02d} | `{prefix}` | `보고서_유닛 시험!'+', '.join(addresses)+'` | 내용 반영 및 접두어 보존 / 남은 설정·실행 자료는 해당 문구 참조 / 2026-10-16 |\n'
 sec+='\n비교 시트는 현재 미비 위치에서 제외했다. 원본 E8300 앞부분과 마지막 검토 질문의 내용은 유지하되 D19의 도구/자료 안내와 D24의 기존 시험자 확인 범위를 명확히 했다. [저장 및 검사 근거](../../../tasks/인허가/20261012_2102_QCC-설계문서-반영/qa/unit-rebuild-20261016/publication.json).\n<!-- SUVR-REBUILD-20261016:END -->\n'
 marker='<!-- SUVR-REBUILD-20261016:BEGIN -->';assert marker not in text
 REG.write_text(text+sec,encoding='utf-8')
 print(json.dumps(result,ensure_ascii=True))

if __name__=='__main__':
 if sys.argv[1]=='record':task_record()
 if sys.argv[1]=='publish':publish()
