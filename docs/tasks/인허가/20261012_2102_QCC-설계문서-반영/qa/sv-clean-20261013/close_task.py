from pathlib import Path
from datetime import datetime
import re,json
import prepare_clean as p

h=Path(__file__).resolve().parent;task=h.parents[1]
manifest=p.load(h/'final-output-manifest.json');pub=p.load(h/'published.json');render=p.load(h/'render-checks.json');visual=p.load(h/'visual-review.json')
assert pub['pass'] and visual['pass'] and render['allMainMarkerFree']
stamp=datetime.now().astimezone().isoformat(timespec='seconds')
path=task/'작업.md';text=path.read_text(encoding='utf-8-sig')
block='\n'.join(['<!-- TDC-TASK-STATUS:BEGIN v1 -->','| 항목 | 내용 |','|---|---|','| 상태 | 완료(SV Excel 본문과 변경 비교 시트 분리) |','| 현재 결과 | SV 약칭/25번 지침 갱신. Excel7개 원래9시트는 최종 내용만 표시하고 원문 비교는9개 _비교 시트에 보존. G 현재본 저장/검증 완료 |','| 남은 일 | 이번 요청 범위의 잔여 작업 없음. 기존 QCC 실제 자료·미실행 판정 대기는 문서와 중앙 목록에 유지 |','| 다음 행동 | 다음 SV 문서 작업 시 같은 지침의 시작·작성 중·최종 검사를 적용 |','| 다음 담당 | 사용자 검토 후 필요한 다음 요청 / Codex |',f'| 확인 시각 | {stamp} |','<!-- TDC-TASK-STATUS:END -->'])
text=re.sub(r'<!-- TDC-TASK-STATUS:BEGIN v1 -->.*?<!-- TDC-TASK-STATUS:END -->',lambda _:block,text,count=1,flags=re.S)
heading='## 2026-10-13 후속 — SV Excel 본문과 변경 비교 분리'
assert heading not in text
text+='\n\n'+heading+'\n\n'
text+='SV는 Software Validation의 프로젝트 약칭으로 정했다. 루트 문서 기본 서식, 프로젝트 AGENTS.md와 29개 작성·검증 지침의25번을 함께 갱신했다. 별도 언급이 없어도 SV 문서 작업의 시작·작성 중·최종 검사에서 적용한다. 원래 시트의 예상값/실제 시험 결과 비교는 그대로 유지하고, 문구 변경 이력만 별도 `_비교` 시트에서 확인한다.\n\n'
text+='현재 Excel7개의 비교표식 셀1507개를 정리하고 SRS의 일반 추가문구9셀 색상을 본문색으로 복원했다. 본문에는 적용 내용과 기존 미비 표시를 남겼으며, 원래의 rich text와 삭제/추가 이력은9개 비교 시트에 보존했다. SRS의 삭제 전용 행62/68/81은15pt로 줄이고 폐기 RID82의 빈 항목행50을 숨겨 빈 페이지와 고아 분류를 정리했다. 셀 주소·수식·ID는 이동하지 않았고 비교 시트의 행 높이는 원문 그대로다.\n\n'
text+='1차 native 검사에서 복제된 비교 탭의 선택 상태가 본문과 그룹을 만드는 문제를 발견했다. 비교 탭만 비선택으로 고치고 원래 활성 탭은 보존한 뒤 다시 확인했다. 편집 도구의 빈 shared-string 가져오기와 비교 병합 복사 문제도 실제 저장본 검사로 확인하여 바로잡았고, 원본의 메모·그림·프린터 설정 등은 유지했다.\n\n'
text+=f"검증 결과: 본문 비교표식0, 비교 시트 원문/현재문구 대조 및3쌍 기준서·보고서 공통값 일치, SRS 본문/비교 ID 수식164개 native 값 일치. 실제 Excel에서7개 파일을 읽기 전용으로 열었고 본문9시트의 전체 인쇄영역({render['mainPages']}쪽)에서 표식이 없는 것을 확인했다. 이미지 확인은 SRS 본문과 비교 발췌 전체 및 시험 문서의 대표 범위이며, 시험 문서 모든 페이지의 육안 확인으로 확대하지 않는다. 현재 미비 위치{manifest['current_location_count']}건은 비교 이력을 제외한 본문에서 재수집했다.\n\n"
text+='보호 원본23개와 이번 후속 범위 밖의 Word5개는 해시가 같다. 새 제품 시험, 측정값, 서명·승인은 만들지 않았다. 실제 G 저장본7개는 검증한 후보와 SHA256이 같으며 이전 산출물 기록과 복구 사본은 이력으로 남겼다.\n\n'
text+='근거: [현재 산출물 기록](evidence/final-output-manifest.json), [분리 계획·패키지 및 공통값 검사](qa/sv-clean-20261013/plan.json), [독립 검토](qa/sv-clean-20261013/independent-review.json), [실제 Excel 검사](qa/sv-clean-20261013/native-checks.json), [표시 검토](qa/sv-clean-20261013/visual-review.json), [지침 검토](qa/sv-clean-20261013/policy-review.json), [저장본 및 보호 파일 해시](qa/sv-clean-20261013/published.json).\n'
path.write_text(text,encoding='utf-8')
print(json.dumps({'taskUpdated':True,'at':stamp}))
