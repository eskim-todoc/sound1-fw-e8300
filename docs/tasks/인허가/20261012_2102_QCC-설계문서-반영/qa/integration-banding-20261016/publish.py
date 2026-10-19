from banding import *
from urllib.parse import quote
def publish():
 assert digest(TARGET)==BASE_SHA,'Target changed; do not overwrite'
 verify()
 candidate=H/'candidate.xlsx'
 shutil.copyfile(candidate,TARGET)
 assert digest(TARGET)==digest(candidate)
 save('publication.json',{'target':str(TARGET),'beforeSHA':BASE_SHA,'afterSHA':digest(TARGET),'status':'PASS','backup':str(H/'before.xlsx')})
 project=H.parents[5]
 registry=project/'docs/참고/인허가/사운드1/QCC 자료 추가 위치 관리.md'
 text=registry.read_text(encoding='utf-8');lines=text.splitlines();url=quote(str(TARGET).replace('\\','/'),safe='/:')
 for i,line in enumerate(lines):
  if line.startswith('| QCC-NEW-SITR-Q /'):
   lines[i]=f'| QCC-NEW-SITR-Q / [SITR]({url}) | QCC (현재 본문에 미비 접두어 없음) | `보고서_통합시험!D19`; 시험 `SW-FW-IntegrationTest-ID-020, 023, 024` / `B136:O136, B142:O144` | 사용자 지정 `[완료]` 현재본의 실제 위치 확인. 이번 변경은 시험별 배경색만 적용했으며 기존 내용/판정 유지. 현재 본문에서 QCC 미비 접두어 미검출 / 2026-10-16 |'
  elif line.startswith('| QCC-NEW-SITR-QE /'):
   lines[i]=f'| QCC-NEW-SITR-QE / [SITR]({url}) | QCC/E8300 (현재 본문에 미비 접두어 없음) | 시험 `SW-FW-IntegrationTest-ID-018, 019, 021, 022` / `보고서_통합시험!B131:O135, B137:O141` | 사용자 지정 `[완료]` 현재본의 실제 위치 확인. 이번 변경은 시험별 배경색만 적용했으며 기존 내용/판정 유지. 현재 본문에서 공동 미비 접두어 미검출 / 2026-10-16 |'
 registry.write_text('\n'.join(lines)+'\n',encoding='utf-8')
 task=H.parent.parent/'작업.md'
 with task.open('a',encoding='utf-8') as f:
  f.write('''
## 2026-10-16 완료 통합시험 보고서의 시험별 배경색 정리

사용자 지정 `[E8300-QCC] [완료] SITR-FW-TD24A 소프트웨어 통합시험 보고서.xlsx`의 `보고서_통합시험!B25:O144`를 수정했다. 001~024의 홀수 시험은 흰색 `#FFFFFF`, 짝수 시험은 기존 연회색 `#F5F5F5`이며 한 시험의 모든 행/병합 셀에 같은 색을 적용했다. 기존 파일의 채우기 외 내용, 수식, 글꼴, 정렬, 테두리, 병합, 행높이, 열폭, 인쇄설정과 비교 시트를 보존했다.

- 주 작업자: 수정·원본 대비 검사·저장. 독립 검토자 qcc_design: 같은 후보의 숨김 Excel 읽기 전용 열기와 배경색/대표 화면 확인. 원본 편집 및 타 작업 변경 되돌리기 없음.
- [검사 근거](qa/integration-banding-20261016/validation.json): 24시험 1,680셀 색상 전수 확인. 기존 셀 값과 채우기 외 속성 동일, 다른 ZIP 부품은 바이트 동일. [저장 근거/복구본 위치](qa/integration-banding-20261016/publication.json).
- SV 원칙 7·22·24·25·27·28: 이번 색상 변경 및 원본 서식/내용/판정/비교 시트 보존 범위에서 통과. 새로운 본문 비교 표식이나 시트는 추가하지 않았다.
- SV 원칙 23: 현재 `[완료]` 파일명과 실제 시험 위치로 중앙 목록을 갱신했다. 접두어는 작업 전부터 본문에 없었으며 이번 작업에서 제거하지 않았다. 완료 파일명만으로 제품 시험/승인을 새로 판정하지 않았다.
- SV 원칙 1~6·8~21·26·29: 내용·추적·시험 실행·Word 작성은 이번 배경색 변경 범위에 해당없음. 기준서 내용 전체 재검증과 제품 재시험은 미실행.

''')
 print(json.dumps({'published':str(TARGET),'sha':digest(TARGET)},ensure_ascii=True))
if __name__=='__main__':publish()
