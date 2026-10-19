"""Close the existing task after the saved/native manifest has passed."""
from pathlib import Path
from datetime import datetime, timezone, timedelta
import hashlib
import json
import re
import sys

sys.stdout.reconfigure(encoding='utf-8')
HERE = Path(__file__).resolve().parent
TASK = HERE.parents[1]
ROOT = HERE.parents[5]
def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))
manifest = read(HERE / 'final-output-manifest.json')
assert len(manifest['files']) == 12
assert all(p['unchanged'] for p in manifest['protected_originals'])
assert read(HERE / 'qcc_requirements/word-visual-coverage.json')['status'] == 'PASS'
assert read(HERE / 'qcc_design/checks-29.json')['status'] == '통과'
assert read(HERE / 'qcc_tests/qa.json').get('final_native_display_pass') is True
for item in manifest['files']:
    assert hashlib.sha256(Path(item['path']).read_bytes()).hexdigest().upper() == item['sha256']
guide = ROOT / 'docs/참고/인허가/사운드1/SW 밸리데이션 문서 작성·검증 지침.md'
titles = [(int(n), title) for n, title in re.findall(r'^### (\d+)\. (.+)$', guide.read_text(encoding='utf-8-sig'), re.M)]
assert [n for n, _ in titles] == list(range(1, 30))
reasons = {
1: '중복 RID82를 기존 RID69/FW15에 통합. 활성82개(1~81,83), 기존 상위 추적 유지, Q 원시 ID를 다른 발생원으로 추가하지 않음. 실제 설계 정보가 본문에서 독립적으로 설명됨.',
2: '실제 읽기 전용 매트릭스에 연결된50항목의 첫 문구 동일. 상세는 다음 줄. 기존 상위가 빈32개 파생 요구에 임의 ID를 만들지 않음.',
3: '요구 계약, SAD 정/역방향 추적, SDDD14유닛/15시험과 세 시험 단계의 실제 ID/기능 연결을 담당 간 교차 확인. 69/시험82와83/ACDOC17,18,20 정합.',
4: '제공 QCC 원본5개와 기존 E8300 원본의 본문/표를 실제 확인. 스마트케이스/CC 미갱신 표기는 외부기 기능으로 판단하고 참고 원문은 보존.',
5: '구조 원본/현재 E8300 근거72파일 차이0, 사용자 확정 DMIC2와 최종 그림/본문 대조. QCC 내부 구현/핀/클록/실측까지 확정하지 않음.',
6: '유닛 시험의 실제 조건에서 지원 요구 누락을 발견해 SAD27칸/SDDD5유닛으로 돌아가 보완. 통합22의 실제 CFX005/008/015와 Q008 역할을 통합 계획/기준/보고에서 맞춤. 혼합/자극 SIT013과 구분하고 반복 재검사.',
7: 'QCC 전용 소속 유닛/원문 추적 틀 제거. 동일한 E8300 설명 수준과 표 구조/폭/본문 기준선 대조, 실제 화면 재검사.',
8: '14유닛의 목적은 표 밖 본문, I/O와 처리 표28개는 기존 체계. 실제 계약/오류/인증/오디오 처리와 남은 미확정 사항을 구체적으로 설명.',
9: 'I/A/D/T 정의를 실제 비교 근거에 맞춰 수정. 정량/배열 비교는 T 또는 A/T, 물리 시연은 D, 분석의 범위를 구분하고 기준/보고 동시 반영.',
10: '31유닛/19시스템 정량 방법을 수정하고 RTT 확인과 물리 계측을 구분. 도구 이름이나 AUTH 패턴 전달을 실제 CRA/전송 암호화 통과로 확대하지 않음.',
11: 'E8300 준비에 들어 있던 호출/입력 변경/측정은 절차로 이동. QCC 조건은 장비/입력/제한/예상 응답과 부족한 계약을 해당 행에 명시.',
12: '세 쌍의 공통 셀/행/순서/병합 차이0. 보고서 결과/판정만 별도 열, 페이지 경계 보완도 기준/보고 공통 구조로 적용.',
13: '구체 입력/예상 바이트/배열과 기존 실제 관찰을 같은 조건에서 비교. 자료가 없는 실제 응답/허용오차를 임의 생성하지 않고 판정 대기 유지.',
14: 'RTT72그룹의 전체 문자열 대조: 71출력의 모든 줄 일치, 나머지1그룹은 실제 로그 없음 조건. 예상/코드 분석/시험자 확인/실측의 증거 구분.',
15: '유닛130/통합24/시스템83 ID,237조건과 검증 번호의 누락/추가0. 신규 QCC38/14/3조건 정확 대응.',
16: '실제 과거 관찰을 남기고 새 결과를 지시문/예상값만으로 통과 처리하지 않음. 신규 QCC 결과와 해당 미측정 사항은 미실행/판정 대기.',
17: '기존 시험자 확인과 실제 수집 로그/코드 분석의 범위를 보존. PC/메모리 관찰을 실제 전체 장치 검증으로 확장하지 않음.',
18: '유닛 서론의 전항목 통과를 과거 E8300 확인으로 제한. 시스템14개 과거 통과와 현재 미측정/상충 판정을 분리, 수신 통과를 송신으로 확대하지 않음.',
19: '새 지정 확인 수치를 생성하지 않음. 과거 승인된 중간값/0 방향 절삭과 관찰값을 보존하며 다른 수치/새 QCC 측정에 확대하지 않음.',
20: '예상 도식/실측/사진 자리와 지정 삭제 범위 유지. 수정 아키텍처 그림은 연결 구조이며 측정 화면으로 표시하지 않음. 새 계측 캡처나 사진을 만들지 않음.',
21: '일반 시간 기준5/2와 결과5/29를 같은 입력/예상값과 구분해 명시. 월말/윤년 경계와 과거 작성/승인 날짜는 보존.',
22: 'Word/Excel 미비 안내 전체의 기본+2pt/빨강·파랑·보라/밑줄 확인. 받은 설계 정보와 잔여 미비를 구분하며 사용자 해제 전 QCC/공동 접두어 유지.',
23: '저장된12현재본의 양수 변경/불변 본문에서 중앙 목록 재수집. 삭제 이력/목차/보조 원문 복제행을 제외하고 실제 위치/상태/확인일과 링크를 갱신.',
24: 'Excel7개 native 실제 열기와 전체 본문/수정 시트 출력, 줄바꿈/정렬/병합/테두리/409.5pt 이하 높이 확인. CFX31의 긴 RTT와 시스템 절차/비교 문구 높이 부족, 짧은 준비 문단 페이지 경계를 기준/보고 동일하게 보완 후 재확인.',
25: '변경 전후를 실제 본문 같은 칸의 빨강[-]/초록[+]로 표시. 긴 불변 RTT 전체는 한 번 유지하고 실질 문단 차이를 비교. rich text 서식 보존.',
26: 'Word5개 김은수 실제 삽입/삭제/서식 추적. 새 변경만 거부하면 직전 본문 복원, 기존 변경 문구/실효 서식과 옛 그림 보존. SAD 빈 문단 후보의 native 셀 소실을 발견해 정확히 복구하고 실제 Word 수락/새 변경 거부로 83행×3셀 보존을 검사. native 저장의 중복 빈 서식 이력 정규화는 기록된 문구/서식 보존과 구분한다. 필드 갱신 전후 일반 본문/그림/변경 텍스트 동등, 최종 G 저장본의 실제 Word 수락 결과도 83행×3셀 보존.',
27: '보호 원본23개 해시 동일. 수정 직전12현재본 사본 확보, 기존 G 경로와 [E8300-QCC] 이름 유지. 새 승인/서명/실기 결과 생성 없음.',
28: '최종 저장본 SHA를 native readonly 열기/Word 전체 페이지/Excel 출력/PDF/PNG 및 담당 시각 검사에 연결. XML 검사와 실제 열기/표시 검사를 별도로 완료.',
29: '실제 DOTX59스타일/지원 기반을 적용하고 번호/제목/10pt본문/1.25줄간격/뒤8pt/표/그림/캡션/목차 확인. 정상 크기 빈 문단과 기존 삭제 이력을 구분.'
}
evidence = {
    'requirements': 'qcc_requirements/review-29.json',
    'design': 'qcc_design/checks-29.json',
    'tests': 'qcc_tests/qa.json',
    'saved': 'saved-package-checks.json',
    'native_words': 'final-word/requirements-native.json + design-native.json',
    'native_excel': 'native-excel/SRS-full-native.json + tests-native.json',
    'positions': 'current-positions.json',
    'manifest': 'final-output-manifest.json'
}
evidence['word_visual_coverage'] = 'qcc_requirements/word-visual-coverage.json'
evidence['qa_profile_cleanup'] = 'Word-QA-profile-cleanup-final.json + Word-user-confirmed-settings.json'
now = datetime.now(timezone(timedelta(hours=9))).isoformat()
checks = [{'number': n, 'title': t, 'status': '통과(문서 검증)',
           'inspection_and_result': reasons[n], 'scope': '수정된12문서; 실제 제품 실행/승인은 별도'} for n,t in titles]
closing = {'checked_at': now, 'guide_sha256': hashlib.sha256(guide.read_bytes()).hexdigest().upper(),
           'checks': checks, 'evidence': evidence, 'current_hashes': {f['code']: f['sha256'] for f in manifest['files']},
           'product_state': '신규 QCC 38/14/3조건 미실행/판정 대기, 잔여 계약/검증 자료는 실제 본문 접두어에 표시'}
(HERE / 'integrated-29-review.json').write_text(json.dumps(closing, ensure_ascii=False, indent=2), encoding='utf-8')
path = TASK / '작업.md'
content = path.read_text(encoding='utf-8-sig')
status = '''<!-- TDC-TASK-STATUS:BEGIN v1 -->
| 항목 | 내용 |
|---|---|
| 상태 | 완료(요청한 문서 갱신/검증) |
| 현재 결과 | 2026-10-13 기존 통합12문서를 최신29원칙으로 수정/재검사. 아키텍처 그림 반영, 요구/설계/시험 반복 정합, 최종 native 표시와 원본23개 보존 확인. 제품 시험/새 승인은 별도 |
| 남은 일 | 이번 문서 갱신 범위의 미완료 없음. 실제 QCC 구현 계약/실행/측정 및 사용 설명서 검토는 본문과 중앙 목록의 후속 자료 대기 |
| 다음 행동 | 실제 후속 자료가 주어지면 해당 조건/앞뒤 문서와 판정을 재검사. QCC/공동 접두어는 사용자 해제 전 유지 |
| 다음 담당 | 자료/시험 담당자와 Codex(후속 문서 갱신 요청 시) |
| 확인 시각 | ''' + now + ''' |
<!-- TDC-TASK-STATUS:END -->'''
content = re.sub(r'<!-- TDC-TASK-STATUS:BEGIN v1 -->.*?<!-- TDC-TASK-STATUS:END -->', lambda m: status, content, count=1, flags=re.S)
content = content.replace('최종 내용/서식/전체 렌더·중앙 목록의 재검사는 진행 중이다. 현재 소스 해시와 재검사 결과는 `qa/review-20261013`에 기록하며 최종 저장본과 일치한 뒤 완료 판정을 남긴다.',
                          '아래 최종 재검사를 완료했다. 현재 소스 해시와 실제 저장본/native 출력의 대응은 `qa/review-20261013` 및 최종 산출물 목록에서 확인한다.')
word_pages = sum(p['pages'] for f in manifest['files'] if f['path'].endswith('.docx') for p in f['native_previews'])
excel_pages = sum(p['pages'] for f in manifest['files'] if f['path'].endswith('.xlsx') for p in f['native_previews'])
tail = ['## 2026-10-13 최종 재검사와 저장본', '',
        '요청한12개 기존 통합 사본의 문서 갱신을 완료했다. 파일 이름의 `[E8300-QCC]`는 유지한다. 이 절은 앞선 시작/실패/보류 이력 이후의 현재 결과이며, 제품 시험 통과나 새 승인을 뜻하지 않는다.', '',
        '- 최종 저장본12개의 실제 SHA는 [최종 산출물 목록](evidence/final-output-manifest.json)에 있다. 보호 원본23개는 시작 기준과 동일하다.',
        f'- 실제 저장본을 숨김 Office에서 읽기 전용으로 열어 Word5개 {word_pages}페이지 전체와 Excel7개 본문/수정 시트 {excel_pages}페이지를 출력했다. 원본 파일의 수락/거부 상태와 인쇄 설정은 미리보기 때문에 바꾸지 않았다. 전체/표적 PNG 검사와 최종 SHA 연결은 담당 QA와 목록에 남겼다.',
        '- Excel의 빈 rich run이 실제 Open을 막는 오류를 찾아 빈 run만 제거했다. 실제 텍스트/색/폰트/밑줄/비어 있지 않은 run/다른 파트는 동일하며 실제 Excel에서 열기/출력을 재검사했다.',
        '- 유닛 서론의 전체 통과 확대, 일반 시간 입력의 기준/결과 날짜, 시스템 판정 글자의 페이지 경계, 긴 변경 문단의 페이지 경계를 실제 화면에서 찾아 보완했다. 기존 실제 관찰과 신규 QCC 미실행은 유지했다.',
        '- SAD 목차 뒤 빈 페이지/중복 설명/추적 표 흐름, SIP OTA 안내 고아 문단, SDDD 짧은 키 역할 행과 OTA 안내 분리를 실제 Word 화면에서 찾아 보완했다. SDDD DAC 설명의 표 번호를 실제 표146으로 고쳤다. SAD 빈 문단 삭제 후보에서 실제 Word 수락 시 셀 내용이 소실됨을 발견해 직전 바이트로 복구한 이력을 남겼고, 실제 셀 수락/새 변경 거부 보존을 통과한 후보만 반영했다.',
        '- Word QA v1은 계정 작성자 우선 설정 때문에 임시 작성자가 적용되지 않아 폐기했다. 중단 후 남은 해당 QA 프로세스만 생성시각/PID로 확인해 닫았다. 기록하지 못한 과거 이니셜을 추정해 복원했다고 주장하지 않았으며 사용자 확인을 받아 사용자 이름 김은수/이니셜 eunsu.kim으로 실제 적용·역검사했다. 이후 검증은 시작 옵션을 변경 전에 기록하고 종료 후 대조한다. 문서의 변경 작성자와 제품 시험/승인 상태는 별도 검사했다.',
        '- 구조 근거72개 소스의 차이는 없다. 온라인 매트릭스는 읽기 전용 자료로만 확인했다. QCC 잔여 설계/실행 근거와 설명서 검토는 담당별 안내에 남기고 [중앙 위치 목록](../../../참고/인허가/사운드1/QCC%20자료%20추가%20위치%20관리.md)을 같은 작업에서 갱신했다.', '',
        '아래29항목은 관련 문서/서식/추적/저장 검증의 판정이다. 19번의 새 결과 생성은 해당없지만 기존 지정 수치 처리의 보존을 검사했다. 형식별 항목은 해당 Word/Excel에만 적용한다. 실제 QCC38/14/3조건과 미측정/상충 항목의 제품 판정은 대기다.', '',
        '| 원칙 | 최종 검사 결과와 범위 | 판정 |', '|---|---|---|']
tail += [f'| {n}. {t} | {reasons[n]} | 통과(문서) |' for n,t in titles]
tail += ['', '통합 근거: [29원칙 최종 검사](qa/review-20261013/integrated-29-review.json), [저장본/보호 원본/공통 구조](qa/review-20261013/saved-package-checks.json), [요구사항 QA](qa/review-20261013/qcc_requirements/review-29.json), [설계 QA](qa/review-20261013/qcc_design/checks-29.json), [시험 QA](qa/review-20261013/qcc_tests/qa.json), [현재 미비 위치](qa/review-20261013/current-positions.json).', '',
         '미비 접두어는 자료 반영 여부만으로 제거하지 않았다. 후속 제품 자료를 반영할 때 같은 입력의 예상/관찰/판정을 갱신하고, 영향을 받는 앞뒤 문서와 중앙 목록을 다시 검사한다.', '']
marker = '## 2026-10-13 최종 재검사와 저장본'
if marker in content:
    content = content[:content.index(marker)]
path.write_text(content.rstrip()+'\n\n'+'\n'.join(tail), encoding='utf-8')
print(json.dumps({'checks': len(checks), 'word_pages': word_pages, 'excel_pages': excel_pages, 'task': str(path)}, ensure_ascii=False))
